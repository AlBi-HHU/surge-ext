//
// Created by fnellen on 05.09.2025.
//

#ifndef SURGE_VV_VARIABLE_VALENCE_H
#define SURGE_VV_VARIABLE_VALENCE_H

#include "config.h"


/**
 * @struct VarFamily
 * @brief Variable-valence family grouped by common element name.
 *
 * A family consists of all rows in @c element[] that share the same
 * canonical @c name (e.g., "S") but may have distinct @c inputname
 * (e.g., "S", "Sx", "Sy"). The "bare" variant is the row where
 * @c inputname == name.
 *
 * @var VarFamily::name
 * Canonical element name used for grouping (e.g., "S").
 * @var VarFamily::var_idx
 * Indices into @c element[] for all variants in the family.
 * @var VarFamily::k
 * Number of variants in the family (size of @c var_idx actually used).
 * @var VarFamily::bare_idx
 * Index into @c element[] for the bare variant (inputname==name), or -1 if none.
 * @var VarFamily::bare_total
 * Number of bare atoms present in the parsed formula that must be distributed
 * across the variants during expansion.
 */

typedef struct {
    char name[3];                 /* e.g., "S" */
    int  var_idx[MAXELEMENTS];    /* indices into element[] for this family */
    int  k;                       /* number of variants in family */
    int  bare_idx;                /* index of the bare variant (inputname==name) */
    int  bare_total;              /* how many bare atoms need distributing */
} VarFamily;

/**
 * @struct VVContext
 * @brief Execution context for variable-valence expansion and emission.
 *
 * Holds caller-provided constraints, discovered families to expand, and
 * working/permanent count vectors over @c element[] indices.
 *
 * @var VVContext::fam      Array of discovered variable families.
 * @var VVContext::fam_count Number of valid entries in @c fam.
 * @var VVContext::base_wo_bare Counts per @c element[] with all "bare" family entries zeroed.
 * @var VVContext::work_counts  Mutable counts vector used while enumerating splits.
 */
typedef struct {
    VarFamily fam[MAXELEMENTS];
    int fam_count;

    int base_wo_bare[MAXELEMENTS];
    int work_counts[MAXELEMENTS];

    int    child_count;
    pid_t  child_pids[VV_MAX_CHILDREN];
    char  *child_paths[VV_MAX_CHILDREN];
    int    use_pipes;
    int    pool;
    int    inflight;

    /* ---- progress ---- */
    unsigned long long total_jobs;
    unsigned long long spawned;
    unsigned long long finished; /* completed workers plus pre-spawn rejections */
    unsigned long long skipped;
    double last_render_s;
} VVContext;


/**
 * @brief Append an element token (name + optional count) to a growing buffer.
 *
 * Writes @p name and, if @p cnt > 1, its decimal count into the buffer tracked by (@p w, @p room).
 * The function updates both pointers on success. If there is insufficient @p room, nothing is written
 * for that piece (name or count), and the pointers are not advanced for the failed part.
 *
 * @param[in,out] w     Pointer to the current write cursor; advanced past what was written.
 * @param[in,out] room  Pointer to the remaining byte count in the destination buffer; decreased accordingly.
 * @param[in]     name  Token name to append (e.g., "S", "Sx", "Sy", "O").
 * @param[in]     cnt   Non-negative count. If 0 or negative, nothing is appended. If 1, the count is omitted.
 *
 * @see count_plain_S_and_strip(), run_with_variable_sulfur()
 */
static void append_token(char **w, size_t *room, const char *name, int cnt);

/**
 * @brief Parse a formula string into counts indexed by the global @c element[] table.
 *
 * Tokenizes @p formula as <Capital>[lowercase]*[digits]* (e.g., "C4S2O3H2").
 * Each token is mapped to an @c element[] index via @c elementindex() and its
 * multiplicity added to @p counts[idx]. Characters not starting a token are skipped.
 *
 * @param[in]  formula  NUL-terminated input formula.
 * @param[out] counts   Output array of length @c MAXELEMENTS; zeroed on entry, filled with counts.
 *
 * @warning Unknown symbols should be handled by @c elementindex() (e.g., abort or error).
 */
static void parse_formula_counts(SurgeCtx *ctx, int *counts);

