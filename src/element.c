//
// Created by fnellen on 31.08.2025.
//

#include "element.h"

#include       <ctype.h>      

#include "gtools.h"
#include "utils.h"

/*
    The order of listing does not matter.
    Other elements can be added to the table by following the same
    pattern, up to any limit. All extra elements must be marked
    as non-organic.
    The inputname field must have the form: Aa or Aa(n.n.n)
    where Aa is the element symbol (one or two letters) and
    (x.y.z) is the valence state in the form:
    - x = number of valences
    - y = sign of charge (0=neutral, 1=negative, 2=positiv)
    - z = formal charge of the element in this state
    One example is C(2.1.2) for carbon with 2 valences, negative sign and -2 formal charge.
    When inserting elements with variances, at least one entry without variance
    must be present for each element symbol.
    The inputname field is used to identify the element in the input formula and must be unique.
    At the end of the table, a NULL entry must be added to mark the end.
*/
Element element[MAXELEMENTS] =
{
    {"H", "H", true, 1, 0, 1, 1, 0},
    {"B", "B", false, 3, 0, 3, 4, 0},
    {"B(1.1.2)", "B", false, 1, 0, 1, 2, -2},
    {"B(2.1.1)", "B", false, 2, 0, 2, 3, -1},
    {"C", "C", true, 4, 0, 4, 11, 0},
    //{"C(1.1.1)", "C", true, 1, 0, 1, 5, -1},
    {"C(1.1.3)", "C", true, 1, 0, 1, 6, -3},
    //{"C(2.0.0)", "C", true, 2, 0, 2, 7, 0},
    {"C(2.1.2)", "C", true, 2, 0, 2, 8, -2},
    {"C(3.2.1)", "C", true, 3, 0, 3, 9, 1},
    {"C(3.1.1)", "C", true, 3, 0, 3, 10, -1},
    {"N", "N", true, 3, 0, 3, 14, 0},
    {"N(1.1.2)", "N", true, 1, 0, 1, 12, -2},
    {"N(2.1.1)", "N", true, 2, 0, 2, 13, -1},
    {"N(4.2.1)", "N", true, 4, 0, 4, 15, 1},
    {"O", "O", true, 2, 0, 2, 17, 0},
    {"O(1.1.1)", "O", true, 1, 0, 1, 16, -1},
    {"O(3.2.1)", "O", true, 3, 0, 3, 18, 1},
    {"F", "F", false, 1, 0, 1, 19, 0},
    {"F(2.2.1)", "F", false, 2, 0, 2, 20, 1},
    {"F(3.2.2)", "F", false, 3, 0, 3, 21, 2},
    {"F(4.2.3)", "F", false, 4, 0, 4, 22, 3},
    {"P", "P", true, 3, 0, 3, 26, 0},
    {"P(1.1.2)", "P", true, 1, 0, 1, 23, -2},
    {"P(2.1.1)", "P", true, 2, 0, 2, 24, -1},
    {"P(2.1.3)", "P", true, 2, 0, 2, 25, -3},
    {"P(3.1.2)", "P", true, 3, 0, 3, 27, -2},
    {"P(4.2.1)", "P", true, 4, 0, 4, 28, 1},
    {"P(4.1.1)", "P", true, 4, 0, 4, 29, -1},
    {"P(5.0.0)", "P", true, 5, 0, 5, 30, 0},
    {"S", "S", true, 2, 0, 2, 32, 0},
    {"S(1.1.1)", "S", true, 1, 0, 1, 31, -1},
    {"S(2.1.2)", "S", true, 2, 0, 2, 33, -2},
    {"S(3.2.1)", "S", true, 3, 0, 3, 34, 1},
    {"S(3.1.1)", "S", true, 3, 0, 3, 35, -1},
    {"S(3.1.3)", "S", true, 3, 0, 3, 36, -3},
    {"S(4.2.2)", "S", true, 4, 0, 4, 37, 2},
    {"S(4.0.0)", "S", true, 4, 0, 4, 38, 0},
    {"S(4.1.2)", "S", true, 4, 0, 4, 39, -2},
    {"S(5.2.1)", "S", true, 5, 0, 5, 40, 1},
    {"S(5.1.1)", "S", true, 5, 0, 5, 41, -1},
    {"S(6.0.0)", "S", true, 6, 0, 6, 42, 0},
    {"Cl", "Cl", false, 1, 0, 1, 43, 0},
    {"Cl(2.2.1)", "Cl", false, 2, 0, 2, 44, 1},
    {"Cl(2.1.1)", "Cl", false, 2, 0, 2, 45, -1},
    {"Cl(3.2.2)", "Cl", false, 3, 0, 3, 46, 2},
    {"Cl(3.0.0)", "Cl", false, 3, 0, 3, 47, 0},
    {"Cl(3.1.2)", "Cl", false, 3, 0, 3, 48, -2},
    {"Cl(4.2.3)", "Cl", false, 4, 0, 4, 49, 3},
    {"Cl(4.2.1)", "Cl", false, 4, 0, 4, 50, 1},
    {"Cl(4.1.1)", "Cl", false, 4, 0, 4, 51, -1},
    {"Cl(4.1.3)", "Cl", false, 4, 0, 4, 52, -3},
    {"Cl(5.2.2)", "Cl", false, 5, 0, 5, 53, 2},
    {"Cl(5.0.0)", "Cl", false, 5, 0, 5, 54, 0},
    {"Cl(5.1.2)", "Cl", false, 5, 0, 5, 55, -2},
    {"Cl(6.2.1)", "Cl", false, 6, 0, 6, 56, 1},
    {"Cl(6.1.1)", "Cl", false, 6, 0, 6, 57, -1},
    {"Cl(7.0.0)", "Cl", false, 7, 0, 7, 58, 0},
    {"Si", "Si", false, 4, 0, 4, 14 , 0},
    {"Si", "Si", false, 4, 0, 4, 59, 0},
    {"Si(1.1.3)", "Si", false, 1, 0, 1, 60, -3},
    {"Si(2.1.2)", "Si", false, 2, 0, 2, 61, -2},
    {"Si(3.2.1)", "Si", false, 3, 0, 3, 62, 1},
    {"Si(3.1.1)", "Si", false, 3, 0, 3, 63, -1},
    {"Al", "Al",false, 3,0,3, 15 , 0},
    {"Al(1.1.2)", "Al", false, 1, 0, 1, 64, -2},
    {"Al(2.1.1)", "Al", false, 2, 0, 2, 65, -1},
    { NULL, NULL, false, 0, 0, 0, 0, 0}
};

