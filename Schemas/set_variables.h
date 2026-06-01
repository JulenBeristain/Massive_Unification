#ifndef SET_VARIABLES_H
#define SET_VARIABLES_H

#include <stdint.h>
#include <stdbool.h>
#include "arena.h"
#include "utils.h"

/**
 * In this file we declare structs and functions to work with sets of variables (as unsigned > 0).
 * 
 * The set is going to be an array of linked lists (for hash collisions) of unsigned integers (variables).
 */

// VARIABLES /////////////////////////////////////////////////////////
typedef unsigned Variable;
//////////////////////////////////////////////////////////////////////

// HASH SET //////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////

// NOTE: in other kinds of hash sets we could implement removing operations. In our case, for sets of variables, we don't need them.
// NOTE: other interesting operations are (checked and unchecked) set operations: union, intersection, etc.
// Function declarations /////////////////////////////////////////////
HashSetVariables create_hash_set_variables(uint32_t num_buckets);
HashSetVariables create_hash_set_variables_defnumbuckets();
HashSetVariables create_hash_set_variables_arena(uint32_t num_buckets, Arena *arena);
void free_hash_set_variables(HashSetVariables hs);
void clear_hash_set_variables(HashSetVariables *hs);
void clear_hash_set_variables_arena(HashSetVariables *hs);
SetInsertReturnCode insert_to_hash_set_variables(HashSetVariables *hs, Variable v);
SetInsertReturnCode unchecked_insert_to_hash_set_variables(HashSetVariables *hs, Variable v);
SetInsertReturnCode insert_to_hash_set_variables_arena(HashSetVariables *hs, Variable v, Arena *arena);
SetInsertReturnCode unchecked_insert_to_hash_set_variables_arena(HashSetVariables *hs, Variable v, Arena *arena);
bool is_in_hash_set_variables(HashSetVariables hs, Variable v);
LinkedListVariablesNode *get_node_in_hash_set_variables(HashSetVariables hs, Variable v);
void print_hash_set_variables(HashSetVariables hs);
void println_hash_set_variables(HashSetVariables hs);
//////////////////////////////////////////////////////////////////////

// Foreach macros ////////////////////////////////////////////////////
#define foreach_in_hashsetvariables(set_vars, var_node_ptr)                                     \
    for(LinkedListVariablesNode **_linked_list_ptr = (set_vars).list_variables,                 \
                                **_end = (set_vars).list_variables + (set_vars).num_buckets;    \
        _linked_list_ptr < _end;                                                                \
        ++_linked_list_ptr                                                                      \
    )                                                                                           \
        for(LinkedListVariablesNode *var_node_ptr = *_linked_list_ptr;                          \
            var_node_ptr; var_node_ptr = var_node_ptr->next)

#define foreach_in_hashsetvariablesptr(set_vars_ptr, var_node_ptr)                                          \
    for(LinkedListVariablesNode **_linked_list_ptr = (set_vars_ptr)->list_variables,                        \
                                **_end = (set_vars_ptr)->list_variables + (set_vars_ptr)->num_buckets;      \
        _linked_list_ptr < _end;                                                                            \
        ++_linked_list_ptr                                                                                  \
    )                                                                                                       \
        for(LinkedListVariablesNode *var_node_ptr = *_linked_list_ptr;                                      \
            var_node_ptr; var_node_ptr = var_node_ptr->next)

#define foreach_bucket_in_hashsetvariables(hash_set_vars, list_head_pptr)                               \
    for(LinkedListVariablesNode **list_head_pptr = (hash_set_vars).list_variables,                      \
                                **_end = (hash_set_vars).list_variables + (hash_set_vars).num_buckets;  \
        list_head_pptr < _end;                                                                          \
        ++list_head_pptr)

#define foreach_bucket_in_hashsetvariablesptr(hash_set_vars, list_head_pptr)                                \
    for(LinkedListVariablesNode **list_head_pptr = (hash_set_vars)->list_variables,                         \
                                **_end = (hash_set_vars)->list_variables + (hash_set_vars)->num_buckets;    \
        list_head_pptr < _end;                                                                              \
        ++list_head_pptr)
//////////////////////////////////////////////////////////////////////

// Default kind of Set for variables /////////////////////////////////
typedef HashSetVariables SetVariables;
typedef LinkedListVariablesNode NodeVariable;
#define create_set_variables create_hash_set_variables
#define create_set_variables_defsize create_hash_set_variables_defnumbuckets
#define create_set_variables_arena create_hash_set_variables_arena
#define free_set_variables free_hash_set_variables
#define clear_set_variables clear_hash_set_variables
#define clear_set_variables_arena clear_hash_set_variables_arena
#define insert_to_set_variables insert_to_hash_set_variables
#define unchecked_insert_to_set_variables unchecked_insert_to_hash_set_variables
#define insert_to_set_variables_arena insert_to_hash_set_variables_arena
#define unchecked_insert_to_set_variables_arena unchecked_insert_to_hash_set_variables_arena
#define is_in_set_variables is_in_hash_set_variables
#define get_node_in_set_variables get_node_in_hash_set_variables
#define print_set_variables print_hash_set_variables
#define foreach_in_setvariables foreach_in_hashsetvariables
#define foreach_in_setvariablesptr foreach_in_hashsetvariablesptr
//////////////////////////////////////////////////////////////////////

#endif // SET_VARIABLES_H