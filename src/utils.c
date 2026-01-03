//
// Created by fnellen on 31.08.2025.
//
#include "utils.h"
#include "config.h"
#include "element.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>


bool is_hydrogen(int i) {
    return strcmp(element[i].name, "H") == 0;
}

void put_int(char **p, unsigned long x) {
    if (x == 0) {
        *(*p)++ = '0';
        return;
    }
    char tmp[21];
    char *sp = tmp;
    while (x) {
        *sp++ = (char) ('0' + (x % 10));
        x /= 10;
    }
    while (sp > tmp) {
        *(*p)++ = *--sp;
    }
}

void space(char **p) {
    *(*p)++ = ' ';
}

void put_str(char **p, const char *s) {
    while (*s != '\0') {
        *(*p)++ = *s++;
    }
}

void put_bnd(char **p, int bnd) {
    if (bnd == 0) *(*p)++ = '-';
    else if (bnd == 1) *(*p)++ = '=';
    else *(*p)++ = '#';
}

static char *dup_string(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

static char *g_error_log_path = NULL;

static const char *resolve_error_log_path(void) {
    if (g_error_log_path) return g_error_log_path;

    const char *from_env = getenv("SURGE_ERROR_LOG");
    if (from_env && *from_env) {
        char *copy = dup_string(from_env);
        if (copy) {
            g_error_log_path = copy;
            return g_error_log_path;
        }
    }

    return "surge_errors.log";
}

void surge_set_error_log_path(const char *path) {
    if (!path || !*path) return;

    char *copy = dup_string(path);
    if (!copy) return;

    free(g_error_log_path);
    g_error_log_path = copy;
}

void surge_dump_context(FILE *fp, const SurgeCtx *ctx) {
    if (!fp) return;
    fputs("---- Surge context snapshot ----\n", fp);
    if (!ctx) {
        fputs("  <no context available>\n", fp);
        return;
    }

    const SurgeFlags *f = &ctx->flags;
    fprintf(fp,
            "  flags: uswitch=%d verbose=%d outlevel=%d smiles=%d alphabetic=%d gzip=%d planar=%d bipartite=%d xswitch=%d notriples=%d tswitch=%d fswitch=%d pswitch=%d vvswitch=%d zw_enable=%d zw_maxpairs=%d ions=%d vv_max_procs=%d\n",
            f->uswitch, f->verbose, f->outlevel, f->smiles, f->alphabetic,
            f->gzip, f->planar, f->bipartite, f->xswitch, f->notriples,
            f->tswitch, f->fswitch, f->pswitch, f->vvswitch, f->zw_enable,
            f->zw_maxpairs, f->ions, f->vv_max_procs);

    fprintf(fp, "  input: formula=%s extra1=%s\n",
            ctx->in.formula ? ctx->in.formula : "(null)",
            ctx->in.extra1 ? ctx->in.extra1 : "(null)");

    fprintf(fp,
            "  limits: maxc=%d maxd=%d mine=%d maxe=%d maxbond=%d\n",
            ctx->lim.maxc, ctx->lim.maxd, ctx->lim.mine, ctx->lim.maxe,
            ctx->lim.maxbond);

    fprintf(fp, "  split: res=%ld mod=%ld\n", ctx->split.res, ctx->split.mod);

    fprintf(fp,
            "  formula: nv=%d numtypes=%d valencesum=%d hydrogens=%d needcoordtest=%d numelements=%d hyd[MAXN sample0]=%d\n",
            ctx->fm.nv, ctx->fm.numtypes, ctx->fm.valencesum,
            ctx->fm.hydrogens, ctx->fm.needcoordtest, ctx->fm.numelements,
            ctx->fm.hyd[0]);

    fprintf(fp,
            "  rings: needcycles=%d maxcycles=%d min3=%ld max3=%ld min4=%ld max4=%ld min5=%ld max5=%ld cyclecount=%d\n",
            ctx->rings.needcycles, ctx->rings.maxcycles,
            ctx->rings.min3rings, ctx->rings.max3rings,
            ctx->rings.min4rings, ctx->rings.max4rings,
            ctx->rings.min5rings, ctx->rings.max5rings,
            ctx->rings.cyclecount);

    fprintf(fp,
            "  stats: gengout=%llu genggood=%llu vcolgout=%llu multigout=%llu skipped=%llu\n",
            (unsigned long long)ctx->stats.gengout,
            (unsigned long long)ctx->stats.genggood,
            (unsigned long long)ctx->stats.vcolgout,
            (unsigned long long)ctx->stats.multigout,
            (unsigned long long)ctx->stats.skipped);

    fprintf(fp,
            "  groups: vgroupsize=%ld egroupsize=%ld vgroupalloc=%zu egroupalloc=%zu vgroupcount=%ld\n",
            ctx->groups.vgroupsize, ctx->groups.egroupsize,
            ctx->groups.vgroupalloc, ctx->groups.egroupalloc,
            ctx->groups.vgroupcount);

    fprintf(fp, "  graph: numedges=%d\n", ctx->g.numedges);

    fprintf(fp, "  smiles: length=%d\n", ctx->smi.smileslen);

    fprintf(fp, "  output: filename=%s stdout?=%d file_open=%d",
            ctx->out.filename ? ctx->out.filename : "(null)",
            ctx->out.filename && strcmp(ctx->out.filename, "stdout") == 0,
            ctx->out.outfile != NULL);
#ifdef ZLIB
    fprintf(fp, " gzip_file_open=%d\n", ctx->out.gzoutfile != NULL);
#else
    fputc('\n', fp);
#endif
}

void surge_fatal(const char *msg) {
    const char *message = msg ? msg : ">E surge : fatal error\n";
    const char *log_path = resolve_error_log_path();
    SurgeCtx *ctx = surge_get_thread_ctx();

    FILE *fp = fopen(log_path, "a");
    if (fp) {
        time_t now = time(NULL);
        struct tm tm;
        char tbuf[32];
        if (localtime_r(&now, &tm) &&
            strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M:%S", &tm) > 0) {
            fprintf(fp, "\n[%s] fatal\n", tbuf);
        } else {
            fputs("\n[fatal]\n", fp);
        }

        fputs(message, fp);
        size_t len = strlen(message);
        if (len == 0 || message[len - 1] != '\n') fputc('\n', fp);

        surge_dump_context(fp, ctx);
        fclose(fp);
    } else {
        fprintf(stderr, "%s", message);
        fprintf(stderr, "Failed to open error log '%s': %s\n", log_path, strerror(errno));
        if (ctx) {
            fprintf(stderr, "Dumping context to stderr due to logging failure.\n");
            surge_dump_context(stderr, ctx);
        }
        exit(EXIT_FAILURE);
    }

    exit(EXIT_FAILURE);
}

void surge_print_error_log(void) {
    const char *log_path = resolve_error_log_path();
    FILE *fp = fopen(log_path, "r");
    if (!fp) {
        fprintf(stderr, "Unable to open error log '%s': %s\n", log_path, strerror(errno));
        return;
    }

    fprintf(stderr, "---- Surge error log (%s) ----\n", log_path);
    char buf[512];
    while (fgets(buf, sizeof buf, fp)) {
        fputs(buf, stderr);
    }
    fprintf(stderr, "---- End surge error log ----\n");
    fclose(fp);
}

int parse_int(const char *s, int *out) {
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') return -1;
    *out = (int) v;
    return 0;
}

int parse_long_pair_slash(const char *s, long *a, long *b) {
    /* res/mod (e.g., "1/4") */
    const char *slash = strchr(s, '/');
    if (!slash) return -1;
    char lhs[64], rhs[64];
    size_t left_len  = (size_t) (slash - s);
    if (left_len  >= sizeof(lhs) || strlen(slash + 1) >= sizeof(rhs)) return -1;
    memcpy(lhs, s, left_len );
    lhs[left_len ] = '\0';
    strcpy(rhs, slash + 1);
    char *end = NULL;
    long x = strtol(lhs, &end, 10);
    if (end == lhs || *end != '\0') return -1;
    long y = strtol(rhs, &end, 10);
    if (end == rhs || *end != '\0') return -1;
    *a = x;
    *b = y;
    return 0;
}

int parse_long_pair_range(const char *s, long *a, long *b) {
    /* min:max or min-max (accept both) */
    const char *sep = strchr(s, ':');
    if (!sep) sep = strchr(s, '-');
    if (!sep) return -1;
    char lhs[64], rhs[64];
    size_t left_len  = (size_t) (sep - s);
    if (left_len  >= sizeof(lhs) || strlen(sep + 1) >= sizeof(rhs)) return -1;
    memcpy(lhs, s, left_len );
    lhs[left_len ] = '\0';
    strcpy(rhs, sep + 1);
    char *end = NULL;
    long x = strtol(lhs, &end, 10);
    if (end == lhs || *end != '\0') return -1;
    long y = strtol(rhs, &end, 10);
    if (end == rhs || *end != '\0') return -1;
    *a = x;
    *b = y;
    return 0;
}

void set_badlist_from_csv(RingState *ring_state, const char *csv) {
    /* e.g., "-B1,3,7" → R->bad[1], R->bad[3], R->bad[7] = true */
    const char *p = csv;
    while (*p) {
        while (*p == ',' || isspace((unsigned char)*p)) ++p;
        if (!*p) break;
        char tmp[8];
        int k = 0;
        while (*p && isdigit((unsigned char)*p) && k < (int) sizeof(tmp) - 1) tmp[k++] = *p++;
        tmp[k] = '\0';
        if (k > 0) {
            int idx = atoi(tmp);
            if (idx >= 1 && idx <= BADLISTS)
                ring_state->bad[idx] = true;
        }
        while (*p && *p != ',') ++p;
    }
}

int nCk(int n, int k) {
    double sum = 1;

    for (int i = 1; i <= k; i++){

        sum = sum * (n - k + i) / i;
    }
    return (int)sum;
}

void vv_logf(SurgeCtx *ctx, const char *fmt, ...) {
    if (!F.verbose) return;
    va_list ap;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);

    char tbuf[32];
    strftime(tbuf, sizeof tbuf, "%H:%M:%S", &tm);

    fprintf(stderr, "[%s.%03ld] pid=%ld VV ",
            tbuf, ts.tv_nsec/1000000L, (long)getpid());

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}