int elementindex(SurgeCtx *ctx, char *inputname)
{
    int i;

    for (i = 0 ; i < FM. numelements; ++i)
        if (strcmp(element[i].inputname,inputname) == 0)
            break;

    if (  i < FM. numelements) return i;
    else
    {
        fprintf(stderr,">E Unknown element %s\n",inputname);
        exit(1);
    }
}

void addelement(SurgeCtx *ctx, char *inputname, char *name, int valence, int maxcoord)
{
    int i;

    if (FM.numelements == MAXELEMENTS)
        surge_fatal(">E increase MAXELEMENTS\n");

    if (name == NULL) name = inputname;
    if (maxcoord == 0 || maxcoord > valence) maxcoord = valence;

    if (inputname == NULL || valence <= 0)
        surge_fatal(">E invalid parameters in addelement()\n");

    for (i = 0 ; i < FM. numelements; ++i)
    {
        if (strcmp(element[i].inputname,inputname) == 0)
        {
            fprintf(stderr,">E element %s is already present\n",name);
            exit(1);
        }
    }

    if (!isupper(inputname[0]) || (inputname[1] != '\0'
             && (!islower(inputname[1]) || inputname[2] != '\0')))
        surge_fatal(">E element names must have form A or Aa\n");
    if (!isupper(name[0]) || (name[1] != '\0'
             && (!islower(name[1]) || name[2] != '\0')))
        surge_fatal(">E element names must have form A or Aa\n");

    if (3*maxcoord < valence || valence < 0)
        surge_fatal(">E impossible maxcoord/valence\n");

    element[FM.numelements].inputname = strdup(inputname);
    element[FM.numelements].name = strdup(name);
    element[FM.numelements].valence = valence;
    element[FM.numelements].lowervalence = 0;
    element[FM.numelements].maxcoord = maxcoord;
    element[FM.numelements].organic = false;
    element[FM.numelements].index = ++FM.maxindex;

    if (F.verbose)
	fprintf(stderr,"Added element %s input %s, valence=%d maxcoord=%d\n",
		element[FM.numelements].name,element[FM.numelements].inputname,
                element[FM.numelements].valence,element[FM.numelements].maxcoord);
    ++FM.numelements;
}

void calculate_number_of_elements(SurgeCtx *ctx) {
    FM.maxindex = 0;
    int i;
    for (i = 0; i < MAXELEMENTS; ++i)
        if (element[i].inputname == NULL) break;
        else if (element[i].index < FM.maxindex && !is_hydrogen(i))
            FM.maxindex = element[i].index;
    FM.numelements = i;
}
