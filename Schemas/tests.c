/*
To compile:
gcc -Wall -Wextra -g Schemas/tests.c Schemas/set_variables.c -o build/tests
*/

#include "set_variables.h"
#include "arraylist.h"
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

void test_set_variables(){
    SetVariables set = create_set_variables_defsize();

    for(Variable v = 1; v <= 10; ++v){
        insert_to_set_variables(&set, v);
    }
    print_set_variables(set); printf("\n");

    for(Variable v = 1; v <= 20; ++v){
        char *inSet = lookup_set_variables(set, v) ? "True" : "False";
        printf("%d in set = %s\n", v, inSet);
    }

    clear_set_variables(set);
    print_set_variables(set); printf("\n");

    free_set_variables(set);
}

void test_typeof_or_auto_type(){
    int x = 10;
    __auto_type y = x;
    int *xptr = &x;
    __auto_type yptr = &y; // NOTE: there is no * before yptr
    printf("x=%d - y=%d\n", x, y);
    printf("&x=%p\n&y=%p\n", xptr, yptr);
}

void test_arraylist_ints(){
    ArrayListInt list = create_array_list_int_defsize();

    print_array_list_int(list); printf("\n");

    for(int i = 0; i < 20; ++i){
        add_to_array_list_int(&list, i);
    }

    print_array_list_int(list); printf("\n");

    int first, median, last;
    get_from_array_list_int(list, 0, &first);
    get_from_array_list_int(list, list.size / 2, &median);
    get_from_array_list_int(list, list.size - 1, &last);
    printf("%d - %d - %d\n", first, median, last);

    remove_index_from_array_list_int(&list, 0);
    remove_index_from_array_list_int(&list, list.size / 2);
    remove_index_from_array_list_int(&list, list.size - 1);

    print_array_list_int(list); printf("\n");

    remove_element_from_array_list_int(&list, 7);

    print_array_list_int(list); printf("\n");
}

bool is_white_line(char *line, size_t len){
    for(size_t i = 0; i < len; ++i){
        if(!isspace(line[i])){ return false; }
    }
    return true;
}

static inline unsigned num_digits(unsigned n){
    unsigned res = 1;
    while((n /= 10) != 0){ ++res; }
    return res;
}

Schema read_schema(char **schema_str){
    Schema result;
    
    unsigned v;
    if(sscanf(*schema_str, "$%u", &v) == 1){
        *schema_str += 1 + num_digits(v);
        init_variable_schema(&result, v);
        return result;
    }

    unsigned arity;
    if(sscanf(*schema_str, "%u:[", &arity) == 1){
        init_general_schema(&result, arity); // size = 1, needs to be updated if arity > 0
        *schema_str += num_digits(arity) + 2;
        for(size_t i = 0; i < arity; ++i){
            result.subschemas[i] = read_schema(schema_str);
            result.size += result.subschemas[i].size;
        }
        *schema_str += 1;
        return result;
    }

    fprintf(stderr, "read_schema: Unexpected schema format!\n");
    exit(1);
}

unsigned scan_num_schemas_in_set(char *line){
    unsigned num_cols = 0;
    unsigned brackets = 0;
    for(char *lineptr = line; lineptr; ++lineptr){
        unsigned v, arity;
        if(sscanf(lineptr, "$%u,", &v) == 1){
            if(brackets == 0) { ++num_cols; }
            lineptr += 1 + num_digits(v);
        }
        else if (sscanf(lineptr, "%u:[", &arity) == 1){
            if(brackets == 0) { ++num_cols; }
            ++brackets;
            lineptr += num_digits(arity) + 2;
        }
        else if (*lineptr == '\n'){
            continue;
        }
        else {
            fprintf(stderr, "read_set_schema: Unexpected set schema format!\n");
            exit(2);
        }
        if(*lineptr == ']') { --brackets; }
    }
    return num_cols;
}

ArrayListSchema read_set_schema(char *line){
    // Precalculate the number of columns; i.e., the number of schemas in the set-schema
    unsigned num_cols = scan_num_schemas_in_set(line);
    ArrayListSchema set_schema = create_array_list_schema(num_cols);
    
    // Parse each schema
    unsigned i = 0;
    while(*line != '\0' && *line != '\n'){
        // NOTE: we can directly set without bound checking thanks to the precalculation of the number of columns
        set_schema.array[i++] = read_schema(&line);
        // Skip the comma separating the schemas in the set-schema (or the new line at the end)
        ++line;
    }

    return set_schema;
}

ArrayListDependencyPair read_set_dependencies(FILE *stream, unsigned num_vars_with_dependencies){
    char *line = NULL; // TODO: see if we need to pass this by pointer...
    size_t len = 0;
    ssize_t read;

    ArrayListDependencyPair dependencies = create_array_list_dependency_pair(num_vars_with_dependencies);
    
    for(unsigned i = num_vars_with_dependencies; i < num_vars_with_dependencies; ++i){
        read = getline(&line, &len, stream);
        unsigned v;
        if(sscanf("$%u <- [", &v) == 0){
            fprintf(stderr, "read_set_dependencies: Unexpected list of dependencies format!\n");
            exit(3);
        }
        
        // Adapt the read line to obtain the number of schemas and the schemas themselves
        line[read-2] = '\0';            // read-1 == '\n', -2 == ']' (the closing braquet of the list of dependencies of v)
        line += 1 + num_digits(v) + 5;  // Focus on the beginning of the first schema

        DependencyPair pair = { .v = v, .schemas = read_set_schema(line) };
        //NOTE: we can directly set without bound checking thanks to having read the number of vars with dependencies
        dependencies.array[i] = pair;
    }

    free(line);
    return dependencies;
}

