//
// Created by fnellen on 05.09.2025.
//

#include "formula.h"
#include "element.h"
#include <ctype.h>
#include <string.h>

#include "gtools.h"
#include "utils.h"


void decode_formula(SurgeCtx *ctx)
/* Parse the input formula. The distinct elements go to
   elementlist[0..numtypes-1] and elementcount[0..numtypes-1].
   *mine and *maxe have an edge range from -e and are updated.
   *mind and *maxc come from -d and -c and are updated.
   *nv gets the number of atoms.
*/
{
    int i,j,d,mult,val,cnt,totval,dbe,forced;
    int maxvcoord,localmine,localmaxe,localmaxd,xi,yi;
    char *s1,*s2,*p;
    int count[FORMULALEN];

    if (FM.numelements > FORMULALEN)
        surge_fatal(">E surge : increase FORMULALEN\n");

    /* First we fill in count[*], which is parallel to element[*] */

    for (i = 0; i < FM.numelements; ++i) count[i] = 0;

    for (s1 = IN.formula; *s1 != '\0'; )
    {
        if (!isupper((unsigned char)*s1)) surge_fatal(">E surge : unknown element name\n");

        s2 = s1 + 1;
        while (islower((unsigned char)*s2)) ++s2;
        if (*s2 == '(')
        {
            char *close = s2 + 1;
            while (*close != '\0' && *close != ')') ++close;
            if (*close != ')') surge_fatal(">E surge : unterminated element variant spec\n");
            s2 = close + 1;
        }

        for (i = 0; i < FM.numelements; ++i)
        {
            size_t token_len = (size_t)(s2 - s1);
            if (strncmp(element[i].inputname, s1, token_len) == 0
                && element[i].inputname[token_len] == '\0')
                break;
        }
        if (i == FM.numelements) surge_fatal(">E surge : unknown element name\n");

        char *digits = s2;
        if (isdigit((unsigned char)*digits))
        {
            mult = 0;
            do
            {
                mult = 10 * mult + (*digits - '0');
                ++digits;
            } while (isdigit((unsigned char)*digits));
            count[i] += mult;
            s1 = digits;
        }
        else
        {
            ++count[i];
            s1 = digits;
        }
    }

    /* Next we collect elements actually used into elementtype[0..numtypes-1]
       and elementcount[0..numtypes-1]. */

    FM.numtypes = FM.hydrogens = 0;
    for (i = 0; i < FM.numelements; ++i)
    {
        cnt = count[i];
        if (cnt > 0)
        {
            if (is_hydrogen(i)) {
                FM.hydrogens = cnt;
            }
            else {
                FM.elementtype[FM.numtypes] = i;
                FM.elementcount[FM.numtypes] = cnt;
                ++FM.numtypes;
            }
        }
    }

    /* Next we adjust *maxd and *maxc, as well as the maxcoord
       fields of elements */

    maxvcoord = 0;
    for (i = 0; i < FM.numtypes; ++i)
        if (element[FM.elementtype[i]].maxcoord > maxvcoord)
            maxvcoord = element[FM.elementtype[i]].maxcoord;
    if (maxvcoord < L.maxc)
        L.maxc = maxvcoord;
    else if (maxvcoord > L.maxc)
        for (i = 0; i < FM.numtypes; ++i)
            if (element[FM.elementtype[i]].maxcoord > L.maxc)
                element[FM.elementtype[i]].maxcoord = L.maxc;
    if (L.maxd > L.maxc) L.maxd = L.maxc;

    /* Next we find some bounds on the number of vertices
       with various simple degrees. */

    FM.min1 = FM.min12 = FM.max34 = FM.max4 = 0;
    for (i = 0; i < FM.numelements; ++i)
    {
        if (!is_hydrogen(i)) {
            cnt = count[i];
            val = element[i].maxcoord;
            if (val <= 1) FM.min1 += cnt;   // Check logic
            if (val <= 2) FM.min12 += cnt;
            if (val >= 3) FM.max34 += cnt;
            if (val >= 4) FM.max4 += cnt;
            // Could add max5, could use both bounds everywhere
        }
    }

    /* Now sort by decreasing maximum coordination number */

    for (i = 1; i < FM.numtypes; ++i)  /* really 1 */
    {
        xi = FM.elementtype[i];
        yi = FM.elementcount[i];
        for (j = i; element[FM.elementtype[j-1]].maxcoord < element[xi].maxcoord; )
        {
            FM.elementtype[j] = FM.elementtype[j-1];
            FM.elementcount[j] = FM.elementcount[j-1];
            if (--j < 1) break;
        }
        FM.elementtype[j] = xi;
        FM.elementcount[j] = yi;
    }

    /* Make "canonical" molecule name (used for -A output) */

    p = FM.canonform;
    for (i = 0; i < FM.numtypes; ++i)
    {
        put_str(&p, element[FM.elementtype[i]].inputname);
        if (FM.elementcount[i] > 1) put_int(&p, FM.elementcount[i]);
    }
    if (FM.hydrogens > 0) put_str(&p, "H");
    if (FM.hydrogens > 1) put_int(&p, FM.hydrogens);
    *p = '\0';

    /* Calculate *nv, totalval, forced */

    FM.nv = forced = 0;
    totval = FM.hydrogens;
    for (i = 0; i < FM.numtypes; ++i)
    {
        j = FM.elementtype[i];
        cnt = FM.elementcount[i];
        FM.nv += cnt;
        totval += cnt * element[j].valence;
        if (element[j].valence > element[j].maxcoord)
            forced += element[j].valence - element[j].maxcoord;
    }
    forced = (forced+1) / 2;

    if ((totval & 1)) surge_fatal(">E surge : impossible parity\n");
    dbe = totval / 2 - (FM.nv + FM.hydrogens - 1) - forced;
    if (dbe < 0) surge_fatal(">E surge : negative DBE\n");
    if (FM.nv > MAXN) surge_fatal(">E surge : too many atoms\n");
    FM.valencesum = totval - FM.hydrogens;

    localmine = FM.nv - 1;
    localmaxe = localmine + dbe;
    if (localmaxe > FM.nv * (FM.nv-1) / 2) localmaxe = FM.nv * (FM.nv-1) / 2;

    if (localmine > L.mine) L.mine = localmine;
    if (localmaxe < L.maxe) L.maxe = localmaxe;
    if (L.mine > L.maxe) surge_fatal(">E surge : edge range is empty\n");

    vv_logf(ctx, "%s  ",FM.canonform);
    vv_logf(ctx, "H=%d",FM.hydrogens);

    for (i = 0; i < FM.numtypes; ++i)
        vv_logf(ctx," %s=%d",element[FM.elementtype[i]].inputname,FM.elementcount[i]);

    vv_logf(ctx,"  nv=%d edges=%d-%d DBE=%d maxd=%d maxc=%d\n",
            FM.nv,L.mine,L.maxe,dbe,L.maxd,L.maxc);
    if (L.maxe > MAXNE) surge_fatal(">E surge : too many edges\n");

    for (d = 1; d <= L.maxd; ++d)
    {
        for (i = 0; i < FM.numtypes; ++i)
        {
            val = element[FM.elementtype[i]].maxcoord;
            if (d <= val) FM.maxtype[d] = i;
        }
    }
}

