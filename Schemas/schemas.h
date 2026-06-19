#ifndef SCHEMAS_H
#define SCHEMAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "set_variables.h"
#include "arena.h"
#include "arraylist.h"
#include "../structures.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// TYPE DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO(OPT): another approach would be to use a simple struct where all the data is introduced (forgetting about the SchemaType)
//  and use v to discern if it is a variable or not (if equal to 0, general schema). This optimization sinergizes with 
//  extra indirection, uniqueness of leaves, Pool Allocation and RefCountingGC of Schema nodes.
typedef enum { VARIABLE_SCHEMA, GENERAL_SCHEMA } SchemaType;
typedef struct Schema Schema, *SchemaPtr;
struct Schema {
    union {
        unsigned v;
        unsigned arity;
    };
    SchemaType type;

    unsigned size_;     // NOTE: initialized to 0. Valid values start at 1. Computed just before the value is going to be used.
    unsigned depth_;    // NOTE: initialized to 0. Valid values start at 1. Computed just before the value is going to be used.

    Schema *subschemas;
};

// Macros to traverse general schemas //////////////////////////////////////////////////////////////////////////////////////
#define foreach_in_schema(schema, sub) \
    for(Schema *sub = (schema).subschemas, *_end = (schema).subschemas + (schema).arity; sub < _end; ++sub)

#define foreach_in_schema_after_first(schema, sub) \
    for(Schema *sub = (schema).subschemas + 1, *_end = (schema).subschemas + (schema).arity; sub < _end; ++sub)

#define foreach_in_schemas(schema_short, schema_long, sub1, sub2) \
    for(Schema *sub1 = (schema_short).subschemas, *sub2 = (schema_long).subschemas, \
               *_end = (schema_short).subschemas + (schema_short).arity; \
        sub1 < _end; \
        ++sub1, ++sub2)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Schema iterator /////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    Schema *schema;
    unsigned next_child;
} SchemaIteratorNode;

DECLARE_ARRAYLIST_TYPE(SchemaIteratorNode)
DEFINE_ARRAYLIST_FREE(SchemaIteratorNode, schema_iterator_node)

typedef struct {
    ArrayListSchemaIteratorNode stack;
    bool first_next;
} SchemaIterator;

SchemaIterator create_schema_iterator(Schema *schema);
SchemaIterator create_schema_iterator_arena(Schema *schema, Arena *arena);
static inline void free_schema_iterator(SchemaIterator iterator){ free_array_list_schema_iterator_node(iterator.stack); }
bool schema_iterator_next(SchemaIterator *iterator, Schema *next);
void schema_iterator_skip(SchemaIterator *iterator);

// End Schema iterator /////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: we reuse the type of Schema to define a SetSchema. Even in that case, it is a good idea to have set_schema version
//  of operations like size, because the size of the SetSchema is equal to its underground schema's size minus 1.
typedef Schema SetSchema;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_ARRAYLIST_TYPE(SchemaPtr)

typedef struct {
    Variable v;
    ArrayListSchemaPtr schemas;
} DependencyPair, *DependencyPairPtr;

DECLARE_ARRAYLIST_TYPE(DependencyPair)
typedef ArrayListDependencyPair SetDependencies;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END TYPE DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FUNCTION DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define VARIABLE_SCHEMA_LITERAL(v) (Schema){ .type = VARIABLE_SCHEMA, .v = (v)}
static inline void init_variable_schema(Schema *s, Variable v){
    *s = VARIABLE_SCHEMA_LITERAL(v);
}

#define GENERAL_SCHEMA_LITERAL(arity_value, arena) \
    (Schema){ .type = GENERAL_SCHEMA, .arity = (arity_value), .subschemas = allocate((arena), (arity_value) * sizeof(Schema))}
static inline void init_general_schema_arena(Schema* s, unsigned arity, Arena* arena){
    *s = GENERAL_SCHEMA_LITERAL(arity, arena);
}

#define EMPTY_SCHEMA_LITERAL (Schema){ .type = GENERAL_SCHEMA, .size_ = 1, .depth_ = 1 }
static inline Schema empty_schema(){ 
    return EMPTY_SCHEMA_LITERAL;
}
static inline bool is_empty(Schema s) { return s.type == GENERAL_SCHEMA && s.arity == 0; }

