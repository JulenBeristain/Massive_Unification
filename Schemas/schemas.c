#include "schemas.h"
#include "utils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST SCHEMAS ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_ARRAYLIST_CREATE_ARENA(Schema, schema)

DEFINE_ARRAYLIST_RESIZE_ARENA(Schema, schema)
DEFINE_ARRAYLIST_ADD_ARENA(Schema, schema)

DEFINE_ARRAYLIST_FIND(Schema, schema, equal_schemas) // CONTAINS in .h
DEFINE_ARRAYLIST_ADD_NO_REPEATED_ARENA(Schema, schema)

DEFINE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(Schema, schema)

DEFINE_ARRAYLIST_REMOVE_INDEX(Schema, schema)

unsigned find_equivalent_in_array_list_schema(ArrayListSchema list, Schema elem, Variable* mapping)
{
    unsigned i = 0;
    for (Schema *iter = (list).array, *_end = (list).array + (list).size; iter < _end; ++iter) {
        if (equivalent_schemas(elem, *iter, mapping)) { // NOTE: this is the difference with find
            return i;
        }
        ++i;
    }
    return list.size;
}
// NOTE: contains_equivalent in .h

DEFINE_ARRAYLIST_PRINT_SEPARATORS_1(Schema, schema, print_schema, PrintingMode, printing_mode)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST SCHEMAS ///////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OF DEPENDENCIES /////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST DEPENDENCY PAIRS //////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_ARRAYLIST_CREATE_ARENA(DependencyPair, dependency_pair)

DEFINE_ARRAYLIST_RESIZE_ARENA(DependencyPair, dependency_pair)
DEFINE_ARRAYLIST_ADD_ARENA(DependencyPair, dependency_pair)

DEFINE_ARRAYLIST_EXTEND_ARENA(DependencyPair, dependency_pair)

DEFINE_ARRAYLIST_REMOVE_INDEX(DependencyPair, dependency_pair)

