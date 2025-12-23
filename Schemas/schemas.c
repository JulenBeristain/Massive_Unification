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

// NOTE: this initialization functions won't be used because we are not going to work with inductive
//  terms in C.
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
    for(Schema *end = subschema + s->arity; subschema < end; ++subschema){
        total_size += schema_size(subschema);
    }
    return total_size;
}


/**
 * NOTE: pointers to repeated_vars and vars are perfectly valid candidates for
 *       the term struct.
 * NOTE: these functions won't be used because we are not working with inductive terms in C.
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

// NOTE: useful for arraylist of pointers to Schemas
bool equal_schemas(Schema *s1, Schema *s2){
    if(s1->type == VARIABLE_SCHEMA && s2->type == VARIABLE_SCHEMA && s1->v == s2->v) { return true; }
    if(s1->type == GENERAL_SCHEMA && s2->type == GENERAL_SCHEMA && s1->arity == s2->arity) {
        unsigned arity = s1->arity;
        if(arity == 0) { return true; }
        Schema *sub1 = s1->subschemas;
        Schema *sub2 = s2->subschemas;
        for(; arity; --arity, ++sub1, ++sub2){
            if(!equal_schemas(sub1, sub2)) { return false; }
        }
        return true;
    }
    return false;
}

// NOTE: useful for arraylist of pointers to Schemas
void print_schema(Schema *s){
    if(s->type == VARIABLE_SCHEMA){
        printf("V%d", s->v);
    } else {
        printf("<");
        if(s->arity) { print_schema(s->subschemas); }
        Schema *sub = s->subschemas + 1;
        Schema *end = s->subschemas + s->arity;
        for(; sub < end; ++sub){
            printf(", ");
            print_schema(sub);
        }
        printf(">");
    }
}


// TODO:
//  common-schemas combination function; common-set-schema combination function; theta operator over sets of dependency;
//  check self dependency in set of dependencies (halt theta as soon as one self-dependency is found) --> check isFiniteSchema;


// TODO: init (set-)schema from file; (after understanding the Prolog source code perfectly)