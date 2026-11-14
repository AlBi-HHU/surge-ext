//
// Created by fnellen on 21.09.2025.
//

#include <sys/wait.h>
#include <errno.h>
#include <string.h>

#include "vv_sched.h"
#include "config.h"
#include "gtools.h"
#include "pipeline.h"
#include "utils.h"
#include "variable_valence.h"
#include "vv_progress.h"


void vv_spawn_child_for_formula(SurgeCtx *ctx, VVContext *vvc, const char *derived) {
    char *part_path = vv_make_part_path(O.filename ? O.filename : "stdout", vvc->child_count);
    if (!part_path) surge_fatal(">E surge : out of memory creating part file\n");
    vvc->use_pipes = 0;

    /* Do not let a worker's exit() flush inherited output a second time. */
    if (fflush(NULL) == EOF) surge_fatal(">E surge : error flushing output before fork\n");
    pid_t pid = fork();
    if (pid < 0) {
        vv_logf(ctx, "ERROR fork() failed for formula=\"%s\"", derived);
        surge_fatal(">E surge : fork() failed for VV split\n");
    }

    if (pid == 0) {
        /* ---- CHILD ---- */
        F.vv_worker = true;
        vv_logf(ctx, "CHILD start child_seq=%d via=%s target=%s formula=\"%s\"",
            vvc->child_count, part_path, (O.filename ? O.filename : "-"), derived);

        FILE *f = fopen(part_path, "w");
        if (!f) surge_fatal(">E surge : cannot open part file for VV child\n");
        if (O.outfile && O.outfile != stdout) fclose(O.outfile);
        O.outfile = f;
#ifdef ZLIB
        /* Workers write plain parts; only the parent owns the gzip stream. */
        F.gzip = false;
        O.gzoutfile = NULL;
#endif

        memset(&ctx->stats, 0, sizeof ctx->stats);

        double t1 = vv_now_s();
        surge_run_pipeline(ctx);
        double t2 = vv_now_s();

        vv_logf(ctx, "CHILD done child_seq=%d formula=\"%s\"", vvc->child_count, derived);

        /* close output */
        if (O.outfile && O.outfile != stdout) fclose(O.outfile);
        O.outfile = NULL;

        /* ---- write stats sidecar ---- */
        char *stats_path = vv_make_stats_path(part_path);
        if (stats_path) {
            FILE *sf = fopen(stats_path, "w");
            if (sf) {
                /* gengout genggood vcolgout multigout secs */
                fprintf(sf, "%llu %llu %llu %llu %.6f\n",
                        ST.gengout, ST.genggood, ST.vcolgout, ST.multigout, (t2 - t1));
                fclose(sf);
            }
            free(stats_path);
        }
        _exit(0);
    }

    // ---- PARENT ----
    vv_logf(ctx, "PARENT spawned pid=%ld child_seq=%d via=%s target=%s formula=\"%s\"",
        (long)pid, vvc->child_count, part_path, (O.filename ? O.filename : "-"), derived);

    vvc->child_pids[vvc->child_count]  = pid;
    vvc->child_paths[vvc->child_count] = part_path;
    vvc->child_count++;
    vvc->inflight++;

    vvc->spawned++;
    vv_progress_render(ctx, vvc, NULL);
}

char *vv_make_part_path(const char *final, int seq) {
    size_t n = strlen(final) + 32;
    char *p = malloc(n);
    if (!p) return NULL;
    snprintf(p, n, "%s.part.%06d", final, seq);
    return p;
}

void vv_wait_one(SurgeCtx *ctx, VVContext *vvc) {
    if (vvc->inflight <= 0) return;
    int status;
    pid_t pid = wait(&status);
    if (pid <= 0) {
        vv_logf(ctx, "WARN wait() returned %ld", (long)pid);
        return;
    }
    int idx = -1;
    for (int i = 0; i < vvc->child_count; ++i) {
        if (vvc->child_pids[i] == pid) { idx = i; break; }
    }

    if (idx >= 0) {
        char *stats_path = vv_make_stats_path(vvc->child_paths[idx]);
        if (stats_path) {
            FILE *sf = fopen(stats_path, "r");
            if (sf) {
                unsigned long long gengout=0, genggood=0, vcolgout=0, multigout=0;
                double secs=0.0;
                int n = fscanf(sf, "%llu %llu %llu %llu %lf",
                               &gengout, &genggood, &vcolgout, &multigout, &secs);
                fclose(sf);
                if (n >= 4) {
                    ST.gengout  += gengout;
                    ST.genggood += genggood;
                    ST.vcolgout += vcolgout;
                    ST.multigout+= multigout;
                }
                remove(stats_path);
            }
            free(stats_path);
        }
    }

    bool skipped = false;
    if (WIFEXITED(status)) {
        int rc = WEXITSTATUS(status);
        vv_logf(ctx, "REAP pid=%ld child_seq=%d exit=%d",
                (long)pid, idx, rc);
        if (rc != 0) skipped = true;
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        vv_logf(ctx, "REAP pid=%ld child_seq=%d signaled=%d",
                (long)pid, idx, sig);
        skipped = true;
    } else {
        vv_logf(ctx, "REAP pid=%ld child_seq=%d status=0x%x",
                (long)pid, idx, status);
        skipped = true;
    }

    if (skipped && idx >= 0) {
        ++ST.skipped;
        ++vvc->skipped;
        char *part = vvc->child_paths[idx];
        if (part) {
            if (remove(part) != 0 && errno != ENOENT) {
                vv_logf(ctx, "WARN failed to remove part file %s: %s", part, strerror(errno));
            }
            free(part);
            vvc->child_paths[idx] = NULL;
        }
    }

    if (idx >= 0) vvc->child_pids[idx] = 0;

    vvc->inflight--;
    vvc->finished++;
    vv_logf(ctx, "SCHED inflight now %d", vvc->inflight);
    vv_progress_render(ctx, vvc, skipped ? "  skipped" : NULL);
}

void vv_wait_all(SurgeCtx *ctx, VVContext *vvc) {
    while (vvc->inflight > 0) vv_wait_one(ctx, vvc);
    vv_progress_done(ctx, vvc);
}

char *vv_make_stats_path(const char *part_path) {
    size_t n = strlen(part_path) + 8;
    char *p = (char*)malloc(n);
    if (!p) return NULL;
    snprintf(p, n, "%s.stats", part_path);
    return p;
}
