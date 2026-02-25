#ifndef SCHEMAS_H
#define SCHEMAS_H

#include <stdbool.h>
#include <stddef.h>
#include "set_variables.h"
#include "arena.h"
#include "arraylist.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// TYPE DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
        size_t ___;        // NOTE: candidate for uint16/32_t if padding may arise
    };
};

//TODO: implement a macro to traverse general schemas...
//TODO: implement a macro for basic recursive structure?

DECLARE_ARRAYLIST_TYPE(Schema)

/// SET OF DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct DependencyPair DependencyPair, *DependencyPairPtr;
struct DependencyPair {
    Variable v;                 // NOTE: in this case, we could use a uint64_t to take advantage of the inevitable padding
    ArrayListSchema schemas;
};

DECLARE_ARRAYLIST_TYPE(DependencyPair)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END TYPE DEFINITIONS ////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FUNCTION DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: declare ONLY the public functions that are going to be used in the main.c module (reorganize the order of the functions in .h).
/// SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void init_variable_schema(Schema *schema, Variable v);
void init_general_schema(Schema *schema, unsigned arity);
void init_general_schema_arena(Schema *schema, unsigned arity, Arena *arena);
unsigned schema_size(Schema *s);

// NOTE: for arraylist of pointers to Schemas
bool equal_schemas(Schema s1, Schema s2);
bool equal_schemasptr(Schema *s1, Schema *s2);

bool common_set_schema_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena);

DECLARE_ARRAYLIST_ADD_ARENA(Schema, schema)
DECLARE_ARRAYLIST_ADD_NO_REPEATED_ARENA(Schema, schema)
DECLARE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(Schema, schema)
DECLARE_ARRAYLIST_CREATE_ARENA(Schema, schema)

/// SET OF DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_ARRAYLIST_ADD_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_EXTEND_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_CREATE_ARENA(DependencyPair, dependency_pair)

/// DEBUGGING PRINT ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum { PRINT_VISUALLY, PRINT_FILE_FORMAT } PrintingMode;
void print_schema(Schema *s, PrintingMode mode);
void print_set_schema(ArrayListSchema *set_schema, PrintingMode mode);
void print_set_dependencies(ArrayListDependencyPair *set_dependencies, PrintingMode mode);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END FUNCTION DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: make a typedef for SetSchema-s as ArrayLists of Schemas, and similar with the set of dependencies...

/**
 * FUTURE WORK:
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

#endif //SCHEMAS_H