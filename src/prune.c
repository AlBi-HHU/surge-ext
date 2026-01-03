//
// Created by fnellen on 05.09.2025.
//

#include "prune.h"

#include "config.h"

extern int geng_maxe;

int surgepreprune(graph *g, int n, int maxn)
/* This function is called by the PREPRUNE service of geng.
 It does -B9 which is more efficient here. If OLDGENG is
 defined, it also speeds up the generation of connected
 graphs.  (For new geng, that function is built in.) */
{

    SurgeCtx *ctx = surge_get_thread_ctx();

    setword notvisited,queue;
    setword c34,w,ww,cyc;
    int ne,nc,i;

    if (R.bad[8])    /* cycle34verts */
    {
        if (n <= 2)
            R.cycle34verts[n] = 0;
        else
        {
            c34 = R.cycle34verts[n-1];
            if (!F.tswitch)
            {
                w = g[n-1];
                while (w)
                {
                    TAKEBIT(i,w);
                    ww = g[i] & w;
                    if (POPCOUNT(ww) > 1) return 1;
                    if ((ww))
                    {
                        cyc = bit[n-1] | bit[i] | ww;
                        if ((c34 & cyc)) return 1;
                        c34 |= cyc;
                    }
                }
            }
            if (!F.fswitch)
            {
                for (i = n-1; --i >= 0;)
                {
                    w = g[i] & g[n-1];
                    if (POPCOUNT(w) > 2) return 1;
                    if (POPCOUNT(w) == 2)
                    {
                        cyc = bit[n-1] | bit[i] | w;
                        if ((c34 & cyc)) return 1;
                        c34 |= cyc;
                    }
                }
            }
            R.cycle34verts[n] = c34;
        }
    }

#ifdef OLDGENG
    if (n == maxn || geng_maxe - maxn >= 5) return 0;

    ne = 0;
    for (i = 0; i < n; ++i) ne += POPCOUNT(g[i]);
    ne /= 2;

    nc = 0;
    notvisited = ALLMASK(n);

    while (notvisited)
    {
        ++nc;
        queue = SWHIBIT(notvisited);
        notvisited &= ~queue;
        while (queue)
        {
            TAKEBIT(i,queue);
            notvisited &= ~bit[i];
            queue |= g[i] & notvisited;
        }
    }

    if (ne - n + nc > geng_maxe - maxn + 1) return 1;
#endif

    return 0;
}

