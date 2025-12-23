//
// Created by fnellen on 05.09.2025.
//

#ifndef SURGE_VV_PIPELINE_H
#define SURGE_VV_PIPELINE_H
#include "config.h"

void surge_run_pipeline(SurgeCtx *ctx);

void surgeproc(FILE *outfile, graph *gin, int n);

#endif //SURGE_VV_PIPELINE_H