/**
 * @brief Check whether a family with canonical name @p ename is already recorded.
 *
 * @param[in] fam        Array of existing families.
 * @param[in] fam_count  Number of valid entries in @p fam.
 * @param[in] ename      Canonical element name to search (e.g., "S").
 * @return 1 if present, 0 otherwise.
 */
static int already_added_family(const VarFamily *fam, int fam_count, const char *ename);

/**
 * @brief Build variable-valence families by grouping @c element[] entries by .name.
 *
 * A family is recorded if:
 *  - There are at least two entries with the same @c name (variants), and
 *  - One of them is a "bare" variant (inputname==name), and
 *  - The parsed formula has a positive count for that bare variant (needs distribution).
 *
 * @param[in]  base_counts  Counts per @c element[] index from the raw formula.
 * @param[out] fam          Output array to receive families.
 * @param[in]  maxfam       Capacity of @p fam (maximum number of families to record).
 * @return Number of families written to @p fam.
 */
static int build_variable_families(const SurgeCtx *ctx, const int *base_counts,
                                   VarFamily *fam, int maxfam);

/**
 * @brief Build a derived formula string from @c work_counts and run the generator.
 *
 * Serializes all positive entries of @c ctx->work_counts into a formula string,
 * then calls:
 *   decode_formula(derived, &nv, &mine, &maxe, &maxd, &maxc);
 *   start_geng(nv, maxd, maxc, mine, maxe, extra1, res, mod);
 *
 * @param[in,out] vv_ctx  Enumeration context containing counts and run parameters.
 *
 * @note Re-initializes (mine,maxe,maxd,maxc) from the *_in fields for each emission.
 */
static void vv_emit_and_run(SurgeCtx *ctx, VVContext *vv_ctx);

/**
 * @brief Apply or remove a family's split to the working counts vector.
 *
 * For each variant t in family @p F, adds (sign * split[t]) to
 * @c ctx->work_counts[F->var_idx[t]].
 *
 * @param[in,out] ctx   Enumeration context with @c work_counts.
 * @param[in]     var_family     Family descriptor.
 * @param[in]     split Array of length @c F->k with non-negative integers.
 * @param[in]     sign  +1 to apply (add), -1 to undo (subtract).
 */
static void vv_apply_family_split(VVContext *ctx, const VarFamily *var_family,
                                  const int *split, int sign);

/**
 * @brief Enumerate all splits for one family (stars-and-bars) and recurse.
 *
 * Recursively assigns non-negative integers to @p split[0..F->k-1] that sum to
 * @p remaining. When the last position is set, the split is applied to
 * @c work_counts and either the next family is processed or the current
 * combination is emitted.
 *
 * @param[in,out] vv_ctx       Enumeration context.
 * @param[in]     f_idx     Index of the family being processed in @c ctx->fam.
 * @param[in]     pos       Current position within the family's variants [0..k-1].
 * @param[in]     remaining Remaining bare atoms to distribute within this family.
 * @param[in,out] split     Contiguous split buffer; must have capacity for all families'
 *                          variants (sum of k). The caller passes the appropriate offset.
 */
static void vv_enumerate_splits_for_family(SurgeCtx *ctx, VVContext *vv_ctx, int f_idx,
                                           int pos, int remaining,
                                           int *split);

/**
 * @brief Drive enumeration across all families and emit each combination.
 *
 * Computes the total number of variant positions across families to size a
 * temporary @p split buffer, then starts enumeration at family 0.
 *
 * @param[in,out] vv_ctx  Enumeration context (must have fam/fam_count initialized
 *                     and work_counts seeded from base counts).
 *
 * @note This implementation uses @c alloca for the temporary buffer on some platforms.
 *       If strict ISO C is required, replace with @c malloc/@c free.
 */
static void vv_expand_families(SurgeCtx *ctx, VVContext *vv_ctx);

/**
 * @brief Expand all variable-valence families present in @p formula and run the generator.
 *
 * Parses @p formula into counts, discovers families dynamically from @c element[],
 * zeros bare entries to form a base vector, then enumerates the Cartesian product of
 * per-family splits of bare atoms across variants. For each combination, emits a
 * derived formula and calls the normal pipeline.
 *
 * If @c vvswitch is false or no families require expansion, executes a single normal run.
 *
 * @param[in,out] ctx  Execution context with input formula and run parameters.
 */
void run_with_variable_valences(SurgeCtx *ctx);

#endif //SURGE_VV_VARIABLE_VALENCE_H
