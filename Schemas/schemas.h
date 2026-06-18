#ifndef SCHEMAS_H
#define SCHEMAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "set_variables.h"
#include "arena.h"
#include "arraylist.h"
#include "../structures.h"

/**
 * FUTURE WORK (TODO(YA-REVIEW)):
 * Obtention of column index mapping
 * Optimize management of schemas (if bad time measurements...)
 * Optimize AND SIMPLIFY further the core of the unification with matrices (and parallelize it)
 * Postprocess results to normalize the mappings...
 * Management of exception blocks...
 * 
 * Manage the inductive terms; i.e., implement flatenning matrix extension in C
 * Implement boolean operations between matrices in C
 * Repropose the code removing a dimension from the matrix...
 */

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// TYPE DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * The structure that represents a Schema. It is inductively defined. It can be:
 * 
 * 1) A variable
 * 2) An ordered set of subschemas, including the empty set (<>).
 * 
 */
// OPT: using an Arena for Schemas only, we could use a uint32 instead of a pointer for subschemas to use a position instead of a pointer (although more pointer arithmetic cost when accesing...)
// OPT: another approach would be to use a simple struct where all the data is introduced (forgetting about the SchemaType)
//  and use v to discern if it is a variable or not (if equal to 0, general schema).
typedef enum { VARIABLE_SCHEMA, GENERAL_SCHEMA } SchemaType;
typedef union Schema Schema, *SchemaPtr;
union Schema {
    struct {
        SchemaType type;
        Variable v;
        uint32_t size_; // NOTE: initialized to 0. Valid values start at 1. Computed just before the value is going to be used.
        uint32_t depth_; // NOTE: initialized to 0. Valid values start at 1. Computed just before the value is going to be used.
        void *_;
    };

    struct {
        SchemaType __;
        unsigned arity;
        uint32_t ___;
        uint32_t ____;
        Schema *subschemas;
    };
};

// Macros to traverse general schemas //////////////////////////////////////////////////////////////////////////////////////
#define foreach_in_schema(schema, sub) \
    for(Schema *sub = (schema).subschemas, *_end = (schema).subschemas + (schema).arity; sub < _end; ++sub)
#define foreach_in_schemaptr(schemaptr, sub) \
    for(Schema *sub = (schemaptr)->subschemas, *_end = (schemaptr)->subschemas + (schemaptr)->arity; sub < _end; ++sub)

#define foreach_in_schemas(schema_short, schema_long, sub1, sub2) \
    for(Schema *sub1 = (schema_short).subschemas, *sub2 = (schema_long).subschemas, \
               *_end = (schema_short).subschemas + (schema_short).arity; \
        sub1 < _end; \
        ++sub1, ++sub2)
#define foreach_in_schemaptrs(schema_short, schema_long, sub1, sub2) \
    for(Schema *sub1 = (schema_short)->subschemas, *sub2 = (schema_long)->subschemas, \
               *_end = (schema_short)->subschemas + (schema_short)->arity; \
        sub1 < _end; \
        ++sub1, ++sub2)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Schema iterator /////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: we will use the already implemented ArrayListSchema to be the base of the Stack that will be the Schema iterator.
// NOTE: we could also use an ArrayList of pointers to Schemas or a simple linked list...
// TODO(YA): maybe is more interesting to have an ArrayList of pointers to Schema, so size calculation don't have a chance to
//  be wasted on temporal copies.
typedef struct SchemaIteratorNode SchemaIteratorNode;
struct SchemaIteratorNode {
    Schema schema;
    unsigned next_child;
};

DECLARE_ARRAYLIST_TYPE(SchemaIteratorNode)
DEFINE_ARRAYLIST_FREE(SchemaIteratorNode, schema_iterator_node)

typedef struct SchemaIterator SchemaIterator;
struct SchemaIterator {
    ArrayListSchemaIteratorNode stack;
    bool first_next;
};

SchemaIterator create_schema_iterator(Schema schema);
SchemaIterator create_schema_iterator_arena(Schema schema, Arena *arena);
static inline void free_schema_iterator(SchemaIterator iterator){ free_array_list_schema_iterator_node(iterator.stack); }
bool schema_iterator_next(SchemaIterator *iterator, Schema *next);
void schema_iterator_skip(SchemaIterator *iterator);

// End Schema iterator /////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_ARRAYLIST_TYPE(Schema)
typedef struct SetSchema SetSchema;
struct SetSchema {
    ArrayListSchema list;
    uint32_t size_;         // NOTE: sum of the sizes of the schemas. Init to 0. Valid values start at 1 (no empty SetSchemas).
};                          // NOTE: no depth in SetSchema because we create iterators of Schemas

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct DependencyPair DependencyPair, *DependencyPairPtr;
struct DependencyPair {
    Variable v;
    ArrayListSchema schemas;
};