unsigned schema_size_rec(Schema s);
unsigned schema_depth_rec(Schema s);
unsigned schema_size(Schema *s);
unsigned schema_depth(Schema *s);
static inline unsigned set_schema_size_rec(SetSchema set_schema){
    return schema_size_rec(set_schema) - 1;
}
static inline unsigned set_schema_size(SetSchema *set_schema){
    return schema_size(set_schema) - 1;
}

bool equal_schemas(Schema s1, Schema s2);
static inline bool equal_set_schemas(SetSchema set_schema1, SetSchema set_schema2) {
    return equal_schemas(set_schema1, set_schema2);
}
bool equivalent_schemas(Schema s1, Schema s2, Variable *mapping);
static inline bool equivalent_set_schemas(SetSchema set_schema1, SetSchema set_schema2, Variable *mapping){
    return equivalent_schemas(set_schema1, set_schema2, mapping);
}

bool schema_contains_variables(Schema schema);
static inline bool set_schema_contains_variables(SetSchema set_schema){
    return schema_contains_variables(set_schema);
}
void variables_in_schema(Schema schema, SetVariables *vars);
static inline void variables_in_set_schema(SetSchema set_schema, SetVariables *vars){
    variables_in_schema(set_schema, vars);
}
Variable min_v_in_schema(Schema schema);
static inline Variable min_v_in_set_schema(SetSchema set_schema){
    return min_v_in_schema(set_schema);
}
Variable max_v_in_schema(Schema schema);
static inline Variable max_v_in_set_schema(SetSchema set_schema){
    return max_v_in_schema(set_schema);
}
unsigned num_distinct_schema_variables(Schema schema);
static inline unsigned num_distinct_set_schema_variables(SetSchema set_schema) {
    return num_distinct_schema_variables(set_schema);
}
void increment_variables_in_schema(Schema *schema, Variable increment);
static inline void increment_variables_in_set_schema(SetSchema *set_schema, Variable increment){
    increment_variables_in_schema(set_schema, increment);
}
void decrement_variables_in_schema(Schema* schema, Variable decrement);
static inline void decrement_variables_in_set_schema(SetSchema *set_schema, Variable decrement){
    decrement_variables_in_schema(set_schema, decrement);
}
void normalize_schema_variables_arena(Schema *schema, Arena *arena);
static inline void normalize_set_schema_variables_arena(SetSchema *set_schema, Arena *arena){
    normalize_schema_variables_arena(set_schema, arena);
}
void normalize_schema_variables(Schema *schema);
static inline void normalize_set_schema_variables(SetSchema *set_schema){
    normalize_schema_variables(set_schema);
}

void substitute_arena(Schema original, Variable v, Schema substitution, Schema *result, Arena *arena);
void substitute_vars_arena(Schema original, SetVariables vars, Schema substitution, Schema *result, Arena *arena);

SetSchema read_set_schema(char *line, Arena *arena);

DECLARE_ARRAYLIST_CREATE_ARENA(SchemaPtr, schema_ptr)
DECLARE_ARRAYLIST_ADD_ARENA(SchemaPtr, schema_ptr)
DECLARE_ARRAYLIST_FIND(SchemaPtr, schema_ptr)
DEFINE_ARRAYLIST_CONTAINS(SchemaPtr, schema_ptr)
DECLARE_ARRAYLIST_ADD_NO_REPEATED_ARENA(SchemaPtr, schema_ptr)
DECLARE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(SchemaPtr, schema_ptr)
DECLARE_ARRAYLIST_REMOVE_INDEX(SchemaPtr, schema_ptr)
unsigned find_equivalent_in_array_list_schema_ptr(ArrayListSchemaPtr list, Schema *elem, Variable *mapping);
static inline bool contains_equivalent_array_list_schema_ptr(ArrayListSchemaPtr list, Schema *elem, Variable *mapping){
    return list.size != find_equivalent_in_array_list_schema_ptr(list, elem, mapping);
}
DECLARE_ARRAYLIST_PRINTLN(SchemaPtr, schema_ptr)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool equal_set_dependencies(SetDependencies dependencies1, SetDependencies dependencies2);
bool equivalent_set_dependencies(SetDependencies dependencies1, SetDependencies dependencies2, Variable *mapping);
bool equivalent_set_dependencies_ignoring_empties(SetDependencies dependencies1, SetDependencies dependencies2, Variable* mapping);
void increment_variables_in_set_dependencies(SetDependencies dependencies, Variable increment);
void decrement_variables_in_set_dependencies(SetDependencies dependencies, Variable decrement);
ArrayListDependencyPair read_set_dependencies(FILE *stream, unsigned num_vars_with_dependencies, Arena *arena);
int read_set_schema_with_dependencies(FILE *stream, SetSchema *set_schema, SetDependencies *dependencies, Arena *arena);

