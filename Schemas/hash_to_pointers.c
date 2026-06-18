#include <stdio.h>
#include "hash_to_pointers.h"

// TODO: I think that this hash maps of pointers was intended to be used in postprocessing, to save and then access efficiently
//  only the rows that we removed. I have already implemented it in a different way, so this file should be removed. 
//  Until the general macros for defining HashMaps are defined, we will keep saving this file. But then, it has to be removed.

// Form hash value for string s
static unsigned hash_string(char *s) {
    unsigned hashval;
    for (hashval = 0; *s != '\0'; s++)
        hashval = *s + 31 * hashval;
    return hashval;
}

/**
 * @brief A professional, high-distribution hash function for void pointers.
 * * This uses the MurmurHash3 64-bit mixer avalanche step. It is incredibly fast
 * and ensures that even if pointers are closely clustered (or aligned to 8/16 bytes),
 * the resulting hashes are uniformly distributed across the entire uint64_t space.
 *
 * @param ptr The void pointer to hash.
 * @return uint64_t A highly distributed 64-bit hash value.
 */
uint64_t hash_pointer(const void *ptr) {
    // 1. Convert the pointer to a 64-bit unsigned integer
    uintptr_t value = (uintptr_t)ptr;
    
    // If you are on a 32-bit system, uintptr_t is 32-bit. 
    // We cast it up to 64-bit safely here.
    uint64_t x = (uint64_t)value;

    // 2. MurmurHash3 64-bit Mixer Constants & Shifts
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;

    return x;
}

HashMapStringToPointer create_hash_map_string_to_pointer(uint32_t num_buckets) {
    HashMapStringToPointer hm;
    hm.buckets = calloc(num_buckets, sizeof(*hm.buckets));
    CHECK_CALLOC(hm.buckets);
    hm.num_buckets = num_buckets;
    hm.num_pairs = 0;
    return hm;
}

void clear_hash_map_string_to_pointer(HashMapStringToPointer *hm) {
    hm->num_pairs = 0;
    SET_TO_ZERO(hm->buckets, hm->num_buckets * sizeof(*hm->buckets));
}

#define load_factor(hash_structure, num_elems_member_name, num_buckets_member_name) \
    ((float)(hash_structure).num_elems_member_name / (hash_structure).num_buckets_member_name)

static inline void resize_hash_map_string_to_pointer(HashMapStringToPointer *hm){
    uint32_t new_num_buckets = hm->num_buckets ? hm->num_buckets * 2 : INITIAL_HASH_MAP_SIZE;
    StringToPointer *new_buckets = calloc(new_num_buckets, sizeof(*new_buckets));
    CHECK_CALLOC(new_buckets);

    foreach_in_hashmap_string_to_pointer(*hm, pair){
        if (pair->str) {
            uint32_t bucket_i = hash_string(pair->str) % new_num_buckets;
#ifndef NDEBUG
            uint32_t initial_bucket_i = bucket_i;
#endif
            while(new_buckets[bucket_i].str != NULL){
                bucket_i = (bucket_i + 1) % new_num_buckets;
                // NOTE: thanks to the load factor we should always have an empty spot
                assert(bucket_i != initial_bucket_i);
            }

            new_buckets[bucket_i] = *pair;
        }
    }

    free(hm->buckets);
    hm->buckets = new_buckets;
    hm->num_buckets = new_num_buckets;
    //hm->num_pairs remains equal
}
static uint32_t find_bucket(HashMapStringToPointer *hm, char *key){
    uint32_t bucket_i = hash_string(key) % hm->num_buckets;
#ifndef NDEBUG
    uint32_t initial_bucket_i = bucket_i;
#endif
    StringToPointer *buckets = hm->buckets;
    while(buckets[bucket_i].str != NULL && strcmp(key, buckets[bucket_i].str) != 0){
        bucket_i = (bucket_i + 1) % hm->num_buckets;
        // NOTE: thanks to the load factor we should always have an empty spot
        assert(bucket_i != initial_bucket_i);
    }

    return bucket_i;
}
MapInsertReturnCode insert_to_hash_map_string_to_pointer(HashMapStringToPointer *hm, char *name, void *ptr) {
    uint32_t bucket_i = find_bucket(hm, name);
    StringToPointer *buckets = hm->buckets;
    if(buckets[bucket_i].str == NULL){
        // NOTE: not in the hashmap, insert
        buckets[bucket_i].str = name;
        buckets[bucket_i].ptr = ptr;

        hm->num_pairs++;

        #define MAX_LOAD_FACTOR 0.75f
        if(load_factor(*hm, num_pairs, num_buckets) > MAX_LOAD_FACTOR){
            resize_hash_map_string_to_pointer(hm);
            return MAP_INSERT_ADDED_RESIZING;
        }
        #undef MAX_LOAD_FACTOR

        return MAP_INSERT_ADDED;

    } else {
        // NOTE: equivalent in the hashmap, do not insert
        return MAP_INSERT_ALREADY_CONTAINED;
    }
}

