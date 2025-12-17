#include "schemas.h"
#include <stdlib.h>

/**
 * TODO: if Schemas are modified at some point, so they are not immutable, then the use of the
 *       size attribute must be reconsidered.
 */

void init_variable_schema(Schema *s, Variable v){
    s->type = VARIABLE_SCHEMA;
    s->v = v;
    s->size = 1;
}

/**
 * NOTE: size is simply set to 1. If nested subschemas, their own sizes have to be added to this
 *      value in creation.
 */
void init_general_schema(Schema *s, unsigned arity) {
    s->type = GENERAL_SCHEMA;
    s->arity = arity;
    if (arity) {
        s->subschemas = malloc(arity * sizeof(*(s->subschemas)));
    }
    s->size = 1;
}

void init_schema_from_term_and_variables(Schema *schema, Term *term, SetVariables set_variables){
    if(term->type == VARIABLE_TERM){
        if(lookup_set(set_variables, term->v)){
            init_variable_schema(schema, term->v);
        } else {
            init_general_schema(schema, 0);
        }
    } else {
        init_general_schema(schema, term->arity);
        Schema *subschema = schema->subschemas;
        Term *subterm = term->subterms;
        for(uint32_t i = term->arity; i; --i, ++subschema, ++subterm){
            init_schema_from_term_and_variables(subschema, subterm, set_variables);
            schema->size += subschema->size;
        }
    }
}

void init_schema_from_term(Schema *schema, Term *term) {
    SetVariables rep_t = repeated_vars_in_term(term);
    init_schema_from_term_and_variables(schema, term, rep_t);
}

/**
 * Function that computes the schema size, without looking at the precached sizes.
 */
unsigned schema_size(Schema *s){
    if(s->type == VARIABLE_SCHEMA || s->arity == 0){ return 1; }
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
    if(t->type == VARIABLE_TERM){
        SetInsertReturnCode code = insert_to_set(vars, t->v);
        if(code == SET_INSERT_ALREADY_CONTAINED) {
            insert_to_set(repeated_vars, t->v);
        }
    } else {
        Term *subterm = t->subterms;
        for(unsigned i = t->arity; i; --i, ++subterm){
            repeated_vars_in_term_(subterm, vars, repeated_vars);
        }
    }
}