DECLARE_ARRAYLIST_CREATE_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_ADD_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_EXTEND_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_REMOVE_INDEX(DependencyPair, dependency_pair)
unsigned find_v_in_array_list_dependency_pair(ArrayListDependencyPair list, Variable v);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// JOINED EQUIVALENCE TESTS OF SCHEMAS AND DEPENDENCIES ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: check schema and dependency equivalence encapsulating the calculation of variable mapping.
bool equivalent_set_schemas_and_dependencies(
    SetSchema set_schema1, SetSchema set_schema2,
    SetDependencies dependencies1, SetDependencies dependencies2);

bool equivalent_set_schemas_and_dependencies_ignoring_empties(
    SetSchema set_schema1, SetSchema set_schema2,
    SetDependencies dependencies1, SetDependencies dependencies2);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMMON SET SCHEMAS /////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool common_set_schema_baseline(
    SetSchema set_schema1, SetDependencies dependencies1,
    SetSchema set_schema2, SetDependencies dependencies2, 
    SetSchema *common_set_schema, SetDependencies *common_dependencies,
    Arena *arena);

bool common_set_schema_strict_baseline(
    SetSchema set_schema1, SetDependencies dependencies1,
    SetSchema set_schema2, SetDependencies dependencies2, 
    SetSchema *common_set_schema, SetDependencies *common_dependencies,
    Arena *arena);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PRINT //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum { PRINT_VISUALLY, PRINT_FILE_FORMAT } PrintingMode;
void print_schema(Schema s, PrintingMode mode);
void println_schema(Schema s, PrintingMode mode);
void printdef_schema(Schema s);
void print_set_schema(SetSchema set_schema, PrintingMode mode);
void println_set_schema(SetSchema set_schema, PrintingMode mode);
void printdef_set_schema(SetSchema set_schema);
void print_dependency_pair(DependencyPair pair, char opening_brace, char closing_brace, char *schema_separator);
void print_set_dependencies(SetDependencies set_dependencies, PrintingMode mode);
void println_set_dependencies(SetDependencies set_dependencies, PrintingMode mode);
void printdef_set_dependencies(SetDependencies dependencies);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END FUNCTION DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GET COLUMN INDEX MAPPING ////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef char *CharPtr;
DECLARE_ARRAYLIST_TYPE(CharPtr)
DECLARE_ARRAYLIST_CREATE(CharPtr, char_ptr)
DEFINE_ARRAYLIST_FREE(CharPtr, char_ptr)
DECLARE_ARRAYLIST_CREATE_ARENA(CharPtr, char_ptr)
DECLARE_ARRAYLIST_ADD_ARENA(CharPtr, char_ptr)
DECLARE_ARRAYLIST_EXTEND_ARENA(CharPtr, char_ptr)
static inline bool equal_strings(char *str1, char *str2){ return strcmp(str1, str2) == 0; }
DECLARE_ARRAYLIST_EQUAL(CharPtr, char_ptr)
DECLARE_ARRAYLIST_FIND(CharPtr, char_ptr)
DEFINE_ARRAYLIST_CONTAINS(CharPtr, char_ptr)
DECLARE_ARRAYLIST_ADD_NO_REPEATED_ARENA(CharPtr, char_ptr)
DECLARE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(CharPtr, char_ptr)
void insertion_sort_arraylist_char_ptr(ArrayListCharPtr list);
static inline void print_string(char *s) { printf("%s", s); }
DECLARE_ARRAYLIST_PRINTLN(CharPtr, char_ptr)

static inline void print_uint(unsigned i) { printf("%u", i); }
typedef unsigned UInt;
DECLARE_ARRAYLIST_TYPE(UInt)
DECLARE_ARRAYLIST_CREATE(UInt, uint)
DECLARE_ARRAYLIST_CREATE_ARENA(UInt, uint)
DEFINE_ARRAYLIST_FREE(UInt, uint)
DEFINE_ARRAYLIST_CLEAR(UInt, uint)
DECLARE_ARRAYLIST_PRINT_SEPARATORS(UInt, uint)
DECLARE_ARRAYLIST_PRINT(UInt, uint)
DECLARE_ARRAYLIST_PRINTLN(UInt, uint)

