//
// Created by fnellen on 05.09.2025.
//

#include "output.h"
#include "gtools.h"
#include "element.h"
#include "utils.h"
#include <string.h>

static void write_output(SurgeCtx *ctx, const char *text)
{
#ifdef ZLIB
    if (F.gzip) {
        if (gzputs(O.gzoutfile, text) < 0)
            surge_fatal(">E surge : zlib output error\n");
        return;
    }
#endif
    if (fputs(text, O.outfile) == EOF) surge_fatal(">E surge : output error\n");
}

void SMILESoutput(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne)
/* Write molecules in SMILES format */
{
    char *p,line[20*MAXNE];
    int i,x,y,r,m;
    Element *thiselement;

    p = line;

    for (i = 0; i < SM.smileslen; ++i)
    {
        x = SM.smilesskeleton[i].x;
        y = SM.smilesskeleton[i].y;
        switch(SM.smilesskeleton[i].item)
        {
         case SM_ATOM :
            thiselement = &element[vcol[x]];
            const int q = thiselement->charge;
            const int need_brackets = (!thiselement->organic) ||
                (thiselement->lowervalence > 0 && FM.hyd[x] > 0) ||
                (q != 0) || (strcmp(thiselement->inputname, "C(2.0.0)") == 0);
            if (need_brackets) {
                *(p++) = '[';
                put_str(&p, thiselement->name);
                if (FM.hyd[x] > 0) *(p++) = 'H';
                if (FM.hyd[x] > 1) put_int(&p, FM.hyd[x]);
                if (q != 0) {
                    *(p++) = (q > 0 ? '+' : '-');
                    int mag = q > 0 ? q : -q;
                    if (mag > 1) put_int(&p, (unsigned long)mag);
                }
                *(p++) = ']';
            } else {
                put_str(&p, thiselement->name);
            }
            break;
         case SM_BOND :
            m = mult[G.edgenumber[x][y]];
            if      (m == 1) *(p++) = '=';
            else if (m == 2) *(p++) = '#';
            break;
         case SM_OPEN :
            *(p++) = '(';
            break;
         case SM_CLOSE :
            *(p++) = ')';
            break;
         case SM_RING0 :
            m = mult[G.edgenumber[x][y]];
            if      (m == 1) *(p++) = '=';
            else if (m == 2) *(p++) = '#';
            r = SM.smilesskeleton[i].r;
            if (r < 10)
                *(p++) = '0' + r;
            else
            {
                *(p++) = '%';
                *(p++) = '0' + r/10;
                *(p++) = '0' + r%10;
            }
            break;
         case SM_RING1 :
            r = SM.smilesskeleton[i].r;
            if (r < 10)
                *(p++) = '0' + r;
            else
            {
                *(p++) = '%';
                *(p++) = '0' + r/10;
                *(p++) = '0' + r%10;
            }
            break;
        default:
                surge_fatal(">E surge : internal error in SMILESoutput\n");
        }
    }

    *(p++) = '\n';
    *p = '\0';

    write_output(ctx, line);
}


void SDFformat(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne)
/* Write molecules in SDF format */
{
    char line[128];
    write_output(ctx, "\nSurge 1.0\n\n");
    snprintf(line, sizeof line, "%3d%3d  0  0  0  0            999 V2000\n", n, ne);
    write_output(ctx, line);

    for (int i = 0; i < n; ++i) {
        snprintf(line, sizeof line, "    0.0000    0.0000    0.0000 %-2s"
                "  0  0  0  0  0%3d  0  0  0  0  0  0\n",
                element[vcol[i]].name,element[vcol[i]].valence);
        write_output(ctx, line);
    }

    for (int i = 0; i < ne; ++i) {
        snprintf(line, sizeof line, "%3d%3d%3d  0  0  0  0\n",
            G.edge[i].x+1,G.edge[i].y+1,mult[i]+1);
        write_output(ctx, line);
    }

    /* V2000 permits at most eight atom/charge pairs per M  CHG record. */
    int atoms[8], charges[8], count = 0;
    for (int i = 0; i < n; ++i) {
        int charge = element[vcol[i]].charge;
        if (charge != 0) {
            atoms[count] = i + 1;
            charges[count++] = charge;
        }
        if (count == 8 || (i == n - 1 && count > 0)) {
            char *p = line;
            p += snprintf(p, sizeof line, "M  CHG%3d", count);
            for (int j = 0; j < count; ++j)
                p += snprintf(p, sizeof line - (size_t)(p - line), "%4d%4d",
                              atoms[j], charges[j]);
            *p++ = '\n';
            *p = '\0';
            write_output(ctx, line);
            count = 0;
        }
    }
    write_output(ctx, "M  END\n$$$$\n");
}

void multigoutput(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne)
/* Write output equal to the multig -T format. */
{
    char line[10+60*MAXN],*p;
    int i;

    p = line;
    put_int(&p, n); space(&p); put_int(&p, ne);
    for (i = 0; i < n; ++i)
    {
        space(&p);
        put_int(&p,element[vcol[i]].index);
    }
    space(&p);
    for (i = 0; i < ne; ++i)
    {
        space(&p); put_int(&p, G.edge[i].x);
        space(&p); put_int(&p, G.edge[i].y);
        space(&p); put_int(&p, mult[i]);
    }
    *(p++) = '\n';
    *p = '\0';

    write_output(ctx, line);
}

/****************************************************************/

void alphabeticoutput(SurgeCtx *ctx, int *vcol, int n, int *mult, int ne)
/* Write alphabetic output */
{
    char line[10+60*MAXN],*p;
    int i,xx,yy;

    p = line;
    put_int(&p,n);
    space(&p);
    put_int(&p,ne);
    space(&p);
    put_str(&p, FM.canonform);

    for (i = 0; i < ne; ++i)
    {
        space(&p);
        xx = FM.newlabel[G.edge[i].x];
        yy = FM.newlabel[G.edge[i].y];
        if (xx < yy)
        {
            put_int(&p, xx); put_bnd(&p, mult[i]); put_int(&p, yy);
        }
        else
        {
            put_int(&p, yy); put_bnd(&p, mult[i]); put_int(&p, xx);
        }
    }
    *(p++) = '\n';
    *p = '\0';

    write_output(ctx, line);
}
