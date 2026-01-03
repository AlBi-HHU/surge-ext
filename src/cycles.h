//
// Created by fnellen on 05.09.2025.
//

#ifndef SURGE_VV_CYCLES_H
#define SURGE_VV_CYCLES_H
#include "config.h"

static void inducedpaths(SurgeCtx *ctx, graph *g, int origin, int start, setword body,
                                        setword last, setword path);

void findinducedcycles(SurgeCtx *ctx, graph *g, int n);

#endif //SURGE_VV_CYCLES_H