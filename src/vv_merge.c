//
// Created by fnellen on 21.09.2025.
//

#include "vv_merge.h"

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "gtools.h"
#include "utils.h"
#include "variable_valence.h"
#include "vv_progress.h"
#include "vv_sched.h"

/* human-readable bytes (e.g., 12.3MB) */
void vv_human_bytes(char *buf, size_t bufsz, unsigned long long b)
{
    const char *u[] = {"B","KB","MB","GB","TB"};
    int i = 0;
    double x = (double)b;
    while (x >= 1024.0 && i < 4) { x /= 1024.0; ++i; }
    snprintf(buf, bufsz, "%.1f%s", x, u[i]);
}

void vv_mergepb_render(VVMergePB *pb, const char *phase)
{
    if (pb->inline_mode == -1) {
        pb->inline_mode = isatty(STDERR_FILENO) ? 1 : 0;
        setvbuf(stderr, NULL, _IONBF, 0);
    }

    double now = vv_now_s();
    if (!phase && (now - pb->last_render_s) < 0.05) return; /* ~20 fps */
    pb->last_render_s = now;

    double pct = pb->total_bytes ? (100.0 * (double)pb->copied_bytes / (double)pb->total_bytes) : 0.0;
    enum { BARW = 40 };
    int fill = (int)((pct / 100.0) * BARW); if (fill < 0) fill = 0; if (fill > BARW) fill = BARW;

    char bar[BARW+2];
    for (int i=0;i<BARW;++i) bar[i] = (i<fill ? '=' : ' ');
    if (fill < BARW) bar[fill] = '>';
    bar[BARW] = '\0';

    char cur[32], tot[32];
    vv_human_bytes(cur, sizeof cur, pb->copied_bytes);
    vv_human_bytes(tot, sizeof tot, pb->total_bytes);

    char sp = spin_frame(now);

    if (pb->inline_mode) {
        fprintf(stderr, "\r%c [MERGE %s] %3.0f%%  %s / %s%s",
                sp, bar, pct, cur, tot, (phase ? phase : ""));
    } else {
        fprintf(stderr, "\r [MERGE %s] %3.0f%%  %s / %s%s\n",
                bar, pct, cur, tot, (phase ? phase : ""));
    }
    fflush(stderr);
}

void vv_mergepb_done(VVMergePB *pb)
{
    if (pb->inline_mode == 1) { fputc('\n', stderr); fflush(stderr); }
}

void vv_merge_parts_into_final(SurgeCtx *ctx, VVContext *vvc) {
    vv_logf(ctx, "MERGE begin use_pipes=%d children=%d", vvc->use_pipes, vvc->child_count);
    vv_wait_all(ctx, vvc);

    // If output is to stdout, nothing more to do
    if (vvc->use_pipes) return;

    VVMergePB mpb = {0};
    mpb.inline_mode = -1;
    for (int i = 0; i < vvc->child_count; ++i) {
        const char *path = vvc->child_paths[i];
        if (!path) continue;
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) {
            mpb.total_bytes += (unsigned long long)st.st_size;
        }
    }

    // Open output file
    int to_stdout =
#ifdef ZLIB
    (!O.filename || strcmp(O.filename,"stdout")==0 || strcmp(O.filename,"-")==0) && !F.gzip;
#else
        (!O.filename || strcmp(O.filename,"stdout")==0 || strcmp(O.filename,"-")==0);
#endif

    FILE *dst = O.outfile ? O.outfile : stdout;
    if (!dst) surge_fatal(">E surge : cannot open final output file for merging\n");

    enum { BUFSZ = 256 * 1024 };
    char *buf = (char*)malloc(BUFSZ);
    if (!buf) surge_fatal(">E surge : OOM allocating merge buffer\n");

    mpb.copied_bytes = 0;
    mpb.last_render_s = 0.0;
    vv_mergepb_render(&mpb, "  starting…");

    for (int i = 0; i < vvc->child_count; ++i) {
        const char *path = vvc->child_paths[i];
        if (!path) continue;

        FILE *src = fopen(path, "rb");
        if (!src) continue; /* skip missing; or abort if you prefer */

        for (;;) {
            size_t r = fread(buf, 1, BUFSZ, src);
            if (r == 0) break;
            size_t w;
#ifdef ZLIB
            if (F.gzip) {
                int written = gzwrite(O.gzoutfile, buf, (unsigned)r);
                w = written > 0 ? (size_t)written : 0;
            } else
#endif
                w = fwrite(buf, 1, r, dst);
            if (w != r) {
                fclose(src);
                free(buf);
                fclose(dst);
                surge_fatal(">E surge : write error during merge\n");
            }
            mpb.copied_bytes += (unsigned long long)r;
            vv_mergepb_render(&mpb, NULL); /* live update */
        }

        fclose(src);

        /* optional: remove part */
        remove(path);
        free((void*)path);
        vvc->child_paths[i] = NULL;
    }

    free(buf);

#ifdef ZLIB
    if (F.gzip) {
        vv_mergepb_done(&mpb);
        return; /* The parent closes the gzip stream after enumeration. */
    }
#endif
    fflush(dst);
    if (!to_stdout) {
        fclose(dst);
        O.outfile = NULL;
    }

    vv_mergepb_done(&mpb);
}