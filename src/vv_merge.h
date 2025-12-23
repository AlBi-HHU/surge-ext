//
// Created by fnellen on 21.09.2025.
//

#ifndef SURGE_VV_VV_MERGE_H
#define SURGE_VV_VV_MERGE_H
#include <stddef.h>

#include "config.h"
#include "variable_valence.h"

/* ----- MERGE progress bar ----- */
typedef struct {
    unsigned long long total_bytes;
    unsigned long long copied_bytes;
    double last_render_s;
    int inline_mode; /* -1 unknown, 0 newline per update, 1 \r inline */
} VVMergePB;

void vv_human_bytes(char *buf, size_t bufsz, unsigned long long b);

void vv_mergepb_render(VVMergePB *pb, const char *phase);

void vv_mergepb_done(VVMergePB *pb);

void vv_merge_parts_into_final(SurgeCtx *ctx, VVContext *vvc);

#endif //SURGE_VV_VV_MERGE_H