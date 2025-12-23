//
// config.c — implementation for SURGE run context
//

#include "config.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   /* fileno */

#include "element.h"
#include "gtools.h"
#include "utils.h"

#ifndef ZLIB
  /* make it obvious at link-time if someone references gz* accidentally */
#endif

/* ---- TLS bridge ------------------------------------------------------- */
static _Thread_local SurgeCtx *g_tls_ctx = NULL;

void surge_set_thread_ctx(SurgeCtx *ctx) { g_tls_ctx = ctx; }
SurgeCtx* surge_get_thread_ctx(void)     { return g_tls_ctx; }

/* ---- Initialization --------------------------------------------------- */
void surge_ctx_init(SurgeCtx *ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    /* Flags (defaults) */
    F.uswitch    = false;
    F.verbose    = false;
    F.outlevel   = 4;     /* default if -O not provided */
    F.smiles     = false;
    F.alphabetic = false;
    F.gzip       = false;
    F.planar     = false;
    F.bipartite  = false;
    F.xswitch    = false;
    F.notriples  = false;
    F.tswitch    = false;
    F.fswitch    = false;
    F.pswitch    = false;
    F.vvswitch   = false;
    F.vv_max_procs = 1;
    F.zw_enable = false;
    F.zw_maxpairs = 2;
    F.ions      = false;

    /* Input */
    IN.formula = NULL;
    IN.extra1  = NULL;

    /* Limits */
    L.maxc    = 4;
    L.maxd    = 4;
    L.mine    = 0;
    L.maxe    = NOLIMIT;
    L.maxbond = 2;

    /* Split */
    SP.res = 0;
    SP.mod = 1;

    ST.genggood = 0;
    ST.gengout  = 0;
    ST.vcolgout = 0;
    ST.multigout= 0;
    ST.maxegroup = 0;
    ST.maxvgroup = 0;
    ST.multignontriv = 0;
    ST.vcolgnontriv = 0;
    ST.skipped = 0;

    /* Rings / cycles */
    R.needcycles = false;
    R.maxcycles  = MAXCYCLES;
    R.min3rings = R.max3rings = 0;
    R.min4rings = R.max4rings = 0;
    R.min5rings = R.max5rings = 0;
    for (int i=0;i<=BADLISTS;i++) R.bad[i] = false;
    R.cyclecount = 0;
    for (int i=0;i<=MAXN;i++) {
        R.count3ring[i] = 0;
        R.count4ring[i] = 0;
        R.count5ring[i] = 0;
        R.cycle34verts[i] = 0;
    }
    for (int i=0;i<MAXCYCLES;i++) R.inducedcycle[i] = 0;

    /* Formula state */
    FM.nv = 0; FM.numtypes = 0; FM.valencesum = 0;
    FM.min1 = FM.min12 = 0;
    FM.max34 = FM.max4 = 0;
    memset(FM.elementtype, 0, sizeof(FM.elementtype));
    memset(FM.elementcount, 0, sizeof(FM.elementcount));
    memset(FM.maxtype, 0, sizeof(FM.maxtype));
    memset(FM.newlabel, 0, sizeof(FM.newlabel));
    FM.canonform[0] = '\0';
    FM.numelements = 0;
    FM.hydrogens = 0;
    FM.needcoordtest = false;
    memset(FM.hyd, 0, sizeof(FM.hyd));

    /* Group buffers */
    GR.vgroupsize = GR.egroupsize = 0;
    GR.vgroupalloc = GR.egroupalloc = 0;
    GR.vgroup = GR.egroup = NULL;
    GR.vgroupcount = 0;

    /* Graph workspace */
    G.numedges = 0;
    for (int i=0;i<MAXN;i++) {
        G.deg[i] = 0;
        G.multdeg[i] = 0;
        for (int j=0;j<MAXN;j++) G.edgenumber[i][j] = -1;
    }

    /* SMILES scratch */
    SM.smileslen = 0;

    /* Output */
    O.filename = "stdout";
    O.outfile  = NULL;
#ifdef ZLIB
    O.gzoutfile = NULL;
#endif
}

