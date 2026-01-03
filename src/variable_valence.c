//
// Created by fnellen on 05.09.2025.
//

#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "gtools.h"
#include "vv_merge.h"
#include "vv_sched.h"
#include "variable_valence.h"
#include "element.h"
#include "pipeline.h"
#include "vv_progress.h"
#include "utils.h"

static void append_token(char **w, size_t *room, const char *name, int cnt) {
    if (cnt <= 0 || *room == 0) return;

    int n = (cnt > 1)
        ? snprintf(*w, *room, "%s%d", name, cnt)
        : snprintf(*w, *room, "%s", name);

    if (n < 0) return;                 /* formatting error: do nothing */

    size_t wn = (size_t)n;
    if (wn >= *room) {                 /* TRUNCATED: cap at end */
        if (*room) {
            *w    += *room - 1;        /* point at last valid char */
            *room  = 0;                /* no space left for more */
        }
        return;                        /* keep string NUL-terminated */
    }

    *w    += wn;
    *room -= wn;
}

static void parse_formula_counts(SurgeCtx *ctx, int *counts)
{
    const char *p = IN.formula;

    memset(counts, 0, MAXELEMENTS * sizeof(int));

    while (*p) {
        char namebuf[32];
        int cnt;

        if (!isupper((unsigned char)*p)) {
            ++p;
            continue;
        }

        const char *sym_start = p;
        ++p;
        while (islower((unsigned char)*p)) ++p;
        if (*p == '(') {
            const char *close = p + 1;
            while (*close && *close != ')') ++close;
            if (*close != ')') surge_fatal(">E surge : unterminated element variant spec\n");
            p = close + 1;
        }

        size_t token_len = (size_t)(p - sym_start);
        if (token_len >= sizeof namebuf) token_len = sizeof namebuf - 1;
        memcpy(namebuf, sym_start, token_len);
        namebuf[token_len] = '\0';

        const char *digits = p;
        if (isdigit((unsigned char)*digits)) {
            long v = 0;
            do {
                v = v * 10 + (*digits - '0');
                ++digits;
            } while (isdigit((unsigned char)*digits));
            cnt = (int)v;
            p = digits;
        } else {
            cnt = 1;
            p = digits;
        }

        int idx = elementindex(ctx, namebuf);
        counts[idx] += cnt;
    }
}

static int already_added_family(const VarFamily *fam, int fam_count, const char *ename)
{
    for (int i = 0; i < fam_count; ++i) {
        if (strcmp(fam[i].name, ename) == 0) {
            return 1;
        }
    }
    return 0;
}

static int build_variable_families(const SurgeCtx *ctx, const int *base_counts,
                                   VarFamily *fam, int maxfam)
{
    int fam_count = 0;

    for (int i = 0; i < FM.numelements; ++i) {
        if (!element[i].name) {
            continue;
        }
        if (already_added_family(fam, fam_count, element[i].name)) {
            continue;
        }

        /* collect all entries sharing the same .name */
        int k = 0;
        int bare_idx = -1;
        for (int j = 0; j < FM.numelements; ++j) {
            if (!element[j].name) {
                continue;
            }
            /* skip charged elements unless -I or -Z is given */
            if (!F.ions && !F.zw_enable) {
                if (element[j].charge != 0) {
                    continue;
                }
            }
            if (strcmp(element[j].name, element[i].name) == 0) {
                if (k < MAXELEMENTS) {
                    fam[fam_count].var_idx[k] = j;
                    ++k;
                }
                if (strcmp(element[j].inputname, element[j].name) == 0) {
                    bare_idx = j;
                }
            }
        }
        if (k <= 1 || bare_idx < 0) {
            /* not a variable family or no bare variant */
            continue;
        }

        int bare_total = base_counts[bare_idx];
        if (bare_total <= 0) {
            /* no bare atoms present in the input → nothing to distribute */
            continue;
        }

        /* record this family */
        strncpy(fam[fam_count].name, element[i].name, sizeof fam[fam_count].name - 1);
        fam[fam_count].name[sizeof fam[fam_count].name - 1] = '\0';
        fam[fam_count].k          = k;
        fam[fam_count].bare_idx   = bare_idx;
        fam[fam_count].bare_total = bare_total;
        ++fam_count;

        if (fam_count >= maxfam) {
            break;
        }
    }

    return fam_count;
}

static void vv_emit_and_run(SurgeCtx *ctx, VVContext *vv_ctx)
{
    enum { BUF = 4 * FORMULALEN };
    char  derived[BUF];
    char *w = derived;
    size_t room = sizeof derived;

    // check formal charge balance
    int pos_charge = 0, neg_charge = 0;
    for (int i = 0; i < FM.numelements; ++i) {
        const int c = element[i].charge;
        if (c > 0) pos_charge += vv_ctx->work_counts[i] * c;
        if (c < 0) neg_charge += vv_ctx->work_counts[i] * -c;
    }
    // imbalance of formal charge in zwitterion mode
    if (F.zw_enable && (pos_charge != neg_charge )) {
        ST.skipped ++;
        vv_ctx->skipped++;
        if (F.verbose) printf(">[DEBUG] Skipping: charge imbalance %d vs %d\n", pos_charge, neg_charge);
        return;
    }
    // ions should only be negative or positive
    if (F.ions && (pos_charge > 0 && neg_charge > 0)) {
        ST.skipped ++;
        vv_ctx->skipped++;
        if (F.verbose) printf(">[DEBUG] Skipping: both positive and negative charges in ion mode\n");
        return;
    }

    for (int i = 0; i < MAXELEMENTS; ++i) {
        int c = vv_ctx->work_counts[i];
        if (c > 0) append_token(&w, &room, element[i].inputname, c);
    }

    if (room) {
        *w = '\0';
    } else {
        derived[BUF - 1] = '\0';
    }

    surge_restore_element_runtime(ctx);
    surge_reset_limits_from_seed(ctx);
    surge_reset_formula_workspace(ctx);
    IN.formula = derived;

    // Stall while we have too many children
    while (vv_ctx->inflight >= vv_ctx->pool) {
        vv_wait_one(ctx, vv_ctx);
    }

    vv_spawn_child_for_formula(ctx, vv_ctx, derived);

}