int read_next_set_schema_with_dependencies(
    FILE *stream, ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies)
{
    char *line = NULL; // TODO: see if we need to pass this by pointer...
    size_t len = 0;
    ssize_t read;

    while((read = getline(&line, &len, stream)) != -1){
        if(len == 0 || line[0] == '%' || is_white_line(line, len)){ continue; }
    }
    if (read == -1) {
        return -1;  // EOF
    }

    unsigned num_vars_with_dependencies;
    if(sscanf(line, "%u, ", &num_vars_with_dependencies) != 1) {
        free(line);
        return 1; // Common schema
    }

    // Skip "num_vars_with_dependencies, "
    line += num_digits(num_vars_with_dependencies) + 2;

    *set_schema = read_set_schema(line);
    *dependencies = read_set_dependencies(stream, num_vars_with_dependencies);

    free(line);
    return 0;
}

bool equal_set_schemas(ArrayListSchema set_schema1, ArrayListSchema set_schema2){
    if(set_schema1.size != set_schema2.size) { return false; }
    unsigned size = set_schema1.size;

    for(unsigned i = 0; i < size; ++i){
        if(!equal_schemas(set_schema1.array + i, set_schema2.array + i)){ return false; }
    }

    return true;
}

bool equal_set_dependencies(ArrayListDependencyPair dependencies1, ArrayListDependencyPair dependencies2){
    if(dependencies1.size != dependencies2.size) { return false; }
    unsigned num_vars_with_dependencies = dependencies1.size;

    for(unsigned index1 = 0; index1 < num_vars_with_dependencies; ++index1){
        DependencyPair pair1 = dependencies1.array[index1];
        Variable v = pair1.v;
        ArrayListSchema schemas1 = pair1.schemas;  // Taking by value is not problematic because we're not modifying

        // Find v in dependencies2
        unsigned index2 = 0;
        for(; index2 < num_vars_with_dependencies; ++index2){
            if(dependencies2.array[index2].v == v) { break; }
        }
        if(index2 == num_vars_with_dependencies){ //no break
            return false;  // The variable in dependencies1 wasn't found in dependencies2
        }
        ArrayListSchema schemas2 = dependencies2.array[index2].schemas;  // Taking by value is not problematic because we're not modifying

        if(schemas1.size != schemas2.size) { return false; }
        unsigned num_dependencies_of_v = schemas1.size;

        // Unordered equals between schemas1 and schemas2
        for(unsigned i = 0; i < num_dependencies_of_v; ++i){
            Schema *s1 = schemas1.array + i;
            // Find s1 in schemas2
            unsigned j = 0;
            for(; j < num_dependencies_of_v; ++j){
                Schema *s2 = schemas2.array + j;
                if(equal_schemas(s1, s2)){
                    break;
                }
            }
            if(j == num_dependencies_of_v){ //no break
                return false;  // There is a dependency for v in dependencies1 that is not found in dependencies2
            }
        }
    }
    return true;
}

// Read lines until the test begin line or EOF is found.
unsigned read_test_number(FILE *stream){
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while((read = getline(&line, &len, stream)) != -1){
        unsigned test_number;
        if(sscanf(line, "%%%%%% BEGIN common schema test %u %%%%%%", &test_number) == 1){
            return test_number;
        }
    }
    return 0; //EOF
}

void test_schema_management(){
    char *line = NULL; //TODO: see if we need to pass this to following functions and if the resource cleaning is done in every place it should be done...
    size_t len = 0;
    ssize_t read;

    //getline, free, sscanf, strchr, strstr, printf, fprintf(stderr, ...), strtok, strdup, strtoul, strtol, strcmp, atoi, etc.

    char *filename = "data/schemas/AGT006+1_truncated.txt";
    FILE *stream = fopen(filename, "r");

    for(;;){
        ArrayListSchema set_schema1, set_schema2, common_set_schema, computed_common_set_schema;
        ArrayListDependencyPair dependencies1, dependencies2, common_dependencies, computed_common_dependencies;

        unsigned test_number = read_test_number(stream);
        if(test_number == 0){
            break; // EOF
        }

        read = read_next_set_schema_with_dependencies(stream, &set_schema1, &dependencies1);
        assert(read != -1 && read != 1);

        read = read_next_set_schema_with_dependencies(stream, &set_schema2, &dependencies2);
        assert(read != -1 && read != 1);

        read = read_next_set_schema_with_dependencies(stream, &common_set_schema, &common_dependencies);
        if(read == -1){
            break; // EOF
        }
        bool common_schema_exists = read != 1; // read == 0

        // TODO_YA: Compute common schema and its dependencies with operands 1 and 2
        common_set_schema_baseline(&set_schema1, &dependencies1, &set_schema2, &dependencies2,
                                   &computed_common_set_schema, &computed_common_dependencies);

        // Compare the computed common schema and its dependencies with the read ones
        bool set_schemas_ok = equal_set_schemas(computed_common_set_schema, common_set_schema);
        bool dependencies_ok = equal_set_dependencies(computed_common_dependencies, common_dependencies);
        if(!(set_schemas_ok && dependencies_ok)){
            printf("Test=%u - set_schemas_ok=%u - dependencies_ok=%u\n", 
                    test_number, set_schemas_ok, dependencies_ok);
        }

        // TODO_YA: free the schemas and dependencies (be careful with dangling pointers!)
    }

    free(line);
    fclose(stream);
}

int main(/*int argc, char const *argv[]*/)
{
    //test_set_variables();
    //test_typeof_or_auto_type();
    //test_arraylist_ints();
    test_schema_management();
    return 0;
}
