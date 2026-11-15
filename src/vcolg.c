//
// Created by fnellen on 05.09.2025.
//

#include "vcolg.h"

#include "element.h"
#include "gtools.h"
#include "multig.h"
#include "naugroup.h"
#include "utils.h"

void colourvertices(SurgeCtx *ctx, graph *g, int n)
/* Main procedure for vcolg phase */
{
    static DEFAULTOPTIONS_GRAPH(options);
    statsblk stats;
    setword workspace[2*MAXN];
    grouprec *group;
    int lab[MAXN],ptn[MAXN],orbits[MAXN];
    int prev[MAXN]; /* If >= 0, earlier point that must have greater colour */
    int weight[MAXN];
    int maxcolindex[MAXN];  /* Max colour index for each vertex */
    int remain[FORMULALEN];  /* Remaining number of colours for each vertex */
    int vcolindex[MAXN];  /* Index into elementtype[] */
    int i,j;

#ifdef SURGEPLUGIN_STEP1
    SURGEPLUGIN_STEP1
#endif

    for (i = 0; i < n; ++i)
    {
        prev[i] = -1;
        weight[i] = n*POPCOUNT(g[i]);
    }

    for (i = 0; i < n; ++i)
    {
        if (POPCOUNT(g[i]) != 1) continue;
        /* Find most recent equivalent j */
        for (j = i; --j >= 0; )
            if (g[j] == g[i]) break;

        if (j >= 0)
        {
            prev[i] = j;
            weight[i] = weight[j] + 1;
        }
    }

    options.userautomproc = groupautomproc;
    options.userlevelproc = grouplevelproc;
    options.defaultptn = FALSE;

    setlabptn(weight,lab,ptn,n);

    nauty(g,lab,ptn,NULL,orbits,&options,&stats,workspace,2*MAXN,1,n,NULL);

    if (stats.grpsize2 > 0 || stats.grpsize1 > 1e7)
        surge_fatal(">E surge : vgroup size greater than 10^7 encountered\n");
    GR.vgroupsize = stats.grpsize1 + 0.01;

    for (i = 0; i < FM.numtypes; ++i) remain[i] = FM.elementcount[i];
    for (i = 0; i < n; ++i) maxcolindex[i] = FM.maxtype[G.deg[i]];

    if (GR.vgroupsize == 1)  /* Trivial group */
    {
        vscan(ctx, 0,vcolindex,g,prev,maxcolindex,remain,n);
        return;
    }

    ++ST.vcolgnontriv;
    if (GR.vgroupsize > ST.maxvgroup) ST.maxvgroup = GR.vgroupsize;

    group = groupptr(FALSE);
    makecosetreps(group);

    if (GR.vgroupalloc < (GR.vgroupsize-1) * n)
    {
        if (GR.vgroup) free(GR.vgroup);
        if ((GR.vgroup = malloc((GR.vgroupsize+48)*n*sizeof(int))) == NULL)
            surge_fatal(">E surge : Can't allocate space for vgroup\n");
        GR.vgroupalloc = (GR.vgroupsize+48) * n;
    }

    GR.vgroupcount = 0;
    allgroup(group,storevgroup);

    if (GR.vgroupcount != GR.vgroupsize) surge_fatal(">E surge : vgroup error\n");

   /* Check the logic of the next section. What about maxdeg? */
    if (FM.numtypes == 1)
    {
        for (i = 0; i < n; ++i) vcolindex[i] = 0;
        ++ST.vcolgout;
        colouredges(ctx, g,vcolindex,n);
        return;
    }

    j = n;      /* Can choose a better orbit? */
    for (i = 0; i < n; ++i)
        if (orbits[i] < i && orbits[i] < j) j = orbits[i];
    for (i = j + 1; i < n; ++i)
        if (orbits[i] == j) prev[i] = j;

    vscan(ctx, 0,vcolindex,g,prev,maxcolindex,remain,n);
}