/* Reset just the per-candidate graph area (size n) */
void surge_ctx_reset_graph(SurgeCtx *ctx, int n)
{
    if (n < 0) n = 0;
    if (n > MAXN) n = MAXN;

    G.numedges = 0;
    for (int i=0;i<n;i++) {
        G.deg[i] = 0;
        G.multdeg[i] = 0;
        for (int j=0;j<n;j++) G.edgenumber[i][j] = -1;
    }
    SM.smileslen = 0;

    /* per-candidate ring counters (if used that way) */
    for (int i=0;i<=n;i++) {
        R.count3ring[i] = 0;
        R.count4ring[i] = 0;
        R.count5ring[i] = 0;
        R.cycle34verts[i] = 0;
    }
    R.cyclecount = 0;
}

/* ---- Finalization (derive state, validate, open output) --------------- */
static int open_output(SurgeCtx *ctx)
{
#ifdef ZLIB
    if (F.gzip) {
        if (!O.filename ||
            strcmp(O.filename,"stdout")==0 ||
            strcmp(O.filename,"-")==0) {
            O.gzoutfile = gzdopen(fileno(stdout), "wb");
        } else {
            O.gzoutfile = gzopen(O.filename, "wb");
        }
        if (!O.gzoutfile) return SURGE_ERR_OPEN_OUTPUT;
        /* If your zlib lacks gzbuffer, it will just be a no-op/warning */
        (void)gzbuffer(O.gzoutfile, 1<<16);
        configure_error_log_from_output(O.filename);
        return SURGE_OK;
    }
#else
    (void)ctx; /* avoid unused warning if not using gzip path below */
#endif

    if (!O.filename ||
        strcmp(O.filename,"stdout")==0 ||
        strcmp(O.filename,"-")==0) {
        O.outfile = stdout;
        return SURGE_OK;
    }

    /* Replicate old behavior: create/truncate then reopen append */
    FILE *f = fopen(O.filename, "w");
    if (f) fclose(f);
    O.outfile = fopen(O.filename, "a");
    if (!O.outfile) return SURGE_ERR_OPEN_OUTPUT;

    return SURGE_OK;
}

int surge_ctx_finalize(SurgeCtx *ctx)
{
#ifndef ZLIB
    if (F.gzip) return SURGE_ERR_ZLIB_DISABLED;
#endif
    if (F.alphabetic && F.smiles) return SURGE_ERR_A_AND_S;

    /* Derive maxbond from -T/notriples */
    L.maxbond = (F.notriples ? 1 : 2);

    /* Choose output filename default if missing */
    if (!O.filename || strcmp(O.filename,"-")==0)
        O.filename = "stdout";

    /* Badlist & cycle normalizations (mirror legacy main) */
    if (R.bad[5]) R.bad[6] = false; /* bad6 ⊂ bad5 */
    if (F.notriples) R.bad[1] = false;

    if (F.tswitch && F.fswitch &&
        (R.max3rings + R.max4rings) <= 1) {
        R.bad[9] = false;
    }

    R.needcycles =
        (R.bad[1] || R.bad[2] || R.bad[3] ||
         R.bad[4] || R.bad[6]);

    if (F.fswitch && R.max4rings < 6) R.bad[7] = false;

    if (F.tswitch && R.max3rings < 3) R.bad[8] = false;
    if (F.fswitch && R.max4rings < 2) R.bad[8] = false;
    if (F.pswitch && R.max5rings == 0) R.bad[8] = false;

    /* Validate residue/mod */
    if (SP.mod <= 0) SP.mod = 1; /* be forgiving */
    if (SP.res < 0 || SP.res >= SP.mod)
        return SURGE_ERR_BAD_M_RANGE;

    /* Open output stream(s) */
    return open_output(ctx);
}

