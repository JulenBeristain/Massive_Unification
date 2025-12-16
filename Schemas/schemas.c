#include "schemas.h"
#include <stdlib.h>

/**
 * NOTE: if Schemas are modified at some point, so they are not immutable, then the use of the
 *       size attribute must be reconsidered.
 */

/**
 * This is called in the general case, not variable or empty.
 * 
 * Therefore, size is initialized to 1 already, then the sizes of the subschemas
 * are to be added.
 */
void init_nested_schema(Schema *s, unsigned arity) {
    s->v = 0;
    s->arity = arity;
    if (arity) {
        s->subschemas = malloc(arity * sizeof(*(s->subschemas)));
    }
    s->size = 1;
}

Schema *schema_from_term_and_variables(Term *term, SetVariables set_variables){
    Schema *schema = malloc(sizeof(*schema));
    if(schema == NULL){
        perror("malloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    if(is_variable(term)){
        if(lookup_set(set_variables, term->v)){
            schema->v = term->v;
        } else {
            schema->v = 0;
            schema->arity = 0;
        }
        schema->size = 1;
    } else {
        init_nested_schema(schema, term->arity);
        Schema *subschema = schema->subschemas;
        Term *subterm = term->subterms;
        for(uint32_t i = term->arity; i; --i, ++subschema, ++subterm){
            *subschema = *schema_from_term_and_variables(term, set_variables);
            schema->size += subschema->size;
        }
    }
    return schema;
}

/**
 * Function that computes the schema size, without looking at the precached sizes.
 */
unsigned schema_size(Schema *s){
    if(is_schema_variable(s) || is_schema_empty(s)){ return 1; }
    unsigned total_size = 1;
    Schema *subschema = s->subschemas;
    for(unsigned i = s->arity; i; --i, ++subschema){
        total_size += schema_size(subschema);
    }
    return total_size;
}


/**
 * NOTE: pointers to repeated_vars and vars are perfectly valid candidates for
 *       the term struct.
 */
SetVariables repeated_vars_in_term(Term *t){
    SetVariables vars = create_set_defsize();
    SetVariables repeated_vars = create_set_defsize();
    repeated_vars_in_term_(t, &vars, &repeated_vars);
    return repeated_vars;
}

static void repeated_vars_in_term_(Term *t, SetVariables *vars, SetVariables *repeated_vars){
    if(is_term_variable(t)){
        if(lookup_set(*vars, t->v)){
            insert_to_set(repeated_vars, t->v);
        } else {
            insert_to_set(vars, t->v);
        }
    }

    if(is_term_empty(t)){

    }

    //General case
}
