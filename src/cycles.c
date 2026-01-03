//
// Created by fnellen on 05.09.2025.
//

#include "cycles.h"

#include "gtools.h"
#include "utils.h"

void findinducedcycles(SurgeCtx *ctx, graph *g, int n)
/* Find all the induced cycles */
{
    setword body,last,cni;
    int i,j;

#if 0
    body = 0;
    for (i = 0; i < n; ++i)
        if (POPCOUNT(g[i]) > 1) body |= bit[i];
#else
    body = ALLMASK(n);
#endif

    R.cyclecount = 0;

    for (i = 0; i < n-2; ++i)
    {
        body &= ~bit[i];
        last = g[i] & body;
        cni = g[i] | bit[i];
        while (last)
        {
            TAKEBIT(j,last);
            inducedpaths(ctx, g,i,j,body&~cni,last,bit[G.edgenumber[i][j]]);
        }
        if (R.cyclecount > 3*MAXCYCLES/4) surge_fatal(">E increase MAXCYCLES\n");
    }

    if (R.cyclecount > R.maxcycles) R.maxcycles = R.cyclecount;

#if 0
    printf("cyclecount=%d\n",cyclecount);

    {
        setword cyc; int i,j;

        for (i = 0; i < cyclecount; ++i)
        {
            cyc = inducedcycle[i];
            while (cyc)
            {
                TAKEBIT(j,cyc);
                printf(" %d-%d",edge[j].x,edge[j].y);
            }
            printf("\n");
        }
    }
#endif
}

static void inducedpaths(SurgeCtx *ctx, graph *g, int origin, int start, setword body,
                                        setword last, setword path)
/* Number of induced paths in g starting at start, extravertices within
 * body and ending in last.
 * {start}, body and last should be disjoint. */
{
    setword gs,w;
    int i;

    gs = g[start];

    w = gs & last;
    while (w)
    {
        TAKEBIT(i,w);
        R.inducedcycle[R.cyclecount++]
           = path | bit[G.edgenumber[start][i]] | bit[G.edgenumber[origin][i]];
    }

    w = gs & body;
    while (w)
    {
        TAKEBIT(i,w);
        inducedpaths(ctx, g,origin,i,body&~gs,last&~bit[i]&~gs,
                                         path|bit[G.edgenumber[start][i]]);
    }
}
