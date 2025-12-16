#include "set_variables.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
/* 
   NOTE:

   In our concrete high-performance use case, where we are only concerned 
   about unsigned ints, a limited version, which only performs a simplified 
   bit-mixing finalizer, is enough.

   Depending on the distribution of the variables in the sets, even the %
   operator could be sufficient (i.e., hash = id | id(x)=x), or we could
   even try another data structures as BitFields (there are other options too:
   hash sets with lookup of the next buckets, balanced binary search trees...)
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
/// Auxiliary functions of Linked Lists of Variables ///////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

static inline void free_linked_list_variables(LinkedListVariablesNode *linked_list){
    LinkedListVariablesNode *current = linked_list;
    while(current){
        LinkedListVariablesNode *next = current->next;
        free(current);
        current = next;
    }
}

static inline void insert_to_linked_list_variables_head(LinkedListVariablesNode **linked_list_ptr, Variable v){
    LinkedListVariablesNode *new_head = malloc(sizeof(*new_head));
    if(new_head == NULL){
        perror("malloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    new_head->v = v;
    new_head->next = *linked_list_ptr;
    *linked_list_ptr = new_head;
}

/**
 * NOTE: another possibility is to return the node itself...
 */
static inline bool lookup_linked_list_variables(LinkedListVariablesNode *linked_list, Variable v){
    LinkedListVariablesNode *current = linked_list;
    for(; current; current = current->next){
        if(current->v == v){
            return true;
        }
    }
    return false;
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

HashSetVariables create_hash_set_variables_defsize(){
    static const uint32_t INITIAL_HASH_SET_VARIABLES_SIZE = 1 << 7;
    return create_hash_set_variables(INITIAL_HASH_SET_VARIABLES_SIZE);
}

void free_hash_set_variables(HashSetVariables hs) {
    LinkedListVariablesNode **current_bucket = hs.listsVariables;

    for(uint32_t i = hs.num_buckets; i; --i, ++current_bucket){
        LinkedListVariablesNode *linked_list = *current_bucket;
        free_linked_list_variables(linked_list);
    }

    free(hs.listsVariables);
}

void clear_hash_set_variables(HashSetVariables hs) {
    LinkedListVariablesNode **current_bucket = hs.listsVariables;

    for(uint32_t i = hs.num_buckets; i; --i, ++current_bucket){
        LinkedListVariablesNode *linked_list = *current_bucket;
        free_linked_list_variables(linked_list);
    }

    memset(hs.listsVariables, 0, hs.num_buckets * sizeof(*hs.listsVariables));
    hs.num_variables = 0;
    //hs.num_buckets remains equal (TODO: add deleting and shrinking logic if needed)
}

// Inserting and resizing
// In this case, the semantics of the operations favor passing by reference (value of the pointer to the struct)

static inline float load_factor(HashSetVariables hs) { 
    return (float)hs.num_variables / (float)hs.num_buckets;
}

static inline void resize_hash_set_variables(HashSetVariables *hs){
    uint32_t new_num_buckets = hs->num_buckets * 2;
    LinkedListVariablesNode **new_buckets = calloc(new_num_buckets, sizeof(*new_buckets));
    if(new_buckets == NULL){
        perror("calloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    LinkedListVariablesNode **current_bucket = hs->listsVariables;
    for(uint32_t i = hs->num_buckets; i; --i, ++current_bucket){
        LinkedListVariablesNode *linked_list = *current_bucket;
        LinkedListVariablesNode *current_node = linked_list;
        for(; current_node; current_node = current_node->next){
            Variable v = current_node->v;
            uint32_t new_bucket_i = hash(v) % new_num_buckets;
            LinkedListVariablesNode **new_linked_list_ptr = new_buckets + new_bucket_i;
            insert_to_linked_list_variables_head(new_linked_list_ptr, v);
        }
    }

    hs->listsVariables = new_buckets;
    hs->num_buckets = new_num_buckets;
    //hs->num_variables remains equal
}

void insert_to_hash_set_variables(HashSetVariables *hs, Variable v){
    uint32_t bucket_i = hash(v) % hs->num_buckets;
    LinkedListVariablesNode **linked_list_ptr = hs->listsVariables + bucket_i;
    
    if(lookup_linked_list_variables(*linked_list_ptr, v)){
        return;
    }

    insert_to_linked_list_variables_head(linked_list_ptr, v);
    ++(hs->num_variables);

    static const float MAX_LOAD_FACTOR = 0.75f;
    if(load_factor(*hs) > MAX_LOAD_FACTOR){
        resize_hash_set_variables(hs);
    }
}

// Lookup
// Again, simply pass by value

bool lookup_hash_set_variables(HashSetVariables hs, Variable v){
    uint32_t bucket_i = hash(v) % hs.num_buckets;
    LinkedListVariablesNode *linked_list = hs.listsVariables[bucket_i];
    return lookup_linked_list_variables(linked_list, v);
}

// Printing for debugging
/**
 * Copilot generated...
 */
void print_hash_set_variables(HashSetVariables hs){
    printf("{");
    bool first = true;
    for(uint32_t i = 0; i < hs.num_buckets; ++i){
        LinkedListVariablesNode *node = hs.listsVariables[i];
        while(node){
            if(!first){
                printf(", ");
            }
            first = false;
            printf("%u", (unsigned)node->v);
            node = node->next;
        }
    }
    printf("}\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