void starting_column_indexes(SetSchema fragment_set_schema, unsigned *starting_columns);
SetSchema normalized_set_schema(SetSchema set_schema, SetDependencies dependencies, Arena* arena);
ArrayListCharPtr final_free_vars_ordering(ArrayListCharPtr free_vars1, ArrayListCharPtr free_vars2, Arena *arena);

bool common_set_schema_free_vars_baseline(
    SetSchema set_schema1, SetDependencies dependencies1, ArrayListUInt free_var_positions1,
    SetSchema set_schema2, SetDependencies dependencies2, ArrayListUInt free_var_positions2,
    SetSchema *common_set_schema, SetDependencies *common_dependencies,
    Arena *arena);

//TODO(YA): see if with pop_to is possible to simplify row_vars and schema_iterators arena to a single one 
void mapping_column_indexes_side(
    SetSchema normalized_set_schema, ArrayListUInt free_var_positions, 
    SetSchema normalized_common_set_schema,
    unsigned *starting_col_indices, int *row,
    unsigned *mapping_side,
    Arena *row_vars_arena,
    Arena *schema_iterator_arena);

void mapping_column_indexes_side_lineal(
    SetSchema normalized_set_schema, ArrayListUInt free_var_positions, 
    SetSchema normalized_common_set_schema,
    unsigned *starting_col_indices, int *row,
    unsigned *mapping_side,
    Arena *schema_iterator_arena);

void extend_row(
    SetSchema normalized_set_schema, ArrayListUInt free_var_positions, 
    SetSchema normalized_common_set_schema,
    unsigned *starting_col_indices, int *row,
    int *extended_row,
    Arena *row_vars_arena,
    Arena *schema_iterator_arena);

void extend_row_lineal(
    SetSchema normalized_set_schema, ArrayListUInt free_var_positions, 
    SetSchema normalized_common_set_schema,
    unsigned *starting_col_indices, int *row,
    int *extended_row,
    Arena *schema_iterator_arena);

static inline int is_original_var_first_appearence(int row_value) {
    return row_value == 0;
}
static inline int is_extending_var_first_appearence(int row_value) {
    return row_value == 1;
}
static inline int is_var_first_appearence(int row_value) {
    return is_extending_var_first_appearence(row_value) || is_original_var_first_appearence(row_value);
}
static inline int is_var_repeated_appearence(int row_value) {
    return row_value < 0;
}
static inline int is_variable(int row_value) {
    return row_value < 2;
}
static inline int is_symbol(int row_value) {
    return !is_variable(row_value);
}
enum { 
    FIRST_ORIGINAL_VAR_APPEARENCE = 0,
    FIRST_EXTENDING_VAR_APPEARENCE = 1,
    FIRST_FUNCTION_SYMBOL_POSITIVE = 2,
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END GET COLUMN INDEX MAPPING ////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// POSTPROCESSING OF MATRICES: OBTAIN SET SCHEMA OF ROW DENORMALIZING NORMALIZED FRAGMENT SET SCHEMA BASED ON THE RESULTING ROW  ///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void denormalized_set_schema(
    SetSchema normalized_set_schema, int *row, 
    SetSchema *row_set_schema, SetDependencies *row_dependencies,
    Arena* arena);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END POSTPROCESSING OF MATRICES: OBTAIN SET SCHEMA OF ROW DENORMALIZING NORMALIZED FRAGMENT SET SCHEMA BASED ON THE RESULTING ROW  ///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DEEP COPYING  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Schema schema_deepcopy_except_root(Schema *schema, Arena *arena);
static inline SetSchema set_schema_deepcopy_except_root(SetSchema *set_schema, Arena *arena) {
    return schema_deepcopy_except_root(set_schema, arena);
}
Schema *schema_deepcopy(Schema *schema, Arena *arena);
static inline SetSchema *set_schema_deepcopy(SetSchema *set_schema, Arena *arena){
    return schema_deepcopy(set_schema, arena);
}

ArrayListSchemaPtr deepcopy_arraylist_schema_ptr(ArrayListSchemaPtr list, Arena *arena);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END DEEP COPYING  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //SCHEMAS_H