static void storevgroup(int *p, int n)
/* Called by allgroup; store full group at vcolg phase */
{
    SurgeCtx *ctx = surge_get_thread_ctx();
    int *gp,i;

    if (GR.vgroupcount == 0)
    {
        GR.vgroupcount = 1;   /* Don't store identity */
        return;
    }

    gp = GR.vgroup + n * (GR.vgroupcount-1);
    for (i = 0; i < n; ++i) gp[i] = p[i];

    ++GR.vgroupcount;
}

static void vscan(SurgeCtx *ctx, int level, int *colindex, graph *g, int *prev,
                               int *maxcolindex, int *remain, int n)
/* Recursive vertex colour scan */
{
    int k,max;

    if (level == n)
    {
        if (GR.vgroupsize == 1 || testvmax(ctx, colindex,n))
        {
            ++ST.vcolgout;
            if (F.outlevel == 2)
            {
                if (!F.uswitch) vcolgoutput(ctx, g,colindex,n);
            }
            else
                colouredges(ctx, g,colindex,n);
        }
        return;
    }

    max = maxcolindex[level];
    if (prev[level] >= 0 && colindex[prev[level]] < max)
        max = colindex[prev[level]];

    for (k = 0; k <= max; ++k)
    {
        if (remain[k] == 0) continue;
        colindex[level] = k;
        --remain[k];
        vscan(ctx, level+1,colindex,g,prev,maxcolindex,remain,n);
        ++remain[k];
    }
}

static boolean testvmax(SurgeCtx *ctx, int *colindex, int n)
/* Test if vertex colouring is maximum wrt group. If so, return group.
   If not, return a safe level to return to. */
{
    int *gp,i,j;
    long kgp;

    /* kgp really starts at 1 on the next line */
    for (kgp = 1, gp = GR.vgroup; kgp < GR.vgroupsize; ++kgp, gp += n)
    {
        for (i = 0; i < n; ++i)
        {
            j = gp[i];
            if      (colindex[j] > colindex[i]) return FALSE;
            else if (colindex[j] < colindex[i]) break;
        }
    }

    return TRUE;
}

static void vcolgoutput(SurgeCtx *ctx, graph *g, int *vcolindex, int n)
/* Write output equal to the vcolg -T format. */
{
    char line[10+30*MAXN],*p;
    int i,j,ne;
    setword w;

    ne = 0;
    for (i = 0; i < n; ++i) ne += POPCOUNT(g[i]);
    ne /= 2;

    p = line;
    put_int(&p, n); space(&p); put_int(&p, ne);
    for (i = 0; i < n; ++i)
    {
        space(&p);
        put_int(&p, element[FM.elementtype[vcolindex[i]]].index);
    }
    space(&p);
    for (i = 0; i < n; ++i)
    {
        w = g[i] & BITMASK(i);
        while (w)
        {
            TAKEBIT(j,w);
            space(&p); put_int(&p, i); space(&p); put_int(&p, j);
        }
    }
    *(p++) = '\n';
    *p = '\0';

#ifdef ZLIB
    if (F.gzip)
    {
        if (gzputs(O.gzoutfile,line) < 0)
            surge_fatal(">E surge : zlib output error\n");
        return;
    }
#endif

    if (fputs(line, O.outfile) == EOF) surge_fatal(">E surge : output error\n");
}

void findegroup(SurgeCtx *ctx, int *vcol, int n, int ne)
/* Set egroup to the set of vertex-colour-preserving vgroup elements */
{
    int *vgp,*egp,i,j,kgp;

    egp = GR.egroup;

    /* kgp really starts at 1 on the next line */
    for (kgp = 1, vgp = GR.vgroup; kgp < GR.vgroupsize; ++kgp, vgp += n)
    {
        for (i = 0; i < n; ++i)
            if (vcol[vgp[i]] != vcol[i]) break;
        if (i == n)
            for (j = 0; j < ne; ++j)
                *(egp++) = G.edgenumber[vgp[G.edge[j].x]][vgp[G.edge[j].y]];
    }

    GR.egroupsize = 1 + (egp - GR.egroup) / ne;
}