/* ---- Close output ----------------------------------------------------- */
void surge_close_output(SurgeCtx *ctx)
{
#ifdef ZLIB
    if (F.gzip) {
        if (O.gzoutfile) {
            gzclose(O.gzoutfile);
            O.gzoutfile = NULL;
        }
        return;
    }
#endif
    if (O.outfile && O.outfile != stdout) {
        fclose(O.outfile);
    }
    O.outfile = NULL;
}

/* Call this ONCE after you finish parsing -E options and know numelements */
void surge_snapshot_element_runtime(SurgeCtx *ctx)
{
    for (int i = 0; i < MAXELEMENTS; ++i)
        FM.element_maxcoord0[i] = element[i].maxcoord;
}

/* Call this BEFORE each decode_formula() when iterating VV splits */
void surge_restore_element_runtime(SurgeCtx *ctx)
{
    for (int i = 0; i < MAXELEMENTS; ++i)
        element[i].maxcoord = ctx->fm.element_maxcoord0[i];
}

/* Capture the original limits once (after CLI parsing) */
void surge_capture_limit_seed(SurgeCtx *ctx)
{
    ctx->lim.seed_maxc    = ctx->lim.maxc;
    ctx->lim.seed_maxd    = ctx->lim.maxd;
    ctx->lim.seed_mine    = ctx->lim.mine;
    ctx->lim.seed_maxe    = ctx->lim.maxe;
    ctx->lim.seed_maxbond = ctx->lim.maxbond;
}

/* Reset limits for each VV emission */
void surge_reset_limits_from_seed(SurgeCtx *ctx)
{
    ctx->lim.maxc    = ctx->lim.seed_maxc;
    ctx->lim.maxd    = ctx->lim.seed_maxd;
    ctx->lim.mine    = ctx->lim.seed_mine;
    ctx->lim.maxe    = ctx->lim.seed_maxe;
    ctx->lim.maxbond = ctx->lim.seed_maxbond;
}

/* Clear only the formula workspace (NOT the element table or counts) */
void surge_reset_formula_workspace(SurgeCtx *ctx)
{
    ctx->fm.nv = 0;
    ctx->fm.numtypes = 0;
    ctx->fm.valencesum = 0;
    ctx->fm.min1 = ctx->fm.min12 = 0;
    ctx->fm.max34 = ctx->fm.max4 = 0;
    memset(ctx->fm.elementtype, 0, sizeof(ctx->fm.elementtype));
    memset(ctx->fm.elementcount, 0, sizeof(ctx->fm.elementcount));
    memset(ctx->fm.maxtype, 0, sizeof(ctx->fm.maxtype));
    memset(ctx->fm.newlabel, 0, sizeof(ctx->fm.newlabel));
    ctx->fm.canonform[0] = '\0';
    ctx->fm.hydrogens = 0;
    memset(ctx->fm.hyd, 0, sizeof(ctx->fm.hyd));
    ctx->fm.needcoordtest = false;
}

