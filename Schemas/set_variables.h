#ifndef SET_VARIABLES_H
#define SET_VARIABLES_H

#include <stdint.h>

/**
 * In this file we declare structs and functions to work with sets of variables (as unsigned > 0).
 * 
 * The set is going to be an array of linked lists (for hash collisions) of unsigned integers (variables).
 */


typedef unsigned Variable;

typedef struct LinkedListVariablesNode LinkedListVariablesNode;
struct LinkedListVariablesNode {
    Variable v;
    LinkedListVariablesNode *next;
};

typedef HashSetVariables SetVariables; // Default kind of Set for variables 
typedef struct HashSetVariables HashSetVariables;
struct HashSetVariables {
    uint32_t num_variables;
    uint32_t num_buckets;
    LinkedListVariablesNode **listsVariables;
};

#define MAX_LOAD_FACTOR 0.75f

HashSetVariables create_hash_set_variables(uint32_t num_buckets);
void clear_hash_set_variables(HashSetVariables hs);

#endif // SET_VARIABLES_H