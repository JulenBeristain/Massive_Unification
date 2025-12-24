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

// NOTE: useful for arraylists of pointers to Schemas
typedef Schema *SchemaPtr;

// NOTE: set-schemas can be represented simply as (generic) schemas! If not, we could define arraylist of schemas.
//  ArrayList of schemas could also be used for subschemas, instead of plain arrays (but then, it would be interesting
//  to optimize the space ocupied by arity with the size of the ArrayList).

// NOTE: we won't be using these kind of inductevely defined terms in C. We are going to work directly with
//  flattened matrices and their corresponding Schemas (which will be inductive).
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

// NOTE: the functions for extending matrices based on the calculated common-set-schemas and dependency sets are not
//  implemented because we work with already flattened matrices

void init_variable_schema(Schema *schema, Variable v);
void init_general_schema(Schema *schema, unsigned arity);
//void init_schema_from_term_and_variables(Schema *schema, Term *term, SetVariables set_variables);
//void init_schema_from_term(Schema *schema, Term *term);
unsigned schema_size(Schema *s);

//SetVariables repeated_vars_in_term(Term *t);

// NOTE: for arraylist of pointers to Schemas
bool equal_schemas(Schema *s1, Schema *s2);
void print_schema(Schema *s);

// TODO: declare the public functions that are going to be used in the main.c module.

/**
 * FUTURE WORK:
 * Finish the managing of schemas and obtention of column index mapping
 * Optimize further the core of the unification with matrices (and parallelize it)
 * Postprocess results to normalize the mappings...
 * Manage the inductive terms; i.e., implement flatenning matrix extension in C
 * Implement boolean operations between matrices in C
 */

#endif //SCHEMAS_H