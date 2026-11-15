//
// Created by fnellen on 21.09.2025.
//

#include <time.h>
#include <string.h>

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
        vv_progress_inline = surge_terminal_live() ? 1 : 0;
        /* unbuffered so we see updates immediately */
        setvbuf(stderr, NULL, _IONBF, 0);
    }

    double now = vv_now_s();
    bool complete = phase && strstr(phase, "complete");
    bool starting = phase && strstr(phase, "starting");
    double interval = vv_progress_inline ? 0.1 : 1.0;
    if (!complete && !starting && now - vvc->last_render_s < interval) return;
    vvc->last_render_s = now;

    unsigned long long total   = vvc->total_jobs;
    unsigned long long spawned = vvc->spawned;
    unsigned long long done    = vvc->finished;
    unsigned long long running = (unsigned long long)(vvc->inflight);
    unsigned long long processed = done + running;
    unsigned long long waiting = (processed < total ? total - processed : 0ULL);
    unsigned long long skipped = vvc->skipped;

    const double pct = (total > 0) ? (100.0 * (double)done  / (double)total) : 0.0;

    if (vv_progress_inline) {
        char primary[64], secondary[128];
        char total_text[32], pool_text[32];
        snprintf(total_text, sizeof total_text, "%llu", total);
        snprintf(pool_text, sizeof pool_text, "%d", vvc->pool);
        int digits = (int)strlen(total_text);
        snprintf(primary, sizeof primary, "%*llu/%s%s", digits, done, total_text,
                 surge_terminal_width() >= 48 ? " splits" : "");
        snprintf(secondary, sizeof secondary, "%*llu active, %*llu skipped",
                 (int)strlen(pool_text), running, digits, skipped);
        surge_terminal_progress("Enumerate", pct, primary, secondary, complete);
    } else {
        fprintf(stderr,
            "Enumerate %3.0f%%  spawned=%llu  running=%llu  waiting=%llu  skipped=%llu  done=%llu/%llu%s\n",
            pct, spawned, running, waiting, skipped, done, total,
            complete ? "  complete" : starting ? "  starting" : "");
    }
    fflush(stderr);
}

/* newline at the very end */
void vv_progress_done(const SurgeCtx *ctx, VVContext *vvc) {
    if (vvc->total_jobs == 0) return;
    vv_progress_render(ctx, vvc, "  complete");
    if (vv_progress_inline) fputc('\n', stderr);
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
