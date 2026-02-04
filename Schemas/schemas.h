#ifndef SCHEMAS_H
#define SCHEMAS_H

#include <stdbool.h>
#include <stddef.h>
#include "set_variables.h"
#include "set_dependencies.h"

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
typedef union Schema {
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
} Schema, *SchemaPtr; // NOTE: pointer type useful for arraylists of pointers to Schemas

// TODO: make a typedef for SetSchema-s as ArrayLists of Schemas (not SchemaPtrs...)

void init_variable_schema(Schema *schema, Variable v);
void init_general_schema(Schema *schema, unsigned arity);
unsigned schema_size(Schema *s);

// NOTE: for arraylist of pointers to Schemas
bool equal_schemas(Schema *s1, Schema *s2);
void print_schema(Schema *s);

// TODO: declare the public functions that are going to be used in the main.c module.
bool common_set_schema_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies);


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