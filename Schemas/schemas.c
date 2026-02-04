#include "schemas.h"
#include <stdlib.h>

/**
 * TODO: if Schemas are modified at some point, so they are not immutable, then the use of the
 *       size attribute must be reconsidered.
 */
void init_variable_schema(Schema *s, Variable v){
    s->type = VARIABLE_SCHEMA;
    s->size = 1;
    s->v = v;
}

/**
 * NOTE: size is simply set to 1. If nested subschemas, their own sizes have to be added to this
 *      value in creation.
 */
void init_general_schema(Schema *s, unsigned arity) {
    s->type = GENERAL_SCHEMA;
    s->size = 1;
    s->arity = arity;
    if (arity) {
        s->subschemas = malloc(arity * sizeof(*(s->subschemas)));
        if(s->subschemas == NULL){
            perror("malloc failed to allocate memory");
            exit(EXIT_FAILURE);
        }
    }
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

// TODO: implement a "disjoint" version that first checks if the pointers are equal (useful at all? In what
//  case would we call to this function with the same pointer as both arguments?). Note
//  that if we end up caching all the schemas so no Schema appears more than once in memory, this function  
//  can be simplified to a pointer comparison.
// NOTE: useful for arraylist of pointers to Schemas
bool equal_schemas(Schema *s1, Schema *s2){
    if(s1->type == VARIABLE_SCHEMA && s2->type == VARIABLE_SCHEMA && s1->v == s2->v) { return true; }
    if(s1->type == GENERAL_SCHEMA && s2->type == GENERAL_SCHEMA && s1->arity == s2->arity) {
        Schema *sub1 = s1->subschemas;
        Schema *sub2 = s2->subschemas;
        for(unsigned arity = s1->arity; arity; --arity, ++sub1, ++sub2){
            if(!equal_schemas(sub1, sub2)) { return false; }
        }
        return true;
    }
    return false;
}

// TODO: modify or add an extra version to mimic Javier's format when printing schemas
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

// TODO: implement read_schema | read_dependencies

// TODO: implement a version that receives two set of dependencies? Or is it enough to instantiate a single set of dependencies
//  and insert to it the dependencies of both set-schemas? Is this problematic? ...
// TODO: even here should we check for self dependencies to avoid unnecessary computation
// PRE: common should have been just defined or allocated, and the set of dependencies should be empty in the first call (not
//  in the recursive ones, since we are calcullating the union of all the dependencies).
// RES: further subschemas are allocated when needed. Shallow copies of the tail schemas are made in case of schemas with 
//  different arities. We insert all the dependencies found into the set of dependencies passed by reference.
void common_schema(Schema *s1, Schema *s2, Schema *common, SetDependencies *dependencies){
    // TODO: what if both schemas represent the same variable in the first case? Dependency v->v is strange... -->
    //  --> start by NOT inserting it in the set of dependencies...
    // TODO: in the set of dependencies we have pointers to Schemas as values. Be careful with dangling pointers (see where 
    //  the schemas are freed) --> Remove the schemas freeing only the set schema that contains them. Anywhere else, don't
    //  call free upon them (dangling pointer).
    // TODO: modify insert to set dependencies as needed to avoid having the same dependency more than once (respect set semantics)
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
        if(num_new_deps == -1){ return -1; }
        num_new_dependencies += num_new_deps;
    }
    return num_new_dependencies;
}

// TODO: common-set-schema combination function (just an iteration over schemas).
//  When dealing with M1 and M2 matrices, to have set-schemas with the same length, first is necessary to 
//  set an ordering of all free variables...


