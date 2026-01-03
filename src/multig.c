//
// Created by fnellen on 05.09.2025.
//

#include "multig.h"

#include "element.h"
#include "gtools.h"
#include "output.h"
#include "vcolg.h"
#include "utils.h"


void colouredges(SurgeCtx *ctx, graph *g, int *vcolindex, int n)
/* This procedure receives graphs from the vcolg phase and
   colours the edges. */
{
    int i,j,k,ne;
    int mult[MAXNE];
    int prev[MAXNE]; /* If >= 0, earlier point that must have greater colour */
    int needed;  /* Extra edges needed */
    int iter[FORMULALEN];
    int vcol[MAXN];  /* index into element[] */

    ne = G.numedges;

    for (i = 0; i < n; ++i)
    {
        vcol[i] = FM.elementtype[vcolindex[i]];
        FM.hyd[i] = element[vcol[i]].valence - G.deg[i];
        if (element[vcol[i]].valence > element[vcol[i]].maxcoord
                    || (element[vcol[i]].valence > 4 && FM.hyd[i] > 0))
            FM.needcoordtest = TRUE;
    }

#ifdef SURGEPLUGIN_STEP2
    SURGEPLUGIN_STEP2
#endif

    needed = (FM.valencesum - FM.hydrogens)/2 - ne;  /* Extra edges needed */

    if (ne == 0 && needed > 0) return;

    if (F.alphabetic)
    {
        iter[0] = 0;
        for (i = 1; i < FM.numtypes; ++i) iter[i] = iter[i-1] + FM.elementcount[i-1];

        for (i = 0; i < n; ++i) FM.newlabel[i] = iter[vcolindex[i]]++;
    }

    if (needed == 0)
    {
        gotone(ctx, vcol,n,mult,ne,0);
        return;
    }

    for (i = 0; i < ne; ++i) prev[i] = -1;

    for (i = 0; i < n; ++i)
    {
        if (G.deg[i] != 1) continue;
        /* Find most recent equivalent j */
        for (j = i; --j >= 0; )
            if (g[i] == g[j] && vcol[j] == vcol[i])
                break;

        if (j >= 0)
        {
            k = FIRSTBITNZ(g[i]);
            prev[G.edgenumber[i][k]] = G.edgenumber[j][k];
        }
    }

    if (GR.vgroupsize == 1)
        GR.egroupsize = 1;
    else
    {
        if (GR.egroupalloc < GR.vgroupsize*ne)
        {
            if (GR.egroup) free(GR.egroup);
            if ((GR.egroup = malloc((GR.vgroupsize+48)*ne*sizeof(int))) == NULL)
                surge_fatal(">E surge : Can't allocate space for egroup\n");
            GR.egroupalloc = (GR.vgroupsize+48) * ne;
        }
        findegroup(ctx, vcol,n,ne);
        if (GR.vgroupsize % GR.egroupsize != 0) surge_fatal(">E egroup error\n");
    }

    if (GR.egroupsize == 1 && needed == 1)
    {
        for (i = 0; i < ne; ++i) mult[i] = 0;
        for (i = 0; i < ne; ++i)
            if (prev[i] < 0 && G.edge[i].maxmult >= 1
                && FM.hyd[G.edge[i].x] > 0 && FM.hyd[G.edge[i].y] > 0)
            {
                mult[i] = 1;
                --FM.hyd[G.edge[i].x];
                --FM.hyd[G.edge[i].y];
                gotone(ctx, vcol,n,mult,ne,ne);
                ++FM.hyd[G.edge[i].x];
                ++FM.hyd[G.edge[i].y];
                mult[i] = 0;
            }
        return;
    }

    if (GR.egroupsize != 1) ++ST.multignontriv;
    if (GR.egroupsize > ST.maxegroup) ST.maxegroup = GR.egroupsize;

    if (R.bad[4] || R.bad[5]) escan2(ctx, 0,needed,vcol,prev,n,mult,ne);
    else              escan(ctx, 0,needed,vcol,prev,n,mult,ne);
}

