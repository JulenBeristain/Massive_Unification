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
 */

//typedef enum : uint8_t { VARIABLE, GENERAL } SchemaType; // TODO: for two types a Byte (even a bit) is enough. Because of padding, no effect
typedef enum { VARIABLE_SCHEMA, GENERAL_SCHEMA } SchemaType;
typedef union Schema Schema;
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
        size_t size;        // Note, candidate for uint16/32_t if padding may arise | 
                            //TODO: might be interesting to define a Schema wrapper if we are only interested in the 
                            //      sizes of the outermost Schemas, so not every sub-schema stores its size.
    };
};

// TODO: we are going to parse some file to term structs like this? Or are we going to directly parse terms into
//  Schemas? The latter is strange, we should have the terms we are working with somehow in memory...
/**
 * Very similar to Schemas, but with the extra main symbol.
 */
typedef enum { VARIABLE_TERM, GENERAL_TERM } TermType; // TODO: one byte...
typedef union Term Term;
union Term {
    struct {
        TermType type;
        unsigned mainSymbol;
        Variable v;
    };

    struct {
        TermType _;
        unsigned __;
        unsigned arity;
        unsigned size;  //TODO: might be interesting to define a Term wrapper if we are only interested in the 
                        //      sizes of the outermost Terms, so not every sub-schema stores its size.
        // TODO: see if an extra indirection (**subterms) is necessary/helpful.
        Term *subterms;            
    };
};

void init_variable_schema(Schema *schema, Variable v);
void init_general_schema(Schema *schema, unsigned arity);
void init_schema_from_term_and_variables(Schema *schema, Term *term, SetVariables set_variables);
void init_schema_from_term(Schema *schema, Term *term);
unsigned schema_size(Schema *s);

SetVariables repeated_vars_in_term(Term *t);

#endif //SCHEMAS_H