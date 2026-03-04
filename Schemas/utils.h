#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

#define GET_BIT(Num, Bit) (((Num) >> (Bit)) & 1)
#define MOST_SIGNIFICANT_BIT(Type) (sizeof(Type) * 8 - 1)
#define SET_TO_ZERO(AllocatedPtr, NumBytes) memset((AllocatedPtr), 0, (NumBytes))

//extern size_t global_line_number;
extern bool global_print_debugging;
extern unsigned global_create_memory_block_calls;

static inline unsigned num_digits(unsigned n){
    unsigned res = 1;
    while((n /= 10) != 0){ ++res; }
    return res;
}

#endif