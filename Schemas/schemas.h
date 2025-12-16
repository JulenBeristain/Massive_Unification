#ifndef SCHEMAS_H
#define SCHEMAS_H

#include <stdbool.h>
#include <stddef.h>
#include "set_variables.h"

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
 * 
 */
typedef struct Schema Schema;
struct Schema
{
    unsigned v;         // v > 0, variable | v = 0, general case
    unsigned arity;     // if isVariable, ignore. Else, number of sub-schemas (could be 0 for <>)
    // TODO: see if an extra indirection (**subschemas) is necessary/helpful.
    Schema *subschemas; // if isVariable, ignore. Else if arity = 0, ignore. Else, the sub-schemas in a compact array
    size_t size;        // Note, candidate for uint32_t if padding may arise | 
                            //TODO: might be interesting to define a Schema wrapper if we are only interested in the 
                            //      sizes of the outermost Schemas, so not every sub-schema stores its size.
};

// TODO: we are going to parse some file to term structs like this? Or are we going to directly parse terms into
//  Schemas? The latter is strange, we should have the terms we are working with somehow in memory...
/**
 * Very similar to Schemas, but with the extra main symbol.
 */
typedef struct Term Term;
struct Term
{
    unsigned mainSymbol;
    unsigned size;          // See Schemas comment...
    unsigned v;             // v > 0, variable | v = 0, function (either constant or with subterms)
    unsigned arity;
    Term *subterms;
};

static inline bool is_schema_variable(Schema *s) { return s->v > 0; }
static inline bool is_schema_empty(Schema *s) { return s->v == 0 && s->arity == 0; }
static inline bool is_term_variable(Term *t) { return t->v > 0; }
static inline bool is_term_empty(Term *t) { return t->v == 0 && t->arity == 0; }

void init_nested_schema(Schema *schema, unsigned arity);
Schema *schema_from_term_and_variables(Term *term, SetVariables set_variables);
unsigned schema_size(Schema *s);

SetVariables repeated_vars_in_term(Term *t);

#endif //SCHEMAS_H