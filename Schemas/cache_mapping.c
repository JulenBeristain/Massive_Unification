#include "cache_mapping.h"
#include "utils.h"
#include <stdio.h>
#include <assert.h>

// NOTE: for the optimization of the calculation of mapping sides

#define FNV_OFFSET_BASIS 2166136261U
#define FNV_PRIME 16777619U
#define POSITIVE_MARKER 0xABCDE123U // NOTE: an arbitrary big number with several active bits (bigger difference w.r.t. 0 and small negatives than 1)

uint32_t fnv1a_hash(int32_t *arr, size_t len) {
    uint32_t hash = FNV_OFFSET_BASIS;
    
    for (size_t i = 0; i < len; i++) {
        uint32_t value_to_hash;
        
        // NOTE: preprocess - collapse positives and keep 0 and negatives
        if (arr[i] > 0) {
            value_to_hash = POSITIVE_MARKER;
        } else {
            value_to_hash = (uint32_t)arr[i];
        }
        
        hash ^= value_to_hash;
        hash *= FNV_PRIME;
    }
    
    return hash;
}

#define hash_row(A, B) fnv1a_hash((A), (B))

bool equivalent_rows(int *row1, int *row2, size_t len){
    for(size_t i = 0; i < len; ++i){
        int a = row1[i];
        int b = row2[i];

        if((a > 0 && b <= 0) || (a <= 0 && b > 0) || (a <= 0 && a != b)) {
            return false;
        }
    }
    return true;
}

// TODO(YA): 
// - Hash Map where nodes are two arrays of ints (row and unsigned mapping side)

HashMapRowToMappingSide create_hash_map_row_to_mapping_side(uint32_t num_buckets, uint32_t len_rows, uint32_t len_mapping_side){
    HashMapRowToMappingSide hm;
    hm.num_rows = 0;
    hm.num_buckets = num_buckets;
    hm.len_rows = len_rows;
    hm.len_mapping_sides = len_mapping_side;
    hm.buckets = malloc(num_buckets * sizeof(*hm.buckets));
    CHECK_MALLOC(hm.buckets, "create_hash_map_row_to_mapping_side");
    return hm;
}

// NOTE: put this assert as the first statement in the blocks of for loops over hm.buckets!
#define assert_row_not_end(hmptr, row) assert((((hmptr)->buckets[0].row - (row)) / 2) < (hmptr)->num_buckets);

// NOTE: we don't shrink the buckets array in any case.
void clear_hash_map_row_to_mapping_side(HashMapRowToMappingSide *hm){
    for(int *row = hm->buckets[0].row; hm->num_rows; (RowToMappingSide*)row++){
        assert_row_not_end(hm, row);
        if(row){
            *row = NULL;
            hm->num_rows--;
        }
    }
}


static inline float load_factor(HashMapRowToMappingSide hm) { 
    return (float)hm.num_rows / (float)hm.num_buckets;
}
/**
 * Precondition: hs->num_buckets > 0 (if not, calloc undefined behavior and resizing logic wouldn't work)
 */
static inline void resize_hash_map_row_to_mapping_side(HashMapRowToMappingSide *hm){
    uint32_t new_num_buckets = hm->num_buckets * 2;
    RowToMappingSide *new_buckets = calloc(new_num_buckets, sizeof(*new_buckets));
    CHECK_CALLOC(new_buckets, "resize_hash_map_row_to_mapping_side");

    for(int *row = hm->buckets[0].row; hm->num_rows; (RowToMappingSide*)row++){
        assert_row_not_end(hm, row);
        if(row){
            uint32_t bucket_i = hash_row(row, hm->len_rows) % new_num_buckets;
#ifndef NDEBUG
            uint32_t initial_bucket_i = bucket_i;
#endif
            // NOTE: we know that we don't have two equivalents! So simpler while condition and no need to check the exit condition afterwards.
            while(new_buckets[bucket_i].row != NULL){
                bucket_i = (bucket_i + 1) % new_num_buckets;
                // NOTE: thanks to the load factor we should always have an empty spot
                assert(bucket_i != initial_bucket_i);
            }

            new_buckets[bucket_i].row = row;
            new_buckets[bucket_i].mapping_side = (int **)row + 1;
        }
    }

    free(hm->buckets);
    hm->buckets = new_buckets;
    hm->num_buckets = new_num_buckets;
    //hm->num_rows remains equal, as well as len_rows and len_mapping_sides (which are constant)
}
static inline uint32_t find_bucket(HashMapRowToMappingSide *hm, int *row){
    uint32_t bucket_i = hash_row(row, hm->len_rows) % hm->num_buckets;
#ifndef NDEBUG
    uint32_t initial_bucket_i = bucket_i;
#endif
    RowToMappingSide *buckets = hm->buckets;
    while(buckets[bucket_i].row != NULL && !equivalent_rows(row, buckets[bucket_i].row, hm->len_rows)){
        bucket_i = (bucket_i + 1) % hm->num_buckets;
        // NOTE: thanks to the load factor we should always have an empty spot
        assert(bucket_i != initial_bucket_i);
    }

    return bucket_i;
}
// NOTE: another possibility would be to pass the row size to this function
MapInsertReturnCode insert_to_hash_map_row_to_mapping_side(HashMapRowToMappingSide *hm, int *row, int *mapping_side){
    uint32_t bucket_i = find_bucket(hm, row);
    RowToMappingSide *buckets = hm->buckets;
    if(buckets[bucket_i].row == NULL){
        // NOTE: not in the hashmap, insert
        buckets[bucket_i].row = row;
        buckets[bucket_i].mapping_side = mapping_side;

        hm->num_rows++;

        #define MAX_LOAD_FACTOR 0.75f
        if(load_factor(*hm) > MAX_LOAD_FACTOR){
            resize_hash_map_row_to_mapping_side(hm);
            return MAP_INSERT_ADDED_RESIZING;
        }
        #undef MAX_LOAD_FACTOR

        return MAP_INSERT_ADDED;

    } else {
        // NOTE: equivalent in the hashmap, do not insert
        return MAP_INSERT_ALREADY_CONTAINED;
    }
}


bool is_in_hash_map_row_to_mapping_side(HashMapRowToMappingSide hm, int *row){
    uint32_t bucket_i = find_bucket(&hm, row);
    return hm.buckets[bucket_i].row != NULL;
}
RowToMappingSide *get_pair_in_hash_map_row_to_mapping_side(HashMapRowToMappingSide hm, int *row){
    uint32_t bucket_i = find_bucket(&hm, row);
    if(hm.buckets[bucket_i].row != NULL){
        return hm.buckets + bucket_i;
    }
    return NULL;
}


void print_hash_map_row_to_mapping_side(HashMapRowToMappingSide hm){
    
    for(int remaining_rows = hm.num_rows, *row = hm.buckets[0].row; 
        remaining_rows; 
        --remaining_rows, (RowToMappingSide*)row++)
    {
        assert_row_not_end(&hm, row);
        if(row){
            printf("Row:\n");
            print_array_ints(row, hm.len_rows); printf("\n");
            printf("Mapping Side:\n");
            print_array_ints((int **)row+1, hm.len_mapping_sides); printf("\n\n");
        }
    }
}
