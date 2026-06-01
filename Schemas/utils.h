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

#define KILOBYTES(N) ((N) << 10)
#define MEGABYTES(N) ((N) << 20)
#define GIGABYTES(N) ((N) << 30)

// NOTE: returns 0 if n has its most significant bit (63) set.
static inline size_t smallest_greater_power_of_2(size_t n) {
    if (n == 0) {
        return 1;
    }
    unsigned mask = 1 << MOST_SIGNIFICANT_BIT(size_t);
    while(mask){
        if(n & mask){
            return mask << 1;;
        }
        mask >>= 1;
    }
    // NOTE: we know we won't arrive here thanks to the initial check!
}

#define CLAMP(n, min, max)      \
    (((n) < (min)) ? (min) :    \
        ((n) > (max)) ? (max) : \
            (n))


// Operations return code types //////////////////////////////////////
// TODO(FUT): define macros to create HashMaps of any types and move this definitions to that file...
typedef enum MapInsertReturnCode MapInsertReturnCode;
enum MapInsertReturnCode { 
    MAP_INSERT_ADDED,
    MAP_INSERT_ADDED_RESIZING,
    MAP_INSERT_ALREADY_CONTAINED
};

// TODO(FUT): define macros to create HashSets of any types and move this definitions to that file...
typedef enum SetInsertReturnCode SetInsertReturnCode;
enum SetInsertReturnCode { 
    SET_INSERT_ADDED,
    SET_INSERT_ADDED_RESIZING,
    SET_INSERT_ALREADY_CONTAINED
};

#endif