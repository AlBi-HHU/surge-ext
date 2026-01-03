//
// Created by fnellen on 31.08.2025.
//

#ifndef SURGE_VV_ELEMENT_H
#define SURGE_VV_ELEMENT_H
#include "config.h"

extern Element element[MAXELEMENTS];

/* Add an element to element[]. inputname and name must be of the
form A or Aa. If name==NULL then name is the same as inputname.  If
maxcoord==0 or maxcoord > valence then maxcoord is the same as valence. */
void addelement(SurgeCtx *ctx, char *inputname, char *name, int valence, int maxcoord);

/* Index into element[] of element with this element input name,
   or -1 if it isn't present. */
int elementindex(SurgeCtx *ctx, char *inputname);

void calculate_number_of_elements(SurgeCtx *ctx);

#endif //SURGE_VV_ELEMENT_H
