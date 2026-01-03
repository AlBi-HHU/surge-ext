//
// Created by fnellen on 05.09.2025.
//

#include "geng_driver.h"

#include "element.h"

void start_geng(SurgeCtx *ctx)
/* start geng with arguments, extra1 before n, extra2 after n */
{
    int i,geng_argc,mind;
    char *geng_argv[20];
    char arga[30],argb[30];
    char resmod[40];
    char gengargs[80];
    char edgecount[40];

    mind = 1;

    if (FM.hydrogens == 0) {
        for (i = 0; i < FM.numtypes; ++i)
            if (element[FM.elementtype[i]].valence < 4) break;
        if (i == FM.numtypes) mind = 2;
    }

    if (FM.nv == 1) mind = 0;

    sprintf(arga,"-qcd%dD%d",mind, L.maxd);
    sprintf(argb,"%d",FM.nv);
    sprintf(edgecount,"%d:%d", L.mine, L.maxe);

    geng_argv[0] = "geng_surge";
    geng_argv[1] = arga;
    geng_argc = 2;

    if (F.tswitch && R.max3rings == 0)
    {
        geng_argv[geng_argc++] = "-t";
        F.tswitch = FALSE;
    }

    if (F.fswitch && R.max4rings == 0)
    {
        geng_argv[geng_argc++] = "-f";
        F.fswitch = FALSE;
    }

    if (F.bipartite) geng_argv[geng_argc++] = "-b";

    if (IN.extra1)
    {
        snprintf(gengargs,78,"-%s",IN.extra1);
        geng_argv[geng_argc++] = gengargs;
    }
    geng_argv[geng_argc++] = argb;
    geng_argv[geng_argc++] = edgecount;
    if (SP.mod > 1)
    {
        sprintf(resmod,"%ld/%ld",SP.res,SP.mod);
        geng_argv[geng_argc++] = resmod;
    }
    geng_argv[geng_argc] = NULL;

    if (F.verbose)
    {
        fprintf(stderr,">geng");
        for (i = 1; geng_argv[i] != NULL; ++i)
            fprintf(stderr," %s",geng_argv[i]);
        fprintf(stderr,"\n");
    }

    geng_main(geng_argc,geng_argv);
}