static void processEswitch(SurgeCtx *ctx, char **ps, char *id)
/* Process -E starting at *ps = the character after E, and update *ps.
   The value has the form <inputname>[,<name>]<valence>[<maxcoord>],
   where <inputname> and <name> are either <upper> or <upper><lower> and
   <valence> and <maxcoord> are single digits. Inputname comes first. */
{
    char inputname[3],name[3];
    int valence,maxcoord;
    char *s;
    int state,err;

    s = *ps;
    state = 0;

    while (state < 5)
    {
        switch (state)
        {
           case 0:
                if (!isupper(*s))
                {
                    state = 6;
                    break;
                }
                inputname[0] = *s++;
                if (islower(*s))
                {
                    inputname[1] = *s++;
                    inputname[2] = '\0';
                }
                else
                    inputname[1] = '\0';
                state = 1;
                break;
           case 1:
                if (isupper(*s))
                    state = 2;
                else
                {
                    name[0] = inputname[0];
                    name[1] = inputname[1];
                    name[2] = inputname[2];
                    state = 3;
                }
                break;
            case 2:
                name[0] = *s++;
                if (islower(*s))
                {
                    name[1] = *s++;
                    name[2] = '\0';
                }
                else
                    name[1] = '\0';
                state = 3;
                break;
            case 3:
                if (!isdigit(*s))
                {
                    state = 7;
                    break;
                }
                valence = *s++ - '0';
                if (!isdigit(*s))
                    maxcoord = valence;
                else
                    maxcoord = *s++ - '0';
                state = 5;
                break;
        }
    }

    if (state == 6)
    {
        fprintf(stderr,">E %s : bad element name\n",id);
        exit(1);
    }
    if (state == 7)
    {
        fprintf(stderr,">E %s : bad valence or maxcoord\n",id);
        exit(1);
    }

    *ps = s;

    addelement(ctx,inputname,name,valence,maxcoord);
}

#define SWELEMENT(c,id) if (sw==c) {processEswitch(&arg,id);}
