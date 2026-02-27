#ifndef SCHEMAS_H
#define SCHEMAS_H

#include <stdbool.h>
#include <stddef.h>
#include "set_variables.h"
#include "arena.h"
#include "arraylist.h"

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
// NOTE: size candidate for uint16/32_t if padding may arise. Furthermore, we can check if the attribute is used at some point.
//  If not, we can simply delete it...
// NOTE: another approach would be to use a simple struct where all the data is instroduced (forgetting about the SchemaType)
//  and use v to discern if it is a variable or not (if equal to 0, general schema).
//typedef enum : uint8_t { VARIABLE, GENERAL } SchemaType; // NOTE: for two types a Byte (even a bit) is enough. Because of padding, no effect
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
        Schema *subschemas;
        size_t ___;
    };
};

// Macros to traverse general schemas //////////////////////////////////////////////////////////////////////////////////////
#define foreach_in_schema(schema, sub) \
    for(Schema *sub = (schema).subschemas, *_end = (schema).subschemas + (schema).arity; sub < _end; ++sub)
#define foreach_in_schemaptr(schemaptr, sub) \
    for(Schema *sub = (schemaptr)->subschemas, *_end = (schemaptr)->subschemas + (schemaptr)->arity; sub < _end; ++sub)
//TODO_YA: see if we can write macros for when traversing two schemas simultaneously (even with different arities, identiyfing which is shorter...)
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


//TODO: for "pure" set-schema management we know that we combine schemas with the same position from the input set-schemas.
//  When dealing with M1 and M2 csv files, the columns can refer to different free variables. Therefore, we would need to
//  store that extra information per schema, to know which schema from M1 combines with which schema of M2. In that case, we
//  also need a strategy to decide the order of all free variables in M3 (simply first all the free variables from
//  M1 in the same order as in M1, and then the variables from M2 that don't appear in M1 in the same relative order as 
//  in M2; OR first the common columns in the same relative order, giving more priority to M1, and the same for the not commons).
//  Therefore, we should have an arraylist of FreeVar-Schema pairs, not just Schemas... Another possibility is to have two 
//  arraylists/arrays (as the sizes have to be equal...) separated...
DECLARE_ARRAYLIST_TYPE(Schema)
typedef ArrayListSchema SetSchema;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct DependencyPair DependencyPair, *DependencyPairPtr;
struct DependencyPair {
    Variable v;                 // NOTE: in this case, we could use a uint64_t to take advantage of the inevitable padding
    ArrayListSchema schemas;
};

DECLARE_ARRAYLIST_TYPE(DependencyPair)
typedef ArrayListDependencyPair SetDependencies;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END TYPE DEFINITIONS ////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FUNCTION DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO_YA: declare ONLY the public functions that are going to be used in the main.c module (reorganize the order of the functions in .h).

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SCHEMAS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void init_variable_schema(Schema *schema, Variable v);
void init_general_schema(Schema *schema, unsigned arity);
void init_general_schema_arena(Schema *schema, unsigned arity, Arena *arena);
unsigned schema_size(Schema *s);

void variables_in_schema_(Schema *schema, SetVariables *vars);
SetVariables variables_in_schema(Schema *schema);
SetVariables variables_in_set_schema(ArrayListSchema set_schema);

bool equal_schemas(Schema s1, Schema s2);
bool equal_schemasptr(Schema *s1, Schema *s2);

bool common_set_schema_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena);

bool common_set_schema_strict_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena);
  
DECLARE_ARRAYLIST_ADD_ARENA(Schema, schema)
DECLARE_ARRAYLIST_ADD_NO_REPEATED_ARENA(Schema, schema)
DECLARE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(Schema, schema)
DECLARE_ARRAYLIST_CREATE_ARENA(Schema, schema)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_ARRAYLIST_ADD_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_EXTEND_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_CREATE_ARENA(DependencyPair, dependency_pair)
DECLARE_ARRAYLIST_REMOVE_INDEX(DependencyPair, dependency_pair)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PRINT //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum { PRINT_VISUALLY, PRINT_FILE_FORMAT } PrintingMode;
void print_schema(Schema *s, PrintingMode mode);
void print_set_schema(ArrayListSchema *set_schema, PrintingMode mode);
void print_set_dependencies(ArrayListDependencyPair *set_dependencies, PrintingMode mode);
void print_dependency_pair(DependencyPair *pair, char opening_brace, char closing_brace, const char *schema_separator, PrintingMode schema_mode);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END FUNCTION DECLARATIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif //SCHEMAS_H