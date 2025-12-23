#ifndef SET_VARIABLES_H
#define SET_VARIABLES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * In this file we declare structs and functions to work with sets of variables (as unsigned > 0).
 * 
 * The set is going to be an array of linked lists (for hash collisions) of unsigned integers (variables).
 */

// VARIABLES
typedef unsigned Variable;
//

// HASH SET
typedef struct LinkedListVariablesNode LinkedListVariablesNode;
struct LinkedListVariablesNode {
    Variable v;
    LinkedListVariablesNode *next;
};

typedef struct HashSetVariables HashSetVariables;
struct HashSetVariables {
    uint32_t num_variables;
    uint32_t num_buckets;
    LinkedListVariablesNode **list_variables;
};

// Operations return code types
typedef enum SetInsertReturnCode SetInsertReturnCode;
enum SetInsertReturnCode { 
    SET_INSERT_ADDED,
    SET_INSERT_ADDED_RESIZING,
    SET_INSERT_ALREADY_CONTAINED
};
//

// Function declarations
HashSetVariables create_hash_set_variables(uint32_t num_buckets);
HashSetVariables create_hash_set_variables_defsize();
void free_hash_set_variables(HashSetVariables hs);
void clear_hash_set_variables(HashSetVariables hs);
SetInsertReturnCode insert_to_hash_set_variables(HashSetVariables *hs, Variable v);
SetInsertReturnCode unchecked_insert_to_hash_set_variables(HashSetVariables *hs, Variable v);
bool lookup_hash_set_variables(HashSetVariables hs, Variable v);
void print_hash_set_variables(HashSetVariables hs);
//

// Foreach macro
#define foreach_in_setvariables(set_vars, var_node_ptr)                                     \
    for(LinkedListVariablesNode **linked_list_ptr = (set_vars).list_variables,              \
                                **end = (set_vars).list_variables + (set_vars).num_buckets; \
        linked_list_ptr < end;                                                              \
        ++linked_list_ptr                                                                   \
    )                                                                                       \
        for(LinkedListVariablesNode *var_node_ptr = *linked_list_ptr;                       \
            var_node_ptr; var_node_ptr = var_node_ptr->next)

#define foreach_in_setvariablesptr(set_vars_ptr, var_node_ptr)                                          \
    for(LinkedListVariablesNode **linked_list_ptr = (set_vars_ptr)->list_variables,                     \
                                **end = (set_vars_ptr)->list_variables + (set_vars_ptr)->num_buckets;   \
        linked_list_ptr < end;                                                                          \
        ++linked_list_ptr                                                                               \
    )                                                                                                   \
        for(LinkedListVariablesNode *var_node_ptr = *linked_list_ptr;                                   \
            var_node_ptr; var_node_ptr = var_node_ptr->next)
//

// Default kind of Set for variables
typedef HashSetVariables SetVariables;
#define create_set_variables create_hash_set_variables
#define create_set_variables_defsize create_hash_set_variables_defsize
#define free_set_variables free_hash_set_variables
#define clear_set_variables clear_hash_set_variables
#define insert_to_set_variables insert_to_hash_set_variables
#define unchecked_insert_to_set_variables unchecked_insert_to_hash_set_variables
#define lookup_set_variables lookup_hash_set_variables
#define print_set_variables print_hash_set_variables
//


#endif // SET_VARIABLES_H