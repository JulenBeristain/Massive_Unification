#include "Schemas/set_variables.h"
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////////////////
/// HASH FUNCTION //////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

/**
 * MurmurHash (https://en.wikipedia.org/wiki/MurmurHash)
 */
/*
static inline uint32_t murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}
uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed) {
	uint32_t h = seed;
    uint32_t k;
    // Read in groups of 4.
    for (size_t i = len >> 2; i; i--) {
        // Here is a source of differing results across endiannesses.
        // A swap here has no effects on hash properties though.
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    // Read the rest.
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    // A swap is *not* necessary here because the preceding loop already
    // places the low bytes in the low places according to whatever endianness
    // we use. Swaps only apply when the memory is copied in a chunk.
    h ^= murmur_32_scramble(k);
    // Finalize.
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}
*/
/* In our concrete high-performance use cases where we are only concerned 
   about unsigned ints a limited version, which only performs a simplified 
   bit-mixing finalizer, is enough.
 */
static inline uint32_t hash(uint32_t k) {
    k ^= k >> 16;
    k *= 0x85ebca6b;
    k ^= k >> 13;
    k *= 0xc2b2ae35;
    k ^= k >> 16;
    return k; 
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
/// HASH SET of Variables //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

// Creation and clearing
// Because its only 16 Bytes, we pass and return it by value

HashSetVariables create_hash_set_variables(uint32_t num_buckets) {
    HashSetVariables hs;
    hs.num_variables = 0;
    hs.num_buckets = num_buckets;
    hs.listsVariables = calloc(num_buckets, sizeof(*hs.listsVariables));

    if (hs.listsVariables == NULL) {
        perror("calloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    return hs;
}

void clear_hash_set_variables(HashSetVariables hs) {
    LinkedListVariablesNode **current_bucket = hs.listsVariables;

    for(uint32_t i = hs.num_buckets; i; --i, ++current_bucket){
        LinkedListVariablesNode *linked_list = *current_bucket;
        free_linked_list(linked_list);
    }

    free(hs.listsVariables);
}

static inline void free_linked_list(LinkedListVariablesNode *linked_list){
    LinkedListVariablesNode *current = linked_list;
    while(current){
        LinkedListVariablesNode *next = current->next;
        free(current);
        current = next;
    }
}

// Inserting and resizing



static inline float load_factor(HashSetVariables hs) { 
    return (float)hs.num_variables / (float)hs.num_buckets;
}

// Lookup

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////