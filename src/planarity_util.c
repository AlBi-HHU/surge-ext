//
// Created by fnellen on 05.09.2025.
//

#include "planarity_util.h"
#include "planarity.h"

boolean isplanar(graph *g, int n)
/* Check if g is planar, assuming g is connected */
{
    t_ver_sparse_rep V[MAXN];
    t_adjl_sparse_rep A[2*MAXNE+1];
    t_dlcl **dfs_tree,**back_edges,**mult_edges;
    t_ver_edge *embed_graph;
    int i,j,k,pop,nv,ne,c;
    int edge_pos,v,w;
    setword ww;
    boolean ans;
    graph h[MAXN];
    int newlab[MAXN];
    setword queue;

    queue = 0;
    ne = 0;
    for (i = 0; i < n; ++i)
    {
        h[i] = g[i];
        pop = POPCOUNT(h[i]);
        ne += pop;
        if (pop <= 2) queue |= bit[i];
    }
    ne /= 2;
    nv = n;

    while (queue && ne >= nv+3)
    {
        TAKEBIT(i,queue);
        pop = POPCOUNT(h[i]);
        if (pop == 1)  /* i--j with deg(i)=1 */
        {
            j = FIRSTBITNZ(h[i]);
            h[i] = 0;
            h[j] &= ~bit[i];
            --nv;
            --ne;
            queue |= bit[j];
        }
        else if (pop == 2)  /* j--i--k with deg(i)=2 */
        {
            j = FIRSTBITNZ(h[i]);
            k = FIRSTBITNZ(h[i] & ~bit[j]);
            h[i] = 0;
            h[j] &= ~bit[i];
            h[k] &= ~bit[i];
            --nv;
            if ((h[j] & bit[k]))
            {
                ne -= 2;
                queue |= (bit[j] | bit[k]);
            }
            else
            {
                --ne;
                h[j] |= bit[k];
                h[k] |= bit[j];
            }
        }
    }

    if (ne <= nv + 2) return TRUE;
    if (nv == 5 && ne <= 9) return TRUE;

    nv = 0;
    for (i = 0; i < n; ++i) if (h[i] != 0) newlab[i] = nv++;

    k = 0;
    for (i = 0; i < n; ++i)
    if (h[i] != 0)
    {
        V[newlab[i]].first_edge = k;
        ww = h[i];
        while (ww)
        {
            TAKEBIT(j,ww);
            A[k].end_vertex = newlab[j];
            A[k].next = k+1;
            ++k;
        }
        A[k-1].next = NIL;
    }

    ne = k/2;

    ans = sparseg_adjl_is_planar(V,nv,A,&c,&dfs_tree,&back_edges,
             &mult_edges,&embed_graph,&edge_pos,&v,&w);

    sparseg_dlcl_delete(dfs_tree,nv);
    sparseg_dlcl_delete(back_edges,nv);
    sparseg_dlcl_delete(mult_edges,nv);
    embedg_VES_delete(embed_graph,nv);

    return ans;
}