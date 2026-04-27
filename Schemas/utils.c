#include "utils.h"
#include <stdio.h>

//size_t global_line_number = 0;

bool global_print_debugging = false;

unsigned global_create_memory_block_calls = 0;

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

void println_array_ints(int *array, size_t len){
    print_array_ints(array, len);
    printf("\n");    
}

void print_array_uints_sep(unsigned *array, size_t len, char *separator){
    if(len == 0){
        return;
    }

    printf("%u", array[0]);
    for(size_t i = 1; i < len; ++i){
        printf("%s%u", separator, array[i]);
    }
}

void print_array_uints(unsigned *array, size_t len){
    print_array_uints_sep(array, len, ", ");    
}

void println_array_uints(unsigned *array, size_t len){
    print_array_uints(array, len);
    printf("\n");    
}
