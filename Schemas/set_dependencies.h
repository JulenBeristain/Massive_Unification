#ifndef SET_DEPENDENCIES_H
#define SET_DEPENDENCIES_h

#include "set_variables.h"  // For Variable
#include "schemas.h"        // For Schema
#include "arraylist.h"      // For ArrayListSchemaPtr
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

// TODO: add declarations...

// TODO: add default typenames, defines of operations...

#endif