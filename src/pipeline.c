//
// Created by fnellen on 05.09.2025.
//

#include "cycles.h"
#include "formula.h"
#include "geng_driver.h"
#include "gtools.h"
#include "utils.h"
#include "planarity_util.h"
#include "smiles_skel.h"
#include "vcolg.h"

void surge_run_pipeline(SurgeCtx *ctx) {
    decode_formula(ctx);
    start_geng(ctx);
}

void surgeproc(FILE *outfile, graph *gin, int n)
/* This is called by geng for each graph. */
{
    SurgeCtx *ctx = surge_get_thread_ctx();

    int i,j,k,d,n1,n12,n34,n4,ne;
    int isize,jsize;
    graph g[MAXN];
    setword w,wxy,ww,pw,cyc,cycle8;
    int x,y,e1,e2,e3;

    n1 = n12 = n34 = n4 = 0;

    ++ST.gengout;

    for (i = 0; i < n; ++i)
    {
        d = POPCOUNT(gin[i]);   /* deg[i] is not defined yet */
        if      (d == 1) { ++n1; ++n12; }
        else if (d == 2) ++n12;
        else if (d == 3) ++n34;
        else             { ++n34; ++n4; }
    }

    if (n > 1 && (n1 < FM.min1 || n12 < FM.min12 || n34 > FM.max34 || n4 > FM.max4))
        return;

    if (F.planar && !isplanar(gin,n)) return;    /* Try later */

    ++ST.genggood;

   /* Reverse to put higher degrees first */

    for (i = 0; i < n; ++i)
    {
        w = gin[n-i-1];
        pw = 0;
        while (w)
        {
            TAKEBIT(j,w);
            pw |= bit[n-j-1];
        }
        g[i] = pw;
    }

   /* Make the edge list with default parameters */

    ne = 0;
    for (i = 0; i < n; ++i)
    {
        G.deg[i] = POPCOUNT(g[i]);
        w = g[i] & BITMASK(i);
        while (w)
        {
            TAKEBIT(j,w);
            G.edge[ne].x = i;
            G.edge[ne].y = j;
            G.edge[ne].xy = bit[i] | bit[j];
            G.edge[ne].maxmult = L.maxbond;
            G.edge[ne].allenemate1 = G.edge[ne].allenemate2 = -1;
            G.edgenumber[i][j] = G.edgenumber[j][i] = ne;
            ++ne;
        }
    }
    G.numedges = ne;

    if (R.needcycles)
    {
        if (ne > WORDSIZE)
            surge_fatal(">E surge : too many edges for badlists\n");
        findinducedcycles(ctx, g,n);
    }

    if (R.bad[0])  /* no triple bonds in rings smaller than 7 */
    {
        for (i = 0; i < R.cyclecount; ++i)
        {
            cyc = R.inducedcycle[i];
            if (POPCOUNT(cyc) <= 7)
                while (cyc)
                {
                    TAKEBIT(j,cyc);
                    if (G.edge[j].maxmult == 2) G.edge[j].maxmult = 1;
                }
        }
    }

    if (R.bad[1])  /* Bredt's rule for one common bond */
    {
        for (i = 0; i < R.cyclecount-1; ++i)
        {
            isize = POPCOUNT(R.inducedcycle[i]);
            if (isize > 6) continue;
            for (j = i+1; j < R.cyclecount; ++j)
            {
                jsize = POPCOUNT(R.inducedcycle[j]);
                if (jsize > 6) continue;

                w = R.inducedcycle[i] & R.inducedcycle[j];
                if (POPCOUNT(w) != 1) continue;

                if (isize*jsize <= 15)
                    G.edge[FIRSTBITNZ(w)].maxmult = 0;

                if (isize+jsize <= 9)
                {
                    wxy = G.edge[FIRSTBITNZ(w)].xy;
                    ww = (R.inducedcycle[i] | R.inducedcycle[j]) & ~w;
                    while (ww)
                    {
                        TAKEBIT(k,ww);
                        if ((G.edge[k].xy & wxy)) G.edge[k].maxmult = 0;
                    }
                }
            }
        }
    }

    if (R.bad[2])  /* Bredt's rule for two common bonds */
    {
        for (i = 0; i < R.cyclecount-1; ++i)
        {
            isize = POPCOUNT(R.inducedcycle[i]);
            if (isize == 3 || isize > 6) continue;

            for (j = i+1; j < R.cyclecount; ++j)
            {
                jsize = POPCOUNT(R.inducedcycle[j]);
                if (jsize == 3 || jsize > 6 || isize+jsize == 12) continue;

                w = R.inducedcycle[i] & R.inducedcycle[j];
                if (POPCOUNT(w) != 2) continue;

                ww = w;
                TAKEBIT(k,ww);
                G.edge[k].maxmult = 0;
                G.edge[FIRSTBITNZ(ww)].maxmult = 0;
                wxy = G.edge[k].xy ^ G.edge[FIRSTBITNZ(ww)].xy;

                ww = (R.inducedcycle[i] | R.inducedcycle[j]) & ~w;
                while (ww)
                {
                    TAKEBIT(k,ww);
                    if ((G.edge[k].xy & wxy)) G.edge[k].maxmult = 0;
                }
            }
        }
    }

    if (R.bad[3]) /* Bredt's rule for two hexagons with 3 bonds in common */
    {
        for (i = 0; i < R.cyclecount-1; ++i)
        {
            isize = POPCOUNT(R.inducedcycle[i]);
            if (isize != 6) continue;

            for (j = i+1; j < R.cyclecount; ++j)
            {
                jsize = POPCOUNT(R.inducedcycle[j]);
                if (jsize != 6) continue;

                w = R.inducedcycle[i] & R.inducedcycle[j];
                if (POPCOUNT(w) != 3) continue;

                ww = R.inducedcycle[i] | R.inducedcycle[j];

                TAKEBIT(e1,w);
                TAKEBIT(e2,w);
                e3 = FIRSTBITNZ(w);

                wxy = G.edge[e1].xy ^ G.edge[e2].xy ^ G.edge[e3].xy;
                while (ww)
                {
                    TAKEBIT(k,ww);
                    if ((G.edge[k].xy & wxy)) G.edge[k].maxmult = 0;
                }
            }
        }
    }

    if (R.bad[4])  /* No A=A=A, whether in ring or not */
    {
        for (i = 0; i < n; ++i)
        if (G.deg[i] == 2)
        {
            x = FIRSTBITNZ(g[i]);
            y = FIRSTBITNZ(g[i]&~bit[x]);
            e1 = G.edgenumber[i][x];
            e2 = G.edgenumber[i][y];
            if (G.edge[e2].allenemate1 < 0)
            {
                 if (e1 < e2) G.edge[e2].allenemate1 = e1;
                 else         G.edge[e1].allenemate1 = e2;
            }
            else
            {
                 if (e1 < e2) G.edge[e2].allenemate2 = e1;
                 else         G.edge[e1].allenemate2 = e2;
            }
        }
    }

    if (R.bad[5])  /* No A=A=A in rings up to length 8 */
    {
        cycle8 = 0;
        for (i = 0; i < R.cyclecount; ++i)
            if (POPCOUNT(R.inducedcycle[i]) <= 8) cycle8 |= R.inducedcycle[i];

        for (i = 0; i < n; ++i)
        if (G.deg[i] == 2)
        {
            x = FIRSTBITNZ(g[i]);
            y = FIRSTBITNZ(g[i]&~bit[x]);
            e1 = G.edgenumber[i][x];
            if (!(bit[e1] & cycle8)) continue;
            e2 = G.edgenumber[i][y];
            if (G.edge[e2].allenemate1 < 0)
            {
                 if (e1 < e2) G.edge[e2].allenemate1 = e1;
                 else         G.edge[e1].allenemate1 = e2;
            }
            else
            {
                 if (e1 < e2) G.edge[e2].allenemate2 = e1;
                 else         G.edge[e1].allenemate2 = e2;
            }
        }
    }

    if (F.outlevel == 1)
    {
        if (!F.uswitch) writeg6(outfile,g,1,n);
        return;
    }

   /* Make a SMILES skeleton structure for later use */
    if (F.smiles) makesmilesskeleton(ctx, g,n);

    colourvertices(ctx, g,n);
}
