#include "utils.h"
#include <stdio.h>

//size_t global_line_number = 0;

bool global_print_debugging = false;

unsigned global_create_memory_block_calls = 0;

unsigned global_starting_t1 = 1;
unsigned global_starting_t2 = 1;

void print_array_ints_sep(int *array, size_t len, char *separator){
    if(len == 0){
        return;
    }

    printf("%d", array[0]);
    for(size_t i = 1; i < len; ++i){
        printf("%s%d", separator, array[i]);
    }
}

void print_array_ints(int *array, size_t len){
    print_array_ints_sep(array, len, ", ");    
}
