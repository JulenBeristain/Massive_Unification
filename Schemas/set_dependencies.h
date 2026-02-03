#ifndef SET_DEPENDENCIES_H
#define SET_DEPENDENCIES_h

#include "set_variables.h"  // For Variable
#include "schemas.h"        // For Schema
#include "arraylist.h"      // For ArrayListSchemaPtr and ArrayListSchema
#include <stdint.h>

/**
 * Sets of dependencies are mappings from variables to schemas associated to common Schemas.
 * Because of that, we are going to implement them as hash-maps from ints to pointers to 
 * Schemas.
 */

// Hash Maps
// NOTE: in each set of dependencies, each variable can have multiple Schemas. Therefore, we
//  have multiple choices. We could simply have a "multi-function"-like hashmap, with the 
//  possibility of multiple pairs with the same key in the same bucket. I prefer to map
//  each value to a list (it could be any other container interface, but for simplicity let's
//  use an arraylist).
// TODO: see if having an arraylist per variable is efficient enough for the theta operator.
typedef struct HashMapDependenciesNode HashMapDependenciesNode;
struct HashMapDependenciesNode {
    HashMapDependenciesNode *next;
    ArrayListSchemaPtr schemas;
    Variable v;
};

typedef struct HashMapDependencies HashMapDependencies;
struct HashMapDependencies{
    uint32_t num_dependencies;
    uint32_t num_buckets;
    HashMapDependenciesNode **lists_dependencies;
};
//

// Operations return code types
typedef enum MapInsertReturnCode MapInsertReturnCode;
enum MapInsertReturnCode { 
    MAP_INSERT_ADDED,
    MAP_INSERT_ADDED_RESIZING,
    MAP_INSERT_ALREADY_CONTAINED
};
//

// Function declarations
HashMapDependencies create_hash_map_dependencies(uint32_t num_buckets);
HashMapDependencies create_hash_map_dependencies_defsize();
void free_hash_map_dependencies(HashMapDependencies hm);
void clear_hash_map_dependencies(HashMapDependencies hm);
int insert_to_hash_map_dependencies(HashMapDependencies *hm, Variable v, Schema *s);
ArrayListSchemaPtr *lookup_hash_map_dependencies(HashMapDependencies hm, Variable v);
void print_hash_map_dependencies(HashMapDependencies hm);
void print_hash_map_dependencies_separator(HashMapDependencies hm, const char *separator);

// Foreach macros
#define foreach_pair_in_hashmap_dependencies(set_dependencies, pair_ptr)                                        \
    for(HashMapDependenciesNode **list_dependencies_ptr = (set_dependencies).lists_dependencies,                \
                                **end = list_dependencies_ptr + (set_dependencies).num_buckets;                 \
        list_dependencies_ptr < end;                                                                            \
        ++list_dependencies_ptr                                                                                 \
    )                                                                                                           \
        for(HashMapDependenciesNode *pair_ptr = *list_dependencies_ptr; pair_ptr; pair_ptr = pair_ptr->next)

#define foreach_pair_in_hashmap_dependencies_ptr(set_dependencies, pair_ptr)                                    \
    for(HashMapDependenciesNode **list_dependencies_ptr = (set_dependencies)->lists_dependencies,               \
                                **end = list_dependencies_ptr + (set_dependencies)->num_buckets;                \
        list_dependencies_ptr < end;                                                                            \
        ++list_dependencies_ptr                                                                                 \
    )                                                                                                           \
        for(HashMapDependenciesNode *pair_ptr = *list_dependencies_ptr; pair_ptr; pair_ptr = pair_ptr->next)

// Default types and operations for sets of dependencies
typedef HashMapDependencies SetDependencies;
#define create_set_dependencies create_hash_map_dependencies
#define create_set_dependencies_defsize create_hash_map_dependencies_defsize
#define free_set_dependencies free_hash_map_dependencies
#define clear_set_dependencies clear_hash_map_dependencies
#define insert_to_set_dependencies insert_to_hash_map_dependencies
#define lookup_set_dependencies lookup_hash_map_dependencies
#define print_set_dependencies print_hash_map_dependencies
#define print_set_dependencies_separator print_hash_map_dependencies_separator


/////////////////////////////////////////////////////////////
/// SIMPLER VERSION OF SET OF DEPENDENCIES FOR A BASELINE
/////////////////////////////////////////////////////////////

// We are going to use a simple ArrayList of these DependencyPairs. See arraylist.c/h

typedef struct DependencyPair {
    Variable v;                 // NOTE: in this case, we could use a uint64_t to take advantage of the inevitable padding
    ArrayListSchema schemas;
} DependencyPair;

void print_dependency_pair(DependencyPair pair);

#endif