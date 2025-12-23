//
// Created by fnellen on 05.09.2025.
//

#ifndef SURGE_VV_SMILES_SKEL_H
#define SURGE_VV_SMILES_SKEL_H
#include "config.h"

static void smilesdfs(graph *g, setword *seen, int v, int par, graph *back,
          struct smilesstruct *smilestemp, int *len);

void makesmilesskeleton(SurgeCtx *ctx, graph *g, int n);

#endif //SURGE_VV_SMILES_SKEL_H