bool surge_parse_cli(int argc, char **argv, SurgeCtx *ctx) {
    int argi = 1, positional = 0;
    while (argi < argc) {
        char *arg = argv[argi++];
        if (arg[0] != '-' || arg[1] == '\0') {
            /* positional */
            if (++positional == 1) {
                IN.formula = arg;
            } else {
                fprintf(stderr, ">E surge: unexpected argument '%s'\n", arg);
                return true;
            }
            continue;
        }

        /* option(s), may be clustered (e.g., -Sv) or carry payload (-O3 / -Gopts) */
        char *p = arg + 1;
        while (*p) {
            char sw = *p++;
            switch (sw) {
                case 'G': { /* -G<extra>, or next token */
                    if (*p=='\0' && argi < argc) { IN.extra1 = argv[argi++]; }
                    else { IN.extra1 = p; }
                    p = (char*)""; /* consumed rest */
                    break;
                }
                case 'o': { /* -o<file> or next token; "-" or "stdout" → stdout */
                    const char *fname = NULL;
                    if (*p=='\0' && argi < argc) { fname = argv[argi++]; }
                    else { fname = p; }
                    O.filename = fname && *fname ? fname : "stdout";
                    p = (char*)"";
                    break;
                }
                case 'm': { /* -mres/mod */
                    const char *spec = NULL;
                    if (*p=='\0' && argi < argc) spec = argv[argi++];
                    else spec = p;
                    long a=0,b=0;
                    if (!spec || parse_long_pair_slash(spec, &a, &b) != 0) {
                        surge_fatal(">E surge : -m expects res/mod\n");
                    }
                    SP.res = a; SP.mod = b;
                    p = (char*)"";
                    break;
                }
                case 'O': { /* -O# */
                    const char *num = NULL;
                    if (*p=='\0' && argi < argc) num = argv[argi++];
                    else num = p;
                    int v=0; if (!num || parse_int(num, &v)!=0) surge_fatal(">E surge : -O expects integer 1..4\n");
                    F.outlevel = v;
                    p = (char*)"";
                    break;
                }
                case 'c': { /* -c# */
                    const char *num = NULL;
                    if (*p=='\0' && argi < argc) num = argv[argi++];
                    else num = p;
                    int v=0; if (!num || parse_int(num, &v)!=0) surge_fatal(">E surge : -c expects integer\n");
                    L.maxc = v;
                    L.seed_maxc = v;
                    p = (char*)""; break;
                }
                case 'd': { /* -d# */
                    const char *num = NULL;
                    if (*p=='\0' && argi < argc) num = argv[argi++];
                    else num = p;
                    int v=0; if (!num || parse_int(num, &v)!=0) surge_fatal(">E surge : -d expects integer\n");
                    L.maxd = v;
                    L.seed_maxd = v;
                    p = (char*)""; break;
                }
                case 'j': { // -jN processes
                    const char *spec = (*p=='\0' && argi < argc) ? argv[argi++] : p;
                    int j = spec ? atoi(spec) : 0;
                    if (j < 1) j = 1;
                    F.vv_max_procs = j;
                    p = (char*)""; break;
                }
                case 'Z': { /* enable zwitterions; optional cap like -Z3 */
                    F.zw_enable = true;
                    if (*p) { /* inline digits */
                        long v = 0; const char *q = p;
                        while (*q && isdigit(*q)) { v = 10*v + (*q - '0'); ++q; }
                        if (v > 0) F.zw_maxpairs = (int)v;
                        p = (char*)q;
                    }
                    break;
                }

                /* booleans */
                case 'u': F.uswitch = true; break;
                case 'v': F.verbose = true; break;
                case 'T': F.notriples = true; break;
                case 'S': F.smiles = true; break;
                case 'z': F.gzip = true; break;
                case 'A': F.alphabetic = true; break;
                case 'b': F.bipartite = true; break;
                case 'P': F.planar = true; break;
                case 'x': F.xswitch = true; break;
                case 'V': F.vvswitch = true; break;
                case 'I': F.ions = true; break;

                /* ranges */
                case 'e': { /* -eMIN:MAX or -eMIN-MAX */
                    const char *spec = NULL;
                    if (*p=='\0' && argi < argc) spec = argv[argi++];
                    else spec = p;
                    long a=0,b=0;
                    if (!spec || parse_long_pair_range(spec, &a, &b)!=0) surge_fatal(">E surge : -e expects min:max (or min-max)\n");
                    L.mine = (int)a;
                    L.seed_mine = (int)a;
                    L.maxe = (int)b;
                    L.seed_maxe = (int)b;
                    p = (char*)""; break;
                }
                case 't': { /* 3-rings MIN:MAX */
                    const char *spec = NULL;
                    if (*p=='\0' && argi < argc) spec = argv[argi++];
                    else spec = p;
                    long a=0,b=0;
                    if (!spec || parse_long_pair_range(spec, &a, &b)!=0) surge_fatal(">E surge : -t expects min:max\n");
                    F.tswitch = true; R.min3rings = a; R.max3rings = b; p = (char*)""; break;
                }
                case 'f': { /* 4-rings MIN:MAX */
                    const char *spec = NULL;
                    if (*p=='\0' && argi < argc) spec = argv[argi++];
                    else spec = p;
                    long a=0,b=0;
                    if (!spec || parse_long_pair_range(spec, &a, &b)!=0) surge_fatal(">E surge : -f expects min:max\n");
                    F.fswitch = true; R.min4rings = a; R.max4rings = b; p = (char*)""; break;
                }
                case 'p': { /* 5-rings MIN:MAX */
                    const char *spec = NULL;
                    if (*p=='\0' && argi < argc) spec = argv[argi++];
                    else spec = p;
                    long a=0,b=0;
                    if (!spec || parse_long_pair_range(spec, &a, &b)!=0) surge_fatal(">E surge : -p expects min:max\n");
                    F.pswitch = true; R.min5rings = a; R.max5rings = b; p = (char*)""; break;
                }

                /* bad list: -B1,3,7 */
                case 'B': {
                    const char *spec = NULL;
                    if (*p=='\0' && argi < argc) spec = argv[argi++];
                    else spec = p;
                    if (!spec || !*spec) surge_fatal(">E surge : -B expects a comma-separated list of 1..9\n");
                    set_badlist_from_csv(&R, spec);
                    p = (char*)"";
                    break;
                }

#ifdef SURGEPLUGIN_SWITCHES
                    /* plugin-defined switches may consume argv; pass control */
                default:
                {
                    int consumed = 0;
                    if (SURGEPLUGIN_SWITCHES(sw, p, argv, &argi, argc, &consumed)) {
                        if (consumed) p = (char*)""; /* plugin ate the rest */
                        break;
                    }
                    surge_fatal(">E surge : unknown switch\n");
                }
#else
                default:
                    surge_fatal(">E surge : unknown switch\n");
#endif
            } /* switch */
        } /* while (*p) */
    } /* while argi */
    return false;
}

