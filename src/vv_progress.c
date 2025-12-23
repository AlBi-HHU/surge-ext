//
// Created by fnellen on 21.09.2025.
//

#include <time.h>

#include "vv_progress.h"
#include "utils.h"

static int vv_progress_inline = -1;
const char SPIN_FRAMES[] = "|/-\\";

char spin_frame(const double t) {
    int idx = (int)(t * 10.0) % (int)(sizeof(SPIN_FRAMES) - 1);
    if (idx < 0) idx = 0;
    return SPIN_FRAMES[idx];
}

/* monotonic seconds */
double vv_now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void vv_progress_render(const SurgeCtx *ctx, VVContext *vvc, const char *phase) {
    if (vvc->total_jobs == 0) return;

    if (vv_progress_inline == -1) {
        vv_progress_inline = isatty(STDERR_FILENO) ? 1 : 0;
        /* unbuffered so we see updates immediately */
        setvbuf(stderr, NULL, _IONBF, 0);
    }

    double now = vv_now_s();
    if (!phase && now - vvc->last_render_s < 0.05) return;
    vvc->last_render_s = now;

    unsigned long long total   = vvc->total_jobs;
    unsigned long long spawned = vvc->spawned;
    unsigned long long done    = vvc->finished;
    unsigned long long running = (unsigned long long)(vvc->inflight);
    unsigned long long waiting = (spawned < total ? total - spawned : 0ULL);
    unsigned long long skipped = vvc->skipped;

    const double pct = (total > 0) ? (100.0 * (double)done  / (double)total) : 0.0;

    enum { BARW = 40 };
    int fill = (int)((pct / 100.0) * BARW);
    if (fill < 0) {
        fill = 0;
    }
    if (fill > BARW) {
        fill = BARW;
    }

    char bar[BARW + 2];
    for (int i = 0; i < BARW; ++i) bar[i] = (i < fill ? '=' : ' ');
    if (fill < BARW) bar[fill] = '>';
    bar[BARW] = '\0';

    char sp = spin_frame(now);

    if (vv_progress_inline) {
        fprintf(stderr,
            "\r%c [%s] %3.0f%%  spawned=%llu  running=%llu  waiting=%llu  skipped=%llu  done=%llu/%llu  out=%llu%s",
            sp, bar, pct, spawned, running, waiting, skipped, done, total, ST.multigout,
            (phase ? phase : ""));
    } else {
        fprintf(stderr,
            "%c [%s] %3.0f%%  spawned=%llu  running=%llu  waiting=%llu  skipped=%llu  done=%llu/%llu%s\n",
            sp, bar, pct, spawned, running, waiting, skipped, done, total,
            (phase ? phase : ""));
    }
    fflush(stderr);
}

/* newline at the very end */
void vv_progress_done(const VVContext *vvc) {
    if (vvc->total_jobs == 0) return;
    fputc('\n', stderr);
    fflush(stderr);
}

/* exact total = product over families of C(bare + k - 1, k - 1) */
unsigned long long vv_count_total_jobs(const VVContext *vvc) {
    unsigned long long total = 1ULL;
    for (int f = 0; f < vvc->fam_count; ++f) {
        int k = vvc->fam[f].k;
        int b = vvc->fam[f].bare_total;
        int n = b + k - 1;
        int r = k - 1;
        unsigned long long ways = (unsigned long long)nCk(n, r);
        if (ways == 0) ways = 1; /* guard */
        if (total > (UINT64_MAX / ways)) return UINT64_MAX;
        total *= ways;
    }
    return total;
}