int i = sizeof(DependencyPair);

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

static inline void init_variable_schema(Schema* s, Variable v){
    s->type = VARIABLE_SCHEMA;
    s->size_ = 0;
    s->depth_ = 0;
    s->v = v;
}
static inline void init_general_schema_arena(Schema* s, unsigned arity, Arena* arena){
    s->type = GENERAL_SCHEMA;
    s->size_ = 0;
    s->depth_ = 0;
    s->arity = arity;
    s->subschemas = allocate(arena, arity * sizeof(*(s->subschemas)));
}
// NOTE: size and depth set to 1, already the correct value.
static inline Schema empty_schema(){ 
    // NOTE: unfortunately designated initializer don't behave with unions as with structs :(
    Schema empty;
    empty.type = GENERAL_SCHEMA;
    empty.size_ = 1;
    empty.depth_ = 1;
    empty.arity = 0;
    empty.subschemas = NULL;
    return empty;
}
static inline bool is_empty(Schema s) { return s.type == GENERAL_SCHEMA && s.arity == 0; }

unsigned schema_size_rec(Schema s);
unsigned schema_depth_rec(Schema s);
unsigned schema_size(Schema *s);
unsigned schema_depth(Schema *s);
unsigned set_schema_list_size_rec(ArrayListSchema set_schema);
unsigned set_schema_list_size(ArrayListSchema set_schema);
static inline unsigned set_schema_size_rec(SetSchema set_schema){
    return set_schema_list_size_rec(set_schema.list);
}
static inline unsigned set_schema_size(SetSchema *set_schema){
    if (set_schema->size_ == 0) {
        set_schema->size_ = set_schema_list_size(set_schema->list);
    }
    return set_schema->size_;
}

bool equal_schemas(Schema s1, Schema s2);
bool equal_set_schemas(ArrayListSchema set_schema1, ArrayListSchema set_schema2);
bool equivalent_schemas(Schema s1, Schema s2, Variable *mapping);
bool equivalent_set_schemas(ArrayListSchema set_schema1, ArrayListSchema set_schema2, Variable *mapping);

bool schema_contains_variables(Schema schema);
bool set_schema_contains_variables(ArrayListSchema set_schema);
void variables_in_schema(Schema schema, SetVariables *vars);
void variables_in_set_schema(ArrayListSchema set_schema, SetVariables *vars);
Variable min_v_in_schema(Schema schema);
Variable min_v_in_set_schema(ArrayListSchema set_schema);
Variable max_v_in_schema(Schema schema);
Variable max_v_in_set_schema(ArrayListSchema set_schema);
void increment_variables_in_schema(Schema *schema, Variable increment);
void increment_variables_in_set_schema(ArrayListSchema set_schema, Variable increment);
void decrement_variables_in_schema(Schema* schema, Variable decrement);
void decrement_variables_in_set_schema(ArrayListSchema set_schema, Variable decrement);

void substitute_arena(Schema original, Variable v, Schema substitution, Schema *result, Arena *arena);
void substitute_vars_arena(Schema original, SetVariables vars, Schema substitution, Schema *result, Arena *arena);

ArrayListSchema read_set_schema(char *line, Arena *arena);

