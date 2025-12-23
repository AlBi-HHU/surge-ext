//
// Created by fnellen on 21.09.2025.
//

#ifndef SURGE_VV_VV_PROGRESS_H
#define SURGE_VV_VV_PROGRESS_H
#include "variable_valence.h"

char spin_frame(double t);

double vv_now_s(void);

void vv_progress_render(const SurgeCtx *ctx, VVContext *vvc, const char *phase);

void vv_progress_done(const VVContext *vvc);

unsigned long long vv_count_total_jobs(const VVContext *vvc);

#endif //SURGE_VV_VV_PROGRESS_H