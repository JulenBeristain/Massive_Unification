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
        if(s->subschemas == NULL){
            perror("malloc failed to allocate memory");
            exit(EXIT_FAILURE);
        }
    }
    s->size = 1;
}

// NOTE: this initialization functions won't be used because we are not going to work with inductive
//  terms in C.
void init_schema_from_term_and_variables(Schema *schema, Term *term, SetVariables set_variables){
    if(term->type == VARIABLE_TERM){
        if(lookup_set_variables(set_variables, term->v)){
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
    SetVariables vars = create_set_variables_defsize();
    SetVariables repeated_vars = create_set_variables_defsize();
    repeated_vars_in_term_(t, &vars, &repeated_vars);
    return repeated_vars;
}
static void repeated_vars_in_term_(Term *t, SetVariables *vars, SetVariables *repeated_vars){
    if(t->type == VARIABLE_TERM){
        SetInsertReturnCode code = insert_to_set_variables(vars, t->v);
        if(code == SET_INSERT_ALREADY_CONTAINED) {
            insert_to_set_variables(repeated_vars, t->v);
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

// PRE: common should have been just defined or allocated, and the set of dependencies should be empty in the first call (not
//  in the recursive ones, since we are calcullating the union of all the dependencies).
// RES: further subschemas are allocated when needed. Shallow copies of the tail schemas are made in case of schemas with 
//  different arities. We insert all the dependencies found into the set of dependencies passed by reference.
void common_schema(Schema *s1, Schema *s2, Schema *common, SetDependencies *dependencies){
    // TODO: what if both schemas represent the same variable in the first case? Dependency v->v is strange...
    // TODO: in the set of dependencies we have pointers to Schemas as values. Be careful with dangling pointers (see where 
    //  the schemas are freed).
    if (s1->type == VARIABLE_SCHEMA) {
        init_variable_schema(common, s1->v);
        insert_to_set_dependencies(dependencies, s1->v, s2);
    } else if (s2->type == VARIABLE_SCHEMA) {
        init_variable_schema(common, s2->v);
        insert_to_set_dependencies(dependencies, s2->v, s1);
    } else {
        size_t min_arity, max_arity;
        Schema *longest_schema;
        if (s1->arity < s2->arity) {
            min_arity = s1->arity;
            max_arity = s2->arity;
            longest_schema = s2;
        } else {
            min_arity = s2->arity;
            max_arity = s1->arity;
            longest_schema = s1;
        }

        init_general_schema(common, max_arity); // .size = 1
        size_t i = 0;
        for(; i < min_arity; ++i){
            common_schema(s1->subschemas + i, s2->subschemas + i, common->subschemas + i, dependencies);
            common->size += common->subschemas[i].size;
        }
        for(; i < max_arity; ++i){
            // TODO: appropriate to have all subschemas in the same array (and not have an extra indirection)?
            //  Here we are doing shallow copies. Deep copies needed?...
            //  Garbage collection needed? We will see in what circumstances we should free the schemas...
            common->subschemas[i] = longest_schema->subschemas[i]; //NOTE: shallow copy...
            common->size += common->subschemas[i].size;
        }
    }
}

/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 * 
 * NOTE: special version for the theta operator that only calculates dependencies.
 */
int common_schema_dependencies(Schema *s1, Schema *s2, SetDependencies *dependencies){
    // TODO: what if both schemas represent the same variable in the first case? Dependency v->v is strange...
    // TODO: in the set of dependencies we have pointers to Schemas as values. Be careful with dangling pointers (see where 
    //  the schemas are freed).
    if (s1->type == VARIABLE_SCHEMA) {
        if(is_self_dependency(s1->v, s2)){ return -1; }
        insert_to_set_dependencies(dependencies, s1->v, s2);
        return 1;
    } 
    if (s2->type == VARIABLE_SCHEMA) {
        if(is_self_dependency(s2->v, s1)){ return -1; }
        insert_to_set_dependencies(dependencies, s2->v, s1);
        return 1;
    }

    int num_new_dependencies = 0;

    size_t min_arity, max_arity;
    Schema *longest_schema;
    if (s1->arity < s2->arity) {
        min_arity = s1->arity;
        max_arity = s2->arity;
        longest_schema = s2;
    } else {
        min_arity = s2->arity;
        max_arity = s1->arity;
        longest_schema = s1;
    }

    size_t i = 0;
    for(; i < min_arity; ++i){
        int num_new_deps = common_schema_dependencies(s1->subschemas + i, s2->subschemas + i, dependencies);
        if(num_new_deps == -1){
            return -1;
        }
        num_new_dependencies += num_new_deps;
    }
    return num_new_dependencies;
}

// NOTE: common-set-schema combination function, if we are just using Schemas to represent common-schemas, is
//  simply the common_schema function itself!


bool is_self_dependency(Variable v, Schema *schema){
    // TODO: if this is the first case; i.e., the Schema is the Variable v itself? If we need a special treatment for this
    //  we can implement a separate non-recursive interface function that handles this special case and then calls to this
    //  function.
    if(schema->type == VARIABLE_SCHEMA){
        return v == schema->v;
    }
    
    Schema *subschema = schema->subschemas, *end = schema->subschemas + schema->arity;
    for(; subschema < end; ++subschema){
        if(is_self_dependency(v, subschema)){
            return true;
        }
    }
    return false;
}

bool has_self_dependency(ArrayListSchemaPtr schemas, Variable v){
    foreach_in_arraylist(SchemaPtr, schema, schemas){
        if(is_self_dependency(v, *schema)){
            return true;
        }
    }
    return false;
}

bool contains_self_dependency(SetDependencies dependencies){
    foreach_pair_in_hashmap_dependencies(dependencies, pair){
        if(has_self_dependency(pair->schemas, pair->v)){
            return true;
        }
    }
    return false;
}

/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 */
int theta_rule1(SetDependencies *dependencies, HashMapDependenciesNode *node_dep){
    int num_new_dependencies = 0;
    ArrayListSchemaPtr schemas = node_dep->schemas;
    // NOTE: since we are making a local copy of the schemas struct, schemas.size will remain the same even if 
    //       new dependencies would be added to schemas.array (so only node->schemas is modified!).
    for(size_t i = 0; i < schemas.size; ++i){
        for(size_t j = i + 1; j < schemas.size; ++j){
            int num_new_deps = common_schema_dependencies(schemas.array + i, schemas.array + j, dependencies);
            // NOTE: since we stop as soon as a self-dependency is found, we are not modifying the same
            //       schemas arraylist we are iterating over. Even in that case, since schemas.size remains the same
            //       (see previous note), we would only apply this rule to the schemas that were initially in the arraylist.
        
            if(num_new_deps == -1){
                return -1;
            }
            num_new_dependencies += num_new_deps;
        }
    }
    return num_new_dependencies;
}

SetVariables variables_in_schema(Schema *schema){
    SetVariables vars = create_set_variables_defsize();
    variables_in_schema_(schema, &vars);
    return vars;
}
void variables_in_schema_(Schema *schema, SetVariables *vars){
    if(schema->type == VARIABLE_SCHEMA){
        insert_to_set_variables(vars, schema->v);
    } else {
        Schema *subschema = schema->subschemas, *end = schema->subschemas + schema->arity;
        for(; subschema < end; ++subschema){
            variables_in_schema_(subschema, vars);
        }
    }
}

// NOTE: we are going to allocate new space for the new Schema that results from the substitution.
//  We copy the structure of original, but changing occurrences of v with substitution.
// NOTE: we are making shallow copies of substitution, not deep copies.
//  TODO: see if this is correct for our program, if deepcopies are necessary instead or even if 
//  we would benefit from having arrays of pointers to subschemas to simply take the pointer, 
//  avoiding even shallow copies. It would be interesting to investigate how functional/logic/
//  declarative programming languages handle this situation where we want to obtain a new object
//  modifying just some recursive subparts of it (if possible avoiding copying the part that original
//  and result keep in common by means of the possibilities of immutable data structures...) --> persistent data structures.
Schema *substitute(Schema *original, Variable v, Schema *substitution){
    Schema *result = malloc(sizeof(*result));
    if(result == NULL){
        perror("malloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    substitute_(original, v, substitution, result);
    return result;
}

void substitute_(Schema *original, Variable v, Schema *substitution, Schema *result){
    if(original->type == VARIABLE_SCHEMA){
        if(original->v == v){
            *result = *substitution;
        } else {
            init_variable_schema(result, v);
        }
    } else {
        init_general_schema(result, original->arity); // NOTE: ->subschemas allocated; ->size = 1
        for(size_t i = 0; i < original->arity; ++i){
            substitute_(original->subschemas + i, v, substitution, result->subschemas + i);
            result->size += result->subschemas[i].size;
        }
    }
}

/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 */
int theta_rule2(SetDependencies *dependencies, HashMapDependenciesNode *node_dep){
    int num_new_dependencies = 0;
    Variable v = node_dep->v;
    ArrayListSchemaPtr *schemas = &node_dep->schemas;
    //NOTE: important to take the pointer to the arraylist schemas, because it is modified by a direct call to
    //  add_to_array_list_schema_ptr later in this function.

    // TODO: we are iterating over an arraylist that we are modifying later. Not taking into account the schemas that we
    //  are inserting now is not that problematic (it could be harmful for the efficiency), but if a resizing happens, the
    //  iterating pointers will be invalid. A possible fix is to do the iteration based on indexes instead of pointers
    //  (that way we would consider even the newly added schemas). But if in the future we are changing the arraylist for
    //  a set, we will need to rethink this for sets.
    foreach_in_arraylistptr(SchemaPtr, schema, schemas){
        Schema *gamma = *schema;
        //TODO: two options: 
        //  1) calculate all variables in gamma and do a simple for-each (no early return, but conceptually clearer
        //     and possible parallelization...) --> This is my choice
        //  2) implement an auxiliar recursive function to iterate over all variables in a schema and perform
        //     the rest of the operations (possible early return)
        //TODO: would be interesting to consider having a pointer to the set of variables in the Schema struct itself,
        //  to avoid recalculating it in each successive call to theta_rule2...
        SetVariables ws = variables_in_schema(gamma);
        foreach_in_setvariables(ws, node_var){
            Variable w = node_var->v;
            ArrayListSchemaPtr *schemas_ = lookup_set_dependencies(*dependencies, w);
            if(schemas_ == NULL){ continue; }
            foreach_in_arraylistptr(SchemaPtr, schema_, schemas_){
                Schema *gamma_ = *schema_;
                Schema *gamma__ = substitute(gamma, w, gamma_);
                //TODO: if we use ArrayListSchemaPtr-s, we can be inserting the same schema several times... No SET semantics...
                //      implement generic (with macros, as arraylist) sets (to instantiate SetSchemaPtr) (O(1), but more complex implementation, which hash function, etc.)
                //  or  implement add_to_array_list_no_repeated (at least for ArrayListSchemaPtr) (simpler implementation, but O(n))
                //  or  implement this functions in a way that we guarantee we are always calculating the dependencies based on different 
                //          starting schemas (don't know if it's possible or if even in that way the uniqueness of the dependencies is guaranteed,
                //          I wouldn't say so because you can take two simple schemas, wrap them in <> so we have different schemas, and recursively
                //          we would arrive to the same base case...)
                //  Related to this: num_new_dependencies has to be updated accordingly. It doesn't have to be incremented each time, but only when
                //  a NEW dependency is added.
                if(is_self_dependency(v, gamma__)){
                    return -1;
                }
                ++num_new_dependencies;
                add_to_array_list_schema_ptr(schemas, gamma__);
            }
        }
        free_set_variables(ws);
    }
    return num_new_dependencies;
}


/**
 * RETURNS: < 0 (-1) if it has halted because a self-dependency was found
 *          else, the number of new dependencies discovered
 */
int theta_operator(SetDependencies *dependencies){
    int num_new_dependencies = 0;
    int num_new_deps1, num_new_deps2; // NOTE: we could have a single foreach accumulator, but this way we can have more information when debugging
    do {
        num_new_deps1 = num_new_deps2 = 0;
        //NOTE: rule1 is much simpler than rule2 as it only needs to access to the schemas a variable depends on, not other
        //  variables. For that reason, we first call to it to try to be more cache-locality-friendly.
        // TODO: we are iterating over dependencies, a hash map, that we are modifying. In the first for loop over the buckets,
        //  if a resizing happens due to an insertion of a new dependency, the pointers that we use to iterate are invalidated!
        //  An easy fix is to add a flag to the struct of HashMaps to turn on/off the capability of resizing, so we would turn
        //  it off before the foreach and on after it (when turning on, it should check the load factor...). Another possibility
        //  would be to do the foreach in terms of indexes instead of pointers (?) Even so, the semantics aren't clear, because
        //  new nodes could be inserted before or after the current node...
        foreach_pair_in_hashmap_dependencies_ptr(dependencies, node_dep){
            //TODO: would be interesting to somehow avoid reaplying this rule to the same pair of gamma-gamma_, because the resulting
            //  derived dependencies in the common_schemas operation will always be the same... See next TODO...
            int num1 = theta_rule1(dependencies, node_dep);
            if(num1 == -1){
                return -1;
            }
            num_new_deps1 += num1;

            //TODO: would be interesting to somehow avoid reaplying this rule to the same pair of gamma-gamma_, because the resulting
            //  gamma__ will always be the same... Maybe add an identifier to each schema and store a set of pairs of identifiers
            //  (or mapping between identifiers) for the theta_rule2 operation (to really avoid iterating and checks, an extra mapping from
            //  identifiers to pointers to Schemas would also be helpful, and that way, the ID would be implicit in that mapping...) 
            //  (but good to think other possibilities...).
            int num2 = theta_rule2(dependencies, node_dep);
            if(num2 == -1){
                return -1;
            }
            num_new_deps2 += num2;
        }
        num_new_dependencies += num_new_deps1 + num_new_deps2;
    } while(num_new_deps1 + num_new_deps2);
    return num_new_dependencies;
}

/**
 * This function should be called just after a common schema was computed. It receives the set of dependencies calculated in the call to
 * common_schema.
 * 
 * NOTE: dependencies is updated with extra dependencies found by the two rules of the theta operator.
 * 
 * TODO: conceptually, it makes sense to define a CommonSchema type, for example, with two pointers (one to the schema, and the other to 
 *  the set of dependencies). Another option is to add a pointer to the set of dependencies to all schemas (to the struct itself). Finally,
 *  we can simply manage both separately.
 */
bool is_finite_schema(SetDependencies *dependencies){
    if(contains_self_dependency(*dependencies)){
        return false;
    }
    
    if(theta_operator(dependencies) == -1){
        return false;
    }

    return true;
}

// TODO: init (set-)schema from file; (after understanding the Prolog source code perfectly)