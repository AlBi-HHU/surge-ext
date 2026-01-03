//
// Created by fnellen on 21.09.2025.
//

#ifndef SURGE_VV_VV_SCHED_H
#define SURGE_VV_VV_SCHED_H
#include "variable_valence.h"

char *vv_make_part_path(const char *final, int seq);
void  vv_spawn_child_for_formula(SurgeCtx *ctx, VVContext *vvc, const char *derived);
void  vv_wait_one(SurgeCtx *ctx, VVContext *vvc);
void  vv_wait_all(SurgeCtx *ctx, VVContext *vvc);
char *vv_make_stats_path(const char *part_path);

#endif //SURGE_VV_VV_SCHED_H