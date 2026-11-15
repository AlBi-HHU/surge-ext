//
// config.h — central configuration and run context for SURGE
//

#ifndef SURGE_VV_CONFIG_H
#define SURGE_VV_CONFIG_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>


/* ------------------- Third-party integration ------------------- */
#include "nauty.h"   /* provides setword, WORDSIZE, etc. */

#ifdef ZLIB
#include <zlib.h>  /* gzFile */
#endif

/* ------------------- Global size limits ------------------- */
/* MAXN must be <= WORDSIZE (32 or 64) for nauty's bitsets. */
#ifndef WORDSIZE
#  error "nauty WORDSIZE must be defined before including config.h"
#endif

#define MAXN            WORDSIZE
#define MAXNE           (MAXN*(MAXN-1)/2)  /* simple graph upper bound */
#define MAXCYCLES       200
#define MAXELEMENTS     100
#define FORMULALEN      300
#define BADLISTS        9

#define VV_MAX_CHILDREN 8192

/* ------------------- Error codes for finalize ------------------- */
#define SURGE_OK                 0
#define SURGE_ERR_ZLIB_DISABLED (-3)
#define SURGE_ERR_A_AND_S       (-4)
#define SURGE_ERR_OPEN_OUTPUT   (-5)
#define SURGE_ERR_BAD_M_RANGE   (-6)

#define F  (ctx->flags)
#define L  (ctx->lim)
#define IN (ctx->in)
#define O  (ctx->out)
#define G  (ctx->g)
#define R  (ctx->rings)
#define FM (ctx->fm)
#define SM (ctx->smi)
#define GR (ctx->groups)
#define SP (ctx->split)
#define ST (ctx->stats)


#define SM_ATOM  1  /* Atom number x */
#define SM_BOND  2  /* Bond x,y */
#define SM_OPEN  3  /* Open ( */
#define SM_CLOSE 4  /* Close ) */
#define SM_RING0 5  /* Broken bond x,y for new ring r */
#define SM_RING1 6  /* End of ring r */

int GENG_MAIN(int argc, char *argv[]);

/* ------------------- Domain types ------------------- */
typedef unsigned long long counter;

typedef struct {
    const char *inputname; /* variant token, e.g., "Sx", "Sy", "S" */
    const char *name; /* canonical family name, e.g., "S"     */
    bool organic; /* belongs to organic subset             */
    int valence; /* typical valence used by SURGE         */
    int lowervalence; /* next lower valence, or 0 if none      */
    int maxcoord; /* max distinct neighbours incl. H       */
    int index; /* used in -T style outputs              */
    int  charge;       /* formal atomic charge in units of e (e.g., +1, -1, 0) */
} Element;

typedef struct {
    int x, y; /* atoms connected by this bond          */
    int maxmult; /* extra multiplicity; total 1..1+maxmult*/
    int allenemate1; /* edge index mutually exclusive (multi) */
    int allenemate2; /* edge index mutually exclusive (multi) */
    setword xy; /* helper bitmask                        */
} edgetype;

/* ------------------- Grouped run context ------------------- */

/* 1) Flags / options toggles */
typedef struct {
    bool uswitch; /* write graphs instead of molecules */
    bool verbose;
    int outlevel; /* -O# (1..4) */

    bool smiles;
    bool alphabetic;
    bool gzip;

    bool planar;
    bool bipartite;
    bool xswitch;
    bool notriples;

    bool tswitch, fswitch, pswitch; /* ring counters enabled */
    bool vvswitch; /* variable valence */
    bool zw_enable;
    int  zw_maxpairs;
    bool ions;

    int vv_max_procs;
    bool vv_worker; /* child pipeline failures are counted as skipped splits */
} SurgeFlags;

/* 2) Input configuration */
typedef struct {
    char *formula; /* required */
    char *extra1; /* payload after -G (may be NULL) */
    bool excluded[MAXELEMENTS]; /* exact element states excluded by -X/-F */
} InputCfg;