static void /* Recursive scan for multiplying edges */
escan(SurgeCtx *ctx, int level, int needed, int *vcol,
      int *prev, int n, int *mult, int ne)
{
    int lev,maxlev,k,max,x,y;

    if (needed == 0)
    {
        if (GR.egroupsize > 1 && !testemax(ctx, mult,ne,level))
            return;
        gotone(ctx, vcol,n,mult,ne,level);
    }
    else
    {
        maxlev = ne + 1 - (needed+ L.maxbond-1) / L.maxbond;
        for (lev = level; lev < maxlev; ++lev)
        {
            x = G.edge[lev].x;
            y = G.edge[lev].y;
            max = G.edge[lev].maxmult;

            if (needed < max) max = needed;
            if (FM.hyd[x] < max) max = FM.hyd[x];
            if (FM.hyd[y] < max) max = FM.hyd[y];
            if (prev[lev] >= 0 && mult[prev[lev]] < max)
                max = mult[prev[lev]];

            for (k = 1; k <= max; ++k)
            {
                mult[lev] = k;
                FM.hyd[x] -= k;
                FM.hyd[y] -= k;
                escan(ctx, lev+1,needed-k,vcol,prev,n,mult,ne);
                FM.hyd[x] += k;
                FM.hyd[y] += k;
            }
            mult[lev] = 0;
        }
    }
}

static void escan2(SurgeCtx *ctx, int level, int needed, int *vcol, int *prev, int n, int *mult, int ne)
{
    int lev,maxlev,k,max,x,y;

    if (needed == 0)
    {
        if (GR.egroupsize > 1 && !testemax(ctx, mult,ne,level))
            return;
        gotone(ctx, vcol,n,mult,ne,level);
        return;
    }
    else
    {
        maxlev = ne + 1 - (needed+ L.maxbond-1)/ L.maxbond;
        for (lev = level; lev < maxlev; ++lev)
        {
            x = G.edge[lev].x;
            y = G.edge[lev].y;
            max = G.edge[lev].maxmult;

            if (needed < max) max = needed;
            if (FM.hyd[x] < max) max = FM.hyd[x];
            if (FM.hyd[y] < max) max = FM.hyd[y];
            if (prev[lev] >= 0 && mult[prev[lev]] < max) max = mult[prev[lev]];
            if (G.edge[lev].allenemate1 >= 0 && mult[G.edge[lev].allenemate1] >= 1)
                max = 0;
            if (G.edge[lev].allenemate2 >= 0 && mult[G.edge[lev].allenemate2] >= 1)
                max = 0;

            for (k = 1; k <= max; ++k)
            {
                mult[lev] = k;
                FM.hyd[x] -= k;
                FM.hyd[y] -= k;
                escan2(ctx, lev+1,needed-k,vcol,prev,n,mult,ne);
                FM.hyd[x] += k;
                FM.hyd[y] += k;
            }

            mult[lev] = 0;
        }
    }
}

static boolean testemax(SurgeCtx *ctx, int *mult, int ne, int level)
/* Test if edge colouring is maximum wrt group. */
{
    int *gp,i,j;
    long kgp;

    for (i = level; i < ne; ++i) mult[i] = 0;

    /* kgp really starts at 1 on the next line */
    for (kgp = 1, gp = GR.egroup; kgp < GR.egroupsize; ++kgp, gp += ne)
    {
        for (i = 0; i < ne; ++i)
        {
            j = gp[i];
            if      (mult[j] > mult[i]) return FALSE;
            else if (mult[j] < mult[i]) break;
        }
    }

    return TRUE;
}

static void gotone(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne, int level)
/* Now we have a completed molecule.
   deg[0..n-1] is the simple graph degrees
*/
{
    int i,val;

    for (i = level; i < ne; ++i) mult[i] = 0;

    if (FM.needcoordtest)
        for (i = 0; i < n; ++i)
        {
            if (G.deg[i] + FM.hyd[i] > element[vcol[i]].maxcoord) return;
            //if (G.deg[i] + FM.hyd[i] > 4 && FM.hyd[i] > 0) return;
        }

#ifdef SURGEPLUGIN_STEP3
    SURGEPLUGIN_STEP3
#endif

    ++ST.multigout;
    if (F.uswitch) return;

    if (F.outlevel == 3)
        multigoutput(ctx,vcol,n,mult,ne);
    else if (F.alphabetic)
        alphabeticoutput(ctx,vcol,n,mult,ne);
    else if (F.smiles)
        SMILESoutput(ctx,vcol,n,mult,ne);
    else
        SDFformat(ctx,vcol,n,mult,ne);
}
