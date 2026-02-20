#include "schemas.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define MIN(A, B) ((A) < (B) ? (A) : (B))

extern bool global_print_debugging;


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST SCHEMAS ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//TODO: ensure new print version is called here
//DEFINE_ARRAYLIST_CREATION(Schema, schema)
//DEFINE_ARRAYLIST_ADDITION(Schema, schema, Schema)
//DEFINE_ARRAYLIST_GETTING(Schema, schema, Schema)
//DEFINE_PRINT_ARRAYLIST_WITH_POINTER(Schema, schema, print_schema)

//DEFINE_ARRAYLIST_CREATION_ARENA(Schema, schema)
ArrayListSchema create_array_list_schema_arena(uint32_t capacity, Arena* arena) {
    ArrayListSchema list;
    list.size = 0;
    list.capacity = capacity;
    list.array = allocate(arena, capacity * sizeof(*list.array));
    return list;
}

//DEFINE_ARRAYLIST_ADDITION_ARENA(Schema, schema, Schema)
int add_to_array_list_schema_arena(ArrayListSchema* list, Schema element, Arena* arena)
{
    int code = NOT_RESIZED;
    if (list->size == list->capacity) {
        if (list->capacity) {
            list->capacity *= 2;
        } else {
            list->capacity = 1;
        }
        // TODO: this is the logic of resizing!!!
        Schema *new_array = allocate(arena, list->capacity * sizeof(*list->array));
        Schema *source = list->array;
        Schema *dest = new_array;
        Schema *end = dest + list->size;
        for(; dest < end; ++dest, ++source){
            *dest = *source;
        }
        list->array = new_array;
        code = RESIZED;
    }
    list->array[list->size] = element;
    ++list->size;
    return code;
}

int add_not_repeated_to_array_list_schema_arena(ArrayListSchema *list, Schema element, Arena *arena){
    // TODO: refactor this to a contains function --> Then, this function could be unnecessary
    foreach_in_arraylistptr(Schema, itptr, list){
        if(equal_schemas(itptr, &element)){
            return CONTAINED;
        }
    }
    return add_to_array_list_schema_arena(list, element, arena);
}


//DEFINE_ARRAYLIST_EXTENSION(Schema, schema, Schema) // TODO: the first resizing logic shouldn't work like the not repeated version

int extend_not_repeated_array_list_schema_arena(ArrayListSchema *list_to_extend, ArrayListSchema *list, Arena *arena){
    int code = NOT_RESIZED; 

    foreach_in_arraylistptr(Schema, elemptr, list){
        add_not_repeated_to_array_list_schema_arena(list_to_extend, *elemptr, arena);
    }

    return code; 
}

//DEFINE_ARRAYLIST_REMOVAL_INDEX(Schema, schema, Schema)
int remove_index_from_array_list_schema(ArrayListSchema* list, uint32_t index)
{
    if (index >= list->size) {
        return OUT_OF_BOUNDS;
    }
    --list->size;
    Schema* removed_ptr = list->array + index;
    uint32_t num_shifted_elements = list->size - index;
    memmove(removed_ptr, removed_ptr + 1, num_shifted_elements * sizeof(*list->array));
    return SUCCESSFUL_REMOVAL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST SCHEMAS ///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
/* void init_general_schema(Schema *s, unsigned arity) {
    s->type = GENERAL_SCHEMA;
    s->size = 1;
    s->arity = arity;
    if (arity) {
        s->subschemas = malloc(arity * sizeof(*(s->subschemas)));
        if (s->subschemas == NULL) {
            perror("malloc failed to allocate memory");
            exit(EXIT_FAILURE);
        }
    }
    else { s->subschemas = NULL; }
} */

/* void free_schema(Schema *s){
    if (s->type == VARIABLE_SCHEMA || s->arity == 0) {
        return;
    }

    unsigned arity = s->arity;
    for (unsigned i = 0; i < arity; ++i) {
        free_schema(s->subschemas + i);
    }
    free(s->subschemas);
} */

/**
 * NOTE: size is simply set to 1. If nested subschemas, their own sizes have to be added to this
 *      value in creation.
 */
void init_general_schema_arena(Schema *s, unsigned arity, Arena *arena) {
    s->type = GENERAL_SCHEMA;
    s->size = 1;
    s->arity = arity;
    s->subschemas = allocate(arena, arity * sizeof(*(s->subschemas)));  
}

/**
 * Function that computes the schema size, without looking at the precached sizes.
 */
unsigned schema_size(Schema *s){
    if (s->type == VARIABLE_SCHEMA || s->arity == 0) {
        return 1;
    }

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
SetVariables variables_in_schema(Schema *schema){
    SetVariables vars = create_set_variables_defsize();
    variables_in_schema_(schema, &vars);
    return vars;
}

SetVariables variables_in_set_schema(ArrayListSchema set_schema){
    SetVariables vars = create_set_variables_defsize();
    foreach_in_arraylist(Schema, s, set_schema){
        variables_in_schema_(s, &vars);
    }
    return vars;
}

///////////////////////////////////////////////////////////////////
/// BASIC IMPLEMENTATION WITH SIMPLE STRUCTURES FOR A BASELINE
///////////////////////////////////////////////////////////////////

// PRE: there must not be any repeated Schemas in the list of Dependences of each variable! (set semantics)
unsigned number_of_dependencies(ArrayListDependencyPair *dependencies){
    unsigned num_dependencies = 0;
    foreach_in_arraylistptr(DependencyPair, pair, dependencies){
        num_dependencies += pair->schemas.size;
    }
    return num_dependencies;
}

bool insert_to_dependencies_baseline(ArrayListDependencyPair *dependencies, Variable v, Schema *s, Arena *arena){
    for(unsigned i = 0; i < dependencies->size; ++i){
        DependencyPair *pair = dependencies->array + i;
        if(pair->v == v){
            // TODO: resizing risk for Arena
            int contained = add_not_repeated_to_array_list_schema_arena(&pair->schemas, *s, arena);
            if (contained != CONTAINED) { return true; }
            return false;
        }
    }

    //First dependency for v
    // TODO: resizing risk for Arena + when resizing, we have to copy whole Schemas, not just pointers to them (24B vs 8B)
    ArrayListSchema first_schemas_v = create_array_list_schema_arena(10, arena);
    //  We don't know the number of dependencies that will end up having the new variable with dependencies v
    add_to_array_list_schema_arena(&first_schemas_v, *s, arena);
    DependencyPair first_dependency_pair_v = { .v = v, .schemas = first_schemas_v };
    //  We don't know how many extra variables with dependies we will have neither...
    add_to_array_list_dependency_pair_arena(dependencies, first_dependency_pair_v, arena);
    return true;
}

// destination gets the dependencies found in source that it didn't contain initially.
void union_of_dependencies_baseline(ArrayListDependencyPair *destination, ArrayListDependencyPair *source, Arena *arena){
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
            // TODO: resizing risk for Arena
            add_to_array_list_dependency_pair_arena(destination, pair2, arena); // NOTE: they share the schemas
        }
        else { //found
            // TODO: resizing risk for Arena
            extend_not_repeated_array_list_schema_arena(&destination->array[j].schemas, &schemas2, arena);
        }
    }
}

