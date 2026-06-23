#include "schemas.h"
#include "utils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES /////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST SCHEMA POINTERS ///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_ARRAYLIST_CREATE_ARENA(SchemaPtr, schema_ptr)
DEFINE_ARRAYLIST_RESIZE_ARENA(SchemaPtr, schema_ptr)
DEFINE_ARRAYLIST_ADD_ARENA(SchemaPtr, schema_ptr)

static inline bool equal_schema_ptrs(Schema *s1, Schema *s2) {
    return equal_schemas(*s1, *s2);
}
DEFINE_ARRAYLIST_FIND(SchemaPtr, schema_ptr, equal_schema_ptrs)

DEFINE_ARRAYLIST_ADD_NO_REPEATED_ARENA(SchemaPtr, schema_ptr)
DEFINE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(SchemaPtr, schema_ptr)
DEFINE_ARRAYLIST_REMOVE_INDEX(SchemaPtr, schema_ptr)

unsigned find_equivalent_in_array_list_schema_ptr(ArrayListSchemaPtr list, Schema *elem, Variable *mapping) {
    unsigned pos = 0;
    foreach_in_arraylist(SchemaPtr, spp, list){
        if (equivalent_schemas(**spp, *elem, mapping)){
            return pos;
        }
        ++pos;
    }
    return pos;
}

static inline void print_schema_ptr(Schema *s) {
    print_schema(*s, PRINT_VISUALLY);
}
DEFINE_ARRAYLIST_PRINT_SEPARATORS(SchemaPtr, schema_ptr, print_schema_ptr)
DEFINE_ARRAYLIST_PRINT(SchemaPtr, schema_ptr)
DEFINE_ARRAYLIST_PRINTLN(SchemaPtr, schema_ptr)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST SCHEMA POINTERS ///////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST DEPENDENCY PAIRS //////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_ARRAYLIST_CREATE_ARENA(DependencyPair, dependency_pair)

DEFINE_ARRAYLIST_RESIZE_ARENA(DependencyPair, dependency_pair)
DEFINE_ARRAYLIST_ADD_ARENA(DependencyPair, dependency_pair)

DEFINE_ARRAYLIST_EXTEND_ARENA(DependencyPair, dependency_pair)

DEFINE_ARRAYLIST_REMOVE_INDEX(DependencyPair, dependency_pair)

unsigned find_v_in_array_list_dependency_pair(ArrayListDependencyPair list, Variable v){
    unsigned i = 0;
    foreach_in_arraylist(DependencyPair, iter, list){
        if (v == iter->v){
            return i;
        }
        ++i;
    }
    return list.size;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST DEPENDENCY PAIRS //////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END SET OF DEPENDENCIES /////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST OF PAIRS VARIABLE-NUM_APPEARENCES /////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    Variable v;
    unsigned num_appearences;
} VarNum, *VarNumPtr;

DECLARE_ARRAYLIST_TYPE(VarNum)
DEFINE_ARRAYLIST_CREATE_ARENA(VarNum, varnum)
DEFINE_ARRAYLIST_RESIZE_ARENA(VarNum, varnum)
DEFINE_ARRAYLIST_ADD_ARENA(VarNum, varnum)

