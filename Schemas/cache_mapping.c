#include "cache_mapping.h"

// NOTE: for the optimization of the calculation of mapping sides
// TODO(YA): 
// - preprocess so all function symbols (int > 0) are considered the same value (1, f.ex.)
// - Hash Map where nodes are two arrays of ints (row and unsigned mapping side)
// - equivalent function for rows (all function symbols equal)
#define FNV_OFFSET_BASIS 2166136261U
#define FNV_PRIME 16777619U

uint32_t fnv1a_hash(uint32_t *arr, size_t len) {
    uint32_t hash = FNV_OFFSET_BASIS;
    for (size_t i = 0; i < len; i++) {
        hash ^= arr[i];
        hash *= FNV_PRIME;
    }
    return hash;
}