void surge_print_summary(SurgeCtx *ctx, double t1, double t2) {
    /* ---- summary ---- */
    if (F.verbose) {
        fprintf(stderr, ">G geng made %llu graphs, %llu accepted\n",
                ST.gengout, ST.genggood);
        if (F.outlevel > 1)
            fprintf(stderr, ">V vcolg %llu nontrivial groups, max size %ld, made %llu graphs\n",
                    ST.vcolgnontriv, ST.maxvgroup, ST.vcolgout);
        if (F.outlevel > 2)
            fprintf(stderr, ">M multig %llu nontrivial groups, max size %ld, made %llu graphs\n",
                    ST.multignontriv, ST.maxegroup, ST.multigout);
    }
    if (R.needcycles)
        fprintf(stderr, "Max cycles = %d\n", R.maxcycles);

#ifdef SURGEPLUGIN_SUMMARY
    SURGEPLUGIN_SUMMARY
#endif

    fprintf(stderr, ">INFO: see error log at surge_errors.log\n");

    fprintf(stderr, ">Z %s %llu -> %llu -> %llu (skipped=%llu) in %.2f sec\n",
            (F.uswitch ? "generated" : "wrote"),
            ST.gengout, ST.vcolgout, ST.multigout, ST.skipped,
            t2 - t1);
}

void surge_validate_operation(int frc) {
    if (frc != SURGE_OK) {
        switch (frc) {
            case SURGE_ERR_ZLIB_DISABLED:
                surge_fatal(">E surge : -z is only allowed if zlib is compiled in\n");
                break;
            case SURGE_ERR_A_AND_S:
                surge_fatal(">E surge : -A and -S are incompatible\n");
                break;
            case SURGE_ERR_OPEN_OUTPUT:
                surge_fatal(">E surge : can't open output file\n");
                break;
            case SURGE_ERR_BAD_M_RANGE:
                surge_fatal(">E surge : -mres/mod needs 0 <= res < mod\n");
                break;
            default:
                surge_fatal(">E surge : invalid arguments\n");
        }
    }
}