bool is_self_dependency(Variable v, Schema *schema){
    // TODO: if this is the first case; i.e., the Schema is the Variable v itself? If we need a special treatment for this
    //  we can implement a separate non-recursive interface function that handles this special case and then calls to this
    //  function. --> At first, we are ignoring v->v dependencies, so, as we know that we won't encounter that case, we don't
    //  need that special treatment.
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

// TODO: if dependencies of a variable change their data structure, modify this too...
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
    //       new dependencies would be added to schemas.array (so only node->schemas is modified!). That way, 
    //       we ensure that we only take into account the schemas that were initially stored in the list of 
    //       dependencies of the variable at the start of this loop iteration.
    // TODO: There are three options: the new schema is added to the current node->schemas; or it's added to a previous
    //  node_pre; or it's added to a posterior node_post. In the latter case we don't have a problem, the new
    //  schema will be taken into account in this loop. In the second case, the simplest option to take it into
    //  account is to keep looping while new dependencies are added; the strategy of avoiding the same gamma x gamma_
    //  operations can remove this necessity. Finally, we could take into account the newly added schemas to the 
    //  current node_dep, but we would need an extra (nested) loop in this function (for each new schema added
    //  to node_dep, we would need to compute the common schema between it and the schemas that were already in node_dep).
    for(size_t i = 0; i < schemas.size; ++i){
        for(size_t j = i + 1; j < schemas.size; ++j){
            int num_new_deps = common_schema_dependencies(schemas.array + i, schemas.array + j, dependencies);
            if(num_new_deps == -1){ return -1; }
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
            init_variable_schema(result, original->v);
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
        //  to avoid recalculating it in each successive call to theta_rule2... watch out modifications!
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
                if(is_self_dependency(v, gamma__)){ return -1; }
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
            //  derived dependencies in the common_schemas operation will always be the same... See next TODO... + set semantics is not
            //  only for efficiency. If it works like this, it will cycle infinitely!
            int num1 = theta_rule1(dependencies, node_dep);
            if(num1 == -1){ return -1; }
            num_new_deps1 += num1;

            //TODO: would be interesting to somehow avoid reaplying this rule to the same pair of gamma-gamma_, because the resulting
            //  gamma__ will always be the same... Maybe add an identifier to each schema and store a set of pairs of identifiers
            //  (or mapping between identifiers) for the theta_rule2 operation (to really avoid iterating and checks, an extra mapping from
            //  identifiers to pointers to Schemas would also be helpful, and that way, the ID would be implicit in that mapping...) 
            //  (but good to think other possibilities...).
            int num2 = theta_rule2(dependencies, node_dep);
            if(num2 == -1){ return -1; }
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

///////////////////////////////////////////////////////////////////
/// BASIC IMPLEMENTATION WITH SIMPLE STRUCTURES FOR A BASELINE
///////////////////////////////////////////////////////////////////

void insert_to_dependencies_baseline(ArrayListDependencyPair *dependencies, Variable v, Schema *s){
    for(unsigned i = 0; i < dependencies->size; ++i){
        DependencyPair *pair = dependencies->array + i;
        if(pair->v == v){
            add_not_repeated_to_array_list_schema(&pair->schemas, *s);
            return;
        }
    }

    //First dependency for v
    ArrayListSchema first_schemas_v = create_array_list_schema_defsize(); // Default capacity = 10 > 1
    add_to_array_list_schema(&first_schemas_v, *s);
    DependencyPair first_dependency_pair_v = { .v = v, .schemas = first_schemas_v };
    add_to_array_list_dependency_pair(dependencies, first_dependency_pair_v);
}

// destination gets the dependencies found in source that it didn't contain initially.
void union_of_dependencies_baseline(ArrayListDependencyPair *destination, ArrayListDependencyPair *source){
    for(unsigned i = 0; i < source->size; ++i){
        DependencyPair pair2 = source->array[i];
        Variable v = pair2.v;
        ArrayListSchema schemas2 = pair2.schemas;

        unsigned j = 0;
        for(; j < destination->size; ++j){
            if(destination->array[j].v == v){
                break;
            }
        }
        if(j == destination->size){ //nobreak = not found
            add_to_array_list_dependency_pair(destination, pair2); // NOTE: they share the schemas
        }
        else { //found
            extend_not_repeated_array_list_schema(&destination->array[j].schemas, &schemas2);
        }
    }
}

// Returns the number of dependencies inserted, or -1 if a self-dependency was arised.
int common_schema_baseline(Schema *s1, Schema *s2, Schema *common, ArrayListDependencyPair *dependencies){
    if(s1->type == VARIABLE_SCHEMA && s2->type == VARIABLE_SCHEMA && s1->v == s2->v){
        // Do not insert v->v dependency
        init_variable_schema(common, s1->v);
        return 0;
    }
    
    if (s1->type == VARIABLE_SCHEMA) {
        Variable v = s1->v;
        if(is_self_dependency(v, s2)){ return -1; }
        init_variable_schema(common, v);
        insert_to_dependencies_baseline(dependencies, v, s2);
        return 1;
    }
    
    if (s2->type == VARIABLE_SCHEMA) {
        Variable v = s2->v;
        if(is_self_dependency(v, s1)){ return -1; }
        init_variable_schema(common, v);
        insert_to_dependencies_baseline(dependencies, v, s1);
        return 1;
    }

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
    int total_inserted_dependencies = 0;
    size_t i = 0;
    for(; i < min_arity; ++i){
        int num_inserted_dependencies = common_schema_baseline(s1->subschemas + i, s2->subschemas + i, 
                                                                common->subschemas + i, dependencies);
        if(num_inserted_dependencies < 0){ return -1; }
        total_inserted_dependencies += num_inserted_dependencies;
        common->size += common->subschemas[i].size;
    }
    for(; i < max_arity; ++i){
        common->subschemas[i] = longest_schema->subschemas[i]; //NOTE: shallow copy...
        common->size += common->subschemas[i].size;
    }
    
    return total_inserted_dependencies;
}

int common_schema_dependencies_baseline(Schema *s1, Schema *s2, ArrayListDependencyPair *dependencies){
    if(s1->type == VARIABLE_SCHEMA && s2->type == VARIABLE_SCHEMA && s1->v == s2->v){
        // Do not insert v->v dependency
        return 0;
    }

    if (s1->type == VARIABLE_SCHEMA) {
        if(is_self_dependency(s1->v, s2)){ return -1; }
        insert_to_dependencies_baseline(dependencies, s1->v, s2);
        return 1;
    } 
    if (s2->type == VARIABLE_SCHEMA) {
        if(is_self_dependency(s2->v, s1)){ return -1; }
        insert_to_dependencies_baseline(dependencies, s2->v, s1);
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
        int num_new_deps = common_schema_dependencies_baseline(s1->subschemas + i, s2->subschemas + i, dependencies);
        if(num_new_deps == -1){ return -1; }
        num_new_dependencies += num_new_deps;
    }
    return num_new_dependencies;
}

/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 */
int theta_rule1_baseline(ArrayListDependencyPair *dependencies, DependencyPair *pair){
    int num_new_dependencies = 0;
    ArrayListSchema schemas = pair->schemas;
    // NOTE: since we are making a local copy of the schemas struct, schemas.size will remain the same even if 
    //       new dependencies would be added to schemas.array (so only node->schemas is modified!). That way, 
    //       we ensure that we only take into account the schemas that were initially stored in the list of 
    //       dependencies of the variable at the start of this loop iteration.
    for(size_t i = 0; i < schemas.size; ++i){
        for(size_t j = i + 1; j < schemas.size; ++j){
            int num_new_deps = common_schema_dependencies_baseline(schemas.array + i, schemas.array + j, dependencies);
            if(num_new_deps == -1){ return -1; }
            num_new_dependencies += num_new_deps;
        }
    }
    return num_new_dependencies;
}

/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 */
int theta_rule2_baseline(ArrayListDependencyPair *dependencies, DependencyPair *pair){
    int num_new_dependencies = 0;
    Variable v = pair->v;
    ArrayListSchema *schemas = &pair->schemas;
    //NOTE: important to take the pointer to the arraylist schemas, because it is modified by a direct call to
    //  add_to_array_list_schema later in this function.

    //NOTE: iteration based on indexes to prevent dangling pointers from resizing
    for(size_t i = 0; i < schemas->size; ++i){
        Schema *gamma = schemas->array + i;
        SetVariables ws = variables_in_schema(gamma);
        foreach_in_setvariables(ws, node_var){
            Variable w = node_var->v;

            ArrayListSchema *schemas_ = NULL;
            for(DependencyPair *it = dependencies->array, *end = dependencies->array + dependencies->size; it < end; ++it){
                if(it->v == w){
                    schemas_ = &it->schemas;
                    break;
                }
            }
            if(schemas_ == NULL){ continue; }

            foreach_in_arraylistptr(Schema, schema_, schemas_){
                Schema *gamma_ = schema_;
                Schema *gamma__ = substitute(gamma, w, gamma_); // TODO_YA: adapt the used helper functions to take an extra parameter for the Arena...
                if(is_self_dependency(v, gamma__)){ return -1; }
                int contained = add_not_repeated_to_array_list_schema(schemas, *gamma__);
                if(contained != CONTAINED){ ++num_new_dependencies; }
            }
        }
        free_set_variables(ws); // TODO_YA: if we are using an Arena even for sets of variables, this free would be unnecessary...
    }
    return num_new_dependencies;
}

/**
 * RETURNS: < 0 (-1) if it has halted because a self-dependency was found
 *          else, the number of new dependencies discovered
 */
int theta_operator_baseline(ArrayListDependencyPair *dependencies){
    int num_new_dependencies = 0;
    int num_new_deps1, num_new_deps2; // NOTE: we could have a single foreach accumulator, but this way we can have more information when debugging
    do {
        num_new_deps1 = num_new_deps2 = 0;
        //NOTE: rule1 is much simpler than rule2 as it only needs to access to the schemas a variable depends on, not other
        //  variables. For that reason, we first call to it to try to be more cache-locality-friendly.
        
        //NOTE: we iterate using indexes instead of pointers because when adding new dependencies the ArrayList of DependencyPairs
        //  can be resized (so the iterating pointers would be invalidated...)

        for(size_t i = 0; i < dependencies->size; ++i){
            DependencyPair *pair = dependencies->array + i;
            //TODO: would be interesting to somehow avoid reaplying this rule to the same pair of gamma-gamma_
            int num1 = theta_rule1_baseline(dependencies, pair);
            if(num1 == -1){ return -1; }
            num_new_deps1 += num1;

            //TODO: would be interesting to somehow avoid reaplying this rule to the same pair of gamma-gamma_
            int num2 = theta_rule2_baseline(dependencies, pair);
            if(num2 == -1){ return -1; }
            num_new_deps2 += num2;
        }
        num_new_dependencies += num_new_deps1 + num_new_deps2;
    } while(num_new_deps1 + num_new_deps2);
    return num_new_dependencies;
}

bool has_self_dependency_baseline(ArrayListSchema schemas, Variable v){
    foreach_in_arraylist(Schema, schema, schemas){
        if(is_self_dependency(v, schema)){
            return true;
        }
    }
    return false;
}

bool contains_self_dependency_baseline(ArrayListDependencyPair dependencies){
    foreach_in_arraylist(DependencyPair, pair, dependencies){
        if(has_self_dependency_baseline(pair->schemas, pair->v)){
            return true;
        }
    }
    return false;
}

bool common_set_schema_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies)
{
    if(set_schema1->size != set_schema2->size){ return false; }
    unsigned num_schemas = set_schema1->size; //num columns
    
    *common_set_schema = create_array_list_schema(num_schemas);
    ArrayListDependencyPair new_dependencies = create_array_list_dependency_pair_defsize(); //TODO: defsize is misleading, in reallity, it's default capacity
    
    for(unsigned i = 0; i < num_schemas; ++i){
        Schema *s1 = set_schema1->array + i;
        Schema *s2 = set_schema2->array + i;
        
        Schema *common_schema = common_set_schema + i;
        int num_new_dependencies = common_schema_baseline(s1, s2, common_schema, &new_dependencies);
        if(num_new_dependencies < 0){ return false; }
    }
    // common_set_schema calculated

    // Start calculating the final common_dependencies set as the union of the two inputs and new_dependencies

    *common_dependencies = create_array_list_dependency_pair(dependencies1->size + dependencies2->size + new_dependencies.size);
    // NOTE: capacity can be greater than final size (we can have dependencies of the same variable in several sets)
    
    extend_array_list_dependency_pair(common_dependencies, dependencies1); // NOTE: Only copy the header of the array, the elements are shared
    union_of_dependencies_baseline(common_dependencies, dependencies2);
    union_of_dependencies_baseline(common_dependencies, &new_dependencies);

    // Now, we must calculate the hidden dependencies, in case there is a self dependency
    int num_new_dependencies = theta_operator_baseline(common_dependencies);
    if(num_new_dependencies == -1){ return false; }
    assert(!contains_self_dependency_baseline(*common_dependencies));
    return true;
}

