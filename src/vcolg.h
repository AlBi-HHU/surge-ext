//
// Created by fnellen on 05.09.2025.
//

#ifndef SURGE_VV_VCOLG_H
#define SURGE_VV_VCOLG_H
#include "config.h"

void colourvertices(SurgeCtx *ctx, graph *g, int n);

void findegroup(SurgeCtx *ctx, int *vcol, int n, int ne);

static void vscan(SurgeCtx *ctx, int level, int *colindex, graph *g, int *prev,
                               int *maxcolindex, int *remain, int n);

static boolean testvmax(SurgeCtx *ctx, int *colindex, int n);

static void storevgroup(int *p, int n);

static void vcolgoutput(SurgeCtx *ctx, graph *g, int *vcolindex, int n);

#endif //SURGE_VV_VCOLG_H