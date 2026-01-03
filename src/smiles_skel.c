//
// Created by fnellen on 05.09.2025.
//

#include "smiles_skel.h"

#include "config.h"

static void smilesdfs(graph *g, setword *seen, int v, int par, graph *back,
          struct smilesstruct *smilestemp, int *len)
/* Recursive DFS to collect SMILES information */
{
    setword gv,w;
    int k;
    boolean first;

    gv = g[v];
    first = TRUE;
    *seen |= bit[v];

    while (gv)
    {
        TAKEBIT(k,gv);
        if ((*seen & bit[k]))
        {
            if (k != par)
            {
                back[v] |= bit[k];
                back[k] |= bit[v];
            }
        }
        else
        {
            if (first)
            {
                if (POPCOUNT(g[k]) == 1)
                {
                    if ((w = (gv & ~*seen)))   /* really = */
                    {
                        gv |= bit[k];
                        k = FIRSTBITNZ(w);
                        gv &= ~bit[k];
                    }
                }
                smilesdfs(g,seen,k,v,back,smilestemp,len);
                first = FALSE;
            }
            else
            {
                smilestemp[(*len)++].item = SM_CLOSE;
                smilesdfs(g,seen,k,v,back,smilestemp,len);
                smilestemp[(*len)++].item = SM_OPEN;
            }
        }
    }

    smilestemp[*len].item = SM_ATOM;
    smilestemp[*len].x = v;
    ++*len;
    if (par >= 0)
    {
        smilestemp[*len].item = SM_BOND;
        smilestemp[*len].x = par;
        smilestemp[*len].y = v;
        ++*len;
    }
}

void makesmilesskeleton(SurgeCtx *ctx, graph *g, int n)
/* Make a skeleton SMILES structure for use in SMILESoutput */
{
    struct smilesstruct smilestemp[4*MAXN+6*MAXNE];
    graph back[MAXN],ring[MAXN];
    setword w,seen;
    int len,ringnumber;
    int i,j,v;

    for (v = n; --v >= 0; ) if (POPCOUNT(g[v]) == 1) break;
    if (v < 0) v = n-1;

    len = 0;
    seen = 0;
    for (i = 0; i < n; ++i) back[i] = 0;
    for (i = 0; i < n; ++i) ring[i] = 0;

    smilesdfs(g,&seen,v,-1,back,smilestemp,&len);

    SM.smileslen = 0;
    ringnumber = 0;
    for (i = len; --i >= 0; )
    {
        SM.smilesskeleton[SM.smileslen++] = smilestemp[i];
        if (smilestemp[i].item == SM_ATOM)
        {
            v = smilestemp[i].x;
            w = ring[v];
            while (w)
            {
                TAKEBIT(j,w);
                SM.smilesskeleton[SM.smileslen].item = SM_RING1;
                SM.smilesskeleton[SM.smileslen].r = j+1;
                ++SM.smileslen;
            }
            w = back[v];
            while (w)
            {
                TAKEBIT(j,w);
                ++ringnumber;
                SM.smilesskeleton[SM.smileslen].item = SM_RING0;
                SM.smilesskeleton[SM.smileslen].x = v;
                SM.smilesskeleton[SM.smileslen].y = j;
                SM.smilesskeleton[SM.smileslen].r = ringnumber;
                ++SM.smileslen;
                ring[j] |= bit[ringnumber-1];
                back[j] &= ~bit[v];
            }
        }
    }

#ifdef SMILESSKELETON
  /* This will print the SMILES skeleton to stdout */

    fprintf(stdout,"len=%d", SM.smileslen);
    for (i = 0; i < SM.smileslen; ++i)
    {
        switch (SM.smilesskeleton[i].item)
        {
         case SM_ATOM :
            fprintf(stdout," %d", SM.smilesskeleton[i].x);
            break;
         case SM_BOND :
            fprintf(stdout," %d-%d", SM.smilesskeleton[i].x, SM.smilesskeleton[i].y);
            break;
         case SM_OPEN :
            fprintf(stdout," (");
            break;
         case SM_CLOSE :
            fprintf(stdout," )");
            break;
         case SM_RING0 :
            fprintf(stdout," R%d:%d-%d", SM.smilesskeleton[i].r,
                           SM.smilesskeleton[i].x, SM.smilesskeleton[i].y);
            break;
         case SM_RING1 :
            fprintf(stdout," R%d", SM.smilesskeleton[i].r);
        }
    }
    fprintf(stdout,"\n");
#endif
}