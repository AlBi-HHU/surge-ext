/* This is a molecule generator based on geng.
   Version 1.0, November 11, 2021.

   Unix-style compilation command would be:

     gcc -o surge -O3 -DWORDSIZE=32 -DMAXN=WORDSIZE -DOUTPROC=surgeproc \
         -march=native -mtune=native -DPREPRUNE=surgepreprune \
         -DPRUNE=surgeprune -DGENG_MAIN=geng_main \
         surge.c geng.c planarity.c nautyW1.a

   You can build-in gzip output using the zlib library (https://zlib.net).
   Add -DZLIB to the compilation, and link with the zlib library either
   by adding -lz or libz.a . This will activate the -z command to gzip
   the output.

     gcc -o surge -O3 -DWORDSIZE=32 -DMAXN=WORDSIZE -DOUTPROC=surgeproc \
         -march=native -mtune=native -DPREPRUNE=surgepreprune -DZLIB \
         -DPRUNE=surgeprune -DGENG_MAIN=geng_main \
         surge.c geng.c planarity.c nautyW1.a -lz

   There is a makefile in the package; edit the first few lines.

   This version works best with geng version 3.2 or later. To use
   with an earlier version, add -DOLDGENG to the compilation command.

***********************************************************************/

#ifdef ZLIB
#define USAGE \
  "[-oFILE] [-z] [-u|-A|-S] [-V] [-Xstate] [-Ffile] [-T] [-e#|-e#:#] [-d#] [-c#] [-m#/#] formula"
#else
#define USAGE \
  "[-oFILE] [-u|-A|-S] [-V] [-Z[#]] [-j#] [-Xstate] [-Ffile] [-T] [-e#|-e#:#] [-d#] [-c#] [-m#/#] formula"
#endif

#define HELPUSECMD

#define HELPTEXT1 \
"Make chemical graphs from a formula. Version 1.0.\n" \
"  Known elements are C,B,N,P,O,S,H,Cl,F,Br,I at their lowest valences.\n" \
"  Higher valences can be selected using Nx (Nitrogen/5), Sx,Sy (Sulfur 4/6)\n" \
"   Px (Phosphorus/5).\n" \
"\n" \
"  formula = a formula like C8H6N2\n" \
"\n" \
"  -u    Just count, don't write\n" \
"  -S    Output in SMILES format\n" \
"  -A    Output in alphabetical format\n" \
"  -O#   Output stage: 1 after geng, 2 after vcolg, 3 after multig\n" \
"        Default is to write SDfile format\n" \
"  -e# -e#:#  Restrict to given range of distinct non-H bonds\n" \
"  -t# -t#:#  Limit number of rings of length 3\n" \
"  -f# -f#:#  Limit number of cycles of length 4\n" \
"  -p# -p#:#  Limit number of cycles of length 5\n" \
"  -b    Only rings of even length (same as only cycles of even length)\n" \
"  -T    Disallow triple bonds\n" \
"  -P    Require planarity\n" \
"  -d#   Maximum degree not counting bond multiplicity or hydrogens (default 6)\n" \
"  -c#   Maximum coordination number (default 6). This is the maximum number\n" \
"         of distinct atoms (including H) that an atom can be bonded to\n" \
"         Coordination number > 4 is only allowed if no neighbours are H\n" \
"  -B#,...,# Specify sets of substructures to avoid (details in manual)\n" \
"     1 = no triple bonds in rings up to length 7\n" \
"     2 = Bredt's rule for two rings ij with one bond in\n" \
"           common (33, 34, 35, 36, 44, 45)\n" \
"     3 = Bredt's rule for two rings ij with two bonds in\n" \
"           common (i,j up to 56)\n" \
"     4 = Bredt's rule for two rings of length 6 sharing three bonds\n" \
"     5 = no substructures A=A=A (in ring or not)\n" \
"     6 = no substructures A=A=A in rings up to length 8\n" \
"        For -B5 and -B6, the central atom only has 2 non-H neighbours\n" \
"     7 = no K_33 or K_24 structure\n" \
"     8 = none of cone of P4 or K4 with 3-ear\n" \
"     9 = no atom in more than one ring of length 3 or 4\n" \
"  -v     Write more information to stderr\n" \
"  -V     Enable variable valences\n" \
"  -Xstate Exclude an exact element state (repeatable; quote parentheses)\n" \
"  -Ffile  Read excluded states, one per line; blank lines and # comments allowed\n" \
"  -j#   Use # threads for variable valence (default 1, max 32)\n" \
"  -m#/#  Do only a part. The two numbers are res/mod where 0<=res<mod.\n" \
"  -oFILE Write the output to the given file rather than to stdout.\n" \
"  -E..  Define a new element (see the manual)\n" \
"  -z     Write output in gzip format (only if compiled with zlib)\n" \
"-Z[#] Also enumerate zwitterions (internal ± pairs), cap to # pairs (default 2)\n"

