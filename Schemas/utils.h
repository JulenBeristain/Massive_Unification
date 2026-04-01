#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include<stdlib.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#define GET_BIT(Num, Bit) (((Num) >> (Bit)) & 1)
#define MOST_SIGNIFICANT_BIT(Type) (sizeof(Type) * 8 - 1)

#define SET_TO_ZERO(AllocatedPtr, NumBytes) memset((AllocatedPtr), 0, (NumBytes))

#define ERROR_MESSAGE(function, message) fprintf(stderr, "%s(Line %u): %s\n", function, __LINE__, message)
#define MALLOC_ERROR_MESSAGE(function) ERROR_MESSAGE(function, "malloc failed to allocate memory")
#define CHECK_MALLOC(pointer, function) \
    if(pointer == NULL) {               \
        MALLOC_ERROR_MESSAGE(function); \
        exit(EXIT_FAILURE);             \
    }
#define CALLOC_ERROR_MESSAGE(function) ERROR_MESSAGE(function, "calloc failed to allocate memory")
#define CHECK_CALLOC(pointer, function) \
    if(pointer == NULL) {               \
        CALLOC_ERROR_MESSAGE(function); \
        exit(EXIT_FAILURE);             \
    }

//extern size_t global_line_number;
extern bool global_print_debugging;
extern unsigned global_create_memory_block_calls;
extern unsigned global_starting_t1;
extern unsigned global_starting_t2;

static inline unsigned num_digits(unsigned n){
    unsigned res = 1;
    while((n /= 10) != 0){ ++res; }
    return res;
}

void print_array_ints(int *array, size_t len);
void print_array_ints_sep(int *array, size_t len, char *separator);

#endif