DECLARE_ARRAYLIST_CREATE_ARENA(Schema, schema)
DECLARE_ARRAYLIST_ADD_ARENA(Schema, schema)
DECLARE_ARRAYLIST_FIND(Schema, schema)
DEFINE_ARRAYLIST_CONTAINS(Schema, schema)
DECLARE_ARRAYLIST_ADD_NO_REPEATED_ARENA(Schema, schema)
DECLARE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(Schema, schema)
DECLARE_ARRAYLIST_REMOVE_INDEX(Schema, schema)
unsigned find_equivalent_in_array_list_schema(ArrayListSchema list, Schema elem, Variable *mapping);
static inline bool contains_equivalent_array_list_schema(ArrayListSchema list, Schema elem, Variable *mapping){
    return list.size != find_equivalent_in_array_list_schema(list, elem, mapping);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool equal_set_dependencies(ArrayListDependencyPair dependencies1, ArrayListDependencyPair dependencies2);
bool equivalent_set_dependencies(ArrayListDependencyPair dependencies1, ArrayListDependencyPair dependencies2, Variable *mapping);
bool equivalent_set_dependencies_ignoring_empties(ArrayListDependencyPair dependencies1, ArrayListDependencyPair dependencies2, Variable* mapping);
void increment_variables_in_set_dependencies(ArrayListDependencyPair dependencies, Variable increment);
void decrement_variables_in_set_dependencies(ArrayListDependencyPair dependencies, Variable decrement);
ArrayListDependencyPair read_set_dependencies(FILE *stream, unsigned num_vars_with_dependencies, Arena *arena);
int read_set_schema_with_dependencies(FILE *stream, ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies, Arena *arena);

DECLARE_ARRAYLIST_CREATE_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_ADD_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_EXTEND_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_REMOVE_INDEX(DependencyPair, dependency_pair)
static inline unsigned find_v_in_array_list_dependency_pair(ArrayListDependencyPair list, Variable v){
    unsigned i = 0;
    foreach_in_arraylist(DependencyPair, iter, list){
        if (v == iter->v){
            return i;
        }
        ++i;
    }
    return list.size;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// JOINED EQUIVALENCE TESTS OF SCHEMAS AND DEPENDENCIES ///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned num_distinct_schema_variables(ArrayListSchema set_schema);

// NOTE: check schema and dependency equivalence encapsulating the calculation of variable mapping.
bool equivalent_set_schemas_and_dependencies(
    ArrayListSchema set_schema1, ArrayListSchema set_schema2,
    ArrayListDependencyPair dependencies1, ArrayListDependencyPair dependencies2);

bool equivalent_set_schemas_and_dependencies_ignoring_empties(
    ArrayListSchema set_schema1, ArrayListSchema set_schema2,
    ArrayListDependencyPair dependencies1, ArrayListDependencyPair dependencies2);


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMMON SET SCHEMAS /////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool common_set_schema_baseline(
    ArrayListSchema set_schema1, ArrayListDependencyPair dependencies1,
    ArrayListSchema set_schema2, ArrayListDependencyPair dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena);

bool common_set_schema_strict_baseline(
    ArrayListSchema set_schema1, ArrayListDependencyPair dependencies1,
    ArrayListSchema set_schema2, ArrayListDependencyPair dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PRINT //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum { PRINT_VISUALLY, PRINT_FILE_FORMAT } PrintingMode;
void print_schema(Schema s, PrintingMode mode);
void println_schema(Schema s, PrintingMode mode);
DECLARE_ARRAYLIST_PRINT_SEPARATORS_1(Schema, schema, PrintingMode, printing_mode)
void print_set_schema(ArrayListSchema set_schema, PrintingMode mode);
void println_set_schema(ArrayListSchema set_schema, PrintingMode mode);
void print_dependency_pair(DependencyPair pair, char opening_brace, char closing_brace, char *schema_separator, PrintingMode schema_mode);
void print_set_dependencies(ArrayListDependencyPair set_dependencies, PrintingMode mode);
void println_set_dependencies(ArrayListDependencyPair set_dependencies, PrintingMode mode);

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

void starting_column_indexes(ArrayListSchema fragment_set_schema, unsigned *starting_columns);
ArrayListSchema normalized_set_schema(ArrayListSchema set_schema, ArrayListDependencyPair dependencies, Arena* arena);
ArrayListCharPtr final_free_vars_ordering(ArrayListCharPtr free_vars1, ArrayListCharPtr free_vars2, Arena *arena);

bool common_set_schema_free_vars_baseline(
    ArrayListSchema set_schema1, ArrayListDependencyPair dependencies1, ArrayListUInt free_var_positions1,
    ArrayListSchema set_schema2, ArrayListDependencyPair dependencies2, ArrayListUInt free_var_positions2,
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena);

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
    ArrayListSchema *row_set_schema, SetDependencies *row_dependencies,
    Arena* arena);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END POSTPROCESSING OF MATRICES: OBTAIN SET SCHEMA OF ROW DENORMALIZING NORMALIZED FRAGMENT SET SCHEMA BASED ON THE RESULTING ROW  ///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DEEP COPYING  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Schema *schema_deepcopy(Schema *schema, Arena *arena);
ArrayListSchema set_schema_contents_deepcopy(ArrayListSchema *set_schema, Arena *arena);
ArrayListSchema *set_schema_deepcopy(ArrayListSchema *set_schema, Arena *arena);

DependencyPair *dependency_pair_deepcopy(DependencyPair *pair, Arena *arena);
SetDependencies *set_dependencies_deepcopy(SetDependencies *dependencies, Arena *arena);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END DEEP COPYING  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //SCHEMAS_H