#include "set_variables.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// TODO: define the hash set of variables via general macros.
//  In the same way, define general macros for linked lists...
//  After that, define only operations that are used for sets of variables, no unused operations for Variables...

////////////////////////////////////////////////////////////////////////////////////////////
/// HASH FUNCTION //////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

//MurmurHash (https://en.wikipedia.org/wiki/MurmurHash)
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

// NOTE: node->next is not set to NULL! (because insert_to_linked_list_variables_head is used later...)
static inline LinkedListVariablesNode *create_linked_list_variables_node(Variable v){
    LinkedListVariablesNode *node = malloc(sizeof(*node));
    if(node == NULL){
        perror("malloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    node->v = v;
    return node;
}

static inline LinkedListVariablesNode *create_linked_list_variables_node_arena(Variable v, Arena *arena){
    LinkedListVariablesNode *node = allocate(arena, sizeof(*node));
    node->v = v;
    return node;
}

static inline void free_linked_list_variables(LinkedListVariablesNode *linked_list){
    LinkedListVariablesNode *current = linked_list;
    while(current){
        LinkedListVariablesNode *next = current->next;
        free(current);
        current = next;
    }
}

static inline void insert_to_linked_list_variables_head(
    LinkedListVariablesNode **linked_list_ptr, LinkedListVariablesNode *new_head)
{
    new_head->next = *linked_list_ptr;
    *linked_list_ptr = new_head;
}

static inline bool is_in_linked_list_variables(LinkedListVariablesNode *linked_list, Variable v){
    LinkedListVariablesNode *current = linked_list;
    for(; current; current = current->next){
        if(current->v == v){
            return true;
        }
    }
    return false;
}

static inline LinkedListVariablesNode *get_node_in_linked_list_variables(LinkedListVariablesNode *linked_list, Variable v){
    LinkedListVariablesNode *current = linked_list;
    for(; current; current = current->next){
        if(current->v == v){
            return current;
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
/// HASH SET of Variables //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

// Because hash sets are only 16 Bytes, we pass and return it by value whenever we can; i.e.,
// unless some of its attributes has to be changed or the semantics of the operation is 
// clearer passing by reference.

////////////////////////////////////////////////////////////////////////////////////////////
// CREATION ////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


/**
 * Precondition: num_buckets > 0 (if not, calloc undefined behavior and resizing logic wouldn't work)
 */
HashSetVariables create_hash_set_variables(uint32_t num_buckets) {
    HashSetVariables hs;
    hs.num_variables = 0;
    hs.num_buckets = num_buckets;
    hs.list_variables = calloc(num_buckets, sizeof(*hs.list_variables));

    if (hs.list_variables == NULL) {
        perror("calloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    return hs;
}

HashSetVariables create_hash_set_variables_defnumbuckets(){
    enum { INITIAL_HASH_SET_VARIABLES_SIZE = 1 << 7 };
    return create_hash_set_variables(INITIAL_HASH_SET_VARIABLES_SIZE);
}

// NOTE: Arenas don't work well with dynamic structures that might get bigger (thus requiring more memory and wasting
//  all the previous block). Thus, we don't implement a defnumbuckets_arena version, as the user should try to avoid 
//  resizing understanding the number of elements that might be introduced (max load factor of 75%). On the other hand,
//  we can always create "mini-arenas" only for a particular dynamic structure that is freed before a more general arena
//  where more persistent data is stored.
HashSetVariables create_hash_set_variables_arena(uint32_t num_buckets, Arena *arena){
    HashSetVariables hs;
    hs.num_variables = 0;
    hs.num_buckets = num_buckets;
    
    size_t num_bytes = num_buckets * sizeof(*hs.list_variables);
    hs.list_variables = allocate(arena, num_bytes);
    memset(hs.list_variables, 0, num_bytes);
    
    return hs;
}

////////////////////////////////////////////////////////////////////////////////////////////
// FREE AND CLEAR //////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////


void free_hash_set_variables(HashSetVariables hs) {
    foreach_bucket_in_hashsetvariables(hs, linked_list_head_pptr){
        free_linked_list_variables(*linked_list_head_pptr);
    }
    free(hs.list_variables);
}

void clear_hash_set_variables(HashSetVariables *hs) {
    foreach_bucket_in_hashsetvariablesptr(hs, linked_list_head_pptr){
        free_linked_list_variables(*linked_list_head_pptr);
    }

    memset(hs->list_variables, 0, hs->num_buckets * sizeof(*hs->list_variables));
    hs->num_variables = 0;
    //hs.num_buckets remains equal (NOTE: add deleting and shrinking logic if needed)
}

// NOTE: clearing a hash set whose memory is allocated in an arena isn't recommendable, because all the memory for 
//  the nodes will become wasted memory in the arena. Nonetheless, it's still better than creating several hash sets,
//  since at least the memory of the hash set "header" is reused.
void clear_hash_set_variables_arena(HashSetVariables *hs){
    memset(hs->list_variables, 0, hs->num_buckets * sizeof(*hs->list_variables));
    hs->num_variables = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////
// INSERT //////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

static inline float load_factor(HashSetVariables hs) { 
    return (float)hs.num_variables / (float)hs.num_buckets;
}

/**
 * Precondition: hs->num_buckets > 0 (if not, calloc undefined behavior and resizing logic wouldn't work)
 */
static inline void resize_hash_set_variables(HashSetVariables *hs){
    uint32_t new_num_buckets = hs->num_buckets * 2;
    LinkedListVariablesNode **new_buckets = calloc(new_num_buckets, sizeof(*new_buckets));
    if(new_buckets == NULL){
        perror("calloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    // NOTE: this is like an uncheked union operation
    foreach_bucket_in_hashsetvariablesptr(hs, list_head_pptr){
        LinkedListVariablesNode *current_node = *list_head_pptr;
        while(current_node){
            LinkedListVariablesNode *next_node = current_node->next;
            uint32_t new_bucket_i = hash(current_node->v) % new_num_buckets;
            LinkedListVariablesNode **new_linked_list_ptr = new_buckets + new_bucket_i;
            insert_to_linked_list_variables_head(new_linked_list_ptr, current_node); // NOTE: current_node->next is modified
            current_node = next_node;
        }
    }

    free(hs->list_variables);
    hs->list_variables = new_buckets;
    hs->num_buckets = new_num_buckets;
    //hs->num_variables remains equal
}

static inline void resize_hash_set_variables_arena(HashSetVariables *hs, Arena *arena){
    uint32_t new_num_buckets = hs->num_buckets * 2;
    LinkedListVariablesNode **new_buckets = allocate(arena, new_num_buckets * sizeof(*new_buckets));

    // NOTE: this is like an uncheked union operation
    foreach_bucket_in_hashsetvariablesptr(hs, list_head_pptr){
        LinkedListVariablesNode *current_node = *list_head_pptr;
        while(current_node){
            LinkedListVariablesNode *next_node = current_node->next;
            uint32_t new_bucket_i = hash(current_node->v) % new_num_buckets;
            LinkedListVariablesNode **new_linked_list_ptr = new_buckets + new_bucket_i;
            insert_to_linked_list_variables_head(new_linked_list_ptr, current_node); // NOTE: current_node->next is modified
            current_node = next_node;
        }
    }

    hs->list_variables = new_buckets;
    hs->num_buckets = new_num_buckets;
    //hs->num_variables remains equal
}

SetInsertReturnCode insert_to_hash_set_variables(HashSetVariables *hs, Variable v){
    uint32_t bucket_i = hash(v) % hs->num_buckets;
    LinkedListVariablesNode **linked_list_ptr = hs->list_variables + bucket_i;
    
    if(is_in_linked_list_variables(*linked_list_ptr, v)){
        return SET_INSERT_ALREADY_CONTAINED;
    }

    LinkedListVariablesNode *new_head = create_linked_list_variables_node(v);
    insert_to_linked_list_variables_head(linked_list_ptr, new_head);
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
    LinkedListVariablesNode **linked_list_ptr = hs->list_variables + bucket_i;

    LinkedListVariablesNode *new_head = create_linked_list_variables_node(v);
    insert_to_linked_list_variables_head(linked_list_ptr, new_head);
    ++(hs->num_variables);

    #define MAX_LOAD_FACTOR 0.75f
    if(load_factor(*hs) > MAX_LOAD_FACTOR){
        resize_hash_set_variables(hs);
        return SET_INSERT_ADDED_RESIZING;
    }
    #undef MAX_LOAD_FACTOR

    return SET_INSERT_ADDED;
}

SetInsertReturnCode insert_to_hash_set_variables_arena(HashSetVariables *hs, Variable v, Arena *arena){
    uint32_t bucket_i = hash(v) % hs->num_buckets;
    LinkedListVariablesNode **linked_list_ptr = hs->list_variables + bucket_i;
    
    if(is_in_linked_list_variables(*linked_list_ptr, v)){
        return SET_INSERT_ALREADY_CONTAINED;
    }

    LinkedListVariablesNode *new_head = create_linked_list_variables_node_arena(v, arena);
    insert_to_linked_list_variables_head(linked_list_ptr, new_head);
    ++(hs->num_variables);

    #define MAX_LOAD_FACTOR 0.75f
    if(load_factor(*hs) > MAX_LOAD_FACTOR){
        resize_hash_set_variables_arena(hs, arena);
        return SET_INSERT_ADDED_RESIZING;
    }
    #undef MAX_LOAD_FACTOR

    return SET_INSERT_ADDED;
}

// NOTE: not used for now... Candidate to deletion...
SetInsertReturnCode unchecked_insert_to_hash_set_variables_arena(HashSetVariables *hs, Variable v, Arena *arena){
    uint32_t bucket_i = hash(v) % hs->num_buckets;
    LinkedListVariablesNode **linked_list_ptr = hs->list_variables + bucket_i;

    LinkedListVariablesNode *new_head = create_linked_list_variables_node_arena(v, arena);
    insert_to_linked_list_variables_head(linked_list_ptr, new_head);
    ++(hs->num_variables);

    #define MAX_LOAD_FACTOR 0.75f
    if(load_factor(*hs) > MAX_LOAD_FACTOR){
        resize_hash_set_variables_arena(hs, arena);
        return SET_INSERT_ADDED_RESIZING;
    }
    #undef MAX_LOAD_FACTOR

    return SET_INSERT_ADDED;
}

////////////////////////////////////////////////////////////////////////////////////////////
// FIND ////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

bool is_in_hash_set_variables(HashSetVariables hs, Variable v){
    uint32_t bucket_i = hash(v) % hs.num_buckets;
    LinkedListVariablesNode *linked_list = hs.list_variables[bucket_i];
    return is_in_linked_list_variables(linked_list, v);
}

LinkedListVariablesNode *get_node_in_hash_set_variables(HashSetVariables hs, Variable v){
    uint32_t bucket_i = hash(v) % hs.num_buckets;
    LinkedListVariablesNode *linked_list = hs.list_variables[bucket_i];
    return get_node_in_linked_list_variables(linked_list, v);
}

////////////////////////////////////////////////////////////////////////////////////////////
// PRINT ///////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

void print_hash_set_variables(HashSetVariables hs){
    printf("{");
    bool first = true;
    foreach_in_hashsetvariables(hs, node){
        if(!first){
            printf(", ");
        }
        first = false;
        printf("%u", (unsigned)node->v);
    }
    printf("}");
}

void println_hash_set_variables(HashSetVariables hs){
    print_hash_set_variables(hs);
    printf("\n");
}
