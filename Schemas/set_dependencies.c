#include "set_dependencies.h"
#include <stdlib.h>

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
/// Auxiliary functions of Linked Lists of Dependencies ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: if needed, we can program more like this; for example, a version that receives the
//  already initialized ArrayList of pointers to Schemas.
// NOTE: node->next is not set to NULL! (because insert_to_linked_list_dependencies_head is used later...)
static inline HashMapDependenciesNode *create_hash_map_dependencies_node(Variable v, Schema *schema){
    HashMapDependenciesNode *node = malloc(sizeof(*node));
    if(node == NULL){
        perror("malloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    node->v = v;
    ArrayListSchemaPtr new_list = create_array_list_schema_ptr_defsize();
    add_to_array_list_schema_ptr(&new_list, schema);
    node->schemas = new_list;
    return node;
}

// NOTE: arraylists of schemas are freed. Be careful with dangling pointers!
static inline void free_linked_list_dependencies(HashMapDependenciesNode *linked_list){
    HashMapDependenciesNode *current = linked_list;
    while(current){
        HashMapDependenciesNode *next = current->next;
        free_array_list_schema_ptr(current->schemas);
        free(current);
        current = next;
    }
}

static inline void insert_to_linked_list_dependencies_head(
    HashMapDependenciesNode **linked_list_ptr, HashMapDependenciesNode *new_head)
{
    new_head->next = *linked_list_ptr;
    *linked_list_ptr = new_head;
}

static inline HashMapDependenciesNode* lookup_linked_list_dependencies(HashMapDependenciesNode *linked_list, Variable v){
    HashMapDependenciesNode *current = linked_list;
    for(; current; current = current->next){
        if(current->v == v){
            return current;
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////
/// HASH MAP for Sets of Dependencies //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////
/// Creation and clearing //////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

// Because its only 16 Bytes, we pass and return it by value

/**
 * Precondition: num_buckets > 0 (if not, calloc undefined behavior and resizing logic wouldn't work)
 */
HashMapDependencies create_hash_map_dependencies(uint32_t num_buckets){
    HashMapDependencies hm;
    hm.num_dependencies = 0;
    hm.num_buckets = num_buckets;
    hm.lists_dependencies = calloc(num_buckets, sizeof(*hm.lists_dependencies));

    if (hm.lists_dependencies == NULL) {
        perror("calloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    return hm;
}

HashMapDependencies create_hash_map_dependencies_defsize(){
    enum { INITIAL_HASH_MAP_DEPENDENCIES_SIZE = 1 << 7 };
    return create_hash_map_dependencies(INITIAL_HASH_MAP_DEPENDENCIES_SIZE);
}

void free_hash_map_dependencies(HashMapDependencies hm){
    HashMapDependenciesNode **current_bucket = hm.lists_dependencies;
    HashMapDependenciesNode **end = hm.lists_dependencies + hm.num_buckets;
    for(; current_bucket < end; ++current_bucket){
        HashMapDependenciesNode *linked_list = *current_bucket;
        free_linked_list_dependencies(linked_list);
    }

    free(hm.lists_dependencies);
}

void clear_hash_map_dependencies(HashMapDependencies hm){
    HashMapDependenciesNode **current_bucket = hm.lists_dependencies;
    HashMapDependenciesNode **end = hm.lists_dependencies + hm.num_buckets;
    for(; current_bucket < end; ++current_bucket){
        HashMapDependenciesNode *linked_list = *current_bucket;
        free_linked_list_dependencies(linked_list);
    }

    memset(hm.lists_dependencies, 0, hm.num_buckets * sizeof(*hm.lists_dependencies));
    hm.num_dependencies = 0;
    //hm.num_buckets remains equal (TODO: add deleting and shrinking logic if needed)
}

////////////////////////////////////////////////////////////////////////////////////////////
/// Inserting and resizing /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

// In this case, the semantics of the operations favor passing by reference (value of the pointer to the struct)

static inline float load_factor(HashMapDependencies hm) { 
    return (float)hm.num_dependencies / (float)hm.num_buckets;
}

/**
 * Precondition: hs->num_buckets > 0 (if not, calloc undefined behavior and resizing logic wouldn't work)
 */
static inline void resize_hash_map_dependencies(HashMapDependencies *hm){
    uint32_t new_num_buckets = hm->num_buckets * 2;
    HashMapDependenciesNode **new_buckets = calloc(new_num_buckets, sizeof(*new_buckets));
    if(new_buckets == NULL){
        perror("calloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    HashMapDependenciesNode **current_bucket = hm->lists_dependencies;
    HashMapDependenciesNode **end = hm->lists_dependencies + hm->num_buckets;
    for(; current_bucket < end; ++current_bucket){
        HashMapDependenciesNode *linked_list = *current_bucket;
        HashMapDependenciesNode *current_node = linked_list;
        for(; current_node; current_node = current_node->next){
            uint32_t new_bucket_i = hash(current_node->v) % new_num_buckets;
            HashMapDependenciesNode **new_linked_list_ptr = new_buckets + new_bucket_i;
            insert_to_linked_list_dependencies_head(new_linked_list_ptr, current_node);
        }
    }

    free(hm->lists_dependencies);
    hm->lists_dependencies = new_buckets;
    hm->num_buckets = new_num_buckets;
    //hm->num_dependencies remains equal
}

int insert_to_hash_map_dependencies(HashMapDependencies *hm, Variable v, Schema *s){
    uint32_t bucket_i = hash(v) % hm->num_buckets;
    HashMapDependenciesNode **linked_list_ptr = hm->lists_dependencies + bucket_i;
    
    HashMapDependenciesNode *node = lookup_linked_list_dependencies(*linked_list_ptr, v);
    if(node){
        add_to_array_list_schema_ptr(&node->schemas, s);
        return MAP_INSERT_ALREADY_CONTAINED;
    }

    HashMapDependenciesNode *new_head = create_hash_map_dependencies_node(v, s);
    insert_to_linked_list_dependencies_head(linked_list_ptr, new_head);
    ++(hm->num_dependencies);
    
    #define MAX_LOAD_FACTOR 0.75f
    if(load_factor(*hm) > MAX_LOAD_FACTOR){
        resize_hash_map_dependencies(hm);
        return MAP_INSERT_ADDED_RESIZING;
    }
    #undef MAX_LOAD_FACTOR

    return MAP_INSERT_ADDED;
}


////////////////////////////////////////////////////////////////////////////////////////////
/// Lookup /////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

// Again, simply pass by value
// The ArrayList of pointers to Schemas is also returned by value (just 16 Bytes)

// NOTE: another possibility is to return a pointer to the node instead of the value 
//  contained in it.
ArrayListSchemaPtr lookup_hash_map_dependencies(HashMapDependencies hm, Variable v){
    uint32_t bucket_i = hash(v) % hm.num_buckets;
    HashMapDependenciesNode *linked_list = hm.lists_dependencies[bucket_i];
    return lookup_linked_list_dependencies(linked_list, v)->schemas;
}

////////////////////////////////////////////////////////////////////////////////////////////
/// Printing for debugging /////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

void print_hash_map_dependencies(HashMapDependencies hm){
    print_hash_map_dependencies_separator(hm, "; ");
}

void print_hash_map_dependencies_separator(HashMapDependencies hm, const char *separator){
    printf("{");
    bool first = true;
    for(uint32_t i = 0; i < hm.num_buckets; ++i){
        HashMapDependenciesNode *node = hm.lists_dependencies[i];
        for(; node; node=node->next){
            if(!first){
                printf("%s", separator);
            }
            first = false;
            printf("V%u -> ", node->v);
            print_array_list_schema_ptr(node->schemas);
        }
    }
    printf("}");
}