static void vv_apply_family_split(VVContext *ctx, const VarFamily *var_family,
                                  const int *split, int sign)
{
    for (int t = 0; t < var_family->k; ++t) {
        int idx = var_family->var_idx[t];
        ctx->work_counts[idx] += sign * split[t];
    }
}

static void vv_enumerate_splits_for_family(SurgeCtx *ctx, VVContext *vv_ctx, int f_idx,
                                           int pos, int remaining,
                                           int *split)
{
    const VarFamily *var_family = &vv_ctx->fam[f_idx];

    if (pos == var_family->k - 1) {
        split[pos] = remaining;

        vv_apply_family_split(vv_ctx, var_family, split, +1);

        if (f_idx + 1 < vv_ctx->fam_count) {
            /* Next family */
            vv_enumerate_splits_for_family(ctx, vv_ctx, f_idx + 1, 0,
                                           vv_ctx->fam[f_idx + 1].bare_total,
                                           split + var_family->k);
        } else {
            /* All families assigned → emit */
            vv_emit_and_run(ctx, vv_ctx);
        }

        vv_apply_family_split(vv_ctx, var_family, split, -1);
        return;
    }

    for (int x = 0; x <= remaining; ++x) {
        split[pos] = x;
        vv_enumerate_splits_for_family(ctx, vv_ctx, f_idx, pos + 1, remaining - x, split);
    }
}

static void vv_expand_families(SurgeCtx *ctx, VVContext *vv_ctx)
{
    /* Allocate one contiguous split buffer large enough for all families */
    int max_k_sum = 0;

    for (int f = 0; f < vv_ctx->fam_count; ++f) {
        max_k_sum += vv_ctx->fam[f].k;
    }

    if (max_k_sum == 0) {
        /* No variable families → just emit once */
        vv_emit_and_run(ctx, vv_ctx);
        return;
    }

    {
        /* One big array; we pass pointers into it as we recurse across families */
        int *split = (int *)alloca(sizeof(int) * (size_t)max_k_sum);
        memset(split, 0, sizeof(int) * (size_t)max_k_sum);

        vv_enumerate_splits_for_family(ctx,vv_ctx, 0, 0, vv_ctx->fam[0].bare_total, split);
    }
}

void run_with_variable_valences(SurgeCtx *ctx)
{
    int base_counts[MAXELEMENTS];
    VVContext vv_ctx;
    memset(&vv_ctx, 0, sizeof vv_ctx);
    setvbuf(stderr, NULL, _IONBF, 0);

    vv_ctx.pool = F.vv_max_procs;
    if (vv_ctx.pool < 1) vv_ctx.pool = 1;

    /* Parse formula into base counts */
    parse_formula_counts(ctx, base_counts);

    /* Build families that need expansion (have bare atoms) */
    vv_ctx.fam_count = build_variable_families(ctx, base_counts, vv_ctx.fam, MAXELEMENTS);

    /* If no families or disabled, just run once normally */
    if (!F.vvswitch || vv_ctx.fam_count == 0) {
        surge_run_pipeline(ctx);
        return;
    }

    /* progress init */
    vv_ctx.spawned = 0;
    vv_ctx.finished = 0;
    vv_ctx.last_render_s = 0.0;
    vv_ctx.total_jobs = vv_count_total_jobs(&vv_ctx);
    if (vv_ctx.total_jobs == 0ULL) vv_ctx.total_jobs = 1ULL; /* show a line at least */
    vv_progress_render(ctx, &vv_ctx, "  starting…");

    /* Base counts with bare entries zeroed; explicit variants are kept */
    memcpy(vv_ctx.base_wo_bare, base_counts, sizeof vv_ctx.base_wo_bare);
    for (int f = 0; f < vv_ctx.fam_count; ++f) {
        vv_ctx.base_wo_bare[vv_ctx.fam[f].bare_idx] = 0;
    }
    memcpy(vv_ctx.work_counts, vv_ctx.base_wo_bare, sizeof vv_ctx.work_counts);

    if (F.zw_enable || F.ions || F.vvswitch) {
        int total_num_variants = 0;
        int total_num_bare     = 0;
        for (int f = 0; f < vv_ctx.fam_count; ++f) {
            total_num_variants += vv_ctx.fam[f].k;
            total_num_bare     += vv_ctx.fam[f].bare_total;
        }
        vv_ctx.total_jobs = vv_count_total_jobs(&vv_ctx);
    }

    /* Enumerate across all families and run for each combination */
    vv_expand_families(ctx, &vv_ctx);

    vv_merge_parts_into_final(ctx, &vv_ctx);

    vv_progress_done(&vv_ctx);
}
