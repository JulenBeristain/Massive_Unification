#ifndef SCHEMAS_H
#define SCHEMAS_H

#include <stdbool.h>
#include "Schemas/set.h"

/**
 * The structure that represents a Schema. It is inductively defined. It can be:
 * 
 * 1) A variable
 * 2) An ordered set of subschemas, including the empty set (<>).
 * 
 * Instead of using a tagged-union, the unsigned which identifies the variable is
 * used to identify the general case 2).
 * 
 * In this version, we only use 16B of memory for each Schema, just the space of
 * two pointers (in x64 modern architectures) the subschemas are continuously 
 * allocated in memory.
 */
typedef struct Schema Schema;
struct Schema
{
    unsigned v;         // v > 0, variable | v = 0, general case
    unsigned arity;     // if isVariable, ignore. Else, number of sub-schemas (could be 0 for <>)
    // NOTE: see if an extra indirection (**subschemas) is necessary/helpful.
    Schema *subschemas; // if isVariable, ignore. Else if arity = 0, ignore. Else, the sub-schemas in a compact array
};

bool isVariable(Schema schema);
bool isEmpty(Schema schema);
void initSchema(Schema *schema, unsigned arity);

// NOTE: we are going to parse some file to term structs like this? Or are we going to directly parse terms into
//  Schemas? The latter is strange, we should have the terms we are working with somehow in memory...
typedef struct Term Term;
struct Term
{
    unsigned v;             // v > 0, variable | v = 0, function (either constant or with subterms)
    unsigned mainSymbol;
    unsigned arity;     
    Term *subterms;
};

Schema *schemaFromTermAndVariables(Term term, Set setVariables);

#endif //SCHEMAS_H