// find_v_in_array_list_dependency_pair in .h

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST DEPENDENCY PAIRS //////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END SET OF DEPENDENCIES /////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ARRAYLIST OF PAIRS VARIABLE-NUM_APPEARENCES /////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct VarNum VarNum, *VarNumPtr;
struct VarNum {
    Variable v;
    unsigned num_appearences;
};

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END ARRAYLIST STRINGS (Char Pointers) ///////////////////////////////////////////////////////////////////////////////////////////
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
        foreach_in_schema(s, sub)
        {
            printf(", ");
            print_schema_(*sub, opening_brace, closing_brace, show_arity);
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

// Set-Schemas /////////////////////////////////////////////////////////////////////////////////////////////
void print_set_schema_(
    ArrayListSchema set_schema, char opening_brace, char closing_brace,
    char* schema_separator, PrintingMode schema_mode)
{
    print_separators_array_list_schema(set_schema, opening_brace, closing_brace, schema_separator, false, 0, schema_mode);
}
void print_set_schema(ArrayListSchema set_schema, PrintingMode mode)
{
    // NOTE: another interesting schema_separator = ",\n\t" (potentially for a certain number of \t if nesting...)
    if (mode == PRINT_VISUALLY) {
        print_set_schema_(set_schema, '{', '}', "; ", PRINT_VISUALLY);
    } else {
        // NOTE: \0 not printable character, so it won't be shown!
        print_set_schema_(set_schema, '\0', '\0', "; ", PRINT_FILE_FORMAT);
    }
}
void println_set_schema(ArrayListSchema set_schema, PrintingMode mode)
{
    print_set_schema(set_schema, mode);
    printf("\n");
}

// Set dependencies /////////////////////////////////////////////////////////////////////////////////////////////
void print_dependency_pair(
    DependencyPair pair, char opening_brace, char closing_brace,
    char* schema_separator, PrintingMode schema_mode)
{
    printf("$%u <- ", pair.v);
    print_set_schema_(pair.schemas, opening_brace, closing_brace, schema_separator, schema_mode);
}
void print_set_dependencies_(
    ArrayListDependencyPair set_dependencies, char opening_brace, char closing_brace,
    char* schema_separator, PrintingMode schema_mode)
{
    if (set_dependencies.size == 0) {
        printf("\n");
        return;
    }

    foreach_in_arraylist(DependencyPair, pair, set_dependencies)
    {
        print_dependency_pair(*pair, opening_brace, closing_brace, schema_separator, schema_mode);
        printf("\n");
    }
}
void print_set_dependencies(ArrayListDependencyPair set_dependencies, PrintingMode mode)
{
    if (mode == PRINT_VISUALLY) {
        print_set_dependencies_(set_dependencies, '{', '}', "; ", PRINT_VISUALLY);
    } else {
        print_set_dependencies_(set_dependencies, '[', ']', "; ", PRINT_FILE_FORMAT);
    }
}
void println_set_dependencies(ArrayListDependencyPair set_dependencies, PrintingMode mode)
{
    print_set_dependencies(set_dependencies, mode);
    printf("\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END PRINTING FUNCTIONS FOR DEBUGGING ////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SCHEMAS /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * NOTE: if Schemas are modified at some point, so they are not immutable, then the use of the
 *       size attribute must be reconsidered.
 */
void init_variable_schema(Schema* s, Variable v)
{
    s->type = VARIABLE_SCHEMA;
    s->size = 1;
    s->v = v;
}

/**
 * NOTE: size is simply set to 1. If nested subschemas, their own sizes have to be added to this
 *      value in creation.
 */
void init_general_schema_arena(Schema* s, unsigned arity, Arena* arena)
{
    s->type = GENERAL_SCHEMA;
    s->size = 1;
    s->arity = arity;
    s->subschemas = allocate(arena, arity * sizeof(*(s->subschemas)));
}

/**
 * Function that computes the schema size, without looking at the precached sizes.
 */
unsigned schema_size(Schema* s)
{
    if (s->type == VARIABLE_SCHEMA || s->arity == 0) {
        return 1;
    }

    unsigned total_size = 1;
    foreach_in_schemaptr(s, sub)
    {
        total_size += schema_size(sub);
    }

    return total_size;
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

bool equal_set_schemas(ArrayListSchema set_schema1, ArrayListSchema set_schema2)
{
    if (set_schema1.size != set_schema2.size) {
        return false;
    }

    foreach_in_arraylists(Schema, schema1, schema2, set_schema1, set_schema2)
    {
        if (!equal_schemas(*schema1, *schema2)) {
            return false;
        }
    }
    return true;
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

bool equivalent_set_schemas(ArrayListSchema set_schema1, ArrayListSchema set_schema2, Variable* mapping)
{
    if (set_schema1.size != set_schema2.size) {
        return false;
    }
    unsigned size = set_schema1.size;

    for (unsigned i = 0; i < size; ++i) {
        if (!equivalent_schemas(set_schema1.array[i], set_schema2.array[i], mapping)) {
            return false;
        }
    }
    return true;
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

void variables_in_set_schema(ArrayListSchema set_schema, SetVariables* result)
{
    foreach_in_arraylist(Schema, s, set_schema)
    {
        variables_in_schema(*s, result);
    }
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
Variable max_v_in_set_schema(ArrayListSchema set_schema)
{
    Variable max_v = 0;
    foreach_in_arraylist(Schema, s, set_schema)
    {
        max_v = MAX(max_v, max_v_in_schema(*s));
    }
    return max_v;
}

void increment_variables_in_schema(Schema* schema, Variable increment)
{
    if (schema->type == VARIABLE_SCHEMA) {
        schema->v += increment;
    } else {
        foreach_in_schemaptr(schema, subschema)
        {
            increment_variables_in_schema(subschema, increment);
        }
    }
}
void increment_variables_in_set_schema(ArrayListSchema set_schema, Variable increment)
{
    foreach_in_arraylist(Schema, s, set_schema)
    {
        increment_variables_in_schema(s, increment);
    }
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
        init_general_schema_arena(result, original.arity, arena); // NOTE: ->subschemas allocated; ->size = 1
        for (size_t i = 0; i < original.arity; ++i) {
            substitute_arena(original.subschemas[i], v, substitution, result->subschemas + i, arena);
            result->size += result->subschemas[i].size;
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
        init_general_schema_arena(result, original.arity, arena); // NOTE: ->subschemas allocated; ->size = 1
        for (size_t i = 0; i < original.arity; ++i) {
            substitute_vars_arena(original.subschemas[i], vars, substitution, result->subschemas + i, arena);
            result->size += result->subschemas[i].size;
        }
    }
}

// PARSING ////////////////////////////////////////////////////////////////////////////////////////////////

Schema read_schema(char** schema_str, Arena* arena)
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
        init_general_schema_arena(&result, arity, arena); // size = 1, needs to be updated if arity > 0
        *schema_str += num_digits(arity) + 2;
        for (size_t i = 0; i < arity; ++i) {
            result.subschemas[i] = read_schema(schema_str, arena);
            result.size += result.subschemas[i].size;
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

unsigned scan_num_schemas_in_set(char* line)
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

ArrayListSchema read_set_schema(char* line, Arena* arena)
{
    // Precalculate the number of columns; i.e., the number of schemas in the set-schema
    unsigned num_cols = scan_num_schemas_in_set(line);
    // NOTE: the set_schema will not be resized, so we can use an Arena without wasting memory
    ArrayListSchema set_schema = create_array_list_schema_arena(num_cols, arena);

    // Parse each schema
    while (*line != '\0' && *line != '\n') { // NOTE: we could also use num_cols as a counter...
        // NOTE: we can directly set without bound checking thanks to the precalculation of the number of columns
        set_schema.array[set_schema.size++] = read_schema(&line, arena);
        // add_to_array_list_schema_arena(&set_schema, read_schema(&line, arena), arena); // MORE Robust option
        //  Skip the comma separating the schemas in the set-schema (or the new line at the end)
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

// OPT: a possible low-level optimization would be to see if passing schemas by pointer is more optimal!

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
// PRE: there must not be any repeated Schemas in the list of Dependences of each variable! (set semantics)
unsigned number_of_dependencies(ArrayListDependencyPair* dependencies)
{
    unsigned num_dependencies = 0;
    foreach_in_arraylistptr(DependencyPair, pair, dependencies)
    {
        num_dependencies += pair->schemas.size;
    }
    return num_dependencies;
}

// NOTE: resizing risk for arena when adding to arraylists. Unavoidable. We don't know the number of dependencies
//  that will end up having the new variable with dependencies v. We don't know how many extra variables with
//  dependencies we will have neither...
bool insert_to_dependencies_baseline(ArrayListDependencyPair* dependencies, Variable v, Schema s, Arena* arena)
{
    for (unsigned i = 0; i < dependencies->size; ++i) {
        DependencyPair* pair = dependencies->array + i;
        if (pair->v == v) {
            int contained = add_no_repeated_to_array_list_schema_arena(&pair->schemas, s, arena);
            return contained != CONTAINED;
        }
    }

    // First dependency for v
    ArrayListSchema first_schemas_v = create_array_list_schema_arena(10, arena);
    add_to_array_list_schema_arena(&first_schemas_v, s, arena);
    DependencyPair first_dependency_pair_v = { .v = v, .schemas = first_schemas_v };
    add_to_array_list_dependency_pair_arena(dependencies, first_dependency_pair_v, arena);
    return true;
}

// NOTE: resizing risk because of Arena and ArrayLists
// destination gets the dependencies found in source that it didn't contain initially.
void union_of_dependencies_baseline(ArrayListDependencyPair* destination, ArrayListDependencyPair source, Arena* arena)
{
    for (unsigned i = 0; i < source.size; ++i) {
        DependencyPair pair2 = source.array[i];
        Variable v = pair2.v;
        ArrayListSchema schemas2 = pair2.schemas;

        unsigned vpos = find_v_in_array_list_dependency_pair(*destination, v);
        if (vpos == destination->size) {
            add_to_array_list_dependency_pair_arena(destination, pair2, arena); // NOTE: they share the schemas (no memory management problem with Arenas)
        } else {
            extend_no_repeated_array_list_schema_arena(&destination->array[vpos].schemas, schemas2, arena);
        }
    }
}

// NOTE: these two functions are for debugging asserts
bool has_self_dependency_baseline(ArrayListSchema schemas, Variable v)
{
    foreach_in_arraylist(Schema, schema, schemas)
    {
        if (is_self_dependency(v, *schema)) {
            return true;
        }
    }
    return false;
}
bool contains_self_dependency_baseline(ArrayListDependencyPair dependencies)
{
    foreach_in_arraylist(DependencyPair, pair, dependencies)
    {
        if (has_self_dependency_baseline(pair->schemas, pair->v)) {
            return true;
        }
    }
    return false;
}

bool equal_set_dependencies(ArrayListDependencyPair dependencies1, ArrayListDependencyPair dependencies2)
{
    if (dependencies1.size != dependencies2.size) {
        return false;
    }
    unsigned num_vars_with_dependencies = dependencies1.size;

    for (unsigned index1 = 0; index1 < num_vars_with_dependencies; ++index1) {
        DependencyPair pair1 = dependencies1.array[index1];
        Variable v = pair1.v;
        ArrayListSchema schemas1 = pair1.schemas; // Taking by value is not problematic because we're not modifying

        // Find v in dependencies2
        unsigned index2 = find_v_in_array_list_dependency_pair(dependencies2, v);
        if (index2 == num_vars_with_dependencies) {
            return false; // v wasn't found in dependencies2
        }

        ArrayListSchema schemas2 = dependencies2.array[index2].schemas; // Taking by value is not problematic because we're not modifying

        if (schemas1.size != schemas2.size) {
            return false;
        }
        unsigned num_dependencies_of_v = schemas1.size;

        // Unordered equals between schemas1 and schemas2
        for (unsigned i = 0; i < num_dependencies_of_v; ++i) {
            Schema s1 = schemas1.array[i];
            if (!contains_array_list_schema(schemas2, s1)) {
                return false; // There is a dependency for v in dependencies1 that is not found in dependencies2
            }
        }
    }
    return true;
}

// PRE: equivalent_set_schemas was called so mapping already contains the variable equivalences!
bool equivalent_set_dependencies(ArrayListDependencyPair dependencies1, ArrayListDependencyPair dependencies2, Variable* mapping)
{
    if (dependencies1.size != dependencies2.size) {
        return false;
    }
    unsigned num_vars_with_dependencies = dependencies1.size;

    for (unsigned index1 = 0; index1 < num_vars_with_dependencies; ++index1) {
        DependencyPair pair1 = dependencies1.array[index1];
        Variable v = pair1.v;
        ArrayListSchema schemas1 = pair1.schemas; // Taking by value is not problematic because we're not modifying

        // Find v's match in dependencies2
        Variable v_match = mapping[v];
        unsigned index2 = find_v_in_array_list_dependency_pair(dependencies2, v_match);
        if (index2 == num_vars_with_dependencies) {
            return false; // v_match wasn't found in dependencies2
        }

        ArrayListSchema schemas2 = dependencies2.array[index2].schemas; // Taking by value is not problematic because we're not modifying

        if (schemas1.size != schemas2.size) {
            return false;
        }
        unsigned num_dependencies_of_v = schemas1.size;

        // Unordered equivalence between schemas1 and schemas2
        for (unsigned i = 0; i < num_dependencies_of_v; ++i) {
            Schema s1 = schemas1.array[i];
            if (!contains_equivalent_array_list_schema(schemas2, s1, mapping)) {
                return false; // There is a dependency for v in dependencies1 that is not found in dependencies2
            }
        }
    }
    return true;
}

void increment_variables_in_set_dependencies(ArrayListDependencyPair dependencies, Variable increment)
{
    foreach_in_arraylist(DependencyPair, pair, dependencies)
    {
        pair->v += increment;
        foreach_in_arraylist(Schema, schema, pair->schemas)
        {
            increment_variables_in_schema(schema, increment);
        }
    }
}

// PARSING ////////////////////////////////////////////////////////////////////////////////////////////////

ArrayListDependencyPair read_set_dependencies(FILE* stream, unsigned num_vars_with_dependencies, Arena* arena)
{
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    // NOTE: no resizing risk for Arena, as we know the numbers of variables with dependencies beforehand
    ArrayListDependencyPair dependencies = create_array_list_dependency_pair_arena(num_vars_with_dependencies, arena);

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

        DependencyPair pair = { .v = v, .schemas = read_set_schema(lineptr, arena) };
        // NOTE: we can directly set without bound checking thanks to having read the number of vars with dependencies
        dependencies.array[dependencies.size++] = pair;
        // add_to_array_list_dependency_pair_arena(&dependencies, pair, arena); // NOTE: more robust option
    }

    if (line) {
        free(line);
    }
    return dependencies;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END DEPENDENCIES ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMMON SCHEMA ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Returns the number of dependencies inserted, or -1 if a self-dependency was arised.
int common_schema_baseline(Schema s1, Schema s2, Schema* common, ArrayListDependencyPair* dependencies, Arena* arena)
{
    if (s1.type == VARIABLE_SCHEMA && s2.type == VARIABLE_SCHEMA && s1.v == s2.v) {
        // Do not insert v->v dependency
        init_variable_schema(common, s1.v);
        return 0;
    }

    if (s1.type == VARIABLE_SCHEMA) {
        Variable v = s1.v;
        if (is_self_dependency(v, s2)) {
            return -1;
        }
        init_variable_schema(common, v);
        return insert_to_dependencies_baseline(dependencies, v, s2, arena);
    }

    if (s2.type == VARIABLE_SCHEMA) {
        Variable v = s2.v;
        if (is_self_dependency(v, s1)) {
            return -1;
        }
        init_variable_schema(common, v);
        return insert_to_dependencies_baseline(dependencies, v, s1, arena);
    }

    size_t min_arity, max_arity;
    Schema* longest_schema;
    if (s1.arity < s2.arity) {
        min_arity = s1.arity;
        max_arity = s2.arity;
        longest_schema = &s2;
    } else {
        min_arity = s2.arity;
        max_arity = s1.arity;
        longest_schema = &s1;
    }

    init_general_schema_arena(common, max_arity, arena); // .size = 1
    int total_inserted_dependencies = 0;
    size_t i = 0;
    for (; i < min_arity; ++i) {
        int num_inserted_dependencies = common_schema_baseline(s1.subschemas[i], s2.subschemas[i],
            common->subschemas + i, dependencies, arena);
        if (num_inserted_dependencies < 0) {
            return -1;
        }
        total_inserted_dependencies += num_inserted_dependencies;
        common->size += common->subschemas[i].size;
    }
    for (; i < max_arity; ++i) {
        common->subschemas[i] = longest_schema->subschemas[i]; // NOTE: shallow copy...
        common->size += common->subschemas[i].size;
    }

    return total_inserted_dependencies;
}

// Special version that is only interested in the new dependencies that may arise
int common_schema_dependencies_baseline(Schema s1, Schema s2, ArrayListDependencyPair* dependencies, Arena* arena)
{
    if (s1.type == VARIABLE_SCHEMA && s2.type == VARIABLE_SCHEMA && s1.v == s2.v) {
        // Do not insert v->v dependency
        return 0;
    }

    if (s1.type == VARIABLE_SCHEMA) {
        if (is_self_dependency(s1.v, s2)) {
            return -1;
        }
        return insert_to_dependencies_baseline(dependencies, s1.v, s2, arena);
    }
    if (s2.type == VARIABLE_SCHEMA) {
        if (is_self_dependency(s2.v, s1)) {
            return -1;
        }
        return insert_to_dependencies_baseline(dependencies, s2.v, s1, arena);
    }

    int num_new_dependencies = 0;
    unsigned min_arity = MIN(s1.arity, s2.arity);
    for (unsigned i = 0; i < min_arity; ++i) {
        int num_new_deps = common_schema_dependencies_baseline(s1.subschemas[i], s2.subschemas[i], dependencies, arena);
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
int theta_rule1_baseline(ArrayListDependencyPair* dependencies, ArrayListSchema v_dependencies, Arena* arena)
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
int theta_rule2_baseline(ArrayListDependencyPair* dependencies, DependencyPair* pair, Arena* arena)
{
    int num_new_dependencies = 0;
    Variable v = pair->v;
    ArrayListSchema* schemas = &pair->schemas;

    // NOTE: iteration based on indexes to prevent dangling pointers from resizing
    // NOTE: necessary to take the initial size of the schemas because it may be incremented, and in case of self-dependencies,
    //   it would possibly loop infinitely!!! Additionally, that way we have the same behaviour in both rules!
    SetVariables ws = create_set_variables_defsize();
    size_t initial_size = schemas->size;
    for (size_t i = 0; i < initial_size; ++i) {
        Schema gamma = schemas->array[i];
        variables_in_schema(gamma, &ws);
        foreach_in_setvariables(ws, node_var)
        {
            Variable w = node_var->v;
            unsigned wpos = find_v_in_array_list_dependency_pair(*dependencies, w);
            if (wpos == dependencies->size) {
                continue;
            }
            ArrayListSchema schemas_ = dependencies->array[wpos].schemas;

            foreach_in_arraylist(Schema, schema_, schemas_)
            {
                Schema gamma_ = *schema_;
                Schema gamma__;
                substitute_arena(gamma, w, gamma_, &gamma__, arena);
                if (is_self_dependency(v, gamma__)) {
                    free_set_variables(ws);
                    return -1;
                }
                int contained = add_no_repeated_to_array_list_schema_arena(schemas, gamma__, arena);
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
    unsigned iteration = 0;
    do {
        ++iteration;

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
    ArrayListSchema* set_schema, ArrayListDependencyPair* dependencies, Arena* arena)
{
    // OPT: to optimize computation of sets of variables would be helpful to add a field of sets of variables to
    //  schemas/set_schemas, but that would be more state to manage too... Maybe we can initially let the field to
    //  NULL and the first time the function to calculate sets of variables is called it changes the value of that
    //  pointer (only if it was initially NULL). We would need to be carefull about how we store those sets of variables...
    //  Maybe a bitset would be more efficient, if we know that variables go from 1 to n?
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
    foreach_in_arraylistptr(DependencyPair, pair, dependencies)
    {
        // NOTE: from right to left to diminish leftwise copying and to avoid extra index corrections and overhead of call function to get
        ArrayListSchema* dependency_schemas = &pair->schemas;
        for (int i = dependency_schemas->size - 1; i >= 0; --i) {
            // NOTE: we take by value because if not, when removing, it will point to the next schema before applying the substitution by <>
            Schema schema = dependency_schemas->array[i];
            variables_in_schema(schema, &schema_vs);

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
                remove_index_from_array_list_schema(dependency_schemas, i);
            }
            // If only contains some removed variable without dependencies (not removed_vs_with_dependencies nor final_vs):
            //  remove and add substitution (all those variables) with <>
            else if (removed_vs_without_dependencies_in_schema.num_variables) {
                remove_index_from_array_list_schema(dependency_schemas, i);
                Schema empty = empty_schema();
                Schema emptied;
                substitute_vars_arena(schema, removed_vs_without_dependencies_in_schema, empty, &emptied, arena);
                add_no_repeated_to_array_list_schema_arena(dependency_schemas, emptied, arena);
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
    ArrayListSchema* set_schema1, ArrayListDependencyPair* dependencies1,
    ArrayListSchema* set_schema2, ArrayListDependencyPair* dependencies2,
    ArrayListSchema* common_set_schema, ArrayListDependencyPair* common_dependencies,
    Arena* arena, bool remove_variables_not_in_resulting_common_schema)
{
    if (set_schema1->size != set_schema2->size) {
        return false;
    }
    unsigned num_schemas = set_schema1->size; // num columns

    // NOTE: no resizing risk for Arena - Num schemas initialized here to the precalculated value because then we take pointers to each Schema, as if it already was a valid arraylist element
    *common_set_schema = create_array_list_schema_arena(num_schemas, arena);
    common_set_schema->size = num_schemas;
    // NOTE: resizing risk for Arena, unknown number of variables with new dependencies (even variables that initially didn't have any)
    ArrayListDependencyPair new_dependencies = create_array_list_dependency_pair_arena(10, arena);

    for (unsigned i = 0; i < num_schemas; ++i) {
        Schema s1 = set_schema1->array[i];
        Schema s2 = set_schema2->array[i];
        Schema* common_schema = common_set_schema->array + i;
        int num_new_dependencies = common_schema_baseline(s1, s2, common_schema, &new_dependencies, arena);
        if (num_new_dependencies < 0) {
            return false;
        }
    }
    // common_set_schema calculated

    // Start calculating the final common_dependencies set as the union of the two inputs and new_dependencies
    // NOTE: capacity can be greater than final size (we can have dependencies of the same variable in several sets)
    //  therefore, there is no risk of resizing
    *common_dependencies = create_array_list_dependency_pair_arena(dependencies1->size + dependencies2->size + new_dependencies.size, arena);
    // NOTE: Only copy the header of the array, the elements are shared
    extend_array_list_dependency_pair_arena(common_dependencies, *dependencies1, arena);
    // NOTE: here, when adding new depencies to the array list of an already stored variable, we can have resizing
    union_of_dependencies_baseline(common_dependencies, *dependencies2, arena);
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
    ArrayListSchema* set_schema1, ArrayListDependencyPair* dependencies1,
    ArrayListSchema* set_schema2, ArrayListDependencyPair* dependencies2,
    ArrayListSchema* common_set_schema, ArrayListDependencyPair* common_dependencies,
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

void calculate_num_appearences_of_variables_in_set_schema(ArrayListSchema set_schema, ArrayListVarNum* varnums, Arena* arena)
{
    foreach_in_arraylist(Schema, schema, set_schema)
    {
        calculate_num_appearences_of_variables_in_schema(*schema, varnums, arena);
    }
}

// OPT: for the strict version of common_set_schema_baseline
// - First check:
//      + Better O-performance with ordered maps (and their iterators) with better search performance for variables than O(n) like arraylists
//      + Another composed approach would be to huse unordered-(hash-)maps for number of appearences, and an arraylist for the order of the vars
bool first_check(ArrayListSchema set_schema1, ArrayListSchema set_schema2, Arena* arena)
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
bool unique_dependency_between_vars_(ArrayListSchema schemas, SetVariables* variables)
{
    foreach_in_arraylist(Schema, s, schemas)
    {
        unsigned num_vars_in_schema = 0;
        if (!unique_dependency_between_vars__(*s, variables, &num_vars_in_schema)) {
            return false;
        }
    }
    return true;
}
bool unique_dependency_between_vars(ArrayListDependencyPair dependencies)
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
    ArrayListSchema* set_schema1, ArrayListDependencyPair* dependencies1,
    ArrayListSchema* set_schema2, ArrayListDependencyPair* dependencies2,
    ArrayListSchema* common_set_schema, ArrayListDependencyPair* common_dependencies,
    Arena* arena)
{
    bool result = first_check(*set_schema1, *set_schema2, arena) && common_set_schema_baseline_(set_schema1, dependencies1, set_schema2, dependencies2, common_set_schema, common_dependencies, arena, false) && first_check(*set_schema1, *common_set_schema, arena) && unique_dependency_between_vars(*common_dependencies);

    if (!result) {
        return false;
    }

    remove_variables_not_in_set_schema_from_dependencies(common_set_schema, common_dependencies, arena);
    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END COMMON SET SCHEMA STRICT VERSION ////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// OBTAIN COLUMN MAPPING FROM COMMON SET SCHEMA ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// #region Set-Schema Normalization
Schema longest_dependency(ArrayListDependencyPair dependencies, Variable v){
    unsigned v_i = find_v_in_array_list_dependency_pair(dependencies, v);
    if (v_i == dependencies.size) {
        // v without dependencies --> longest dependency is the empty schema
        return empty_schema();
    }
    ArrayListSchema schemas = dependencies.array[v_i].schemas;

    unsigned max_size = 0;
    unsigned max_i = 0;
    unsigned i = 0;
    foreach_in_arraylist(Schema, schema, schemas){
        if (schema->size > max_size) {
            max_size = schema->size;
            max_i = i;
        }
        ++i;
    }
    return schemas.array[max_i];
}

// NOTE: the input schema is not overwritten, a new copy is done for the normalized schema.
Schema normalized_schema(Schema schema, ArrayListDependencyPair dependencies, Arena* arena){
    if (schema.type == VARIABLE_SCHEMA) {
        return longest_dependency(dependencies, schema.v);
    }

    Schema result;
    init_general_schema_arena(&result, schema.arity, arena);
    foreach_in_schemas(result, schema, subres, sub){
        *subres = normalized_schema(*sub, dependencies, arena);
    }
    return result;
}

// NOTE: copy is made for the normalized one, not in-memory modification.
// TODO: if we see that we only need the normalized one, we can change this.
ArrayListSchema normalized_set_schema(ArrayListSchema set_schema, ArrayListDependencyPair dependencies, Arena* arena){
    // NOTE: no resizing risk.
    ArrayListSchema result = create_array_list_schema_arena(set_schema.size, arena);
    foreach_in_arraylists(Schema, res, schema, result, set_schema){
        *res = normalized_schema(*schema, dependencies, arena);
    }
    return result;
}
// #endregion

// NOTE: column indexes are integers between and [1, total_cols]
//  We have one partition per each schema, i.e., per each free variable, that are constituted by contiguous integers, so
//  we will identify each partition with the starting value until (excluding) the starting value of the next partition (the
//  last one doesn't need a second number, total_cols is its final value).
unsigned *partition_column_indexes(ArrayListSchema fragment_set_schema, unsigned total_cols, Arena *arena){
    unsigned *partitions = allocate(arena, sizeof(*partitions) * fragment_set_schema.size);
    partitions[0] = 1;
    for(unsigned i = 1; i < fragment_set_schema.size; ++i){
        partitions[i] = partitions[i - 1] + fragment_set_schema.array[i].size;
    }
    return partitions;
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

// Calculate common set schema considering free vars
bool common_set_schema_free_vars_baseline_(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1, ArrayListCharPtr free_vars1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, ArrayListCharPtr free_vars2,
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies, ArrayListCharPtr *final_free_vars,
    Arena *arena, bool remove_variables_not_in_resulting_common_schema)
{
    // NOTE: no resizing risk for Arena
    *common_set_schema = create_array_list_schema_arena(final_free_vars->size, arena);

    // NOTE: resizing risk for Arena, unknown number of variables with new dependencies (even variables that initially didn't have any)
    ArrayListDependencyPair new_dependencies = create_array_list_dependency_pair_arena(10, arena);

    foreach_in_arraylistptr(CharPtr, free_var, final_free_vars){
        unsigned pos1 = find_in_array_list_char_ptr(free_vars1, *free_var);
        unsigned pos2 = find_in_array_list_char_ptr(free_vars2, *free_var);
        assert((pos1 < free_vars1.size) || (pos2 < free_vars2.size));

        if(pos1 == free_vars1.size){
            // M1 doesn't contain free_var --> Take schema from M2 and no new dependencies
            add_to_array_list_schema_arena(common_set_schema, set_schema2->array[pos2], arena);
        } else if (pos2 == free_vars2.size) {
            // M2 doesn't contain free_var --> same with M1
            add_to_array_list_schema_arena(common_set_schema, set_schema1->array[pos1], arena);
        } else {
            // free_var is common
            Schema s1 = set_schema1->array[pos1];
            Schema s2 = set_schema2->array[pos2];
            Schema common_schema;
            int num_new_dependencies = common_schema_baseline(s1, s2, &common_schema, &new_dependencies, arena);
            if (num_new_dependencies < 0) {
                return false;
            }
            add_to_array_list_schema_arena(common_set_schema, common_schema, arena);
        }
    }
    // common_set_schema calculated

    // TODO: refactor this code as its common for al the functions that compute common_set_schemas... (if finally all of them remain)
    // Start calculating the final common_dependencies set as the union of the two inputs and new_dependencies
    // NOTE: capacity can be greater than final size (we can have dependencies of the same variable in several sets)
    //  therefore, there is no risk of resizing
    *common_dependencies = create_array_list_dependency_pair_arena(dependencies1->size + dependencies2->size + new_dependencies.size, arena);
    // NOTE: Only copy the header of the array, the elements are shared
    extend_array_list_dependency_pair_arena(common_dependencies, *dependencies1, arena);
    // NOTE: here, when adding new depencies to the array list of an already stored variable, we can have resizing
    union_of_dependencies_baseline(common_dependencies, *dependencies2, arena);
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

bool common_set_schema_free_vars_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1, ArrayListCharPtr free_vars1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, ArrayListCharPtr free_vars2,
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies, ArrayListCharPtr *final_free_vars,
    Arena *arena, bool remove_variables_not_in_resulting_common_schema)
{
    *final_free_vars = final_free_vars_ordering(free_vars1, free_vars2, arena);
    return common_set_schema_free_vars_baseline_(
        set_schema1, dependencies1, free_vars1, 
        set_schema2, dependencies2, free_vars2, 
        common_set_schema, common_dependencies, final_free_vars,
        arena, true);
}

// TODO_YA: fix this for free_vars version!
bool first_check_free_vars(
    ArrayListSchema set_schema1, ArrayListSchema set_schema2, 
    ArrayListCharPtr free_vars1, ArrayListCharPtr free_vars2, ArrayListCharPtr final_free_vars,
    Arena* arena)
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

// TODO: since set_schema1 and other inputs are not changed we should take them by value... (in other alike functions too...)
bool common_set_schema_strict_free_vars_baseline(
    ArrayListSchema *set_schema1, ArrayListDependencyPair *dependencies1, ArrayListCharPtr free_vars1,
    ArrayListSchema *set_schema2, ArrayListDependencyPair *dependencies2, ArrayListCharPtr free_vars2,
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies, ArrayListCharPtr *final_free_vars,
    Arena *arena)
{
    *final_free_vars = final_free_vars_ordering(free_vars1, free_vars2, arena);
    
    bool result = first_check_free_vars(
                    *set_schema1, *set_schema2, 
                    free_vars1, free_vars2, *final_free_vars, 
                    arena) && 
                  common_set_schema_free_vars_baseline_(
                    set_schema1, dependencies1, free_vars1,
                    set_schema2, dependencies2, free_vars2, 
                    common_set_schema, common_dependencies, final_free_vars, 
                    arena, false) && 
                  first_check_free_vars(
                    *set_schema1, *common_set_schema, 
                    free_vars1, free_vars2, *final_free_vars, 
                    arena) &&
                  unique_dependency_between_vars(*common_dependencies);

    if (!result) {
        return false;
    }

    remove_variables_not_in_set_schema_from_dependencies(common_set_schema, common_dependencies, arena);
    return true;
}

// First pass through extended rows (R1 and R2) to map each original var in the inductive terms (at least) with its intermediate extending vars
// * How do we identify vars? Like Alex? (0 first appearence, -col_first_appearence for next repeated ones)
// * Iterate through extende row as we do recursion over the input common set schema (not the calculated one)
//  - When a variable is found the first time (0 value) if its corresponding schema's size is >1, then the comming size-1 variables
//  (0, since it is the first time) will be intra-fragment extending variable (not present in the original inductive term)
//      --> Map the variable (identified with -col) with those following variables (-col-1, -2, ...)
// * If needed, we can store the corresponding normalized schema too, and the list of all appearences

// Second pass after common_set_schema_free_vars is calculated: according to the final_free_vars ordering
//  - If not free vars add as many virtual columns as the size of the other's schema
//  - Otherwise, calculate the new virtual extending variables

// Final loop to get the mapping
// In the mapping we will have as much pairs as the size of the resulting common set schema (summation of its schemas)
// Loop from 1 to that size setting the L and R column indexes
//  - Get the L and R cols: matching the flattened partition with the resulting common schema
//  - If original num_col, just that value (once only in the mapping)
//  - If an extending virtual column, its value (if virtual column of a repeated variable, it will appear more than once)


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END OBTAIN COLUMN MAPPING FROM COMMON SET SCHEMA ////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
