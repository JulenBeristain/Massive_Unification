#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(a, b) ((a) < (b) ? (b) : (a))
typedef struct { unsigned min, max; } MinMax;
static inline MinMax min_max(unsigned a, unsigned b){
    if(a < b){
        return (MinMax){ .min = a, .max = b };
    } else {
        return (MinMax){ .min = b, .max = a };
    }
}

#define GET_BIT(Num, Bit) (((Num) >> (Bit)) & 1)
#define MOST_SIGNIFICANT_BIT(Type) (sizeof(Type) * 8 - 1)

#define SET_TO_ZERO(AllocatedPtr, NumBytes) memset((AllocatedPtr), 0, (NumBytes))
#define SET_TO_ONE(AllocatedPtr, NumBytes) memset((AllocatedPtr), 1, (NumBytes))

#define ERROR_MESSAGE(message) fprintf(stderr, "%s(Line %u): %s\n", __func__, __LINE__, message)
#define MALLOC_ERROR_MESSAGE ERROR_MESSAGE("malloc failed to allocate memory")
#define CHECK_MALLOC(pointer) \
    if(pointer == NULL) {               \
        MALLOC_ERROR_MESSAGE;           \
        exit(EXIT_FAILURE);             \
    }
#define CALLOC_ERROR_MESSAGE ERROR_MESSAGE("calloc failed to allocate memory")
#define CHECK_CALLOC(pointer) \
    if(pointer == NULL) {               \
        CALLOC_ERROR_MESSAGE;           \
        exit(EXIT_FAILURE);             \
    }

//extern size_t global_line_number;
extern bool global_print_debugging;
extern unsigned global_create_memory_block_calls;

static inline unsigned num_digits(unsigned n){
    unsigned res = 1;
    while((n /= 10) != 0){ ++res; }
    return res;
}

void println_array_ints(int *array, size_t len);
void print_array_ints(int *array, size_t len);
void print_array_ints_sep(int *array, size_t len, char *separator);
void println_array_uints(unsigned *array, size_t len);
void print_array_uints_sep(unsigned *array, size_t len, char *separator);
void print_array_uints(unsigned *array, size_t len);


static inline double incremental_mean(double mean, double new, unsigned n){
    return mean + ((new - mean) / n);
}

#endif