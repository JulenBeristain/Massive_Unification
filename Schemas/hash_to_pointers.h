#ifndef HASH_TO_POINTERS_H
#define HASH_TO_POINTERS_H

#include <stdint.h>
#include <stdlib.h>
#include "utils.h"

// Hashmap that goes from Strings to void pointers (for Matrix pointers, to store the named set of Matrices in the program)
typedef struct {
    char *str;
    void *ptr;
} StringToPointer;

typedef struct HashMapStringToPointer HashMapStringToPointer;
struct HashMapStringToPointer {
    uint32_t num_pairs;
    uint32_t num_buckets;
    StringToPointer *buckets;
};

// Function declarations /////////////////////////////////////////////
HashMapStringToPointer create_hash_map_string_to_pointer(uint32_t num_buckets);
enum { INITIAL_HASH_MAP_SIZE = 1 << 7 };
static inline HashMapStringToPointer create_hash_map_string_to_pointer_defnumbuckets(){
    return create_hash_map_string_to_pointer(INITIAL_HASH_MAP_SIZE);
}
static inline void free_hash_map_string_to_pointer(HashMapStringToPointer hm){ free(hm.buckets); }
void clear_hash_map_string_to_pointer(HashMapStringToPointer *hm);
MapInsertReturnCode insert_to_hash_map_string_to_pointer(HashMapStringToPointer *hm, char *name, void *ptr);
bool is_in_hash_map_string_to_pointer(HashMapStringToPointer hm, char *name);
StringToPointer *get_pair_in_hash_map_string_to_pointer(HashMapStringToPointer hm, char *name);
void print_hash_map_string_to_pointer(HashMapStringToPointer hm);

// Foreach macros ////////////////////////////////////////////////////
#define foreach_in_hashmap_string_to_pointer(hm, pairptr)           \
    for(StringToPointer *pairptr = (hm).buckets,                    \
                         *_end = (hm).buckets + (hm).num_buckets;   \
        pairptr < _end;                                             \
        ++pairptr                                                   \
    )

#define foreach_in_hashmap_string_to_pointer_ptr(hmptr, pairptr)            \
    for(StringToPointer *pairptr = (hmptr)->buckets,                        \
                         *_end = (hmptr)->buckets + (hmptr)->num_buckets;   \
        pairptr < _end;                                                     \
        ++pairptr                                                           \
    )



// Hashset of void pointers (for Schema pointers, to keep the shallow copies structure when copying current active Schemas
//  from the filled SchemasArena to a new one)

typedef struct {
    uint32_t num_elements;
    uint32_t num_buckets;
    void **buckets;
} HashSetPointers;

// Function declarations /////////////////////////////////////////////
HashSetPointers create_hash_set_pointers(uint32_t num_buckets);
enum { INITIAL_HASH_SET_SIZE = 1 << 7 };
static inline HashSetPointers create_hash_set_pointers_defnumbuckets(){
    return create_hash_set_pointers(INITIAL_HASH_SET_SIZE);
}
static inline void free_hash_set_pointers(HashSetPointers hs){ free(hs.buckets); }
void clear_hash_set_pointers(HashSetPointers *hs);
SetInsertReturnCode insert_to_hash_set_pointers(HashSetPointers *hs, void *ptr);
bool is_in_hash_set_pointers(HashSetPointers hs, void *ptr);
void print_hash_set_pointers(HashSetPointers hs);

// Foreach macros ////////////////////////////////////////////////////
#define foreach_in_hashset_pointers(hs, ptrptr)                     \
    for(void **ptrptr = (hs).buckets,                               \
                         *_end = (hs).buckets + (hs).num_buckets;   \
        ptrptr < _end;                                              \
        ++ptrptr                                                    \
    )

#define foreach_in_hashset_pointers_ptr(hsptr, ptrptr)                      \
    for(void **ptrptr = (hsptr)->buckets,                                   \
                         *_end = (hsptr)->buckets + (hsptr)->num_buckets;   \
        ptrptr < _end;                                                      \
        ++ptrptr                                                            \
    )

#endif