bool is_in_hash_map_string_to_pointer(HashMapStringToPointer hm, char *name) {
    uint32_t bucket_i = find_bucket(&hm, name);
    return hm.buckets[bucket_i].str != NULL;
}

StringToPointer *get_pair_in_hash_map_string_to_pointer(HashMapStringToPointer hm, char *name) {
    uint32_t bucket_i = find_bucket(&hm, name);
    if(hm.buckets[bucket_i].str != NULL){
        return hm.buckets + bucket_i;
    }
    return NULL;
}

void print_hash_map_string_to_pointer(HashMapStringToPointer hm) {
    printf("{\n");
    foreach_in_hashmap_string_to_pointer(hm, pair) {
        if(pair->str){
            printf("\t%s: %p\n", pair->str, pair->ptr);
        }
    }
    printf("}\n");
}




HashSetPointers create_hash_set_pointers(uint32_t num_buckets) {
    HashSetPointers hs;
    hs.buckets = calloc(num_buckets, sizeof(*hs.buckets));
    CHECK_CALLOC(hs.buckets);
    hs.num_buckets = num_buckets;
    hs.num_elements = 0;
    return hs;
}

void clear_hash_set_pointers(HashSetPointers *hs) {
    hs->num_elements = 0;
    SET_TO_ZERO(hs->buckets, hs->num_buckets * sizeof(*hs->buckets));
}

static inline void resize_hash_set_pointers(HashSetPointers *hs){
    uint32_t new_num_buckets = hs->num_buckets ? hs->num_buckets * 2 : INITIAL_HASH_SET_SIZE;
    void **new_buckets = calloc(new_num_buckets, sizeof(*new_buckets));
    CHECK_CALLOC(new_buckets);

    foreach_in_hashset_pointers_ptr(hs, ptrptr){
        void *ptr = *ptrptr;
        if (ptr) {
            uint32_t bucket_i = hash_pointer(ptr) % new_num_buckets;
#ifndef NDEBUG
            uint32_t initial_bucket_i = bucket_i;
#endif
            while(new_buckets[bucket_i] != NULL){
                bucket_i = (bucket_i + 1) % new_num_buckets;
                // NOTE: thanks to the load factor we should always have an empty spot
                assert(bucket_i != initial_bucket_i);
            }

            new_buckets[bucket_i] = ptr;
        }
    }

    free(hs->buckets);
    hs->buckets = new_buckets;
    hs->num_buckets = new_num_buckets;
    //hs->num_elements remains equal
}
static uint32_t find_bucket_in_set(HashSetPointers *hs, void *key){
    uint32_t bucket_i = hash_pointer(key) % hs->num_buckets;
#ifndef NDEBUG
    uint32_t initial_bucket_i = bucket_i;
#endif
    void **buckets = hs->buckets;
    while(buckets[bucket_i] != NULL && buckets[bucket_i] != key){
        bucket_i = (bucket_i + 1) % hs->num_buckets;
        // NOTE: thanks to the load factor we should always have an empty spot
        assert(bucket_i != initial_bucket_i);
    }

    return bucket_i;
}
SetInsertReturnCode insert_to_hash_set_pointers(HashSetPointers *hs, void *ptr) {
    uint32_t bucket_i = find_bucket_in_set(hs, ptr);
    void **buckets = hs->buckets;
    if(buckets[bucket_i] == NULL){
        // NOTE: not in the hashmap, insert
        buckets[bucket_i] = ptr;

        hs->num_elements++;

        #define MAX_LOAD_FACTOR 0.75f
        if(load_factor(*hs, num_elements, num_buckets) > MAX_LOAD_FACTOR){
            resize_hash_set_pointers(hs);
            return SET_INSERT_ADDED_RESIZING;
        }
        #undef MAX_LOAD_FACTOR

        return SET_INSERT_ADDED;

    } else {
        // NOTE: equivalent in the hashmap, do not insert
        return SET_INSERT_ALREADY_CONTAINED;
    }
}

bool is_in_hash_set_pointers(HashSetPointers hs, void *ptr) {
    uint32_t bucket_i = find_bucket_in_set(&hs, ptr);
    return hs.buckets[bucket_i] != NULL;
}

void print_hash_set_pointers(HashSetPointers hs) {
    printf("{\n");
    foreach_in_hashset_pointers(hs, ptrptr) {
        if(*ptrptr){
            printf("\t%p\n", *ptrptr);
        }
    }
    printf("}\n");
}