int surgeprune(graph *g, int n, int nmax)
/* This is a procedure that geng will call at each level
using the PRUNE service.
The options -t, -f, -p, -B7,8 are implemented here by
incrementally updating the required counts. */
{
    SurgeCtx *ctx = surge_get_thread_ctx();

    setword w,ax,ay,gx,gy,gxy,gxya,gi,gj;
    int i,j,x,y,k,a,b,extra;
    int i1,i2,i3,i4,d1,d2,d3,d4;
    int v[MAXN];

    if (F.tswitch)
    {
        if (n <= 2)
            R.count3ring[n] = 0;
        else
        {
            extra = 0;
            w = g[n-1];
            while (w)
            {
                TAKEBIT(i,w);
                extra += POPCOUNT(g[i]&w);
            }
            R.count3ring[n] = R.count3ring[n-1] + extra;
            if (R.count3ring[n] > R.max3rings) return 1;
        }
        if (n == nmax && R.count3ring[n] < R.min3rings)
            return 1;
    }

    if (F.fswitch)
    {
        if (n <= 3)
            R.count4ring[n] = 0;
        else
        {
            extra = 0;
            for (i = 0; i < n-1; ++i)
            {
                k = POPCOUNT(g[i]&g[n-1]);
                extra += k*k - k;
            }
            R.count4ring[n] = R.count4ring[n-1] + extra/2;
            if (R.count4ring[n] > R.max4rings) return 1;
        }
        if (n == nmax && R.count4ring[n] < R.min4rings)
            return 1;
    }

    if (F.pswitch)
    {
        if (n <= 4)
            R.count5ring[n] = 0;
        else
        {
            extra = 0;
            for (y = 1; y < n-1; ++y)
            {
                w = g[y] & ~BITMASK(y);
                while (w)
                {
                    TAKEBIT(x,w);
                    ax = (g[x] & g[n-1]) & ~bit[y];
                    ay = (g[y] & g[n-1]) & ~bit[x];
                    extra += POPCOUNT(ax)*POPCOUNT(ay) - POPCOUNT(ax&ay);
                }
            }
            R.count5ring[n] = R.count5ring[n-1] + extra;
            if (R.count5ring[n] > R.max5rings) return 1;
        }
        if (n == nmax && R.count5ring[n] < R.min5rings)
            return 1;
    }

    if (R.bad[6] && n >= 6)
    {
          /* For K_33 we can assume that vertex n-1 is included */
        for (x = n-1; --x >= 1;)
        if (POPCOUNT(g[x]&g[n-1]) >= 3)
        {
             for (y = x; --y >= 0;)
                if (POPCOUNT(g[y]&g[x]&g[n-1]) >= 3) return 1;
        }

          /* But for K_24 we can't */
        for (x = n; --x >= 1;)
        if (POPCOUNT(g[x]) >= 4)
        {
            for (y = x; --y >= 0;)
                if (POPCOUNT(g[x]&g[y]) >= 4) return 1;
        }
    }

    if (R.bad[7])
    {
       /* cone of P4 */
        if (n >= 5)
        {
            for (x = n; --x >= 0;)
            if (POPCOUNT(g[x]) == 4)
            {
              /* Think of a better way to do this */
                w = gx = g[x];
                TAKEBIT(i1,w);
                TAKEBIT(i2,w);
                TAKEBIT(i3,w);
                i4 = FIRSTBITNZ(w);
                d1 = POPCOUNT(g[i1]&gx);
                d2 = POPCOUNT(g[i2]&gx);
                d3 = POPCOUNT(g[i3]&gx);
                d4 = POPCOUNT(g[i4]&gx);
                if (d1 > 0 && d2 > 0 && d3 > 0 && d4 > 0
                   && ((d1 >= 2)+(d2 >= 2)+(d3 >= 2)+(d4 >= 2)) >= 2)
                      return 1;
            }
            else if (POPCOUNT(g[x]) > 4)
            {
                w = gx = g[x];
                i = 0;
                while (w)
                {
                    TAKEBIT(y,w);
                    if (POPCOUNT(g[y]&gx) >= 2) v[i++] = y;
                }
                for (--i; i >= 1; --i)
                for (j = 0; j < i; ++j)
                    if ((g[v[i]] & bit[v[j]]) &&
                              POPCOUNT((g[v[i]]|g[v[j]])&gx) >= 4)
                        return 1;
            }
        }

        /* K4 with a path of 3 edges between two of its vertices */

        if (n >= 6)
        {
            for (x = n; --x >= 1;)
            if (POPCOUNT(g[x]) >= 4)
            {
                for (y = x; --y >= 0;)
                if (POPCOUNT(g[y]) >= 4 && (g[y]&bit[x]))
                {
                    gxy = g[x] & g[y];
                    while (gxy)
                    {
                        TAKEBIT(a,gxy);
                        gxya = gxy & g[a];
                        while (gxya)
                        {
                            TAKEBIT(b,gxya);
                            w = bit[x] | bit[y] | bit[a] | bit[b];
                            gi = g[x] & ~w;
                            gj = g[y] & ~w;
                            while (gi)
                            {
                                TAKEBIT(i,gi);
                                if ((g[i] & gj)) return 1;
                            }
                        }
                    }
                }
            }
        }
    }

#ifdef SURGEPLUGIN_STEP0
    SURGEPLUGIN_STEP0
#endif

    return 0;
}