// Returns the number of dependencies inserted, or -1 if a self-dependency was arised.
int common_schema_baseline(Schema *s1, Schema *s2, Schema *common, ArrayListDependencyPair *dependencies, Arena *arena){
    if(s1->type == VARIABLE_SCHEMA && s2->type == VARIABLE_SCHEMA && s1->v == s2->v){
        // Do not insert v->v dependency
        init_variable_schema(common, s1->v);
        return 0;
    }
    
    if (s1->type == VARIABLE_SCHEMA) {
        Variable v = s1->v;
        if(is_self_dependency(v, s2)){ return -1; }
        init_variable_schema(common, v);
        return insert_to_dependencies_baseline(dependencies, v, s2, arena);
    }
    
    if (s2->type == VARIABLE_SCHEMA) {
        Variable v = s2->v;
        if(is_self_dependency(v, s1)){ return -1; }
        init_variable_schema(common, v);
        return insert_to_dependencies_baseline(dependencies, v, s1, arena);
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

    init_general_schema_arena(common, max_arity, arena); // .size = 1
    int total_inserted_dependencies = 0;
    size_t i = 0;
    for(; i < min_arity; ++i){
        int num_inserted_dependencies = common_schema_baseline(s1->subschemas + i, s2->subschemas + i, 
                                                                common->subschemas + i, dependencies, arena);
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

int common_schema_dependencies_baseline(Schema *s1, Schema *s2, ArrayListDependencyPair *dependencies, Arena *arena){
    if(s1->type == VARIABLE_SCHEMA && s2->type == VARIABLE_SCHEMA && s1->v == s2->v){
        // Do not insert v->v dependency
        return 0;
    }

    if (s1->type == VARIABLE_SCHEMA) {
        if(is_self_dependency(s1->v, s2)){ return -1; }
        return insert_to_dependencies_baseline(dependencies, s1->v, s2, arena);
    }
    if (s2->type == VARIABLE_SCHEMA) {
        if(is_self_dependency(s2->v, s1)){ return -1; }
        return insert_to_dependencies_baseline(dependencies, s2->v, s1, arena);
    }

    int num_new_dependencies = 0;
    unsigned min_arity = MIN(s1->arity, s2->arity);
    for(unsigned i = 0; i < min_arity; ++i){
        int num_new_deps = common_schema_dependencies_baseline(s1->subschemas + i, s2->subschemas + i, dependencies, arena);
        if(num_new_deps == -1){ return -1; }
        num_new_dependencies += num_new_deps;
    }
    return num_new_dependencies;
}

/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 */
int theta_rule1_baseline(ArrayListDependencyPair *dependencies, DependencyPair *pair, Arena *arena){
    int num_new_dependencies = 0;
    ArrayListSchema schemas = pair->schemas;
    // NOTE: since we are making a local copy of the schemas struct, schemas.size will remain the same even if 
    //       new dependencies would be added to schemas.array (so only node->schemas is modified!). That way, 
    //       we ensure that we only take into account the schemas that were initially stored in the list of 
    //       dependencies of the variable at the start of this loop iteration.
    for(size_t i = 0; i < schemas.size; ++i){
        for(size_t j = i + 1; j < schemas.size; ++j){
            int num_new_deps = common_schema_dependencies_baseline(schemas.array + i, schemas.array + j, dependencies, arena);
            if(num_new_deps == -1){ return -1; }
            num_new_dependencies += num_new_deps;
        }
    }
    return num_new_dependencies;
}

// NOTE: we are going to allocate new space for the new Schema that results from the substitution.
//  We copy the structure of original, but changing occurrences of v with substitution.
// NOTE: we are making shallow copies of substitution, not deep copies.
void substitute_arena_(Schema *original, Variable v, Schema *substitution, Schema *result, Arena *arena){
    if(original->type == VARIABLE_SCHEMA){
        if(original->v == v){
            *result = *substitution;
        } else {
            init_variable_schema(result, original->v);
        }
    } else {
        init_general_schema_arena(result, original->arity, arena); // NOTE: ->subschemas allocated; ->size = 1
        for(size_t i = 0; i < original->arity; ++i){
            substitute_arena_(original->subschemas + i, v, substitution, result->subschemas + i, arena);
            result->size += result->subschemas[i].size;
        }
    }
}
// TODO: don't let the debugging variable in release!
unsigned global_substitute_arena_calls = 0;
// TODO: cleaner to follow the idea of initialializer and not return an allocated pointer to Schema...
Schema *substitute_arena(Schema *original, Variable v, Schema *substitution, Arena *arena){
    ++global_substitute_arena_calls;
    Schema *result = allocate(arena, sizeof(*result));
    // if(global_print_debugging){
    //     printf("Substitution function (call=%d):\n", global_substitute_arena_calls);
    //     printf("Original:\n"); print_schema(original, PRINT_VISUALLY); printf("\n"); print_schema(original, PRINT_FILE_FORMAT); printf("\n");
    //     printf("Variable: $%u\n", v);
    //     printf("Substitution:\n"); print_schema(substitution, PRINT_VISUALLY); printf("\n"); print_schema(substitution, PRINT_FILE_FORMAT); printf("\n");
    // }
    substitute_arena_(original, v, substitution, result, arena);
    // if(global_print_debugging){
    //     printf("Result:\n"); print_schema(result, PRINT_VISUALLY); printf("\n"); print_schema(result, PRINT_FILE_FORMAT); printf("\n");
    // }
    return result;
}


/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 */
int theta_rule2_baseline(ArrayListDependencyPair *dependencies, DependencyPair *pair, Arena *arena){
    int num_new_dependencies = 0;
    Variable v = pair->v;
    ArrayListSchema *schemas = &pair->schemas;
    //NOTE: important to take the pointer to the arraylist schemas, because it is modified by a direct call to
    //  add_to_array_list_schema later in this function.

    //NOTE: iteration based on indexes to prevent dangling pointers from resizing
    //NOTE: necessary to take the initial size of the schemas because it may be incremented, and in case of self-dependencies,
    //  it would possibly loop infinitely!!! Additionally, that way we have the same behaviour in both rules!
    size_t initial_size = schemas->size;
    for(size_t i = 0; i < initial_size; ++i){
        Schema *gamma = schemas->array + i;
        SetVariables ws = variables_in_schema(gamma);
        foreach_in_setvariables(ws, node_var){
            Variable w = node_var->v;

            ArrayListSchema *schemas_ = NULL; // TODO: abstract this to a find function in ArrayLists...
            for(DependencyPair *it = dependencies->array, *end = dependencies->array + dependencies->size; it < end; ++it){
                if(it->v == w){
                    schemas_ = &it->schemas;
                    break;
                }
            }
            if(schemas_ == NULL){ continue; }

            foreach_in_arraylistptr(Schema, schema_, schemas_){
                Schema *gamma_ = schema_;
                if(global_print_debugging){
                    print_dependency_pair(pair, '{', '}', "; ", PRINT_VISUALLY); printf("\n");
                    printf("i = %lu\n", i);
                    printf("gamma: "); print_schema(gamma, PRINT_VISUALLY); printf("\n");
                    printf("w = $%u\n", w);
                    printf("gamma_: "); print_schema(gamma_, PRINT_VISUALLY); printf("\n");
                }
                Schema *gamma__ = substitute_arena(gamma, w, gamma_, arena);
                if(global_print_debugging){
                    printf("gamma__: "); print_schema(gamma__, PRINT_VISUALLY); printf("\n\n");
                }
                if(is_self_dependency(v, gamma__)){ return -1; }
                // TODO: resizing risk for Arena
                int contained = add_not_repeated_to_array_list_schema_arena(schemas, *gamma__, arena);
                if(contained != CONTAINED){ ++num_new_dependencies; }
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
int theta_operator_baseline(ArrayListDependencyPair *dependencies, Arena *arena){
    int num_new_dependencies = 0;
    int num_new_deps1, num_new_deps2; // NOTE: we could have a single foreach accumulator, but this way we can have more information when debugging
    unsigned iteration = 0;
    do {
        ++iteration;
        //if(global_print_debugging) printf("Theta iteration #%u...\n", iteration);

        num_new_deps1 = num_new_deps2 = 0;
        //NOTE: rule1 is much simpler than rule2 as it only needs to access to the schemas a variable depends on, not other
        //  variables. For that reason, we first call to it to try to be more cache-locality-friendly.
        
        //NOTE: we iterate using indexes instead of pointers because when adding new dependencies the ArrayList of DependencyPairs
        //  can be resized (so the iterating pointers would be invalidated...)

        for(size_t i = 0; i < dependencies->size; ++i){
            DependencyPair *pair = dependencies->array + i;
            //TODO: would be interesting to somehow avoid reaplying this rule to the same pair of gamma-gamma_
            //if(global_print_debugging) printf("Start rule 1...\n");
            int num1 = theta_rule1_baseline(dependencies, pair, arena);
            //if(global_print_debugging) printf("End rule 1...\n");
            if(num1 == -1){ return -1; }
            num_new_deps1 += num1;

            if(global_print_debugging){
                printf("---\n"); print_set_dependencies(dependencies, PRINT_VISUALLY);
            }

            //TODO: would be interesting to somehow avoid reaplying this rule to the same pair of gamma-gamma_
            //if(global_print_debugging) printf("Start rule 2...\n");
            int num2 = theta_rule2_baseline(dependencies, pair, arena);
            //if(global_print_debugging) printf("End rule 2...\n");
            if(num2 == -1){ return -1; }
            num_new_deps2 += num2;

            if(global_print_debugging){
                printf("---\n"); print_set_dependencies(dependencies, PRINT_VISUALLY);
            }
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

void remove_variables_not_in_set_schema_from_dependencies(
    ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies, Arena *arena)
{
    // TODO: see if we can avoid this call with previous computations...
    SetVariables final_vs = variables_in_set_schema(*set_schema);
    
    if(global_print_debugging){
        printf("Final variables:\n");
        print_set_variables(final_vs); printf("\n");
    }
    
    SetVariables removed_vs_with_dependencies = create_set_variables_defsize();

    // NOTE: from right to left to diminish leftwise copying and to avoid extra index corrections and overhead of call function to get
    for(int i = dependencies->size - 1; i >= 0; --i){
        DependencyPair pair = dependencies->array[i];
        if(!lookup_set_variables(final_vs, pair.v)){
            remove_index_from_array_list_dependency_pair(dependencies, i);
            insert_to_set_variables(&removed_vs_with_dependencies, pair.v);
        }
    }

    if(global_print_debugging){
        printf("Removed variables with dependencies:\n");
        print_set_variables(removed_vs_with_dependencies); printf("\n");
        print_set_dependencies(dependencies, PRINT_VISUALLY);
    }
    
    // NOTE: thanks to the use of rule 2, the schema resulting from substituting the longest dependency of the removed
    // variables is already in the set of dependencies, so we can simply remove the schemas that contain those variables.
    // We only need to consider the removed variables that didn't have any dependences, to substitute them for <>.
    foreach_in_arraylistptr(DependencyPair, pair, dependencies){
        if(global_print_debugging){
            printf("Variable $%u\n", pair->v);
        }
        // NOTE: from right to left to diminish leftwise copying and to avoid extra index corrections and overhead of call function to get
        ArrayListSchema *dependency_schemas = &pair->schemas;
        for(int i = dependency_schemas->size - 1; i >= 0; --i){
            // NOTE: we take by value because if not, when removing, it will point to the next schema before applying the substitution by <>
            Schema schema = dependency_schemas->array[i];
            // * Contains only some removed v with dependencies --> Remove
            // * Contains only some removed v without dependencies --> Remove and substitute it for the version that contains <> instead of v, insert this to the set of dependencies
            // NOTE: the new schema that instead of v contains <> won't generate new schemas if the theta operator was reaplied, because 
            //  the subcommon schemas calculated with rule 1 won't generate new dependencies (none is obtained when calculating the common
            //  schema with <>) and neither rule 2, since the substitutions that contain v are already included and their appearences of v
            //  will be substituted by <>.
            // * Contains both kinds of vs --> Remove, substitute the v_no_deps with <>, reaply theta rules with this new dependency and do NOT insert it to the resulting set of dependencies
            // NOTE: the same previous note applies here, NO new dependencies will be added at all --> This only needs to the the same operation as case 1
            // * Doesn't contain any removed v --> Do nothing

            // Calculate variables in Schema
            SetVariables schema_vs = variables_in_schema(&schema);

            if(global_print_debugging){
                printf("Schema: "); print_schema(&schema, PRINT_VISUALLY); printf("\n");
                printf("Vars: "); print_set_variables(schema_vs); printf("\n");
            }

            SetVariables removed_vs_without_dependencies_in_schema = create_set_variables_defsize();
            bool contains_removed_v_with_dependencies = false;
            foreach_in_setvariables(schema_vs, v_node){
                Variable v = v_node->v;
                if(lookup_set_variables(removed_vs_with_dependencies, v)){
                    contains_removed_v_with_dependencies = true;
                    break;
                }
                if(!lookup_set_variables(final_vs, v)){
                    insert_to_set_variables(&removed_vs_without_dependencies_in_schema, v);
                }
            }
            
            if(global_print_debugging){
                printf("Removed vars without dependencies: "); print_set_variables(removed_vs_without_dependencies_in_schema); printf("\n");
            }

            // If contains some removed variable with dependencies (in removed_vs_with_dependencies): remove
            if(contains_removed_v_with_dependencies){
                remove_index_from_array_list_schema(dependency_schemas, i);
            }
            // If only contains some removed variable without dependencies (not removed_vs_with_dependencies nor final_vs): remove and add substitution (all those variables) with <>
            else if (removed_vs_without_dependencies_in_schema.num_variables){
                remove_index_from_array_list_schema(dependency_schemas, i);
                if(global_print_debugging) {
                    printf("Just after removing: "); print_set_schema(dependency_schemas, PRINT_VISUALLY); printf("\n");
                }
                Schema empty; init_general_schema_arena(&empty, 0, arena);
                foreach_in_setvariables(removed_vs_without_dependencies_in_schema, v_node){
                    Variable v = v_node->v;
                    schema = *substitute_arena(&schema, v, &empty, arena);
                }
                add_to_array_list_schema_arena(dependency_schemas, schema, arena);
            }

            if(global_print_debugging) {
                printf("Resultant-iter set schema: "); print_set_schema(dependency_schemas, PRINT_VISUALLY); printf("\n");
            }

            free_set_variables(removed_vs_without_dependencies_in_schema);
            free_set_variables(schema_vs);
        }
    }

    if(global_print_debugging){
        printf("Final dependencies after removal of variables that are no longer contained in the common set schema:\n");
        print_set_dependencies(dependencies, PRINT_VISUALLY);
        //print_set_dependencies(dependencies, PRINT_FILE_FORMAT);
    }

    //NOTE: we shouldn't get new dependencies or a self-dependency after the previous operations
    int theta_result = theta_operator_baseline(dependencies, arena);
    if(global_print_debugging && theta_result != 0){
        printf("Unexpected extra dependencies added with theta operator:\n");
        print_set_dependencies(dependencies, PRINT_VISUALLY);
    }
    assert(theta_result == 0);

    free_set_variables(removed_vs_with_dependencies);
    free_set_variables(final_vs);

    assert(!contains_self_dependency_baseline(*dependencies));
}

bool common_set_schema_baseline_(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena, bool remove_variables_not_in_resulting_common_schema)
{
    global_substitute_arena_calls = 0;
    
    if(set_schema1->size != set_schema2->size){ return false; }
    unsigned num_schemas = set_schema1->size; //num columns
    
    // NOTE: no resizing risk for Arena - Num schemas initialized here to the precalculated value because then we take pointers to each Schema, as if it already was a valid arraylist element
    *common_set_schema = create_array_list_schema_arena(num_schemas, arena);
    common_set_schema->size = num_schemas;
    // TODO: resizing risk for Arena, unknown number of variables with new dependencies (even variables that initially didn't have any)
    ArrayListDependencyPair new_dependencies = create_array_list_dependency_pair_arena(10, arena);
    
    for(unsigned i = 0; i < num_schemas; ++i){
        Schema *s1 = set_schema1->array + i;
        Schema *s2 = set_schema2->array + i;
        
        Schema *common_schema = common_set_schema->array + i;
        int num_new_dependencies = common_schema_baseline(s1, s2, common_schema, &new_dependencies, arena);

        if(global_print_debugging){
            printf("Computed Common schema %u:\n", i + 1);
            if(num_new_dependencies < 0){
                printf("Self dependency detected!\n");
            }
            else {
                print_schema(s1, PRINT_VISUALLY);
                printf(" (x) ");
                print_schema(s2, PRINT_VISUALLY);
                printf(" = ");
                print_schema(common_schema, PRINT_VISUALLY);
                printf("\n");
                //print_schema(common_schema, PRINT_FILE_FORMAT); printf("\n");
            }
        }

        if(num_new_dependencies < 0){ return false; }
    }
    // common_set_schema calculated

    if(global_print_debugging){
        printf("Computed Common Set Schema:\n");
        print_set_schema(common_set_schema, PRINT_VISUALLY);
        //print_set_schema(common_set_schema, PRINT_FILE_FORMAT);
        printf("Just the new dependencies:\n");
        print_set_dependencies(&new_dependencies, PRINT_VISUALLY);
        //print_set_dependencies(&new_dependencies, PRINT_FILE_FORMAT);
    }

    // Start calculating the final common_dependencies set as the union of the two inputs and new_dependencies

    // NOTE: capacity can be greater than final size (we can have dependencies of the same variable in several sets)
    //  therefore, there is no risk of resizing
    *common_dependencies = create_array_list_dependency_pair_arena(dependencies1->size + dependencies2->size + new_dependencies.size, arena);
    extend_array_list_dependency_pair_arena(common_dependencies, dependencies1, arena); // NOTE: Only copy the header of the array, the elements are shared
    // TODO: here, when adding new depencies to the array list of an already stored variable, we can have resizing
    union_of_dependencies_baseline(common_dependencies, dependencies2, arena);
    union_of_dependencies_baseline(common_dependencies, &new_dependencies, arena);

    if(global_print_debugging){
        printf("Union of all dependencies:\n");
        print_set_dependencies(common_dependencies, PRINT_VISUALLY);
        //print_set_dependencies(common_dependencies, PRINT_FILE_FORMAT);
    }

    //if(global_print_debugging) printf("Start theta operator...\n");
    // Now, we must calculate the hidden dependencies, in case there is a self dependency
    int num_new_dependencies = theta_operator_baseline(common_dependencies, arena);
    //if(global_print_debugging) printf("End theta operator...\n");

    if(global_print_debugging){
        printf("Total dependencies after theta operator:\n");
        print_set_dependencies(common_dependencies, PRINT_VISUALLY);
        //print_set_dependencies(common_dependencies, PRINT_FILE_FORMAT);
    }

    if(num_new_dependencies < 0){ return false; }
    assert(!contains_self_dependency_baseline(*common_dependencies));
    
    if(remove_variables_not_in_resulting_common_schema){
        remove_variables_not_in_set_schema_from_dependencies(common_set_schema, common_dependencies, arena);
    }

    return true;
}

bool common_set_schema_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena)
{
    return common_set_schema_baseline_(
        set_schema1, dependencies1,
        set_schema2, dependencies2,
        common_set_schema, common_dependencies,
        arena, true);
}

// ARRAYLIST OF PAIRS VARIABLE-NUMAPPEARENCES (TODO: clean where to put this...)
typedef struct VarNum VarNum, *VarNumPtr;
struct VarNum {
    Variable v;
    unsigned num_appearences;
};

//DECLARE_ARRAYLIST_TYPE(VarNum, VarNum) 
typedef struct ArrayListVarNum ArrayListVarNum;
struct ArrayListVarNum {
    uint32_t size;
    uint32_t capacity;
    VarNum* array;
};

//DEFINE_ARRAYLIST_CREATION_ARENA(VarNum, varnum)
ArrayListVarNum create_array_list_varnum_arena(uint32_t capacity, Arena* arena)
{
    ArrayListVarNum list;
    list.size = 0;
    list.capacity = capacity;
    list.array = allocate(arena, capacity * sizeof(*list.array));
    return list;
}

//DEFINE_ARRAYLIST_ADDITION_ARENA(VarNum, varnum, VarNum)
int add_to_array_list_varnum_arena(ArrayListVarNum* list, VarNum element, Arena* arena)
{
    int code = NOT_RESIZED;
    if (list->size == list->capacity) {
        if (list->capacity) {
            list->capacity *= 2;
        } else {
            list->capacity = 1;
        }
        list->array = allocate(arena, list->capacity * sizeof(*list->array));
        code = RESIZED;
    }
    list->array[list->size] = element;
    ++list->size;
    return code;
}

// TODO: we should rename this to generalize mappings of vars to num appearences...
int increment_num_array_list_varnum(ArrayListVarNum list, Variable v){
    foreach_in_arraylist(VarNum, varnum, list){
        if(varnum->v == v){
            varnum->num_appearences++;
            return CONTAINED;
        }
    }
    return 0; // TODO: define NOT_CONTAINED
}

unsigned find_v_in_array_list_varnum(ArrayListVarNum list, Variable v){
    unsigned i = 0;
    foreach_in_arraylist(VarNum, varnum, list){
        if(varnum->v == v){
            return i;
        }
        ++i;
    }
    return list.size;
}

void calculate_num_appearences_of_variables_in_schema(Schema schema, ArrayListVarNum *varnum, Arena *arena){
    if(schema.type == VARIABLE_SCHEMA){
        unsigned varnum_pos = find_v_in_array_list_varnum(*varnum, schema.v);
        if(varnum_pos == varnum->size){
            VarNum initial_mapping = { .v = schema.v, .num_appearences = 1 };
            add_to_array_list_varnum_arena(varnum, initial_mapping, arena);
        } else {
            VarNum *mapping = varnum->array + varnum_pos;
            mapping->num_appearences++;
        }
    } else {
        Schema *subschema = schema.subschemas;
        Schema *end = schema.subschemas + schema.arity;
        for(; subschema < end; ++subschema){
            calculate_num_appearences_of_variables_in_schema(*subschema, varnum, arena);
        }
    }
}

void calculate_num_appearences_of_variables_in_set_schema(ArrayListSchema set_schema, ArrayListVarNum *varnum, Arena *arena){
    foreach_in_arraylist(Schema, schema, set_schema){
        calculate_num_appearences_of_variables_in_schema(*schema, varnum, arena);
    }
}

// TODO: versión estricta de common_set_schema_baseline
// - Primer check:
//      + Se puede hacer de manera eficiente si tuvieramos dos ordered-maps (que mantengan el orden de inserción)
//      y con iteradores sobre esos ordered-maps...
//      + Otra forma es tener un unordered-hash-map para v->num y aparte un arraylist con las variables en orden por cada set-schema operando.
//      Luego iterariamos sobre las mismas posiciones del arraylist, accediendo con el hash al número de apariciones.
bool first_check(ArrayListSchema set_schema1, ArrayListSchema set_schema2, Arena *arena){
    ArrayListVarNum var_to_num1 = create_array_list_varnum_arena(10, arena);
    ArrayListVarNum var_to_num2 = create_array_list_varnum_arena(10, arena);

    calculate_num_appearences_of_variables_in_set_schema(set_schema1, &var_to_num1, arena);
    calculate_num_appearences_of_variables_in_set_schema(set_schema2, &var_to_num2, arena);
    
    // Same quantity of distinct variables in both set_schemas
    if(var_to_num1.size != var_to_num2.size){
        return false;
    }

    // Same quantity of corresponding distinct variables according to order of appearence in each set_schema
    unsigned size = var_to_num1.size;
    for(unsigned i = 0; i < size; ++i){
        if(var_to_num1.array[i].num_appearences != var_to_num2.array[i].num_appearences){
            return false;
        }
    }

    return true;
}

// TODO: the same recursion happens in a great amount of places with little modifications of what's done in the
//  base (variable schema) or general (general schema) case. Is there a way to parameterize this and do a unique
//  function that performs the inorder DFS in set_schemas?
bool unique_dependency_between_vars__(Schema schema, SetVariables *variables){
    if(schema.type == VARIABLE_SCHEMA){
        return insert_to_set_variables(variables, schema.v) != SET_INSERT_ALREADY_CONTAINED;
    }
    Schema *subschema = schema.subschemas;
    Schema *end = schema.subschemas + schema.arity;
    for(; subschema < end; ++subschema){
        if(!unique_dependency_between_vars__(*subschema, variables)){
            return false;
        }
    }
    return true;
}

bool unique_dependency_between_vars_(ArrayListSchema schemas, SetVariables *variables){
    foreach_in_arraylist(Schema, s, schemas){
        if(!unique_dependency_between_vars__(*s, variables)){
            return false;
        }
    }
    return true;
}

bool unique_dependency_between_vars(ArrayListDependencyPair dependencies){
    SetVariables variables = create_set_variables_defsize();
    foreach_in_arraylist(DependencyPair, pair, dependencies){
        if(!unique_dependency_between_vars_(pair->schemas, &variables)){
            free_set_variables(variables);
            return false;
        }
        clear_set_variables(variables);
    }
    free_set_variables(variables);
    return true;
}

bool common_set_schema_strict_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena)
{
    bool result = first_check(*set_schema1, *set_schema2, arena) &&
           common_set_schema_baseline_(set_schema1, dependencies1, set_schema2, dependencies2, 
                                      common_set_schema, common_dependencies, arena, false) &&
           first_check(*set_schema1, *common_set_schema, arena) &&
           unique_dependency_between_vars(*common_dependencies);
    
    if(!result){
        return false;
    }

    remove_variables_not_in_set_schema_from_dependencies(common_set_schema, common_dependencies, arena);
    return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES /////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST DEPENDENCY PAIRS //////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* DEFINE_ARRAYLIST_CREATION(DependencyPair, dependency_pair)
DEFINE_ARRAYLIST_ADDITION(DependencyPair, dependency_pair, DependencyPair)
DEFINE_ARRAYLIST_GETTING(DependencyPair, dependency_pair, DependencyPair)
DEFINE_PRINT_ARRAYLIST(DependencyPair, dependency_pair, print_dependency_pair) */

//DEFINE_ARRAYLIST_CREATION_ARENA(DependencyPair, dependency_pair)
ArrayListDependencyPair create_array_list_dependency_pair_arena(uint32_t capacity, Arena* arena)
{
    ArrayListDependencyPair list;
    list.size = 0;
    list.capacity = capacity;
    list.array = allocate(arena, capacity * sizeof(*list.array));
    return list;
}

//DEFINE_ARRAYLIST_ADDITION_ARENA(DependencyPair, dependency_pair, DependencyPair) 
int add_to_array_list_dependency_pair_arena(ArrayListDependencyPair* list, DependencyPair element, Arena* arena){
    int code = NOT_RESIZED;
    if (list->size == list->capacity) {
        if (list->capacity) {
            list->capacity *= 2;
        } else {
            list->capacity = 1;
        }
        // TODO: this is the logic of resizing!!!
        DependencyPair *new_array = allocate(arena, list->capacity * sizeof(*list->array));
        DependencyPair *source = list->array;
        DependencyPair *dest = new_array;
        DependencyPair *end = dest + list->size;
        for(; dest < end; ++dest, ++source){
            *dest = *source;
        }
        list->array = new_array;
        code = RESIZED;
    }
    list->array[list->size] = element;
    ++list->size;
    return code;
}

//DEFINE_ARRAYLIST_EXTENSION_ARENA(DependencyPair, dependency_pair, DependencyPair) 
int extend_array_list_dependency_pair_arena(ArrayListDependencyPair* list_to_extend, ArrayListDependencyPair* list, Arena* arena)
{
    int code = NOT_RESIZED;
    unsigned extended_size = list_to_extend->size + list->size;
    if (extended_size > list_to_extend->capacity) {
        list_to_extend->capacity = 2 * extended_size;
        // TODO: this is the resizing logic in extending too!!!
        DependencyPair *new_array = allocate(arena, list_to_extend->capacity * sizeof(*list_to_extend->array));
        DependencyPair *source = list_to_extend->array;
        DependencyPair *dest = new_array;
        DependencyPair *end = dest + list_to_extend->size;
        for(; dest < end; ++dest, ++source){
            *dest = *source;
        }
        list_to_extend->array = new_array;
        code = RESIZED;
    }
    for (DependencyPair *elemptr = (list)->array, *_end = (list)->array + (list)->size; elemptr < _end; ++elemptr) {
        // TODO: as we already have extended when necessary, we can avoid the overhead of a function call
        //add_to_array_list_dependency_pair_arena(list_to_extend, *elemptr, arena);
        list_to_extend->array[list_to_extend->size++] = *elemptr;
    }
    return code;
}

//DEFINE_ARRAYLIST_REMOVAL_INDEX(DependencyPair, dependency_pair, DependencyPair)
int remove_index_from_array_list_dependency_pair(ArrayListDependencyPair* list, uint32_t index)
{
    if (index >= list->size) {
        return OUT_OF_BOUNDS;
    }
    --list->size;
    DependencyPair* removed_ptr = list->array + index;
    uint32_t num_shifted_elements = list->size - index;
    memmove(removed_ptr, removed_ptr + 1, num_shifted_elements * sizeof(*list->array));
    return SUCCESSFUL_REMOVAL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST DEPENDENCY PAIRS //////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END SET OF DEPENDENCIES /////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PRINTING FUNCTIONS FOR DEBUGGING ////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Schemas /////////////////////////////////////////////////////////////
void print_schema_(Schema *s, char opening_brace, char closing_brace, bool show_arity){
    if(s->type == VARIABLE_SCHEMA){
        printf("$%d", s->v);
    } else {
        if(show_arity) { printf("%d:", s->arity); }
        printf("%c", opening_brace);
        if(s->arity) { print_schema_(s->subschemas, opening_brace, closing_brace, show_arity); }
        Schema *sub = s->subschemas + 1;
        Schema *end = s->subschemas + s->arity;
        for(; sub < end; ++sub){
            printf(", ");
            print_schema_(sub, opening_brace, closing_brace, show_arity);
        }
        printf("%c", closing_brace);
    }
}
void print_schema(Schema *s, PrintingMode mode){
    if(mode == PRINT_VISUALLY){
        print_schema_(s, '<', '>', false);
    }
    else {
        print_schema_(s, '[', ']', true);
    }
}

// Set-Schemas /////////////////////////////////////////////////////////////
void print_set_schema_(ArrayListSchema *set_schema, char opening_brace, char closing_brace, const char *schema_separator, PrintingMode schema_mode){
    if(opening_brace){ printf("%c", opening_brace); }
    if(set_schema->size) { print_schema(set_schema->array, schema_mode); }
    Schema *schema = set_schema->array + 1;
    Schema *end = set_schema->array + set_schema->size;
    for(; schema < end; ++schema){
        printf("%s", schema_separator);
        print_schema(schema, schema_mode);
    }
    if(closing_brace){ printf("%c", closing_brace); }
}
void print_set_schema(ArrayListSchema *set_schema, PrintingMode mode){
    // NOTE: another interesting schema_separator = ",\n\t" (potentially for a certain number of \t if nesting...)
    if(mode == PRINT_VISUALLY){
        print_set_schema_(set_schema, '{', '}', "; ", PRINT_VISUALLY);
    }
    else {
        print_set_schema_(set_schema, '\0', '\0', "; ", PRINT_FILE_FORMAT);
    }
    printf("\n");
}

// Set dependencies /////////////////////////////////////////////////////////////
void print_dependency_pair(DependencyPair *pair, char opening_brace, char closing_brace, const char *schema_separator, PrintingMode schema_mode){
    printf("$%u <- ", pair->v);
    print_set_schema_(&pair->schemas, opening_brace, closing_brace, schema_separator, schema_mode);
}
void print_set_dependencies_(ArrayListDependencyPair *set_dependencies, char opening_brace, char closing_brace, const char *schema_separator, PrintingMode schema_mode){
    if(set_dependencies->size == 0){
        printf("\n");
        return;
    }

    foreach_in_arraylistptr(DependencyPair, pair, set_dependencies){
        print_dependency_pair(pair, opening_brace, closing_brace, schema_separator, schema_mode);
        printf("\n");
    }
}
void print_set_dependencies(ArrayListDependencyPair *set_dependencies, PrintingMode mode){
    if(mode == PRINT_VISUALLY){
        print_set_dependencies_(set_dependencies, '{', '}', "; ", PRINT_VISUALLY);
    }
    else {
        print_set_dependencies_(set_dependencies, '[', ']', "; ", PRINT_FILE_FORMAT);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END PRINTING FUNCTIONS FOR DEBUGGING ////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