/* 3) Geng limits (caps and ranges) */
typedef struct {
    int maxc; /* -c (default 6) */
    int maxd; /* -d (default 6) */
    int mine; /* from -e */
    int maxe; /* from -e or NOLIMIT */
    int maxbond; /* 1 if -T/notriples, else 2 */
    int seed_maxc, seed_maxd, seed_mine, seed_maxe, seed_maxbond;
} Limits;

/* 4) Work splitting (residue/mod) */
typedef struct {
    long res; /* -m residue */
    long mod; /* -m modulus (>=1) */
} Split;

/* 5) Formula/element-derived state */
typedef struct {
    int nv, numtypes;
    int elementtype[FORMULALEN], elementcount[FORMULALEN];
    char canonform[2 * FORMULALEN];
    int valencesum;
    int maxtype[8];
    int newlabel[MAXN];
    int min1, min12, max34, max4;
    int numelements;
    int maxindex;

    int hydrogens;
    int hyd[MAXN];

    bool needcoordtest;

    int element_maxcoord0[MAXELEMENTS];
} FormulaState;

/* 6) Ring/“badlist” constraints and cycle workspace */
typedef struct {
    bool bad[1 + BADLISTS]; /* indices 1..9 used */
    bool needcycles;
    int maxcycles;

    long min3rings, max3rings;
    long min4rings, max4rings;
    long min5rings, max5rings;

    int count3ring[MAXN + 1];
    int count4ring[MAXN + 1];
    int count5ring[MAXN + 1];

    setword inducedcycle[MAXCYCLES];
    int cyclecount;
    setword cycle34verts[MAXN + 1];
} RingState;

/* 7) Stats / counters */
typedef struct {
    counter gengout, genggood;
    counter vcolgnontriv, vcolgout;
    counter multignontriv, multigout;
    counter skipped;
    long maxvgroup, maxegroup;
} SurgeStats;

/* 8) Vertex/edge group buffers (allocated lazily) */
typedef struct {
    long vgroupsize, egroupsize;
    size_t vgroupalloc, egroupalloc;
    int *vgroup, *egroup;
    long vgroupcount;
} GroupBuffers;

/* 9) Graph-level workspace (current candidate) */
typedef struct {
    int numedges;
    edgetype edge[MAXNE];
    int edgenumber[MAXN][MAXN]; /* -1 if none */
    int deg[MAXN]; /* simple degree */
    int multdeg[MAXN]; /* multiplicity-weighted degree */
} GraphState;

/* 10) SMILES scratch */
typedef struct {
    struct smilesstruct {
        int item, x, y, r;
    } smilesskeleton[4 * MAXN + 6 * MAXNE];

    int smileslen;
} SmilesBuf;

/* 11) Output sinks (and target path) */
typedef struct {
    const char *filename; /* "stdout" | "-" | path */
    FILE *outfile;
#ifdef ZLIB
    gzFile gzoutfile;
#endif
} OutputSinks;

/* 12) Composite run context */
typedef struct {
    SurgeFlags flags;
    InputCfg in;
    Limits lim;
    Split split;
    FormulaState fm;
    RingState rings;
    SurgeStats stats;
    GroupBuffers groups;
    GraphState g;
    SmilesBuf smi;
    OutputSinks out;
} SurgeCtx;

/* ------------------- Lifecycle & helpers ------------------- */
void surge_ctx_init(SurgeCtx *ctx);
void surge_ctx_reset_graph(SurgeCtx *ctx, int n);

int surge_ctx_finalize(SurgeCtx *ctx);
void surge_close_output(SurgeCtx *ctx);

/* Thread-local bridge for geng callbacks (optional but handy) */
void surge_set_thread_ctx(SurgeCtx *ctx);

SurgeCtx *surge_get_thread_ctx(void);

void surge_snapshot_element_runtime(SurgeCtx *ctx);
void surge_restore_element_runtime(SurgeCtx *ctx);
void surge_capture_limit_seed(SurgeCtx *ctx);
void surge_reset_limits_from_seed(SurgeCtx *ctx);
void surge_reset_formula_workspace(SurgeCtx *ctx);

bool surge_parse_cli(int argc, char **argv, SurgeCtx *ctx);
void surge_print_summary(SurgeCtx *ctx, double t1, double t2);
void surge_validate_operation(int frc);

#endif //SURGE_VV_CONFIG_H