#define EXTRAUSAGE ""
#define HELPTEXT2 ""

/* Undocumented options:
  -G...  Anything to the end of the parameter is passed to geng
  -x     Used for development purposes; not useful for users
*/
#include "config.h"
#include "element.h"
#include "gtools.h"
#include "pipeline.h"
#include "utils.h"
#include "variable_valence.h"
#include "vv_progress.h"
#ifdef ZLIB
#include "zlib.h"
#endif

#define DEBUG 0


/******************************************************************/

#undef HELPTEXT2
#ifdef SURGEPLUGIN
/* Note that the value of SURGEPLUGIN must be a filename in quotes, so on
   the compilation command you will probably need something like
   -DSURGEPLUGIN='"myplugin.h"' . */
#include SURGEPLUGIN
#endif

#ifndef HELPTEXT2
#define HELPTEXT2 ""
#endif

/******************************************************************/

#if 0
static unsigned long long
molcode(int *vcol, int *mult, int n, int ne)
/* This makes an isomorph-invariant code for a molecule.  It is intended
for debugging and can't be relied on to distinguish between different
molecules. It only works up to MAXN/2 atoms (but you can compile the
program with WORDSIZE=64 if you really need to). */
{
    graph g[MAXN],h[MAXN];
    int lab[MAXN],ptn[MAXN],orbits[MAXN],weight[MAXN];
    static DEFAULTOPTIONS_GRAPH(options);
    statsblk stats;
    setword workspace[2*MAXN];
    int i,x,y,nv;
    unsigned long long ans1,ans2;

    nv = 2*n;
    if (nv >= MAXN) surge_fatal(">E surge : too big for longcode\n");

    for (i = 0; i < n; ++i) { weight[i] = vcol[i]; weight[n+i] = n+vcol[i]; }
    setlabptn(weight,lab,ptn,nv);

    for (i = 0; i < nv; ++i) g[i] = 0;

    for (i = 0; i < n; ++i) { g[i] |= bit[n+i]; g[n+i] |= bit[i]; }

    for (i = 0; i < ne; ++i)
    {
        x = edge[i].x;
        y = edge[i].y;
        g[x] |= bit[y]; g[y] |= bit[x];
        if (mult[i] > 0) { g[n+x] |= bit[n+y]; g[n+y] |= bit[n+x]; }
        if (mult[i] > 1) 
        {
            g[x] |= bit[y+n]; g[y+n] |= bit[x];
            g[x+n] |= bit[y]; g[y] |= bit[x+n];
        }
    }

    options.defaultptn = FALSE;
    options.getcanon = TRUE;

    nauty(g,lab,ptn,NULL,orbits,&options,&stats,workspace,2*MAXN,1,nv,h);
 
    ans1 = n;
    ans2 = ne;
    for (i = 0; i < nv; ++i)
    {
        ans1 = 177*ans1 + (h[i] >> (WORDSIZE-nv));
        ans2 = 1237*ans2 + (h[i] >> (WORDSIZE-nv));
    }

    return ans1^ans2;
}
#endif


int main(int argc, char *argv[])
{

    surge_print_header();

    SurgeCtx ctx;
    surge_ctx_init(&ctx);

    HELP;

    nauty_check(WORDSIZE,1,1,NAUTYVERSIONID);

#ifdef SURGEPLUGIN_INIT
    SURGEPLUGIN_INIT;
#endif

    calculate_number_of_elements(&ctx);

    if (surge_parse_cli(argc, argv, &ctx)) return 1;

    surge_capture_limit_seed(&ctx);
    surge_snapshot_element_runtime(&ctx);

    if (!ctx.in.formula) {
        fprintf(stderr, ">E Usage: surge <formula> [options]\n");
        return 1;
    }

    /* ---- finalize ctx: derive state, validate, open output ---- */
    int frc = surge_ctx_finalize(&ctx);

    surge_validate_operation(frc);

    /* ---- run ---- */
    surge_set_thread_ctx(&ctx);

    if (surge_terminal_live()) {
        surge_terminal_detail("Formula", ctx.in.formula);
        fputc('\n', stderr);
    }
    double t1 = vv_now_s(), t2;

    if (ctx.flags.vvswitch) {
        run_with_variable_valences(&ctx);
    } else {
        surge_run_pipeline(&ctx);
    }

#ifdef ZLIB
    if (ctx.flags.gzip && ctx.out.gzoutfile) {
        /* Close eager to catch zlib errors early */
        int rc = gzclose(ctx.out.gzoutfile);
        ctx.out.gzoutfile = NULL;
        if (rc != Z_OK) surge_fatal(">E surge : error closing gzip output\n");
    }
#endif

    t2 = vv_now_s();

    surge_print_summary(&ctx, t1, t2);

    surge_close_output(&ctx);
    free(ctx.groups.vgroup);
    free(ctx.groups.egroup);
    return 0;
}
