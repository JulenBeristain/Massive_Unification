#ifndef SCHEMAS_H
#define SCHEMAS_H

#include <stdbool.h>
#include <stddef.h>
#include "set_variables.h"
#include "arena.h"
#include "arraylist.h"

// TODO: cyclic type dependency between schemas.h and set_dependencies.h. The simplest way to solve it was
// to bring the contents of set_dependencies here. See how we can avoid these kind of problems in the future.
//#include "set_dependencies.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// TYPE DEFINITIONS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The structure that represents a Schema. It is inductively defined. It can be:
 * 
 * 1) A variable
 * 2) An ordered set of subschemas, including the empty set (<>).
 * 
 */

//TODO: for "pure" schema management we know that we combine schemas with the same position from the input set-schemas.
//  When dealing with M1 and M2 csv files, the columns can refer to different free variables. Therefore, we would need to
//  store that extra information here too, to know which schema from M1 combine with which schema of M2. In that case, we
//  also need a "strategy" to flatten all free variables in a row (I guess its simply first all the free variables from
//  M1 in the same order as in M1, and then the variables from M2 that don't appear in M1 in the same relative order as 
//  in M2).
//typedef enum : uint8_t { VARIABLE, GENERAL } SchemaType; // TODO: for two types a Byte (even a bit) is enough. Because of padding, no effect
typedef enum { VARIABLE_SCHEMA, GENERAL_SCHEMA } SchemaType;
typedef union Schema Schema, *SchemaPtr;
union Schema {
    struct {
        SchemaType type;
        Variable v;
        void *__;
        size_t size;
    };

    struct {
        SchemaType _;
        unsigned arity;
        // TODO: see if an extra indirection (**subschemas) is necessary/helpful.
        Schema *subschemas;
        size_t ___;        // Note, candidate for uint16/32_t if padding may arise
    };
}; // NOTE: pointer type useful for arraylists of pointers to Schemas

/// ARRAYLIST SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct ArrayListSchema ArrayListSchema;
struct ArrayListSchema {
    uint32_t size;
    uint32_t capacity;
    Schema* array;
};

/// DEPENDENCY PAIR ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct DependencyPair DependencyPair, *DependencyPairPtr;
struct DependencyPair {
    Variable v;                 // NOTE: in this case, we could use a uint64_t to take advantage of the inevitable padding
    ArrayListSchema schemas;
};

/// ARRAYLIST DEPENDENCY PAIRS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct ArrayListDependencyPair ArrayListDependencyPair;
struct ArrayListDependencyPair {
    uint32_t size;
    uint32_t capacity;
    DependencyPair* array;
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END TYPE DEFINITIONS ////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FUNCTION DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void init_variable_schema(Schema *schema, Variable v);
void init_general_schema(Schema *schema, unsigned arity);
void init_general_schema_arena(Schema *schema, unsigned arity, Arena *arena);
unsigned schema_size(Schema *s);

void variables_in_schema_(Schema *schema, SetVariables *vars);
SetVariables variables_in_schema(Schema *schema);
SetVariables variables_in_set_schema(ArrayListSchema set_schema);

// NOTE: for arraylist of pointers to Schemas
bool equal_schemas(Schema *s1, Schema *s2);

// TODO: declare the public functions that are going to be used in the main.c module.
bool common_set_schema_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena);

/// ARRAYLIST SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int add_to_array_list_schema_arena(ArrayListSchema *list, Schema element, Arena *arena);

int add_not_repeated_to_array_list_schema_arena(ArrayListSchema *list, Schema element, Arena *arena);

//int extend_not_repeated_array_list_schema(ArrayListSchema *list_to_extend, ArrayListSchema *list);
int extend_not_repeated_array_list_schema_arena(ArrayListSchema *list_to_extend, ArrayListSchema *list, Arena *arena);

ArrayListSchema create_array_list_schema_arena(uint32_t capacity, Arena *arena);

/// DEPENDENCY PAIRS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//void print_dependency_pair(DependencyPair pair);


/// ARRAYLIST DEPENDENCY PAIRS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int add_to_array_list_dependency_pair_arena(ArrayListDependencyPair *list, DependencyPair element, Arena *arena);


int extend_array_list_dependency_pair_arena(ArrayListDependencyPair *list_to_extend, ArrayListDependencyPair *list, Arena *arena);


ArrayListDependencyPair create_array_list_dependency_pair_arena(uint32_t capacity, Arena *arena);

int remove_index_from_array_list_dependency_pair(ArrayListDependencyPair* list, uint32_t index);

/// DEBUGGING PRINT ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum { PRINT_VISUALLY, PRINT_FILE_FORMAT } PrintingMode;
void print_schema(Schema *s, PrintingMode mode);
void print_set_schema(ArrayListSchema *set_schema, PrintingMode mode);
void print_set_dependencies(ArrayListDependencyPair *set_dependencies, PrintingMode mode);
void print_dependency_pair(DependencyPair *pair, char opening_brace, char closing_brace, const char *schema_separator, PrintingMode schema_mode);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END FUNCTION DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// TODO: make a typedef for SetSchema-s as ArrayLists of Schemas (not SchemaPtrs...)

/**
 * FUTURE WORK:
 * Finish the managing of schemas and obtention of column index mapping
 * Optimize AND SIMPLIFY further the core of the unification with matrices (and parallelize it)
 * Postprocess results to normalize the mappings...
 * Management of exception blocks...
 * 
 * Manage the inductive terms; i.e., implement flatenning matrix extension in C
 * Implement boolean operations between matrices in C
 * Repropose the code removing a dimension from the matrix...
 */

#endif //SCHEMAS_H