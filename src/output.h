//
// Created by fnellen on 05.09.2025.
//

#ifndef SURGE_VV_OUTPUT_H
#define SURGE_VV_OUTPUT_H
#include "config.h"

void SMILESoutput(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne);

void SDFformat(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne);

void multigoutput(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne);

void alphabeticoutput(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne);

#endif //SURGE_VV_OUTPUT_H