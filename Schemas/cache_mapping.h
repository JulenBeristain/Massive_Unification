#ifndef CACHE_MAPPING_H
#define CACHE_MAPPING_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "arena.h"
#include "utils.h"

// TODO: we have tested that this cache mapping is not more efficient than simply calculating each mapping side, even if
//  there might be some repeated. Until the general macros for defining HashMaps are defined, we will keep saving this file.
//  But then, it has to be removed.

// HASH MAP TYPES ////////////////////////////////////////////////////
typedef struct RowToMappingSide RowToMappingSide;
struct RowToMappingSide {
    int *row;
    unsigned *mapping_side;
};

typedef struct HashMapRowToMappingSide HashMapRowToMappingSide;
struct HashMapRowToMappingSide {
    uint32_t num_rows;
    uint32_t num_buckets;
    uint32_t len_rows;
    uint32_t len_mapping_sides;
    RowToMappingSide *buckets;
};
//////////////////////////////////////////////////////////////////////

// Function declarations /////////////////////////////////////////////
HashMapRowToMappingSide create_hash_map_row_to_mapping_side(uint32_t num_buckets, uint32_t len_rows, uint32_t len_mapping_side);
HashMapRowToMappingSide create_hash_map_row_to_mapping_side_arena(uint32_t num_buckets, uint32_t len_rows, uint32_t len_mapping_side, Arena *arena);
static inline HashMapRowToMappingSide create_hash_map_row_to_mapping_side_defnumbuckets(uint32_t len_rows, uint32_t len_mapping_side){
    enum { INITIAL_HASH_MAP_SIZE = 1 << 7 };
    return create_hash_map_row_to_mapping_side(INITIAL_HASH_MAP_SIZE, len_rows, len_mapping_side);
}
static inline void free_hash_map_row_to_mapping_side(HashMapRowToMappingSide hm){ free(hm.buckets); }
void clear_hash_map_row_to_mapping_side(HashMapRowToMappingSide *hm);
MapInsertReturnCode insert_to_hash_map_row_to_mapping_side(HashMapRowToMappingSide *hm, int *row, unsigned *mapping_side);
MapInsertReturnCode insert_to_hash_map_row_to_mapping_side_arena(HashMapRowToMappingSide *hm, int *row, unsigned *mapping_side, Arena *arena);
bool is_in_hash_map_row_to_mapping_side(HashMapRowToMappingSide hm, int *row);
RowToMappingSide *get_pair_in_hash_map_row_to_mapping_side(HashMapRowToMappingSide hm, int *row);
void print_hash_map_row_to_mapping_side(HashMapRowToMappingSide hm);
//////////////////////////////////////////////////////////////////////

// Foreach macros ////////////////////////////////////////////////////
// NOTE: not so appropriate for this kind of hashmap, since we need an if conditional checking if the row pointer of each
//  pair is not NULL, which can not be put in the macro. An iterator would be more handy.
#define foreach_in_hashmap_row_to_mapping_side(hm, pairptr)         \
    for(RowToMappingSide *pairptr = (hm).buckets,                   \
                         *_end = (hm).buckets + (hm).num_buckets;   \
        pairptr < _end;                                             \
        ++pairptr                                                   \
    )

//////////////////////////////////////////////////////////////////////

// Default kind of Map from Rows to Mapping Sides ////////////////////
typedef HashMapRowToMappingSide MapRowToMappingSide;
#define create_map_row_to_mapping_side create_hash_map_row_to_mapping_side
#define create_map_row_to_mapping_side_arena create_hash_map_row_to_mapping_side_arena
#define create_map_row_to_mapping_side_defnumbuckets create_hash_map_row_to_mapping_side_defnumbuckets
#define free_map_row_to_mapping_side free_hash_map_row_to_mapping_side
#define clear_map_row_to_mapping_side clear_hash_map_row_to_mapping_side
#define insert_to_map_row_to_mapping_side insert_to_hash_map_row_to_mapping_side
#define insert_to_map_row_to_mapping_side_arena insert_to_hash_map_row_to_mapping_side_arena
#define is_in_map_row_to_mapping_side is_in_hash_map_row_to_mapping_side
#define get_pair_in_map_row_to_mapping_side get_pair_in_hash_map_row_to_mapping_side
#define print_map_row_to_mapping_side print_hash_map_row_to_mapping_side
//////////////////////////////////////////////////////////////////////

#endif