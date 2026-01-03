//
// Created by fnellen on 05.09.2025.
//

#ifndef SURGE_VV_MULTIG_H
#define SURGE_VV_MULTIG_H
#include "config.h"

void colouredges(SurgeCtx *ctx, graph *g, int *vcolindex, int n);

/* Recursive scan for multiplying edges */
static void escan(SurgeCtx *ctx, int level, int needed, int *vcol,
      int *prev, int n, int *mult, int ne);

/* Recursive scan for multiplying edges
   Version which checks allenemate for -B3 */
static void escan2(SurgeCtx *ctx, int level, int needed, int *vcol, int *prev, int n, int *mult, int ne);

static boolean testemax(SurgeCtx *ctx, int *mult, int ne, int level);

static void gotone(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne, int level);

#endif //SURGE_VV_MULTIG_H