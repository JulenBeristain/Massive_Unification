/*
To compile:
gcc -std=c11 -Wall -Wextra -g Schemas/tests.c Schemas/set_variables.c -o build/tests
*/

#include "set_variables.h"
#include <stdio.h>

int main(/*int argc, char const *argv[]*/)
{
    SetVariables set = create_set_defsize();

    for(Variable v = 1; v <= 10; ++v){
        insert_to_set(&set, v);
    }
    print_set(set);

    for(Variable v = 1; v <= 20; ++v){
        char *inSet = lookup_set(set, v) ? "True" : "False";
        printf("%d in set = %s\n", v, inSet);
    }

    clear_set(set);
    print_set(set);

    free_set(set);

    return 0;
}
