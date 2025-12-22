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

/**
 * Precondition: num_buckets > 0 (if not, calloc undefined behavior and resizing logic wouldn't work)
 */
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
    enum { INITIAL_HASH_SET_VARIABLES_SIZE = 1 << 7 };
    return create_hash_set_variables(INITIAL_HASH_SET_VARIABLES_SIZE);
}

void free_hash_set_variables(HashSetVariables hs) {
    LinkedListVariablesNode **current_bucket = hs.listsVariables;
    LinkedListVariablesNode **end = hs.listsVariables + hs.num_buckets;
    for(; current_bucket < end; ++current_bucket){
        LinkedListVariablesNode *linked_list = *current_bucket;
        free_linked_list_variables(linked_list);
    }

    free(hs.listsVariables);
}

void clear_hash_set_variables(HashSetVariables hs) {
    LinkedListVariablesNode **current_bucket = hs.listsVariables;
    LinkedListVariablesNode **end = hs.listsVariables + hs.num_buckets;
    for(; current_bucket < end; ++current_bucket){
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

/**
 * Precondition: hs->num_buckets > 0 (if not, calloc undefined behavior and resizing logic wouldn't work)
 */
// TODO: check if we could take advantage of already created nodes instead of creating hole new ones...
//  Shouldn't insert to head of linked list simply receive a node?
static inline void resize_hash_set_variables(HashSetVariables *hs){
    uint32_t new_num_buckets = hs->num_buckets * 2;
    LinkedListVariablesNode **new_buckets = calloc(new_num_buckets, sizeof(*new_buckets));
    if(new_buckets == NULL){
        perror("calloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    LinkedListVariablesNode **current_bucket = hs->listsVariables;
    LinkedListVariablesNode **end = hs->listsVariables + hs->num_buckets;
    for(; current_bucket < end; ++current_bucket){
        LinkedListVariablesNode *linked_list = *current_bucket;
        LinkedListVariablesNode *current_node = linked_list;
        for(; current_node; current_node = current_node->next){
            Variable v = current_node->v;
            uint32_t new_bucket_i = hash(v) % new_num_buckets;
            LinkedListVariablesNode **new_linked_list_ptr = new_buckets + new_bucket_i;
            insert_to_linked_list_variables_head(new_linked_list_ptr, v);
        }
    }

    free_hash_set_variables(*hs);

    hs->listsVariables = new_buckets;
    hs->num_buckets = new_num_buckets;
    //hs->num_variables remains equal
}

SetInsertReturnCode insert_to_hash_set_variables(HashSetVariables *hs, Variable v){
    uint32_t bucket_i = hash(v) % hs->num_buckets;
    LinkedListVariablesNode **linked_list_ptr = hs->listsVariables + bucket_i;
    
    if(lookup_linked_list_variables(*linked_list_ptr, v)){
        return SET_INSERT_ALREADY_CONTAINED;
    }

    insert_to_linked_list_variables_head(linked_list_ptr, v);
    ++(hs->num_variables);

    #define MAX_LOAD_FACTOR 0.75f
    if(load_factor(*hs) > MAX_LOAD_FACTOR){
        resize_hash_set_variables(hs);
        return SET_INSERT_ADDED_RESIZING;
    }
    #undef MAX_LOAD_FACTOR

    return SET_INSERT_ADDED;
}

/**
 * Version where we don't check if v is already in the dictionary, for cases where
 * we already know it is the case.
 * 
 * NOTE: not used for now... Candidate to deletion...
 */
SetInsertReturnCode unchecked_insert_to_hash_set_variables(HashSetVariables *hs, Variable v){
    uint32_t bucket_i = hash(v) % hs->num_buckets;
    LinkedListVariablesNode **linked_list_ptr = hs->listsVariables + bucket_i;

    insert_to_linked_list_variables_head(linked_list_ptr, v);
    ++(hs->num_variables);

    #define MAX_LOAD_FACTOR 0.75f
    if(load_factor(*hs) > MAX_LOAD_FACTOR){
        resize_hash_set_variables(hs);
        return SET_INSERT_ADDED_RESIZING;
    }
    #undef MAX_LOAD_FACTOR

    return SET_INSERT_ADDED;
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
        for(; node; node=node->next){
            if(!first){
                printf(", ");
            }
            first = false;
            printf("%u", (unsigned)node->v);
        }
    }
    printf("}\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
