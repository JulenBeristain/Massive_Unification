/*
To compile:
gcc -Wall -Wextra -g Schemas/tests.c Schemas/set_variables.c -o build/tests
*/

#include "schemas.h"
#include "set_variables.h"
#include "arraylist.h"
#include "arena.h"
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

//size_t global_line_number = 0;
bool global_print_debugging = false;

void test_set_variables(){
    SetVariables set = create_set_variables_defsize();

    for(Variable v = 1; v <= 10; ++v){
        insert_to_set_variables(&set, v);
    }
    print_set_variables(set); printf("\n");

    for(Variable v = 1; v <= 20; ++v){
        char *inSet = is_in_set_variables(set, v) ? "True" : "False";
        printf("%d in set = %s\n", v, inSet);
    }

    clear_set_variables(&set);
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

static inline bool equal_ints(int a, int b) { return a == b; }
static inline void print_int(int i) { printf("%d", i); }
typedef int Int;
DECLARE_ARRAYLIST_TYPE(Int)
DEFINE_ARRAYLIST_CREATE(Int, int)
DEFINE_ARRAYLIST_FREE(Int, int)
DEFINE_ARRAYLIST_ADD(Int, int)
DEFINE_ARRAYLIST_REMOVE_INDEX(Int, int)
DEFINE_ARRAYLIST_FIND(Int, int, equal_ints)
DEFINE_ARRAYLIST_REMOVE_ELEMENT(Int, int)
DEFINE_ARRAYLIST_GET(Int, int)
DEFINE_ARRAYLIST_PRINT_SEPARATORS(Int, int, print_int)
DEFINE_ARRAYLIST_PRINT(Int, int)
DEFINE_ARRAYLIST_PRINTLN(Int, int)

void test_arraylist_ints(){
    ArrayListInt list = create_array_list_int_defcapacity();

    println_array_list_int(list);

    for(int i = 0; i < 20; ++i){
        add_to_array_list_int(&list, i);
    }

    println_array_list_int(list);

    int first, median, last;
    get_from_array_list_int(list, 0, &first);
    get_from_array_list_int(list, list.size / 2, &median);
    get_from_array_list_int(list, list.size - 1, &last);
    printf("%d - %d - %d\n", first, median, last);

    remove_index_from_array_list_int(&list, 0);
    remove_index_from_array_list_int(&list, list.size / 2);
    remove_index_from_array_list_int(&list, list.size - 1);

    println_array_list_int(list);

    remove_element_from_array_list_int(&list, 7);

    println_array_list_int(list);

    free_array_list_int(list);
}

static inline bool is_white_line(char *line, size_t len){
    for(size_t i = 0; i < len; ++i){
        if(!isspace(line[i])){ return false; }
    }
    return true;
}

int read_next_set_schema_with_dependencies(
    FILE *stream, ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies, Arena *arena)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while((read = getline(&line, &len, stream)) != -1){
        // Skip whitelines and comments
        if(len == 0 || line[0] == '%' || is_white_line(line, len)){ continue; }

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

void print_mapping(Variable *mapping, unsigned num_vars){
    for(unsigned i = 1; i <= num_vars; ++i){
        printf("[%u]=%u - ", i, mapping[i]);
    }
    printf("\n");
}

// Read lines until a test begin line or EOF is found.
unsigned read_test_number(FILE *stream){
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    unsigned test_number = 0;
    while((read = getline(&line, &len, stream)) != -1){
        if(sscanf(line, "%%%%%% BEGIN common schema test %u %%%%%%", &test_number) == 1){
            break;
        }
    }
    if(line) { free(line); }
    return test_number; // if EOF -> 0
}

void test_schema_management_(const char *filename, Arena *arena){
    printf("#####################################################################################\n");
    printf("#####################################################################################\n");
    printf("Reading the schemas from: %s\n", filename);
    FILE *stream = fopen(filename, "r");

    uint64_t num_correct_cases = 0;
    uint64_t num_total_cases = 0;
    
    for(;;){
        // TODO: a unique dependency set for set_schema1/2 and computed common is enough...
        ArrayListSchema set_schema1, set_schema2, common_set_schema, computed_common_set_schema;
        ArrayListDependencyPair dependencies1, dependencies2, common_dependencies, computed_common_dependencies;

        unsigned test_number = read_test_number(stream);
        if(test_number == 0){
            break; // EOF
        }
        //printf("--- Test=%u ---\n", test_number);

        ssize_t read = read_next_set_schema_with_dependencies(stream, &set_schema1, &dependencies1, arena);
        assert(read != -1 && read != 1);

        read = read_next_set_schema_with_dependencies(stream, &set_schema2, &dependencies2, arena);
        assert(read != -1 && read != 1);

        read = read_next_set_schema_with_dependencies(stream, &common_set_schema, &common_dependencies, arena);
        if(read == -1){
            break; // EOF
        }
        bool common_schema_exists = read != 1; // read == 0

        if(global_print_debugging){
            printf("Set Schema 1:\n");
            print_set_schema(set_schema1, PRINT_VISUALLY);
            //print_set_schema(&set_schema1, PRINT_FILE_FORMAT);
            print_set_dependencies(dependencies1, PRINT_VISUALLY);
            //print_set_dependencies(&dependencies1, PRINT_FILE_FORMAT);

            printf("Set Schema 2:\n");
            print_set_schema(set_schema2, PRINT_VISUALLY);
            //print_set_schema(&set_schema2, PRINT_FILE_FORMAT);
            print_set_dependencies(dependencies2, PRINT_VISUALLY);
            //print_set_dependencies(&dependencies2, PRINT_FILE_FORMAT);

            printf("Common Set Schema:\n");
            if(common_schema_exists){
                print_set_schema(common_set_schema, PRINT_VISUALLY);
                //print_set_schema(&common_set_schema, PRINT_FILE_FORMAT);
                print_set_dependencies(common_dependencies, PRINT_VISUALLY);
                //print_set_dependencies(&common_dependencies, PRINT_FILE_FORMAT);
            } 
            else {
                printf("Common Schema does not exist!\n");
            }
        }

        Variable max_v1 = max_v_in_set_schema(set_schema1);
        increment_variables_in_set_schema(set_schema2, max_v1);
        increment_variables_in_set_dependencies(dependencies2, max_v1);

        if(global_print_debugging){
            printf("Modification of Set Schema 2:\n");
            printf("Max variable in Set Schema 1 = %u\n", max_v1);
            printf("Modified Set Schema 2:\n");
            print_set_schema(set_schema2, PRINT_VISUALLY);
            print_set_dependencies(dependencies2, PRINT_VISUALLY);
        }

        bool computed_common_schema_exists = common_set_schema_strict_baseline(&set_schema1, &dependencies1, 
            &set_schema2, &dependencies2, &computed_common_set_schema, &computed_common_dependencies, arena);

        if(common_schema_exists != computed_common_schema_exists){
            printf("Test=%u - common_schema_exists=%u - computed_common_schema_exists=%u\n", 
                    test_number, common_schema_exists, computed_common_schema_exists);
        }
        else if(common_schema_exists){
            // Compare the computed common schema and its dependencies with the read ones
            
            // TODO: see if we can avoid this recomputation with the work already done in common_set_schema_baseline...
            //  (not so important, as this is only testing code; would be helpful to add a field of sets of variables to
            //  schemas/set_schemas, but that would be more state to manage too...)
            // NOTE: variables are identified from 1 to n in the resulting common schema in the file
            SetVariables read_vars = create_set_variables_defsize(); 
            variables_in_set_schema(common_set_schema, &read_vars);
            unsigned num_read_vars = read_vars.num_variables;
            free_set_variables(read_vars);

            size_t num_bytes_for_mapping = (1 + num_read_vars) * sizeof(Variable);
            Variable *mapping = allocate(arena, num_bytes_for_mapping);
            memset(mapping, 0, num_bytes_for_mapping);

            if(global_print_debugging){
                printf("Computed: "); print_set_schema(computed_common_set_schema, PRINT_VISUALLY);
                printf("Read:     "); print_set_schema(common_set_schema, PRINT_VISUALLY);
            }
            // NOTE: important the order of the common schemas, since the mapping is from the variables from the first to the second!
            bool set_schemas_ok = equivalent_set_schemas(common_set_schema, computed_common_set_schema, mapping);

            bool dependencies_ok = equivalent_set_dependencies(common_dependencies, computed_common_dependencies, mapping);

            if(!(set_schemas_ok && dependencies_ok)){
                printf("Test=%u - set_schemas_ok=%u - dependencies_ok=%u\n", 
                        test_number, set_schemas_ok, dependencies_ok);
            }
            else { ++num_correct_cases; }
        }
        else { ++num_correct_cases; } // the common schema doesn't exist, and it wasn't calculated, as expected
        ++num_total_cases;

        clear_arena(arena);
    }

    fclose(stream);
    printf("num_correct_cases=%lu/%lu\n", num_correct_cases, num_total_cases);
}

void test_schema_management(int argc, char const *argv[]){
    Arena arena;
    init_arena(&arena, sizeof(Schema) * 100000);

    for(int i = 1; i < argc; ++i){
        test_schema_management_(argv[i], &arena);
    }
    
    free_arena(&arena); // TODO: Use Valgrind to ensure we don't leak memory...
}

int main(int argc, char const *argv[])
{
    //test_set_variables();
    //test_typeof_or_auto_type();
    //test_arraylist_ints();
    test_schema_management(argc, argv);
    return 0;
}