unsigned find_v_in_array_list_varnum(ArrayListVarNum list, Variable v)
{
    unsigned i = 0;
    foreach_in_arraylist(VarNum, varnum, list)
    {
        if (varnum->v == v) {
            return i;
        }
        ++i;
    }
    return list.size;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST OF PAIRS VARIABLE-NUM_APPEARENCES /////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST STRINGS (Char Pointers) ///////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_ARRAYLIST_CREATE(CharPtr, char_ptr)
DEFINE_ARRAYLIST_CREATE_ARENA(CharPtr, char_ptr)

DEFINE_ARRAYLIST_RESIZE_ARENA(CharPtr, char_ptr)
DEFINE_ARRAYLIST_ADD_ARENA(CharPtr, char_ptr)
DEFINE_ARRAYLIST_EXTEND_ARENA(CharPtr, char_ptr)

DEFINE_ARRAYLIST_FIND(CharPtr, char_ptr, equal_strings)
DEFINE_ARRAYLIST_ADD_NO_REPEATED_ARENA(CharPtr, char_ptr)
DEFINE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(CharPtr, char_ptr)

// NOTE: since we expect short arraylists of strings, we will use a simple insertion sort
void insertion_sort_arraylist_char_ptr(ArrayListCharPtr list){
    for(unsigned i = 1; i < list.size; ++i){
        unsigned right_pos = 0;
        char *str1 = list.array[i];
        for(unsigned j = 0; j < i; ++j){
            char *str2 = list.array[j];
            if(strcmp(str1, str2) < 0){
                break;
            }
            ++right_pos;
        }
        memmove(list.array + right_pos + 1, list.array + right_pos, sizeof(char*) * (i - right_pos));
        list.array[right_pos] = str1;
    }
}

DEFINE_ARRAYLIST_EQUAL(CharPtr, char_ptr, equal_strings)

DEFINE_ARRAYLIST_PRINT_SEPARATORS(CharPtr, char_ptr, print_string)
DEFINE_ARRAYLIST_PRINT(CharPtr, char_ptr)
DEFINE_ARRAYLIST_PRINTLN(CharPtr, char_ptr)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST STRINGS (Char Pointers) ///////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST UInts /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_ARRAYLIST_CREATE(UInt, uint)
DEFINE_ARRAYLIST_CREATE_ARENA(UInt, uint)
DEFINE_ARRAYLIST_PRINT_SEPARATORS(UInt, uint, print_uint)
DEFINE_ARRAYLIST_PRINT(UInt, uint)
DEFINE_ARRAYLIST_PRINTLN(UInt, uint)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST UInts /////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PRINTING FUNCTIONS FOR DEBUGGING ////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Schemas /////////////////////////////////////////////////////////////////////////////////////////////
void print_schema_(Schema s, char opening_brace, char closing_brace, bool show_arity)
{
    if (s.type == VARIABLE_SCHEMA) {
        printf("$%d", s.v);
    } else {
        if (show_arity) {
            printf("%d:", s.arity);
        }
        printf("%c", opening_brace);
        if (s.arity) {
            print_schema_(s.subschemas[0], opening_brace, closing_brace, show_arity);
        }
        for(unsigned i = 1; i < s.arity; ++i)
        {
            printf(", ");
            print_schema_(s.subschemas[i], opening_brace, closing_brace, show_arity);
        }
        printf("%c", closing_brace);
    }
}
void print_schema(Schema s, PrintingMode mode)
{
    if (mode == PRINT_VISUALLY) {
        print_schema_(s, '<', '>', false);
    } else {
        print_schema_(s, '[', ']', true);
    }
}
void println_schema(Schema s, PrintingMode mode)
{
    print_schema(s, mode);
    printf("\n");
}
void printdef_schema(Schema s) {
    println_schema(s, PRINT_VISUALLY);
}

// Set-Schemas /////////////////////////////////////////////////////////////////////////////////////////////
void print_set_schema_(
    SetSchema set_schema, char opening_brace, char closing_brace,
    char* schema_separator, PrintingMode schema_mode)
{
    assert(set_schema.type == GENERAL_SCHEMA && set_schema.arity > 0);
    printf("%c", opening_brace);
    print_schema(set_schema.subschemas[0], schema_mode);
    foreach_in_schema_after_first(set_schema, s) {
        printf("%s", schema_separator);
        print_schema(*s, schema_mode);
    }
    printf("%c", closing_brace);
}
void print_set_schema(SetSchema set_schema, PrintingMode mode)
{
    // NOTE: another interesting schema_separator = ",\n\t" (potentially for a certain number of \t if nesting...)
    if (mode == PRINT_VISUALLY) {
        print_set_schema_(set_schema, '{', '}', "; ", PRINT_VISUALLY);
    } else {
        // NOTE: \0 not printable character, so it won't be shown!
        print_set_schema_(set_schema, '\0', '\0', "; ", PRINT_FILE_FORMAT);
    }
}
void println_set_schema(SetSchema set_schema, PrintingMode mode)
{
    print_set_schema(set_schema, mode);
    printf("\n");
}
void printdef_set_schema(SetSchema set_schema){
    println_set_schema(set_schema, PRINT_VISUALLY);
}

// Set dependencies /////////////////////////////////////////////////////////////////////////////////////////////
void print_dependency_pair(
    DependencyPair pair, char opening_brace, char closing_brace,
    char* schema_separator)
{
    printf("$%u <- ", pair.v);
    print_separators_array_list_schema_ptr(pair.schemas, opening_brace, closing_brace, schema_separator, false, 0);
}
void print_set_dependencies_(
    SetDependencies set_dependencies, char opening_brace, char closing_brace,
    char* schema_separator, PrintingMode schema_mode)
{
    if (set_dependencies.size == 0) {
        printf("None\n");
        return;
    }

    foreach_in_arraylist(DependencyPair, pair, set_dependencies)
    {
        print_dependency_pair(*pair, opening_brace, closing_brace, schema_separator);
        printf("\n");
    }
}
void print_set_dependencies(SetDependencies set_dependencies, PrintingMode mode)
{
    if (mode == PRINT_VISUALLY) {
        print_set_dependencies_(set_dependencies, '{', '}', "; ", PRINT_VISUALLY);
    } else {
        print_set_dependencies_(set_dependencies, '[', ']', "; ", PRINT_FILE_FORMAT);
    }
}
void println_set_dependencies(SetDependencies set_dependencies, PrintingMode mode)
{
    print_set_dependencies(set_dependencies, mode);
    printf("\n");
}
void printdef_set_dependencies(SetDependencies dependencies) {
    println_set_dependencies(dependencies, PRINT_VISUALLY);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END PRINTING FUNCTIONS FOR DEBUGGING ////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SCHEMAS /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Function that computes the schema size, without looking at the precached sizes.
 */
unsigned schema_size_rec(Schema s)
{
    if (s.type == VARIABLE_SCHEMA || s.arity == 0) {
        return 1;
    }

    unsigned total_size = 1;
    foreach_in_schema(s, sub)
    {
        total_size += schema_size_rec(*sub);
    }

    return total_size;
}

unsigned schema_size(Schema *s){
    // NOTE: since a same schema can be shared as a subschema of several other parent schemas, we have to check if the size
    //  was already calculated!
    if(s->size_ == 0){
        s->size_ = 1;
        if (s->type == GENERAL_SCHEMA) {
            foreach_in_schema(*s, sub) {
                s->size_ += schema_size(sub);
            }
        }
    }
    return s->size_;
}


// NOTE: useful to avoid resizing the ArrayList used as the Stack for SchemaIterator. Therefore, variable schemas and empties will return 1,
//  because they need also to be stored in the Stack.
// NOTE: for debugging schemas' depth computations (we store a precomputed depth in Schemas).
unsigned schema_depth_rec(Schema s){
    if (s.type == VARIABLE_SCHEMA || s.arity == 0) {
        return 1;
    }

    unsigned depth = 0;
    foreach_in_schema(s, sub)
    {
        depth = MAX(depth, schema_depth_rec(*sub));
    }

    return depth + 1;
}

unsigned schema_depth(Schema *s){
    // NOTE: since a same schema can be shared as a subschema of several other parent schemas, we have to check if the depth
    //  was already calculated!
    if(s->depth_ == 0){
        if (s->type == GENERAL_SCHEMA) {
            foreach_in_schema(*s, sub){
                s->depth_ = MAX(s->depth_, schema_depth(sub));
            }
        }
        s->depth_++;
    }
    return s->depth_;
}

bool equal_schemas(Schema s1, Schema s2)
{
    if (s1.type == VARIABLE_SCHEMA && s2.type == VARIABLE_SCHEMA && s1.v == s2.v) {
        return true;
    }
    if (s1.type == GENERAL_SCHEMA && s2.type == GENERAL_SCHEMA && s1.arity == s2.arity) {
        foreach_in_schemas(s1, s2, sub1, sub2)
        {
            if (!equal_schemas(*sub1, *sub2)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

// PRE: mapping has enough size to store all variables of all Schemas s1 we are going to find, i.e.,
//  mapping is an array of unsigneds with max(Schemas s1.v).
// POST: in mapping the first equivalences of variables are registered, so after of the call to the
//  set-schema version, if no conflict is found, mapping contains the correct equivalence between variables.
// NOTE: this is done taking into account that variables are named from 1 to n. If that is not the case,
//  we would use a hash-map from unsigneds to unsigneds.
bool equivalent_schemas(Schema s1, Schema s2, Variable* mapping)
{
    if (s1.type == VARIABLE_SCHEMA && s2.type == VARIABLE_SCHEMA) {
        Variable prev_match = mapping[s1.v];
        if (prev_match == 0) {
            // NOTE: first comparison between variables - register mapping and return true
            mapping[s1.v] = s2.v;
            return true;
        } else {
            return prev_match == s2.v;
        }
    }

    if (s1.type == GENERAL_SCHEMA && s2.type == GENERAL_SCHEMA && s1.arity == s2.arity) {
        foreach_in_schemas(s1, s2, sub1, sub2)
        {
            if (!equivalent_schemas(*sub1, *sub2, mapping)) {
                return false;
            }
        }
        return true;
    }

    return false;
}

bool schema_contains_variables(Schema schema) {
    if (schema.type == VARIABLE_SCHEMA) {
        return true;
    }
    foreach_in_schema(schema, sub) {
        if (schema_contains_variables(*sub)) {
            return true;
        }
    }
    return false;
}

// TODO: we could do an Arena version of this function.
void variables_in_schema(Schema schema, SetVariables* vars)
{
    if (schema.type == VARIABLE_SCHEMA) {
        insert_to_set_variables(vars, schema.v);
    } else {
        foreach_in_schema(schema, subschema)
        {
            variables_in_schema(*subschema, vars);
        }
    }
}

// PRE: variables start from 1 => If 0 returned, there was no variable in the Schema
Variable min_v_in_schema(Schema schema)
{
    if (schema.type == VARIABLE_SCHEMA) {
        return schema.v;
    }

    Variable min_v = 0;
    foreach_in_schema(schema, subschema)
    {
        min_v = MIN(min_v, min_v_in_schema(*subschema));
    }
    return min_v;
}

// PRE: variables start from 1 => If 0 returned, there was no variable in the Schema
Variable max_v_in_schema(Schema schema)
{
    if (schema.type == VARIABLE_SCHEMA) {
        return schema.v;
    }

    Variable max_v = 0;
    foreach_in_schema(schema, subschema)
    {
        max_v = MAX(max_v, max_v_in_schema(*subschema));
    }
    return max_v;
}

unsigned num_distinct_schema_variables(Schema schema) {
    SetVariables vars = create_set_variables_defsize(); 
    variables_in_schema(schema, &vars);
    unsigned result = vars.num_variables;
    free_set_variables(vars);
    return result;
}

void increment_variables_in_schema(Schema *schema, Variable increment)
{
    if (schema->type == VARIABLE_SCHEMA) {
        schema->v += increment;
    } else {
        foreach_in_schema(*schema, subschema)
        {
            increment_variables_in_schema(subschema, increment);
        }
    }
}

void decrement_variables_in_schema(Schema *schema, Variable decrement)
{
    if (schema->type == VARIABLE_SCHEMA) {
        schema->v -= decrement;
    } else {
        foreach_in_schema(*schema, subschema)
        {
            decrement_variables_in_schema(subschema, decrement);
        }
    }
}

void normalize_schema_variables_(Schema *s, Variable *mapping, unsigned *num_found){
    if(s->type == VARIABLE_SCHEMA) {
        Variable old = s->v;
        Variable new = mapping[old];
        if (new) {
            s->v = new;
        } else {
            *num_found++;
            mapping[old] = *num_found;
            s->v = *num_found;
        }
    } else {
        foreach_in_schema(*s, sub) {
            normalize_schema_variables_(sub, mapping, num_found);
        }
    }
}
void normalize_schema_variables_arena(Schema *schema, Arena *arena){
    ArenaState initial_arena_state = register_state_arena(arena);
    
    Variable max_v = max_v_in_schema(*schema);
    Variable *mapping = PUSH_ARRAY_ZERO(arena, *mapping, 1 + max_v);
    
    unsigned num_found = 0;

    normalize_schema_variables_(schema, mapping, &num_found);

    pop_to_state_arena(arena, initial_arena_state);
}
void normalize_schema_variables(Schema *schema){
    Variable max_v = max_v_in_schema(*schema);
    Variable *mapping = CALLOC_ARRAY(*mapping, 1 + max_v);
    
    unsigned num_found = 0;
    
    normalize_schema_variables_(schema, mapping, &num_found);
    
    free(mapping);
}

// NOTE: we are making shallow copies of substitution, not deep copies.
void substitute_arena(Schema original, Variable v, Schema substitution, Schema* result, Arena* arena)
{
    if (original.type == VARIABLE_SCHEMA) {
        if (original.v == v) {
            *result = substitution;
        } else {
            init_variable_schema(result, original.v);
        }
    } else {
        init_general_schema_arena(result, original.arity, arena);
        for (size_t i = 0; i < original.arity; ++i) {
            substitute_arena(original.subschemas[i], v, substitution, result->subschemas + i, arena);
        }
    }
}

// NOTE: version that receives a set of variables to avoid a for loop to the other version, which would require
//  extra copies of consecutive *results into original and several recursions!
void substitute_vars_arena(Schema original, SetVariables vars, Schema substitution, Schema* result, Arena* arena)
{
    if (original.type == VARIABLE_SCHEMA) {
        if (is_in_set_variables(vars, original.v)) { // Only change
            *result = substitution;
        } else {
            init_variable_schema(result, original.v);
        }
    } else {
        init_general_schema_arena(result, original.arity, arena);
        for (size_t i = 0; i < original.arity; ++i) {
            substitute_vars_arena(original.subschemas[i], vars, substitution, result->subschemas + i, arena);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARSING ////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

static Schema read_schema(char** schema_str, Arena* arena)
{
    Schema result;

    unsigned v;
    if (sscanf(*schema_str, "$%u", &v) == 1) {
        *schema_str += 1 + num_digits(v);
        init_variable_schema(&result, v);
        return result;
    }

    unsigned arity;
    if (sscanf(*schema_str, "%u:[", &arity) == 1) {
        init_general_schema_arena(&result, arity, arena);
        *schema_str += num_digits(arity) + 2;
        for (size_t i = 0; i < arity; ++i) {
            result.subschemas[i] = read_schema(schema_str, arena);
            if (i != arity - 1) {
                ++(*schema_str); // Skip comma between subschemas (no comma after the last one!)
            }
        }
        *schema_str += 1; // Skip closing bracket
        return result;
    }

    fprintf(stderr, "read_schema: Unexpected schema format!\n");
    exit(1);
}

static unsigned scan_num_schemas_in_set(char* line)
{
    unsigned num_cols = 0;
    unsigned brackets = 0;
    while (*line != '\n' && *line != '\0') {
        unsigned v, arity;
        if (sscanf(line, "$%u", &v) == 1) {
            if (brackets == 0) {
                ++num_cols;
            }
            line += 1 + num_digits(v);
        } else if (sscanf(line, "%u:[", &arity) == 1) {
            if (brackets == 0) {
                ++num_cols;
            }
            ++brackets;
            line += num_digits(arity) + 1 + 1;
        } else if (*line == ']') {
            --brackets;
            ++line;
        } else if (*line == ',') {
            ++line;
        } else {
            fprintf(stderr, "scan_num_schemas_in_set: Unexpected set schema format!\n");
            exit(2);
        }
    }
    return num_cols;
}

// NOTE: the Schema root node that is returned by value is not stored in the Arena!
SetSchema read_set_schema(char* line, Arena* arena)
{
    // Precalculate the number of columns; i.e., the number of schemas in the set-schema
    unsigned num_cols = scan_num_schemas_in_set(line);
    SetSchema set_schema = GENERAL_SCHEMA_LITERAL(num_cols, arena);

    // Parse each schema
    unsigned i = 0;
    while (*line != '\0' && *line != '\n') { // NOTE: we could also use num_cols as a counter...
        set_schema.subschemas[i++] = read_schema(&line, arena);
        ++line;
    }

    return set_schema;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END SCHEMAS /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: we are ignoring v->v dependencies, so we don't need a special treatment for that case.
bool is_self_dependency(Variable v, Schema schema)
{
    if (schema.type == VARIABLE_SCHEMA) {
        return v == schema.v;
    }

    foreach_in_schema(schema, subschema)
    {
        if (is_self_dependency(v, *subschema)) {
            return true;
        }
    }
    return false;
}

// NOTE: for debugging
// PRE: there must not be any repeated Schemas in the list of Dependencies of each variable! (set semantics)
unsigned number_of_dependencies(SetDependencies *dependencies)
{
    unsigned num_dependencies = 0;
    foreach_in_arraylist(DependencyPair, pair, *dependencies)
    {
        num_dependencies += pair->schemas.size;
    }
    return num_dependencies;
}

// NOTE: resizing risk for arena when adding to arraylists. Unavoidable. We don't know the number of dependencies
//  that will end up having the new variable with dependencies v. We don't know how many extra variables with
//  dependencies we will have neither...
bool insert_to_dependencies_baseline(SetDependencies *dependencies, Variable v, Schema *s, Arena *arena)
{
    for (unsigned i = 0; i < dependencies->size; ++i) {
        DependencyPair* pair = dependencies->array + i;
        if (pair->v == v) {
            int contained = add_no_repeated_to_array_list_schema_ptr_arena(&pair->schemas, s, arena);
            return contained != CONTAINED;
        }
    }

    // First dependency for v
    ArrayListSchemaPtr first_schemas_v = create_array_list_schema_ptr_arena(10, arena);
    add_to_array_list_schema_ptr_arena(&first_schemas_v, s, arena);
    DependencyPair first_dependency_pair_v = { .v = v, .schemas = first_schemas_v };
    add_to_array_list_dependency_pair_arena(dependencies, first_dependency_pair_v, arena);
    return true;
}

// NOTE: resizing risk because of Arena and ArrayLists
// destination gets the dependencies found in source that it didn't contain initially.
void union_of_dependencies_baseline(SetDependencies* destination, SetDependencies source, Arena* arena)
{
    for (unsigned i = 0; i < source.size; ++i) {
        DependencyPair pair2 = source.array[i];
        Variable v = pair2.v;
        ArrayListSchemaPtr schemas2 = pair2.schemas;

        unsigned vpos = find_v_in_array_list_dependency_pair(*destination, v);
        if (vpos == destination->size) {
            add_to_array_list_dependency_pair_arena(destination, pair2, arena); // NOTE: they share the schemas (no memory management problem with Arenas)
        } else {
            extend_no_repeated_array_list_schema_ptr_arena(&destination->array[vpos].schemas, schemas2, arena);
        }
    }
}

// NOTE: these two functions are for debugging asserts
bool has_self_dependency_baseline(ArrayListSchemaPtr schemas, Variable v)
{
    foreach_in_arraylist(SchemaPtr, schema, schemas)
    {
        if (is_self_dependency(v, **schema)) {
            return true;
        }
    }
    return false;
}
bool contains_self_dependency_baseline(SetDependencies dependencies)
{
    foreach_in_arraylist(DependencyPair, pair, dependencies)
    {
        if (has_self_dependency_baseline(pair->schemas, pair->v)) {
            return true;
        }
    }
    return false;
}

bool equal_helper_set_dependencies(SetDependencies dependencies1, SetDependencies dependencies2, bool equivalence, Variable* mapping, bool ignore_empties)
{
    if (dependencies1.size != dependencies2.size) {
        return false;
    }
    unsigned num_vars_with_dependencies = dependencies1.size;

    for (unsigned index1 = 0; index1 < num_vars_with_dependencies; ++index1) {
        DependencyPair pair1 = dependencies1.array[index1];
        Variable v = pair1.v;
        ArrayListSchemaPtr schemas1 = pair1.schemas; // Taking by value is not problematic because we're not modifying

        // Find v's match in dependencies2
        Variable v_match = mapping[v];
        unsigned index2 = find_v_in_array_list_dependency_pair(dependencies2, v_match);
        if (index2 == num_vars_with_dependencies) {
            return false; // v_match wasn't found in dependencies2
        }

        ArrayListSchemaPtr schemas2 = dependencies2.array[index2].schemas; // Taking by value is not problematic because we're not modifying

        if (schemas1.size != schemas2.size) {
            return false;
        }
        unsigned num_dependencies_of_v = schemas1.size;

        // Unordered equivalence between schemas1 and schemas2
        for (unsigned i = 0; i < num_dependencies_of_v; ++i) {
            Schema *s1 = schemas1.array[i];
            
            if (ignore_empties && is_empty(*s1)) {
                continue;
            }
            
            if (equivalence){
                if (!contains_equivalent_array_list_schema_ptr(schemas2, s1, mapping)) {
                    return false; // There is a dependency for v in dependencies1 that is not found in dependencies2
                }
            } else {
                if (!contains_array_list_schema_ptr(schemas2, s1)) {
                    return false; // There is a dependency for v in dependencies1 that is not found in dependencies2
                }
            }
        }
    }
    return true;
}

bool equal_set_dependencies(SetDependencies dependencies1, SetDependencies dependencies2) {
    return equal_helper_set_dependencies(dependencies1, dependencies2, false, NULL, false);
}

// PRE: equivalent_set_schemas was called so mapping already contains the variable equivalences!
bool equivalent_set_dependencies(SetDependencies dependencies1, SetDependencies dependencies2, Variable* mapping) {
    return equal_helper_set_dependencies(dependencies1, dependencies2, true, mapping, false);
}

// NOTE: having the <> schema as a dependency of a variable is not important for normalization, so in most cases we consider
//  equivalent sets of dependencies whose only difference is having empty schemas as extra dependencies.
bool equivalent_set_dependencies_ignoring_empties(SetDependencies dependencies1, SetDependencies dependencies2, Variable* mapping) {
    equal_helper_set_dependencies(dependencies1, dependencies2, true, mapping, true);
}


bool equivalent_set_schemas_and_dependencies_helper(
    SetSchema set_schema1, SetSchema set_schema2,
    SetDependencies dependencies1, SetDependencies dependencies2,
    bool ignore_empties)
{
    unsigned num_vars1 = max_v_in_set_schema(set_schema1);
    
    Variable *mapping = CALLOC_ARRAY(*mapping, 1 + num_vars1);
    CHECK_CALLOC(mapping);
    
    bool ok_set_schemas = equivalent_set_schemas(set_schema1, set_schema2, mapping);
    bool ok_dependendencies = 
        ignore_empties ? equivalent_set_dependencies_ignoring_empties(dependencies1, dependencies2, mapping) :
            equivalent_set_dependencies(dependencies1, dependencies2, mapping);
    
    if (global_print_debugging) printf("ok_set_schemas = %u\nok_dependencies = %u\n", ok_set_schemas, ok_dependendencies);
    free(mapping);
    return ok_set_schemas && ok_dependendencies;
}

// NOTE: check schema and dependency equivalence encapsulating the calculation of variable mapping.
bool equivalent_set_schemas_and_dependencies(
    SetSchema set_schema1, SetSchema set_schema2,
    SetDependencies dependencies1, SetDependencies dependencies2)
{
    return equivalent_set_schemas_and_dependencies_helper(set_schema1, set_schema2, dependencies1, dependencies2, false);
}

bool equivalent_set_schemas_and_dependencies_ignoring_empties(
    SetSchema set_schema1, SetSchema set_schema2,
    SetDependencies dependencies1, SetDependencies dependencies2)
{
    return equivalent_set_schemas_and_dependencies_helper(set_schema1, set_schema2, dependencies1, dependencies2, true);
}

void increment_variables_in_set_dependencies(SetDependencies dependencies, Variable increment)
{
    foreach_in_arraylist(DependencyPair, pair, dependencies)
    {
        pair->v += increment;
        foreach_in_arraylist(SchemaPtr, schema, pair->schemas)
        {
            increment_variables_in_schema(*schema, increment);
        }
    }
}

void decrement_variables_in_set_dependencies(SetDependencies dependencies, Variable decrement)
{
    foreach_in_arraylist(DependencyPair, pair, dependencies)
    {
        pair->v -= decrement;
        foreach_in_arraylist(SchemaPtr, schema, pair->schemas)
        {
            decrement_variables_in_schema(*schema, decrement);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// PARSING ////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

static ArrayListSchemaPtr read_v_s_dependencies(char *line, Arena *arena) {
    Schema root = read_set_schema(line, arena);

    ArrayListSchemaPtr result = create_array_list_schema_ptr_arena(root.arity, arena);
    result.size = root.arity;
    for (unsigned i = 0; i < root.arity; ++i) {
        result.array[i] = root.subschemas + i;
    }

    return result;
}

SetDependencies read_set_dependencies(FILE* stream, unsigned num_vars_with_dependencies, Arena* arena)
{
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    // NOTE: no resizing risk for Arena, as we know the numbers of variables with dependencies beforehand
    SetDependencies dependencies = create_array_list_dependency_pair_arena(num_vars_with_dependencies, arena);

    while (num_vars_with_dependencies--) {
        read = getline(&line, &len, stream);
        unsigned v;
        if (sscanf(line, "$%u <- [", &v) == 0) {
            fprintf(stderr, "read_set_dependencies: Unexpected list of dependencies format!\n");
            exit(3);
        }

        // Adapt the read line to obtain the number of schemas and the schemas themselves (to keep line for the posterior free call) (quite tricky, not robust interaction with read_set_schema)
        line[read - 2] = '\0'; // read-1 == '\n', -2 == ']' (the closing braquet of the list of dependencies of v)
        char* lineptr = line + 1 + num_digits(v) + 5; // Focus on the beginning of the first schema

        DependencyPair pair = { .v = v, .schemas = read_v_s_dependencies(lineptr, arena) };
        // NOTE: we can directly set without bound checking thanks to having read the number of vars with dependencies
        unsafe_add_to_array_list(dependencies, pair);
    }

    if (line) {
        free(line);
    }
    return dependencies;
}

int read_set_schema_with_dependencies(
    FILE *stream, SetSchema *set_schema, SetDependencies *dependencies, Arena *arena)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    if((read = getline(&line, &len, stream)) != -1){
        unsigned num_vars_with_dependencies;
        if(sscanf(line, "%u, ", &num_vars_with_dependencies) != 1) {
            free(line);
            return 1; // Common schema doesn't exist
        }

        // Skip "num_vars_with_dependencies, "
        char *lineptr = line + num_digits(num_vars_with_dependencies) + 2;

        *set_schema = read_set_schema(lineptr, arena);
        *dependencies = read_set_dependencies(stream, num_vars_with_dependencies, arena);

        free(line);
        return 0;   // Common schema exists.
    }

    if(line){ free(line); }
    return -1;  // EOF
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMMON SCHEMA ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO(YA): change common_schema to avoid the precondition of independent variable schema values! with max_op1...
//  No normalization of the schema variables till much later, when they are deepcopied in the matrix's memory.
//  Taking max_op1 is not valid right now for two reasons:
//  1. We are making shallow copies in common schema in case of general schemas with different sizes, so in the 
//      longest one we can have Schema variables 2 whose values are not incremented with max_op1...
//  2. In common set schema, we make a union of dependencies with the original ones, so dependencies 2 will have
//      schema variables 2 that are not incremented with max_op1...
// Have to think a better solution for this problem. Maybe we can define a common set schema version that wraps
//  schema variable renaming/normalization??? Take into account that we would want to minimize the amount of
//  renaming performed, and most importantly, we have to be careful with several renamings to potentially shared
//  schema nodes!!!

// Returns the number of dependencies inserted, or -1 if a self-dependency was arised.
int common_schema_baseline(Schema *s1, Schema *s2, Schema* common, SetDependencies* dependencies, Arena* arena)
{
    if (s1->type == VARIABLE_SCHEMA && s2->type == VARIABLE_SCHEMA && s1->v == s2->v) {
        // Do not insert v->v dependency
        init_variable_schema(common, s1->v);
        return 0;
    }

    if (s1->type == VARIABLE_SCHEMA) {
        Variable v = s1->v;
        if (is_self_dependency(v, *s2)) {
            return -1;
        }
        init_variable_schema(common, v);
        return insert_to_dependencies_baseline(dependencies, v, s2, arena);
    }

    if (s2->type == VARIABLE_SCHEMA) {
        Variable v = s2->v;
        if (is_self_dependency(v, *s1)) {
            return -1;
        }
        init_variable_schema(common, v);
        return insert_to_dependencies_baseline(dependencies, v, s1, arena);
    }

    size_t min_arity, max_arity;
    Schema* longest_schema;
    if (s1->arity < s2->arity) {
        min_arity = s1->arity;
        max_arity = s2->arity;
        longest_schema = s2;
    } else {
        min_arity = s2->arity;
        max_arity = s1->arity;
        longest_schema = s1;
    }

    init_general_schema_arena(common, max_arity, arena);
    int total_inserted_dependencies = 0;
    size_t i = 0;
    for (; i < min_arity; ++i) {
        int num_inserted_dependencies = common_schema_baseline(s1->subschemas + i, s2->subschemas +i,
            common->subschemas + i, dependencies, arena);
        if (num_inserted_dependencies < 0) {
            return -1;
        }
        total_inserted_dependencies += num_inserted_dependencies;
    }
    for (; i < max_arity; ++i) {
        // NOTE: shallow copy, because we are storing the intermediate results in a operation-postprocess arena
        //  and at the end, we deepcopy the final schemas in each matrix's memory
        common->subschemas[i] = longest_schema->subschemas[i];
    }

    return total_inserted_dependencies;
}

// Special version that is only interested in the new dependencies that may arise
int common_schema_dependencies_baseline(Schema *s1, Schema *s2, ArrayListDependencyPair* dependencies, Arena* arena)
{
    if (s1->type == VARIABLE_SCHEMA && s2->type == VARIABLE_SCHEMA && s1->v == s2->v) {
        // Do not insert v->v dependency
        return 0;
    }

    if (s1->type == VARIABLE_SCHEMA) {
        if (is_self_dependency(s1->v, *s2)) {
            return -1;
        }
        return insert_to_dependencies_baseline(dependencies, s1->v, s2, arena);
    }
    if (s2->type == VARIABLE_SCHEMA) {
        if (is_self_dependency(s2->v, *s1)) {
            return -1;
        }
        return insert_to_dependencies_baseline(dependencies, s2->v, s1, arena);
    }

    int num_new_dependencies = 0;
    unsigned min_arity = MIN(s1->arity, s2->arity);
    for (unsigned i = 0; i < min_arity; ++i) {
        int num_new_deps = common_schema_dependencies_baseline(s1->subschemas +i, s2->subschemas + i, dependencies, arena);
        if (num_new_deps == -1) {
            return -1;
        }
        num_new_dependencies += num_new_deps;
    }
    return num_new_dependencies;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END COMMON SCHEMA ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// THETA OPERATOR //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 * NOTE: since we are passing v_dependencies by value, it's size will remain the same even if
 *       new dependencies would be added to its array (only the proper instance in referenced
 *       in dependencies will be updated when inserting new dependencies). That way, we ensure
 *       that we only take into account the schemas that were initially stored in the list of
 *       dependencies of the variable at the start of this loop iteration.
 */
int theta_rule1_baseline(ArrayListDependencyPair* dependencies, ArrayListSchemaPtr v_dependencies, Arena* arena)
{
    int num_new_dependencies = 0;
    for (size_t i = 0; i < v_dependencies.size; ++i) {
        for (size_t j = i + 1; j < v_dependencies.size; ++j) {
            int num_new_deps = common_schema_dependencies_baseline(v_dependencies.array[i], v_dependencies.array[j], dependencies, arena);
            if (num_new_deps == -1) {
                return -1;
            }
            num_new_dependencies += num_new_deps;
        }
    }
    return num_new_dependencies;
}

/**
 * RETURNS: if < 0 (-1), a self-dependency was found (halt).
 *          else, the number of new dependencies added.
 *
 * NOTE: as pair->schemas is modified (DependencyPair itself is composed of ArrayList, not ArrayListPtr),
 *  concretely its size, we need to pass it by reference.
 * NOTE: resizing risk because of Arena and ArrayLists.
 */
int theta_rule2_baseline(SetDependencies* dependencies, DependencyPair* pair, Arena* arena)
{
    int num_new_dependencies = 0;
    Variable v = pair->v;
    ArrayListSchemaPtr* schemas = &pair->schemas;

    // NOTE: iteration based on indexes to prevent dangling pointers from resizing
    // NOTE: necessary to take the initial size of the schemas because it may be incremented, and in case of self-dependencies,
    //   it would possibly loop infinitely!!! Additionally, this way we have the same behaviour in both rules!
    SetVariables ws = create_set_variables_defsize();
    size_t initial_size = schemas->size;
    for (size_t i = 0; i < initial_size; ++i) {
        Schema *gamma = schemas->array[i];
        variables_in_schema(*gamma, &ws);
        foreach_in_setvariables(ws, node_var)
        {
            Variable w = node_var->v;
            unsigned wpos = find_v_in_array_list_dependency_pair(*dependencies, w);
            if (wpos == dependencies->size) {
                continue;
            }
            ArrayListSchemaPtr schemas_ = dependencies->array[wpos].schemas;

            foreach_in_arraylist(SchemaPtr, schema_, schemas_)
            {
                Schema *gamma_ = *schema_;
                Schema *gamma__ = PUSH_SINGLE(arena, *gamma__);
                substitute_arena(*gamma, w, *gamma_, gamma__, arena);
                if (is_self_dependency(v, *gamma__)) {
                    free_set_variables(ws);
                    return -1;
                }
                int contained = add_no_repeated_to_array_list_schema_ptr_arena(schemas, gamma__, arena);
                if (contained != CONTAINED) {
                    ++num_new_dependencies;
                }
            }
        }
        clear_set_variables(&ws);
    }
    free_set_variables(ws);
    return num_new_dependencies;
}

/**
 * RETURNS: < 0 (-1) if it has halted because a self-dependency was found
 *          else, the number of new dependencies discovered
 */
int theta_operator_baseline(ArrayListDependencyPair* dependencies, Arena* arena)
{
    int num_new_dependencies = 0;
    int num_new_deps1, num_new_deps2; // NOTE: we could have a single foreach accumulator, but this way we can have more information when debugging
    do {
        num_new_deps1 = num_new_deps2 = 0;
        // NOTE: rule1 is much simpler than rule2 as it only needs to access to the schemas a variable depends on, not other
        //   variables. For that reason, we first call to it to try to be more cache-locality-friendly.

        // NOTE: we iterate using indexes instead of pointers because when adding new dependencies the ArrayList of DependencyPairs
        //   can be resized (so the iterating pointers would be invalidated...)

        for (size_t i = 0; i < dependencies->size; ++i) {
            // OPT: an obvious possible optimization is to avoid reaplying this rule to the same pair of gamma-gamma_
            DependencyPair* pair = dependencies->array + i;
            int num1 = theta_rule1_baseline(dependencies, pair->schemas, arena);
            if (num1 == -1) {
                return -1;
            }
            num_new_deps1 += num1;

            int num2 = theta_rule2_baseline(dependencies, pair, arena);
            if (num2 == -1) {
                return -1;
            }
            num_new_deps2 += num2;
        }
        num_new_dependencies += num_new_deps1 + num_new_deps2;
    } while (num_new_deps1 + num_new_deps2);
    return num_new_dependencies;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END THETA OPERATOR //////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMMON SET SCHEMA WEAK VERSION //////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void remove_variables_not_in_set_schema_from_dependencies(
    SetSchema* set_schema, SetDependencies* dependencies, Arena* arena)
{
    SetVariables final_vs = create_set_variables_defsize();
    variables_in_set_schema(*set_schema, &final_vs);

    SetVariables removed_vs_with_dependencies = create_set_variables_defsize();
    // NOTE: from right to left to diminish leftwise copying and to avoid extra index corrections and overhead of call function to get
    for (int i = dependencies->size - 1; i >= 0; --i) {
        DependencyPair pair = dependencies->array[i];
        if (!is_in_set_variables(final_vs, pair.v)) {
            remove_index_from_array_list_dependency_pair(dependencies, i);
            insert_to_set_variables(&removed_vs_with_dependencies, pair.v);
        }
    }

    // NOTE: thanks to the use of rule 2, the schema resulting from substituting the longest dependency of the removed
    // variables is already in the set of dependencies, so we can simply remove the schemas that contain those variables.
    // We only need to consider the removed variables that didn't have any dependences, to substitute them for <>.
    SetVariables schema_vs = create_set_variables_defsize();
    SetVariables removed_vs_without_dependencies_in_schema = create_set_variables_defsize();
    foreach_in_arraylist(DependencyPair, pair, *dependencies)
    {
        // NOTE: from right to left to diminish leftwise copying and to avoid extra index corrections and overhead of call function to get
        ArrayListSchemaPtr* dependency_schemas = &pair->schemas;
        for (int i = dependency_schemas->size - 1; i >= 0; --i) {
            // NOTE: we take by value because if not, when removing, it will point to the next schema before applying the substitution by <>
            Schema *schema = dependency_schemas->array[i];
            variables_in_schema(*schema, &schema_vs);

            bool contains_removed_v_with_dependencies = false;
            foreach_in_setvariables(schema_vs, v_node)
            {
                Variable v = v_node->v;
                if (is_in_set_variables(removed_vs_with_dependencies, v)) {
                    contains_removed_v_with_dependencies = true;
                    break;
                }
                if (!is_in_set_variables(final_vs, v)) {
                    insert_to_set_variables(&removed_vs_without_dependencies_in_schema, v);
                }
            }

            // If contains some removed variable with dependencies (in removed_vs_with_dependencies): remove
            if (contains_removed_v_with_dependencies) {
                remove_index_from_array_list_schema_ptr(dependency_schemas, i);
            }
            // If only contains some removed variable without dependencies (not removed_vs_with_dependencies nor final_vs):
            //  remove and add substitution (all those variables) with <>
            else if (removed_vs_without_dependencies_in_schema.num_variables) {
                remove_index_from_array_list_schema_ptr(dependency_schemas, i);
                Schema empty = empty_schema();
                Schema *emptied = PUSH_SINGLE(arena, *emptied);
                substitute_vars_arena(*schema, removed_vs_without_dependencies_in_schema, empty, emptied, arena);
                add_no_repeated_to_array_list_schema_ptr_arena(dependency_schemas, emptied, arena);
            }
            clear_set_variables(&removed_vs_without_dependencies_in_schema);
            clear_set_variables(&schema_vs);
        }
    }
    free_set_variables(removed_vs_without_dependencies_in_schema);
    free_set_variables(schema_vs);

    free_set_variables(removed_vs_with_dependencies);
    free_set_variables(final_vs);

    // NOTE: we shouldn't get new dependencies or a self-dependency after the previous operations
    int theta_result = theta_operator_baseline(dependencies, arena);
    assert(theta_result == 0);

    assert(!contains_self_dependency_baseline(*dependencies));
}

// NOTE: resizing risk because of Arena and ArrayList
bool common_set_schema_baseline_(
    SetSchema set_schema1, SetDependencies dependencies1,
    SetSchema set_schema2, SetDependencies dependencies2,
    SetSchema* common_set_schema, SetDependencies* common_dependencies,
    Arena* arena, bool remove_variables_not_in_resulting_common_schema)
{
    if (set_schema1.arity != set_schema2.arity) {
        return false;
    }
    unsigned num_schemas = set_schema1.arity; // num columns
    assert(num_schemas > 0);

    *common_set_schema = GENERAL_SCHEMA_LITERAL(num_schemas, arena);
    // NOTE: resizing risk for Arena, unknown number of variables with new dependencies (even variables that initially didn't have any)
    SetDependencies new_dependencies = create_array_list_dependency_pair_arena(10, arena);

    for (unsigned i = 0; i < num_schemas; ++i) {
        Schema *s1 = set_schema1.subschemas + i;
        Schema *s2 = set_schema2.subschemas + i;
        Schema *common_schema = common_set_schema->subschemas + i;
        int num_new_dependencies = common_schema_baseline(s1, s2, common_schema, &new_dependencies, arena);
        if (num_new_dependencies < 0) {
            return false;
        }
    }
    // common_set_schema calculated

    // Start calculating the final common_dependencies set as the union of the two inputs and new_dependencies
    // NOTE: capacity can be greater than final size (we can have dependencies of the same variable in several sets)
    //  therefore, there is no risk of resizing
    *common_dependencies = create_array_list_dependency_pair_arena(dependencies1.size + dependencies2.size + new_dependencies.size, arena);

    extend_array_list_dependency_pair_arena(common_dependencies, dependencies1, arena);
    // NOTE: here, when adding new depencies to the array list of an already stored variable, we can have resizing
    union_of_dependencies_baseline(common_dependencies, dependencies2, arena);
    union_of_dependencies_baseline(common_dependencies, new_dependencies, arena);

    // Now, we must calculate the hidden dependencies, in case there is a self dependency
    int num_new_dependencies = theta_operator_baseline(common_dependencies, arena);
    if (num_new_dependencies < 0) {
        return false;
    }

    assert(!contains_self_dependency_baseline(*common_dependencies));

    if (remove_variables_not_in_resulting_common_schema) {
        remove_variables_not_in_set_schema_from_dependencies(common_set_schema, common_dependencies, arena);
    }

    return true;
}

bool common_set_schema_baseline(
    SetSchema set_schema1, SetDependencies dependencies1,
    SetSchema set_schema2, SetDependencies dependencies2,
    SetSchema* common_set_schema, SetDependencies* common_dependencies,
    Arena* arena)
{
    return common_set_schema_baseline_(
        set_schema1, dependencies1,
        set_schema2, dependencies2,
        common_set_schema, common_dependencies,
        arena, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END COMMON SET SCHEMA WEAK VERSION //////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMMON SET SCHEMA STRICT VERSION ////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void calculate_num_appearences_of_variables_in_schema(Schema schema, ArrayListVarNum* varnums, Arena* arena)
{
    if (schema.type == VARIABLE_SCHEMA) {
        unsigned varnum_pos = find_v_in_array_list_varnum(*varnums, schema.v);
        if (varnum_pos == varnums->size) {
            VarNum initial_mapping = { .v = schema.v, .num_appearences = 1 };
            add_to_array_list_varnum_arena(varnums, initial_mapping, arena);
        } else {
            VarNum* mapping = varnums->array + varnum_pos;
            mapping->num_appearences++;
        }
    } else {
        foreach_in_schema(schema, subschema)
        {
            calculate_num_appearences_of_variables_in_schema(*subschema, varnums, arena);
        }
    }
}

static inline void calculate_num_appearences_of_variables_in_set_schema(SetSchema set_schema, ArrayListVarNum* varnums, Arena* arena)
{
    calculate_num_appearences_of_variables_in_schema(set_schema, varnums, arena);   
}

// OPT: for the strict version of common_set_schema_baseline
// - First check:
//      + Better O-performance with ordered maps (and their iterators) with better search performance for variables than O(n) like arraylists
//      + Another composed approach would be to use unordered-(hash-)maps for number of appearences, and an arraylist for the order of the vars
bool first_check(SetSchema set_schema1, SetSchema set_schema2, Arena* arena)
{
    ArrayListVarNum var_to_nums1 = create_array_list_varnum_arena(10, arena);
    ArrayListVarNum var_to_nums2 = create_array_list_varnum_arena(10, arena);

    calculate_num_appearences_of_variables_in_set_schema(set_schema1, &var_to_nums1, arena);
    calculate_num_appearences_of_variables_in_set_schema(set_schema2, &var_to_nums2, arena);

    // Same quantity of distinct variables in both set_schemas
    if (var_to_nums1.size != var_to_nums2.size) {
        return false;
    }

    // Same quantity of corresponding distinct variables according to order of appearence in each set_schema
    unsigned size = var_to_nums1.size;
    for (unsigned i = 0; i < size; ++i) {
        if (var_to_nums1.array[i].num_appearences != var_to_nums2.array[i].num_appearences) {
            return false;
        }
    }

    return true;
}

bool unique_dependency_between_vars__(Schema schema, SetVariables* variables, unsigned* num_vars_in_schema)
{
    if (schema.type == VARIABLE_SCHEMA) {
        ++(*num_vars_in_schema);
        if (*num_vars_in_schema > 1) {
            return false;
        }
        return insert_to_set_variables(variables, schema.v) != SET_INSERT_ALREADY_CONTAINED;
    }
    foreach_in_schema(schema, subschema)
    {
        if (!unique_dependency_between_vars__(*subschema, variables, num_vars_in_schema)) {
            return false;
        }
    }
    return true;
}
bool unique_dependency_between_vars_(ArrayListSchemaPtr schemas, SetVariables* variables)
{
    foreach_in_arraylist(SchemaPtr, s, schemas)
    {
        unsigned num_vars_in_schema = 0;
        if (!unique_dependency_between_vars__(**s, variables, &num_vars_in_schema)) {
            return false;
        }
    }
    return true;
}
bool unique_dependency_between_vars(SetDependencies dependencies)
{
    SetVariables variables_in_list_of_dependencies = create_set_variables_defsize();
    foreach_in_arraylist(DependencyPair, pair, dependencies)
    {
        if (!unique_dependency_between_vars_(pair->schemas, &variables_in_list_of_dependencies)) {
            free_set_variables(variables_in_list_of_dependencies);
            return false;
        }
        clear_set_variables(&variables_in_list_of_dependencies);
    }
    free_set_variables(variables_in_list_of_dependencies);
    return true;
}

bool common_set_schema_strict_baseline(
    SetSchema set_schema1, SetDependencies dependencies1,
    SetSchema set_schema2, SetDependencies dependencies2,
    SetSchema* common_set_schema, SetDependencies* common_dependencies,
    Arena* arena)
{
    bool result = first_check(set_schema1, set_schema2, arena);
    result = result && common_set_schema_baseline_(set_schema1, dependencies1, set_schema2, dependencies2, 
                                           common_set_schema, common_dependencies, arena, false);
    result = result && first_check(set_schema1, *common_set_schema, arena);
    result = result && unique_dependency_between_vars(*common_dependencies);

    if (!result) {
        return false;
    }

    // TODO(YA): should make another version for which this operation is optional or completely removed (both, one
    //  internal and another external). Have to see where is interesting to avoid calling this function. I would say
    //  that we should only call just before we make the deepcopies of the resultant schemas and dependencies to the
    //  resultant matrix's arena. Además, las normalizaciones de los schema variables in place se harán después de 
    //  estos deepcopies.
    remove_variables_not_in_set_schema_from_dependencies(common_set_schema, common_dependencies, arena);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END COMMON SET SCHEMA STRICT VERSION ////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// OBTAIN COLUMN MAPPING FROM COMMON SET SCHEMA ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Schema longest_dependency(SetDependencies dependencies, Variable v){
    unsigned v_i = find_v_in_array_list_dependency_pair(dependencies, v);
    if (v_i == dependencies.size) {
        // v without dependencies --> longest dependency is the empty schema
        return empty_schema();
    }
    ArrayListSchemaPtr schemas = dependencies.array[v_i].schemas;
    assert(schemas.size > 0);

    Schema *result = schemas.array[0];
    foreach_after_first_in_arraylist(SchemaPtr, schema, schemas){
        if (schema_size(*schema) > schema_size(result)) {   
            result = *schema;
        }
    }

    return *result;
}

// NOTE: the input schema is not overwritten, a new copy is done for the normalized schema.
Schema normalized_schema(Schema schema, SetDependencies dependencies, Arena* arena){
    if (schema.type == VARIABLE_SCHEMA) {
        // NOTE: this simple version follows strictly the definition in the paper. We can still have some Schema-Variables,
        //  but we don't need to change them for <> because later we are only interested in the sizes of the schemas, and both
        //  <> and variable-schemas have size 1.
        Schema longest_dep = longest_dependency(dependencies, schema.v);
        return longest_dep;
        // NOTE: the most efficient option would be to define an empty() operation that changes all variables to <>,
        //  since we know that's the only change we need thanks to the theta operator.
    }

    Schema result; init_general_schema_arena(&result, schema.arity, arena);
    foreach_in_schemas(result, schema, subres, sub){
        *subres = normalized_schema(*sub, dependencies, arena);
    }
    return result;
}

// NOTE: column indexes are integers between and [1, total_cols]
//  We have one partition per each schema, i.e., per each free variable, that are constituted by contiguous integers, so
//  we will identify each partition with the starting value until (excluding) the starting value of the next partition (the
//  last one doesn't need a second number, total_cols is its final value).
void starting_column_indexes(SetSchema fragment_set_schema, unsigned *starting_columns){
    assert(fragment_set_schema.type == GENERAL_SCHEMA && fragment_set_schema.arity > 0);
    starting_columns[0] = 1;
    for(unsigned i = 1; i < fragment_set_schema.arity; ++i){
        starting_columns[i] = starting_columns[i - 1] + schema_size(&fragment_set_schema.subschemas[i - 1]);
    }
}

// Decide final ordering of free_vars (alphabetically)
ArrayListCharPtr final_free_vars_ordering(ArrayListCharPtr free_vars1, ArrayListCharPtr free_vars2, Arena *arena){
    // NOTE: no resizing risk (some free capacity in case of repeated vars)
    ArrayListCharPtr result = create_array_list_char_ptr_arena(free_vars1.size + free_vars2.size, arena);

    extend_array_list_char_ptr_arena(&result, free_vars1, arena);
    extend_no_repeated_array_list_char_ptr_arena(&result, free_vars2, arena);
    insertion_sort_arraylist_char_ptr(result);

    return result;
}

SetSchema set_schema_with_respect_to_free_vars(SetSchema set_schema, ArrayListUInt free_var_positions, Arena *arena){
    SetSchema result = GENERAL_SCHEMA_LITERAL(free_var_positions.size, arena);

    // NOTE: the copies are shallow
    unsigned i = 0;
    foreach_in_arraylist(UInt, pos, free_var_positions){
        if(*pos == free_var_positions.size){
            // NOTE: introducing an empty schema. The resulting common schema will be the schema of the other operand that has the free_var
            result.subschemas[i++] = EMPTY_SCHEMA_LITERAL;
        } else {
            result.subschemas[i++] = set_schema.subschemas[*pos];
        }
    }

    return result;
}

bool common_set_schema_free_vars_baseline(
    SetSchema set_schema1, SetDependencies dependencies1, ArrayListUInt free_var_positions1,
    SetSchema set_schema2, SetDependencies dependencies2, ArrayListUInt free_var_positions2,
    SetSchema *common_set_schema, SetDependencies *common_dependencies,
    Arena *arena)
{
    SetSchema set_schema1_wrt_free_vars = set_schema_with_respect_to_free_vars(set_schema1, free_var_positions1, arena);
    SetSchema set_schema2_wrt_free_vars = set_schema_with_respect_to_free_vars(set_schema2, free_var_positions2, arena);
    
    return common_set_schema_baseline(
        set_schema1_wrt_free_vars, dependencies1, 
        set_schema2_wrt_free_vars, dependencies2, 
        common_set_schema, common_dependencies,
        arena);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Schema iterator

DEFINE_ARRAYLIST_CREATE(SchemaIteratorNode, schema_iterator_node)
DEFINE_ARRAYLIST_CREATE_ARENA(SchemaIteratorNode, schema_iterator_node)

SchemaIterator create_schema_iterator(Schema *schema){
    ArrayListSchemaIteratorNode stack = create_array_list_schema_iterator_node(schema_depth(schema));
    SchemaIteratorNode node = { .schema = schema, .next_child = 0 };
    unsafe_add_to_array_list(stack, node);
    SchemaIterator it = { .stack = stack, .first_next = true };
    return it;
}

SchemaIterator create_schema_iterator_arena(Schema *schema, Arena *arena){
    ArrayListSchemaIteratorNode stack = create_array_list_schema_iterator_node_arena(schema_depth(schema), arena);
    SchemaIteratorNode node = { .schema = schema, .next_child = 0 };
    unsafe_add_to_array_list(stack, node);
    SchemaIterator it = { .stack = stack, .first_next = true };
    return it;
}

bool schema_iterator_next(SchemaIterator *iterator, Schema *next){
    if(iterator->stack.size == 0){
        return false;   // END
    }
    
    SchemaIteratorNode *node = &unsafe_last_in_array_list(iterator->stack);
    if(iterator->first_next){
        *next = *(node->schema);
        iterator->first_next = false;

    } else {
        if(node->schema->type == VARIABLE_SCHEMA || node->next_child == node->schema->arity){
            unsafe_remove_last_in_array_list(iterator->stack);
            return schema_iterator_next(iterator, next); // NOTE: recursive call to continue with the next child of parent or END
        }

        // NOTE: General schema with children left
        Schema *next_ptr = node->schema->subschemas + (node->next_child++);
        *next = *next_ptr;
        SchemaIteratorNode new_node = { .schema = next_ptr, .next_child = 0 };
        unsafe_add_to_array_list(iterator->stack, new_node);
    }

    return true;
}

// NOTE: when iterating through the new normalized common schema and the original, a (repeated) variable can have more extra
//  extending variables in the new one. Therefore, in the new one we would need more iteration steps to skip the schema corresponding
//  to the same variable. To avoid all those iterations and to keep both iterators pointing to the same corresponding schemas, 
//  we can ignore all the children nodes of the schema already returned for the variable in both iterators.
void schema_iterator_skip(SchemaIterator *iterator){
    // NOTE: if called before the first call to next, stack will be empty and the subsequent calls to next will return false.
    //  Shouldn't be called just after creation.
    assert(iterator->first_next == false);

    // NOTE: the schema that is on top of the stack was already returned. We simply need to delete it to avoid iterating over its children.
    if(iterator->stack.size){
        SchemaIteratorNode top = unsafe_last_in_array_list(iterator->stack);
        assert(top.next_child == 0);

        unsafe_remove_last_in_array_list(iterator->stack);
    }
}

// End Schema iterator
////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: arena is passed by value, so it is automatically popped to its initial state when returned to the caller.
void mapping_column_indexes_side(
    SetSchema *normalized_set_schema, ArrayListUInt free_var_positions, 
    SetSchema *normalized_common_set_schema,
    unsigned *starting_col_indices, int *row,
    unsigned *mapping_side,
    Arena arena)
{
    unsigned n_common = set_schema_size(normalized_common_set_schema);
    unsigned n_cols_side = set_schema_size(normalized_set_schema);
    unsigned num_free_vars = normalized_common_set_schema->arity;
    assert(num_free_vars > 0);

    // NOTE: in this case, we don't need an extra row_vars_to_new_columns mapping because the order of the columns of
    //  the multiple apparitions of the same repeated variable in the mapping doesn't matter.
    unsigned **row_vars_to_extending_cols = PUSH_ARRAY_ZERO(&arena, *row_vars_to_extending_cols, n_cols_side);
    // NOTE: the number of extending columns is n_common - n_cols_side, since n_common = ori + ext && n_cols_side = ori
    unsigned *extending_cols = PUSH_ARRAY(&arena, *extending_cols, n_common - n_cols_side);
    ArenaState schema_iterators_start_state = register_state_arena(&arena);

    unsigned new_virtual_column = n_cols_side + 1;
    unsigned mapping_pos = 0;

    for(unsigned i = 0; i < num_free_vars; ++i){
        unsigned pos = free_var_positions.array[i];
        Schema *normalized_common_schema = normalized_common_set_schema->subschemas + i;

        bool free_var_contained = pos < num_free_vars;
        if (!free_var_contained) {
            // NOTE: free_var wasn't originally in M. Add as many virtual columns as the size of the normalized common schema
            for(unsigned num_new_virtual_cols = schema_size(normalized_common_schema); num_new_virtual_cols; --num_new_virtual_cols){
                mapping_side[mapping_pos++] = new_virtual_column++;
            }

        } else {
            // NOTE: free_var was originally in M. We have to compare the normalized common schema with the original 
            //  normalized schema to see if we need to add new virtual columns.

            Schema *normalized_original_schema = normalized_set_schema->subschemas + pos;
            if(global_print_debugging){
                printf("Normalized common   schema: "); println_schema(*normalized_common_schema, PRINT_VISUALLY);
                printf("Normalized original schema: "); println_schema(*normalized_original_schema, PRINT_VISUALLY);
            }

            pop_to_state_arena(&arena, schema_iterators_start_state);
            SchemaIterator common_it = create_schema_iterator_arena(normalized_common_schema, &arena);
            SchemaIterator original_it = create_schema_iterator_arena(normalized_original_schema, &arena);

            // NOTE: row_pos is 0 based
            unsigned row_pos = starting_col_indices[pos] - 1;
            unsigned end_pos = (pos == (normalized_set_schema->arity - 1)) ? n_cols_side : starting_col_indices[pos + 1] - 1; // NOTE: exclusive
            
            for(;;){
                Schema common_subschema, original_subschema;
                bool has_next_common = schema_iterator_next(&common_it, &common_subschema);
                bool has_next_original = schema_iterator_next(&original_it, &original_subschema);

                while(common_it.stack.size > original_it.stack.size){
                    unsigned num_virtual_cols = schema_size(&common_subschema);
                    while(num_virtual_cols--){
                        mapping_side[mapping_pos++] = new_virtual_column++;
                    }

                    schema_iterator_skip(&common_it);
                    // NOTE: here's where the stack size will be decremented when all subschemas of the initial level are iterated.
                    has_next_common = schema_iterator_next(&common_it, &common_subschema);
                }
                assert(common_it.stack.size == original_it.stack.size);

                if(!has_next_common){
                    break;
                }

                if(global_print_debugging){
                    printf("row[row_pos=%d] = %d\n", row_pos, row[row_pos]);
                    int common_child_num = (common_it.stack.size == 1) ? -1 : (int)(common_it.stack.array[common_it.stack.size-2].next_child-1);
                    int original_child_num = (original_it.stack.size == 1) ? -1 : (int)(original_it.stack.array[original_it.stack.size-2].next_child-1);
                    printf("Common   Stack level = %d - Child #%d\n", common_it.stack.size, common_child_num);
                    printf("Original Stack level = %d - Child #%d\n", original_it.stack.size, original_child_num);
                    printf("Next common   subschema: "); println_schema(common_subschema, PRINT_VISUALLY);
                    printf("Next original subschema: "); println_schema(original_subschema, PRINT_VISUALLY);
                    printf("---\n");
                }
                assert(has_next_common && has_next_original);
                
                // NOTE: Take original column number, that is, row_pos+1 (1-based column indexes)
                mapping_side[mapping_pos++] = row_pos + 1;
                
                if(is_variable(row[row_pos]) && is_empty(original_subschema)){
                    // NOTE: variable
                    unsigned num_virtual_cols = schema_size(&common_subschema) - 1 ; // 1 = original_subschema.size;
                    int old_col = is_var_first_appearence(row[row_pos]) ? -((int)row_pos + 1) : row[row_pos];
                    
                    // NOTE: remember that row vars are identified with 0-BASED column numbers in row_vars_to_extending_cols
                    bool is_var_seen = row_vars_to_extending_cols[-(old_col + 1)] != NULL;
                    unsigned **var_s_extending_cols_ptr = row_vars_to_extending_cols - (old_col + 1);
                    if(is_var_seen){
                        // NOTE: repeated appearence of the row variable
                        unsigned *var_s_extending_cols = *var_s_extending_cols_ptr;
                        for (unsigned i = 0; i < num_virtual_cols; ++i) {
                            mapping_side[mapping_pos++] = var_s_extending_cols[i];
                        }
                    
                    } else {
                        // NOTE: first appearence of the row variable
                        // NOTE: even in the case of a non-repeated variable, storing it's new virtual columns isn't harmful. It's
                        //  just a "waste" of memory, but this way we avoid having to precalculate which variables are repeated.
                        //  Since new virtual columns of non-repeated variables appear (only) once in the mapping, we won't have 
                        //  "gaps" of unused virtual columns.
                        *var_s_extending_cols_ptr = extending_cols;
                        for (unsigned i = 0; i < num_virtual_cols; ++i, ++extending_cols) {
                            mapping_side[mapping_pos++] = new_virtual_column;
                            *extending_cols = new_virtual_column++;
                        }
                    }
                    
                    schema_iterator_skip(&common_it);
                }
                
                ++row_pos;
            }

            // NOTE: after summing the sizes of the original subschemas we should arrive exactly to the end position in the row portion;
            //  i.e., the (normalized) original subschema's size has to correspond to the length of the row portion.
            assert(row_pos == end_pos);
        }
        assert(mapping_pos <= n_common);
    }
    assert(mapping_pos == n_common);
}

// NOTE: row is only for debugging and to have the same interface as the non-linear version
void mapping_column_indexes_side_lineal(
    SetSchema *normalized_set_schema, ArrayListUInt free_var_positions, 
    SetSchema *normalized_common_set_schema,
    unsigned *starting_col_indices, int *row,
    unsigned *mapping_side,
    Arena arena)
{
    ArenaState schema_iterators_start_state = register_state_arena(&arena);

    unsigned n_common = set_schema_size(normalized_common_set_schema);
    unsigned n_cols_side = set_schema_size(normalized_set_schema);
    unsigned num_free_vars = normalized_common_set_schema->arity;
    assert(num_free_vars > 0);

    unsigned new_virtual_column = n_cols_side + 1;
    unsigned mapping_pos = 0;

    for(unsigned i = 0; i < num_free_vars; ++i){
        unsigned pos = free_var_positions.array[i];
        Schema *normalized_common_schema = normalized_common_set_schema->subschemas + i;

        bool free_var_contained = pos < num_free_vars;
        if (!free_var_contained) {
            // NOTE: free_var wasn't originally in M. Add as many virtual columns as the size of the normalized common schema
            for(unsigned num_new_virtual_cols = schema_size(normalized_common_schema); num_new_virtual_cols; --num_new_virtual_cols){
                mapping_side[mapping_pos++] = new_virtual_column++;
            }

        } else {
            // NOTE: free_var was originally in M. We have to compare the normalized common schema with the original 
            //  normalized schema to see if we need to add new virtual columns.

            Schema *normalized_original_schema = normalized_set_schema->subschemas + pos;
            if(global_print_debugging){
                printf("Normalized common   schema: "); println_schema(*normalized_common_schema, PRINT_VISUALLY);
                printf("Normalized original schema: "); println_schema(*normalized_original_schema, PRINT_VISUALLY);
            }

            pop_to_state_arena(&arena, schema_iterators_start_state);
            SchemaIterator common_it = create_schema_iterator_arena(normalized_common_schema, &arena);
            SchemaIterator original_it = create_schema_iterator_arena(normalized_original_schema, &arena);

            // NOTE: row_pos is 0 based
            unsigned row_pos = starting_col_indices[pos] - 1;
            unsigned end_pos = (pos == (normalized_set_schema->arity - 1)) ? n_cols_side : starting_col_indices[pos + 1] - 1; // NOTE: exclusive
            
            for(;;){
                Schema common_subschema, original_subschema;
                bool has_next_common = schema_iterator_next(&common_it, &common_subschema);
                bool has_next_original = schema_iterator_next(&original_it, &original_subschema);

                while(common_it.stack.size > original_it.stack.size){
                    unsigned num_virtual_cols = schema_size(&common_subschema);
                    while(num_virtual_cols--){
                        mapping_side[mapping_pos++] = new_virtual_column++;
                    }

                    schema_iterator_skip(&common_it);
                    has_next_common = schema_iterator_next(&common_it, &common_subschema);
                }
                assert(common_it.stack.size == original_it.stack.size);

                if(!has_next_common){
                    break;
                }

                if(global_print_debugging){
                    printf("row[row_pos=%d] = %d\n", row_pos, row[row_pos]);
                    int common_child_num = (common_it.stack.size == 1) ? -1 : (int)(common_it.stack.array[common_it.stack.size-2].next_child-1);
                    int original_child_num = (original_it.stack.size == 1) ? -1 : (int)(original_it.stack.array[original_it.stack.size-2].next_child-1);
                    printf("Common   Stack level = %d - Child #%d\n", common_it.stack.size, common_child_num);
                    printf("Original Stack level = %d - Child #%d\n", original_it.stack.size, original_child_num);
                    printf("Next common   subschema: "); println_schema(common_subschema, PRINT_VISUALLY);
                    printf("Next original subschema: "); println_schema(original_subschema, PRINT_VISUALLY);
                    printf("---\n");
                }
                assert(has_next_common && has_next_original);

                // NOTE: Take original column number, that is, row_pos+1 (1-based column indexes)
                mapping_side[mapping_pos++] = row_pos + 1;
                ++row_pos;
            }

            // NOTE: after summing the sizes of the original subschemas we should arrive exactly to the end position in the row portion;
            //  i.e., the (normalized) original subschema's size has to correspond to the length of the row portion.
            assert(row_pos == end_pos);
        }

        assert(mapping_pos <= n_common);
    }
    assert(mapping_pos == n_common);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END OBTAIN COLUMN MAPPING FROM COMMON SET SCHEMA ////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EXTEND ROWS FROM COMMON SET SCHEMA //////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void extend_row(
    SetSchema *normalized_set_schema, ArrayListUInt free_var_positions, 
    SetSchema *normalized_common_set_schema,
    unsigned *starting_col_indices, int *row,
    int *extended_row,
    Arena arena)
{
    unsigned n_common = set_schema_size(normalized_common_set_schema);
    unsigned n_cols_side = set_schema_size(normalized_set_schema);
    unsigned num_free_vars = normalized_common_set_schema->arity;
    assert(num_free_vars > 0);

    int **row_vars_to_extending_vars = PUSH_ARRAY(&arena, *row_vars_to_extending_vars, n_cols_side);
    unsigned *extending_vars = PUSH_ARRAY(&arena, *extending_vars, n_common - n_cols_side);
    // NOTE: we have at most as many row variables in the original row as its length.
    int *row_vars_to_new_columns = PUSH_ARRAY_ZERO(&arena, *row_vars_to_new_columns, n_cols_side);
    ArenaState schema_iterators_start_state = register_state_arena(&arena);

    unsigned extended_pos = 0;

    for(unsigned i = 0; i < num_free_vars; ++i){
        unsigned pos = free_var_positions.array[i];
        Schema *normalized_common_schema = normalized_common_set_schema->subschemas + i;

        bool free_var_contained = pos < num_free_vars;
        if (!free_var_contained) {
            // NOTE: free_var wasn't originally in M. Add as many fresh variables as the size of the normalized common schema
            unsigned num_fresh_vars = schema_size(normalized_common_schema);
            SET_TO_ONE(extended_row + extended_pos, sizeof(*extended_row) * num_fresh_vars);
            extended_pos += num_fresh_vars;

        } else {
            // NOTE: free_var was originally in M. We have to compare the normalized common schema with the original 
            //  normalized schema to see if we need to add new virtual columns.

            Schema *normalized_original_schema = normalized_set_schema->subschemas + pos;
            if(global_print_debugging){
                printf("Normalized common   schema: "); println_schema(*normalized_common_schema, PRINT_VISUALLY);
                printf("Normalized original schema: "); println_schema(*normalized_original_schema, PRINT_VISUALLY);
            }
            
            pop_to_state_arena(&arena, schema_iterators_start_state);
            SchemaIterator common_it = create_schema_iterator_arena(normalized_common_schema, &arena);
            SchemaIterator original_it = create_schema_iterator_arena(normalized_original_schema, &arena);

            // NOTE: row_pos is 0 based
            unsigned row_pos = starting_col_indices[pos] - 1;
            unsigned end_pos = (pos == (normalized_set_schema->arity - 1)) ? n_cols_side : starting_col_indices[pos + 1] - 1; // NOTE: exclusive
            
            for(;;){
                Schema common_subschema, original_subschema;
                bool has_next_common = schema_iterator_next(&common_it, &common_subschema);
                bool has_next_original = schema_iterator_next(&original_it, &original_subschema);

                while(common_it.stack.size > original_it.stack.size){
                    unsigned num_fresh_vars = schema_size(&common_subschema);
                    SET_TO_ONE(extended_row + extended_pos, sizeof(*extended_row) * num_fresh_vars);
                    extended_pos += num_fresh_vars;

                    schema_iterator_skip(&common_it);
                    // NOTE: here's where the stack size will be decremented when all subschemas of the initial level are iterated.
                    has_next_common = schema_iterator_next(&common_it, &common_subschema);
                }
                assert(common_it.stack.size == original_it.stack.size);

                if(!has_next_common){
                    break;
                }

                if(global_print_debugging){
                    printf("row[row_pos=%d] = %d\n", row_pos, row[row_pos]);
                    int common_child_num = (common_it.stack.size == 1) ? -1 : (int)(common_it.stack.array[common_it.stack.size-2].next_child-1);
                    int original_child_num = (original_it.stack.size == 1) ? -1 : (int)(original_it.stack.array[original_it.stack.size-2].next_child-1);
                    printf("Common   Stack level = %d - Child #%d\n", common_it.stack.size, common_child_num);
                    printf("Original Stack level = %d - Child #%d\n", original_it.stack.size, original_child_num);
                    printf("Next common   subschema: "); println_schema(common_subschema, PRINT_VISUALLY);
                    printf("Next original subschema: "); println_schema(original_subschema, PRINT_VISUALLY);
                    printf("---\n");
                }
                assert(has_next_common && has_next_original);
                
                if (is_symbol(row[row_pos])) {
                    // NOTE: a function symbol
                    extended_row[extended_pos++] = row[row_pos];
                } else {
                    // NOTE: variable
                    int old_col = is_var_first_appearence(row[row_pos]) ? -((int)row_pos + 1) : row[row_pos];
                    bool is_already_in_extended_row = row_vars_to_new_columns[-(old_col + 1)] != 0;
                    if (is_already_in_extended_row) {
                        extended_row[extended_pos++] = row_vars_to_new_columns[-(old_col + 1)];
                    } else {
                        extended_row[extended_pos++] = 0;
                        row_vars_to_new_columns[-(old_col + 1)] = -(int)(extended_pos);
                    }
                    
                    if (is_empty(original_subschema)) {
                        unsigned num_extending_vars = schema_size(&common_subschema) - 1 ; // 1 = original_subschema.size;
                        
                        // NOTE: remember that row vars are identified with 0-BASED column numbers in row_vars_to_extending_vars
                        int **var_s_extending_vars_ptr = row_vars_to_extending_vars - (old_col + 1);
                        if(is_already_in_extended_row){
                            // NOTE: repeated appearence of the row variable
                            int *var_s_extending_vars = *var_s_extending_vars;
                            for (unsigned i = 0; i < num_extending_vars; ++i) {
                                extended_row[extended_pos++] = var_s_extending_vars[i];
                            }
                            
                        } else {
                            // NOTE: first appearence of the row variable
                            // NOTE: even in the case of a non-repeated variable, storing it's new virtual columns isn't harmful. It's
                            //  just a "waste" of memory, but this way we avoid having to precalculate which variables are repeated.
                            *var_s_extending_vars_ptr = extending_vars;
                            for (unsigned i = 0; i < num_extending_vars; ++i, ++extending_vars) {
                                extended_row[extended_pos++] = 1;
                                *extending_vars = -(int)(extended_pos); // NOTE: negative 1-based columns of the first appearence of extending vars
                            }
                        }
                        
                        schema_iterator_skip(&common_it);
                    }
                }
                ++row_pos;
            }

            // NOTE: after summing the sizes of the original subschemas we should arrive exactly to the end position in the row portion;
            //  i.e., the (normalized) original subschema's size has to correspond to the length of the row portion.
            assert(row_pos == end_pos);
        }

        assert(extended_pos <= n_common);
    }
    assert(extended_pos == n_common);
}

void extend_row_lineal(
    SetSchema *normalized_set_schema, ArrayListUInt free_var_positions, 
    SetSchema *normalized_common_set_schema,
    unsigned *starting_col_indices, int *row,
    int *extended_row,
    Arena arena)
{
    ArenaState schema_iterators_start_state = register_state_arena(&arena);

    unsigned n_common = set_schema_size(normalized_common_set_schema);
    unsigned n_cols_side = set_schema_size(normalized_set_schema);
    unsigned num_free_vars = normalized_common_set_schema->arity;
    assert(num_free_vars > 0);

    unsigned extended_pos = 0;

    for(unsigned i = 0; i < num_free_vars; ++i){
        unsigned pos = free_var_positions.array[i];
        Schema *normalized_common_schema = normalized_common_set_schema->subschemas + i;

        bool free_var_contained = pos < num_free_vars;
        if (!free_var_contained) {
            // NOTE: free_var wasn't originally in M. Add as many fresh variables as the size of the normalized common schema
            unsigned num_fresh_vars = schema_size(normalized_common_schema);
            SET_TO_ONE(extended_row + extended_pos, sizeof(*extended_row) * num_fresh_vars);
            extended_pos += num_fresh_vars;

        } else {
            // NOTE: free_var was originally in M. We have to compare the normalized common schema with the original 
            //  normalized schema to see if we need to add new fresh variables.

            Schema *normalized_original_schema = normalized_set_schema->subschemas + pos;
            if(global_print_debugging){
                printf("Normalized common   schema: "); println_schema(*normalized_common_schema, PRINT_VISUALLY);
                printf("Normalized original schema: "); println_schema(*normalized_original_schema, PRINT_VISUALLY);
            }

            pop_to_state_arena(&arena, schema_iterators_start_state);
            SchemaIterator common_it = create_schema_iterator_arena(normalized_common_schema, &arena);
            SchemaIterator original_it = create_schema_iterator_arena(normalized_original_schema, &arena);

            // NOTE: row_pos is 0 based
            unsigned row_pos = starting_col_indices[pos] - 1;
            unsigned end_pos = (pos == (normalized_set_schema->arity - 1)) ? n_cols_side : starting_col_indices[pos + 1] - 1; // NOTE: exclusive
            
            for(;;){
                Schema common_subschema, original_subschema;
                bool has_next_common = schema_iterator_next(&common_it, &common_subschema);
                bool has_next_original = schema_iterator_next(&original_it, &original_subschema);

                while(common_it.stack.size > original_it.stack.size){
                    unsigned num_fresh_vars = schema_size(&common_subschema);
                    SET_TO_ONE(extended_row + extended_pos, sizeof(*extended_row) * num_fresh_vars);
                    extended_pos += num_fresh_vars;

                    schema_iterator_skip(&common_it);
                    has_next_common = schema_iterator_next(&common_it, &common_subschema);
                }
                assert(common_it.stack.size == original_it.stack.size);

                if(!has_next_common){
                    break;
                }

                if(global_print_debugging){
                    printf("row[row_pos=%d] = %d\n", row_pos, row[row_pos]);
                    int common_child_num = (common_it.stack.size == 1) ? -1 : (int)(common_it.stack.array[common_it.stack.size-2].next_child-1);
                    int original_child_num = (original_it.stack.size == 1) ? -1 : (int)(original_it.stack.array[original_it.stack.size-2].next_child-1);
                    printf("Common   Stack level = %d - Child #%d\n", common_it.stack.size, common_child_num);
                    printf("Original Stack level = %d - Child #%d\n", original_it.stack.size, original_child_num);
                    printf("Next common   subschema: "); println_schema(common_subschema, PRINT_VISUALLY);
                    printf("Next original subschema: "); println_schema(original_subschema, PRINT_VISUALLY);
                    printf("---\n");
                }
                assert(has_next_common && has_next_original);

                // NOTE: Take original row content
                extended_row[extended_pos++] = row[row_pos++];
            }

            // NOTE: after summing the sizes of the original subschemas we should arrive exactly to the end position in the row portion;
            //  i.e., the (normalized) original subschema's size has to correspond to the length of the row portion.
            assert(row_pos == end_pos);
        }

        assert(extended_pos <= n_common);
    }
    assert(extended_pos == n_common);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END EXTEND ROWS FROM COMMON SET SCHEMA //////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// POSTPROCESSING OF MATRICES: OBTAIN SET SCHEMA OF ROW DENORMALIZING NORMALIZED FRAGMENT SET SCHEMA BASED ON THE RESULTING ROW  ///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void denormalized_schema(
    Schema *normalized_schema, int *modified_row, unsigned *index,
    Schema *row_schema, SetDependencies *row_dependencies, 
    Arena *arena)
{
    if (modified_row[*index] == 0) {
        init_general_schema_arena(row_schema, normalized_schema->arity, arena);
        foreach_in_schemas(*row_schema, *normalized_schema, sub_row, sub_normalized){
            (*index)++;
            denormalized_schema(sub_normalized, modified_row, index, sub_row, row_dependencies, arena);
        }
    } else if (modified_row[*index] > 0) {
        unsigned v = modified_row[*index];
        init_variable_schema(row_schema, v);
        //bool first_time_is_inserted = insert_to_dependencies_baseline(row_dependencies, v, normalized_schema, arena);
        //assert(first_time_is_inserted);
        DependencyPair *dp = row_dependencies->array + (v-1);
        dp->schemas.array[0] = normalized_schema;

        *index += schema_size(normalized_schema);
    } else {
        unsigned pos = -(modified_row[*index] + 1);
        unsigned v = modified_row[pos];
        init_variable_schema(row_schema, v);
        
        //bool should_have_a_single_dependency = !insert_to_dependencies_baseline(row_dependencies, v, normalized_schema, arena);
        //assert(should_have_a_single_dependency);
        DependencyPair *dp = row_dependencies->array + (v-1);
        Schema *inserted = dp->schemas.array[0];
        assert(equal_schemas(*inserted, *normalized_schema));
        
        *index += schema_size(normalized_schema);
    }
}

// NOTE: copy is made for the denormalized one, not in-memory modification. Specially important, because the normalized_set_schema
//  is the same for all rows in a resulting Matrix Block.
void denormalized_set_schema(
    SetSchema *normalized_set_schema, int *row, 
    SetSchema *row_set_schema, SetDependencies *row_dependencies,
    Arena* arena, Arena scratch)
{
    unsigned len_row = set_schema_size(normalized_set_schema);
    int *modified_row = PUSH_ARRAY(&scratch, *modified_row, len_row);
    memcpy(modified_row, row, sizeof(*row) * len_row);
    
    // NOTE: we are only interested on original variables that remain repeated. We change the first apparition of original
    //  repeated variables to positive values (the ones that will be used as the variable names), and keep the backreferences
    //  to them. All the other values are nullified.
    unsigned num_original_repeated_vars_in_row = 0;
    for (unsigned i = len_row - 1; i >= 0; --i) {
        if (modified_row[i] < 0) {
            unsigned referenced_pos1based = -modified_row[i];

            if (modified_row[referenced_pos1based - 1] == 0) {
                // NOTE: the initial 0 value for the repeated original variable is modified to a negative reference to itself.
                assert(referenced_pos1based < (i + 1));
                modified_row[referenced_pos1based - 1] = -(int)(referenced_pos1based);
                continue;
            } else if (referenced_pos1based == (i + 1)){
                // NOTE: Change the self-reference to a valid variable name.
                modified_row[i] = ++num_original_repeated_vars_in_row;
                continue;
            }
        }
        // NOTE: extending variables and symbols are nullified.
        modified_row[i] = 0;
    }

    // NOTE: initiallize row dependencies knowing that each variable will have a single dependency.
    *row_dependencies = create_array_list_dependency_pair_arena(num_original_repeated_vars_in_row, arena);
    row_dependencies->size = num_original_repeated_vars_in_row;
    for (unsigned v = 1; v <= num_original_repeated_vars_in_row; ++v) {
        DependencyPair dp = { .v = v, .schemas = create_array_list_schema_ptr_arena(1, arena) };
        dp.schemas.size = 1;
        row_dependencies->array[v - 1] = dp;
    }

    *row_set_schema = GENERAL_SCHEMA_LITERAL(normalized_set_schema->arity, arena);
    unsigned index = 0;
    foreach_in_schemas(*normalized_set_schema, *row_set_schema, normalized, denormalized){
        denormalized_schema(normalized, modified_row, &index, denormalized, row_dependencies, arena);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END POSTPROCESSING OF MATRICES: OBTAIN SET SCHEMA OF ROW DENORMALIZING NORMALIZED FRAGMENT SET SCHEMA BASED ON THE RESULTING ROW  ///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DEEP COPYING  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Schema schema_deepcopy_except_root(Schema *schema, Arena *arena) {
    Schema result = *schema;
    if (schema->type == GENERAL_SCHEMA) {
        result.subschemas = PUSH_ARRAY(arena, *result.subschemas, result.arity);
        foreach_in_schemas(*schema, result, sub, res) {
            *res = schema_deepcopy_except_root(sub, arena);
        }
    }
    return result;
}

Schema *schema_deepcopy(Schema *schema, Arena *arena) {
    Schema *result = PUSH_SINGLE(arena, *result);
    *result = *schema;
    if (schema->type == GENERAL_SCHEMA) {
        result->subschemas = PUSH_ARRAY(arena, *result->subschemas, result->arity);
        foreach_in_schemas(*schema, *result, sub, res) {
            *res = schema_deepcopy_except_root(sub, arena);
        }
    }
    return result;    
}

static ArrayListSchemaPtr deepcopy_arraylist_schema_ptr(ArrayListSchemaPtr list, Arena *arena) {
    ArrayListSchemaPtr result = create_array_list_schema_ptr_arena(list.capacity, arena);
    result.size = list.size;
    foreach_in_arraylists(SchemaPtr, elem, res, list, result) {
        *res = schema_deepcopy(*elem, arena);
    }
    return result;
}

SetDependencies *set_dependencies_deepcopy(SetDependencies *dependencies, Arena *arena){
    SetDependencies *result = PUSH_SINGLE(arena, *result);
    *result = create_array_list_dependency_pair_arena(dependencies->size, arena);
    result->size = dependencies->size;

    foreach_in_arraylists(DependencyPair, dp, res, *dependencies, *result){
        res->v = dp->v;
        res->schemas = deepcopy_arraylist_schema_ptr(dp->schemas, arena);
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END DEEP COPYING  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
