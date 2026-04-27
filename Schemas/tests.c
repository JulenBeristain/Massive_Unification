/*
To compile:
gcc -Wall -Wextra -g Schemas/tests.c Schemas/set_variables.c -o build/tests
*/

#include "schemas.h"
#include "utils.h"
#include "set_variables.h"
#include "arraylist.h"
#include "arena.h"
#include "../dictionary.h"
#include "../structures.h"
#include "cache_mapping.h"
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

void test_set_variables()
{
    SetVariables set = create_set_variables_defsize();

    for (Variable v = 1; v <= 10; ++v)
    {
        insert_to_set_variables(&set, v);
    }
    print_set_variables(set);
    printf("\n");

    for (Variable v = 1; v <= 20; ++v)
    {
        char *inSet = is_in_set_variables(set, v) ? "True" : "False";
        printf("%d in set = %s\n", v, inSet);
    }

    clear_set_variables(&set);
    print_set_variables(set);
    printf("\n");

    free_set_variables(set);
}

void test_typeof_or_auto_type()
{
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

void test_arraylist_ints()
{
    ArrayListInt list = create_array_list_int_defcapacity();

    println_array_list_int(list);

    for (int i = 0; i < 20; ++i)
    {
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

static inline bool is_white_line(char *line, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        if (!isspace(line[i]))
        {
            return false;
        }
    }
    return true;
}

int read_next_set_schema_with_dependencies(
    FILE *stream, ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies, Arena *arena)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&line, &len, stream)) != -1)
    {
        // Skip whitelines and comments
        if (len == 0 || line[0] == '%' || is_white_line(line, len))
        {
            continue;
        }

        unsigned num_vars_with_dependencies;
        if (sscanf(line, "%u, ", &num_vars_with_dependencies) != 1)
        {
            free(line);
            return 1; // Common schema doesn't exist
        }

        // Skip "num_vars_with_dependencies, "
        char *lineptr = line + num_digits(num_vars_with_dependencies) + 2;

        *set_schema = read_set_schema(lineptr, arena);
        *dependencies = read_set_dependencies(stream, num_vars_with_dependencies, arena);

        free(line);
        return 0; // Common schema exists.
    }

    if (line)
    {
        free(line);
    }
    return -1; // EOF
}

void print_mapping(Variable *mapping, unsigned num_vars)
{
    for (unsigned i = 1; i <= num_vars; ++i)
    {
        printf("[%u]=%u - ", i, mapping[i]);
    }
    printf("\n");
}

// Read lines until a test begin line or EOF is found.
unsigned read_test_number(FILE *stream)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    unsigned test_number = 0;
    while ((read = getline(&line, &len, stream)) != -1)
    {
        if (sscanf(line, "%%%%%% BEGIN common schema test %u %%%%%%", &test_number) == 1)
        {
            break;
        }
    }
    if (line)
    {
        free(line);
    }
    return test_number; // if EOF -> 0
}

void test_schema_management_(const char *filename, Arena *arena)
{
    printf("#####################################################################################\n");
    printf("#####################################################################################\n");
    printf("Reading the schemas from: %s\n", filename);
    FILE *stream = fopen(filename, "r");

    uint64_t num_correct_cases = 0;
    uint64_t num_total_cases = 0;

    for (;;)
    {
        // NOTE: a unique dependency set would be enough for the three set_schemas the computing code works with:
        //  two operand set_schemas and a computed common set schema, because in the end we are interested in the
        //  union of all the dependencies.
        ArrayListSchema set_schema1, set_schema2, common_set_schema, computed_common_set_schema;
        ArrayListDependencyPair dependencies1, dependencies2, common_dependencies, computed_common_dependencies;

        unsigned test_number = read_test_number(stream);
        if (test_number == 0)
        {
            break; // EOF
        }
        // printf("--- Test=%u ---\n", test_number);

        ssize_t read = read_next_set_schema_with_dependencies(stream, &set_schema1, &dependencies1, arena);
        assert(read != -1 && read != 1);

        read = read_next_set_schema_with_dependencies(stream, &set_schema2, &dependencies2, arena);
        assert(read != -1 && read != 1);

        read = read_next_set_schema_with_dependencies(stream, &common_set_schema, &common_dependencies, arena);
        if (read == -1)
        {
            break; // EOF
        }
        bool common_schema_exists = read != 1; // read == 0

        if (global_print_debugging)
        {
            printf("Set Schema 1:\n");
            print_set_schema(set_schema1, PRINT_VISUALLY);
            // print_set_schema(&set_schema1, PRINT_FILE_FORMAT);
            print_set_dependencies(dependencies1, PRINT_VISUALLY);
            // print_set_dependencies(&dependencies1, PRINT_FILE_FORMAT);

            printf("Set Schema 2:\n");
            print_set_schema(set_schema2, PRINT_VISUALLY);
            // print_set_schema(&set_schema2, PRINT_FILE_FORMAT);
            print_set_dependencies(dependencies2, PRINT_VISUALLY);
            // print_set_dependencies(&dependencies2, PRINT_FILE_FORMAT);

            printf("Common Set Schema:\n");
            if (common_schema_exists)
            {
                print_set_schema(common_set_schema, PRINT_VISUALLY);
                // print_set_schema(&common_set_schema, PRINT_FILE_FORMAT);
                print_set_dependencies(common_dependencies, PRINT_VISUALLY);
                // print_set_dependencies(&common_dependencies, PRINT_FILE_FORMAT);
            }
            else
            {
                printf("Common Schema does not exist!\n");
            }
        }

        Variable max_v1 = max_v_in_set_schema(set_schema1);
        increment_variables_in_set_schema(set_schema2, max_v1);
        increment_variables_in_set_dependencies(dependencies2, max_v1);

        if (global_print_debugging)
        {
            printf("Modification of Set Schema 2:\n");
            printf("Max variable in Set Schema 1 = %u\n", max_v1);
            printf("Modified Set Schema 2:\n");
            print_set_schema(set_schema2, PRINT_VISUALLY);
            print_set_dependencies(dependencies2, PRINT_VISUALLY);
        }

        bool computed_common_schema_exists = common_set_schema_strict_baseline(set_schema1, dependencies1,
                                                                               set_schema2, dependencies2, &computed_common_set_schema, &computed_common_dependencies, arena);

        if (common_schema_exists != computed_common_schema_exists)
        {
            printf("Test=%u - common_schema_exists=%u - computed_common_schema_exists=%u\n",
                   test_number, common_schema_exists, computed_common_schema_exists);
        }
        else if (common_schema_exists)
        {
            // Compare the computed common schema and its dependencies with the read ones

            // NOTE: variables are identified from 1 to n in the resulting common schema in the file
            SetVariables read_vars = create_set_variables_defsize();
            variables_in_set_schema(common_set_schema, &read_vars);
            unsigned num_read_vars = read_vars.num_variables;
            free_set_variables(read_vars);

            size_t num_bytes_for_mapping = (1 + num_read_vars) * sizeof(Variable);
            Variable *mapping = allocate(arena, num_bytes_for_mapping);
            memset(mapping, 0, num_bytes_for_mapping);

            if (global_print_debugging)
            {
                printf("Computed: ");
                print_set_schema(computed_common_set_schema, PRINT_VISUALLY);
                printf("Read:     ");
                print_set_schema(common_set_schema, PRINT_VISUALLY);
            }
            // NOTE: important the order of the common schemas, since the mapping is from the variables from the first to the second!
            bool set_schemas_ok = equivalent_set_schemas(common_set_schema, computed_common_set_schema, mapping);

            bool dependencies_ok = equivalent_set_dependencies(common_dependencies, computed_common_dependencies, mapping);

            if (!(set_schemas_ok && dependencies_ok))
            {
                printf("Test=%u - set_schemas_ok=%u - dependencies_ok=%u\n",
                       test_number, set_schemas_ok, dependencies_ok);
            }
            else
            {
                ++num_correct_cases;
            }
        }
        else
        {
            ++num_correct_cases;
        } // the common schema doesn't exist, and it wasn't calculated, as expected
        ++num_total_cases;

        clear_arena(arena);
    }

    fclose(stream);
    printf("num_correct_cases=%llu/%llu\n", num_correct_cases, num_total_cases);
}

void test_schema_management(int argc, char const *argv[])
{
    Arena arena;
    init_arena(&arena, sizeof(Schema) * 100000);

    for (int i = 1; i < argc; ++i)
    {
        test_schema_management_(argv[i], &arena);
    }

    free_arena(&arena);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// TEST MAPPING OBTENTION //////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int read_num_blocks(FILE *stream, unsigned *s)
{
    char *line = NULL;
    size_t len = 0;

    if (getline(&line, &len, stream) == -1)
        return 0; // Failed to read the line
    if (strstr(line, "% BEGIN") == NULL)
    {
        free(line);
        return 0;
    } // Not the correct line

    char *endptr;
    char *e = strchr(line, '(');
    if (!e)
    {
        free(line);
        return 1;
    } // Not the correct line
    int num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1)
    {
        free(line);
        return 1;
    }

    *s = (unsigned)num;

    free(line);
    return 0;
}

unsigned scan_num_free_vars(char *line)
{
    unsigned num_free_vars = 1;
    for (char *c = line; *c; ++c)
    {
        if (*c == ',')
        {
            ++num_free_vars;
        }
    }
    return num_free_vars;
}
ArrayListCharPtr scan_free_vars(char *line, unsigned num_free_vars, Arena *arena)
{
    // NOTE: no resizing risk
    // NOTE: another option would be to use non-arena version, malloc and define the free_pointers for arraylists of charptrs...
    ArrayListCharPtr free_vars = create_array_list_char_ptr_arena(num_free_vars, arena);
    for (char *token = strtok(line, ",\n"); token; token = strtok(NULL, ",\n"))
    {
        char *var_str = allocate(arena, strlen(token) + 1);
        strcpy(var_str, token);
        unsafe_add_to_array_list(free_vars, var_str);
    }
    return free_vars;
}
ArrayListCharPtr read_free_vars(FILE *stream, Arena *arena)
{
    char *line = NULL;
    size_t len;

    if (getline(&line, &len, stream) == -1)
    {
        free(line);
        exit(1);
    } // Failed to read the line
    if (strstr(line, "Free") == NULL)
    {
        free(line);
        exit(1);
    } // Not the correct line

    unsigned num_free_vars = scan_num_free_vars(line);

    // NOTE: no resizing risk
    // NOTE: another option would be to use non-arena version, malloc and define the free_pointers for arraylists of charptrs...
    ArrayListCharPtr free_vars = create_array_list_char_ptr_arena(num_free_vars, arena);
    for (char *token = strtok(line, ",\n"); token; token = strtok(NULL, ",\n"))
    {
        char *var_str = allocate(arena, strlen(token) + 1);
        strcpy(var_str, token);
        unsafe_add_to_array_list(free_vars, var_str);
    }

    free(line);
    return free_vars;
}

void read_dimensions(FILE *stream, unsigned *n, unsigned *m)
{
    char *line = NULL;
    size_t len = 0;
    char *endptr, *e;
    long int num;

    getline(&line, &len, stream);
    if (strstr(line, "% BEGIN") == NULL)
    {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    }

    e = strchr(line, '(');
    if (!e)
    {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    };

    num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1)
    {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    }

    *n = num;

    e = strchr(endptr, ',');
    if (!e)
    {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    }

    num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1)
    {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    }
    *m = num;
    free(line);
}

// NOTE: we could parametirize this in read_line...
Dictionary *var_dict;
Dictionary *symbols_to_ids;
size_t next_symbol_id = 1;
void read_line(char *line, int *row, bool skip_first)
{
    char *tok = strtok(line, ",");
    if (skip_first)
        tok = strtok(NULL, ",\n");

    int col = 1;
    clear(var_dict);
    while (tok)
    {
        if (!isupper(tok[0]))
        { // If no uppercase appears, it is a constant
            struct nlist *token_id = lookup(symbols_to_ids, tok);
            if (token_id)
            {
                row[col - 1] = token_id->defn;
            }
            else
            {
                install(symbols_to_ids, tok, next_symbol_id);
                row[col - 1] = next_symbol_id++;
            }
        }
        else
        { // It is a variable
            if (lookup(var_dict, tok) == NULL)
            {
                row[col - 1] = 0;
                install(var_dict, tok, col);
            }
            else
            {
                row[col - 1] = -(lookup(var_dict, tok)->defn);
            }
        }
        tok = strtok(NULL, ",\n");
        col++;
    }
}

void get_mapping(char *line, unsigned n_pairs, unsigned *mapping)
{
    char *tok = strtok(line, " ,\n");
    unsigned count = 0;

    while (tok != NULL && count < n_pairs * 2)
    {
        char *dash = strchr(tok, '-');
        if (dash)
        {
            *dash = '\0';
            char *first = tok;
            char *second = dash + 1;
            mapping[count++] = (strcmp(first, "_") == 0) ? 0 : (unsigned)atoi(first);
            mapping[count++] = (strcmp(second, "_") == 0) ? 0 : (unsigned)atoi(second);
        }
        tok = strtok(NULL, " ,\n");
    }
}

void read_exception_blocks(FILE *stream, main_term *mt, const bool result)
{
    unsigned n, m;
    unsigned e = mt->e;
    exception_block *eb;
    char *line = NULL;
    size_t len = 0;

    for (unsigned i = 0; i < e; i++)
    {
        // Read dimensions of exception block (subset)
        read_dimensions(stream, &n, &m);

        // Create empty exception block to be populated later
        mt->exceptions[i] = create_empty_exception_block(n, m);
        eb = &(mt->exceptions[i]);

        // Skip unflatened schema
        getline(&line, &len, stream);

        // Read mapping
        getline(&line, &len, stream);
        unsigned *mapping = (unsigned *)malloc(eb->m * 2 * sizeof(unsigned));
        get_mapping(line, eb->m, mapping);
        eb->ms = create_mgu_from_mapping(mapping, eb->m, mt->c, eb->m);
        free(mapping);

        // Skip flatened schema if reading from operand
        if (!result)
            getline(&line, &len, stream);

        for (size_t j = 0; j < n; j++)
        {
            getline(&line, &len, stream);
            char *line_ptr = line;
            if (result)
                line_ptr = strchr(line_ptr, ':') + 2;
            read_line(line_ptr, &eb->mat[j * m], false);

            // Skip unifier if reading from result file
            if (result)
                getline(&line, &len, stream);
        }

        // Skip end exception subset line
        getline(&line, &len, stream);
    }

    free(line);
}

void read_operand_matrix(FILE *stream, operand_block *ob, ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies, Arena *arena)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    unsigned row = 0;

    // Read unflatened schema and the set of dependencies
    read_set_schema_with_dependencies(stream, set_schema, dependencies, arena);

    // Skip flatened schema
    getline(&line, &len, stream);

    // Iterate the main term rows
    while ((read = getline(&line, &len, stream)) != -1 && row < ob->r)
    {

        // If end of matrix reached, exit
        if (strstr(line, "% END") != NULL || strstr(line, "% End") != NULL)
            break;

        // Get first token, which tells the number of exception blocks
        char *line_copy = strdup(line);
        char *tok = strtok(line_copy, ",");
        unsigned e = (unsigned)strtoul(tok, NULL, 10);
        free(line_copy);

        // Initialize the exception blocks
        ob->terms[row] = create_empty_main_term(ob->c, e);

        // Get a pointer to the main term for easier working
        main_term *mt = &(ob->terms[row]);

        // Read the rest of the line
        read_line(line, mt->row, true);

        // Read one by one the exception blocks
        if (e)
            read_exception_blocks(stream, mt, false);

        // Increment the row by 1
        row++;
    }

    free(line);
}
void read_operand_block(FILE *stream, operand_block *ob, ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies, Arena *arena)
{

    // Read operand block dimensions
    unsigned r, c;
    read_dimensions(stream, &r, &c);

    // Create the operand_block structure for later populating it
    *ob = create_empty_operand_block(r, c);

    // Read the operand block and fill the struct
    read_operand_matrix(stream, ob, set_schema, dependencies, arena);
}

void read_result_matrix(
    FILE *stream,
    result_block *rb, ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena)
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Read block header
    getline(&line, &len, stream);
    int matched = sscanf(line, "%% BEGIN: Matrix subset %u-%u (%u-%u,%u-%u,%u)",
                         &rb->t1, &rb->t2, &rb->r1, &rb->r2, &rb->c1, &rb->c2, &rb->c);

    if (matched != 7)
    {
        if (strstr(line, "% END: Matrix M1 & M2 + MGU") != NULL)
        {
            free(line);
            return;
        }
        // if (verbose)
        printf("line: '%s'\n", line);
        fprintf(stderr, "Could not read matrix subset info, matched: %d\n", matched);
        free(line);
        exit(EXIT_FAILURE);
    }

    // Info for all possible main terms from the unification of M1 block (r1) and M2 block (r2)
    rb->r = rb->r1 * rb->r2;
    rb->terms = (main_term *)malloc(rb->r * sizeof(main_term));
    rb->valid = (unsigned *)malloc(rb->r * sizeof(unsigned));
    for (size_t aux = 0; aux < rb->r; aux++)
    {
        rb->valid[aux] = 0;
    }

    read_set_schema_with_dependencies(stream, common_set_schema, common_dependencies, arena);

    // If we have a general mapping, it's a linear result block. Then, get mapping info
    unsigned *mapping = (unsigned *)malloc(rb->c * 2 * sizeof(unsigned));
    getline(&line, &len, stream);
    if (strchr(line, '-'))
    {
        // General mapping -> Linear block
        rb->lineal_lineal = true;
        get_mapping(line, rb->c, mapping);
        rb->ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);

        // Skip largest (flattened) schema
        getline(&line, &len, stream);
    } /* else {
        // No general mapping -> Non-linear block
        //rb->lineal_lineal = false; // NOTE: it was already 0 initialized
        // Flattened schema already skipped
    } */

    // Iterate the main term rows
    unsigned row = 0;
    while ((read = getline(&line, &len, stream)) != -1 && row < rb->r)
    {

        // If end of matrix reached, exit
        if (strstr(line, "% END") != NULL || strstr(line, "% End") != NULL)
            break;

        // If non-linear block, we have to read the mapping
        if (!rb->lineal_lineal)
        {
            unsigned row1, row2;
            int offset;
            if (sscanf(line, "Mapping %u-%u: %n", &row1, &row2, &offset) != 2)
            { // NOTE: %n doesn't consume any input so it doesn't increment the returned value!
                printf("line: '%s'\n", line);
                fprintf(stderr, "read_result_matrix: no mapping in non-linear block %u-%u for rows %u-%u!\n", rb->t1, rb->t2, row1, row2);
                free(line);
                exit(EXIT_FAILURE);
            }
            get_mapping(line + offset, rb->c, mapping);
            rb->terms[row].ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);

            // Get the unified row
            getline(&line, &len, stream);
        }

        // Inspect if line is unifiable, subsumed or not unifiable
        if (strstr(line, "subsumed by exception") != NULL)
        {
            rb->valid[row] = 1;
            ++row;
            continue;
        }
        else if (strstr(line, "not unifiable") != NULL)
        {
            // NOTE: es necesario modificar el term? Si no unifican las filas correspondientes, para qué te creas una fila (main_term) vacía, y además el mgu_schema?
            //   si estoy en lo cierto, en compare_results solo comparamos términos si los valid[i] de ambos result_blocks son 0 (han unificado)...
            //   creo que estas dos lineas se podrían ignorar...
            // rb->terms[row] = create_null_main_term();
            // rb->terms[row].ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);

            rb->valid[row] = 2;
            ++row;
            continue;
        }

        // If unifiable, read number of exception blocks
        unsigned d1, d2, e;
        if (sscanf(line, "Row %u-%u: %u", &d1, &d2, &e) != 3 &&
            sscanf(line, "Rows %u-%u: %u", &d1, &d2, &e) != 3)
        {
            fprintf(stderr, "Could not read number of exception blocks in row\n");
            free(line);
            exit(EXIT_FAILURE);
        }

        // Initialize the exception blocks. NOTE: since in create_empty_main_term ms is set to NULL, we have to remember it!
        // Only necessary for non-linear blocks. If linear, it's better to leave the ms of each row (main term) as NULL for
        // the main_term freeing function.
        mgu_schema *ms = rb->terms[row].ms;
        rb->terms[row] = create_empty_main_term(rb->c, e);
        if (!rb->lineal_lineal)
        {
            rb->terms[row].ms = ms;
        }

        // Get a pointer to the main term for easier working
        main_term *mt = &(rb->terms[row]);

        // Read the rest of the line
        read_line(line, mt->row, true);

        // Skip the unifier line
        getline(&line, &len, stream);

        // Read one by one the exception blocks
        if (e)
            read_exception_blocks(stream, mt, true);

        // Increment the row by 1
        ++row;
    }

    free(line);
    free(mapping);
}

void read_first_result_block(
    FILE *stream, result_block *rb,
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    ArrayListCharPtr *free_vars,
    Arena *arena, Arena *debug_arena)
{
    // Create the operand_block structure for later populating it
    *rb = create_null_result_block();

    char *line = NULL;
    size_t len = 0;

    // Skip matrix header
    getline(&line, &len, stream);
    if (strstr(line, "% END: Matrix M1 & M2 + MGU") != NULL)
    {
        free(line);
    }

    *free_vars = read_free_vars(stream, debug_arena);

    // Read the operand block and fill the struct
    read_result_matrix(stream, rb, common_set_schema, common_dependencies, arena);

    free(line);
}
void read_next_result_block(
    FILE *stream, result_block *rb,
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies,
    Arena *arena)
{
    // Create the operand_block structure for later populating it
    *rb = create_null_result_block();

    // Read the operand block and fill the struct
    read_result_matrix(stream, rb, common_set_schema, common_dependencies, arena);
}

typedef struct
{
    unsigned a;
    unsigned b;
} PairUInt;

bool equal_pair_uint(PairUInt p1, PairUInt p2) { return p1.a == p2.a && p1.b == p2.b; }
void print_pair_uint(PairUInt p) { printf("{a=%u, b=%u}", p.a, p.b); }

DECLARE_ARRAYLIST_TYPE(PairUInt)
DEFINE_ARRAYLIST_CREATE_ARENA(PairUInt, pair_uint)
DEFINE_ARRAYLIST_FIND(PairUInt, pair_uint, equal_pair_uint)
DEFINE_ARRAYLIST_CONTAINS(PairUInt, pair_uint)
//DEFINE_ARRAYLIST_REMOVE_INDEX(PairUInt, pair_uint)
int remove_index_from_array_list_pair_uint(ArrayListPairUInt *list, uint32_t index)
{
    if (index >= list->size)
    {
        return OUT_OF_BOUNDS;
    }
    --list->size;
    PairUInt *removed_ptr = list->array + index;
    uint32_t num_shifted_elements = list->size - index;
    __builtin___memmove_chk(removed_ptr, removed_ptr + 1, num_shifted_elements * sizeof(*list->array), __builtin_object_size(removed_ptr, 0));
    return SUCCESSFUL_REMOVAL;
}
// DEFINE_ARRAYLIST_REMOVE_ELEMENT(PairUInt, pair_uint)
int remove_element_from_array_list_pair_uint(ArrayListPairUInt *list, PairUInt element)
{
    uint32_t index = find_in_array_list_pair_uint(*list, element);
    return remove_index_from_array_list_pair_uint(list, index);
}
DEFINE_ARRAYLIST_PRINT_SEPARATORS(PairUInt, pair_uint, print_pair_uint)
DEFINE_ARRAYLIST_PRINT(PairUInt, pair_uint)
DEFINE_ARRAYLIST_PRINTLN(PairUInt, pair_uint)

bool onerdered_equal_array_lists_pair_uint(ArrayListPairUInt list1, ArrayListPairUInt list2)
{
    if (list1.size != list2.size)
    {
        return false;
    }
    foreach_in_arraylist(PairUInt, pair, list1)
    {
        if (!contains_array_list_pair_uint(list2, *pair))
        {
            return false;
        }
    }
    return true;
}

void test_mapping_obtention_(char *path_m1, char *path_m2, char *path_m3)
{
    // TODO(CLEAN/OPT_MEM - not very relevant...):
    //  we could perform a first read to the operand blocks to determine the total memory we need for them. With the result blocks, we could calculate the size
    //  of the greatest one to allocate that amount of memory which would be reused. In the case of operand schemas we could also determine the total number of schemas in the
    //  operand files to further limit the allocated memory for them. Another option would be to add a header to the files that summarizes the sizes of the operand blocks, the
    //  largest possible resulting block, and so on... to avoid that first read.

    // Open the files and check so
    FILE *stream_M1 = fopen(path_m1, "r");
    FILE *stream_M2 = fopen(path_m2, "r");
    FILE *stream_M3 = fopen(path_m3, "r");

    if (!stream_M1 || !stream_M2 || !stream_M3)
    {
        fprintf(stderr, "Error opening files %s, %s and %s; exiting\n", path_m1, path_m2, path_m3);
        if (stream_M1)
            fclose(stream_M1);
        if (stream_M2)
            fclose(stream_M2);
        if (stream_M3)
            fclose(stream_M3);
        exit(EXIT_FAILURE);
    }

    // Read number of blocks in M1 and M2
    unsigned s1, s2;
    if (read_num_blocks(stream_M1, &s1))
        fprintf(stderr, "Could not read number of blocks in %s\n", path_m1);
    if (read_num_blocks(stream_M2, &s2))
        fprintf(stderr, "Could not read number of blocks in %s\n", path_m2);
    printf("M1 blocks: %u - M2 blocks: %u\n", s1, s2); // Check

    // Read the row identifying the columns' free variables
    char *line1 = NULL, *line2 = NULL;
    size_t len1 = 0, len2 = 0;
    if (getline(&line1, &len1, stream_M1) == -1)
    {
        free(line1);
        exit(1);
    } // Failed to read the line
    if (strstr(line1, "Free") == NULL)
    {
        free(line1);
        exit(1);
    } // Not the correct line
    if (getline(&line2, &len2, stream_M2) == -1)
    {
        free(line2);
        exit(1);
    } // Failed to read the line
    if (strstr(line2, "Free") == NULL)
    {
        free(line2);
        exit(1);
    } // Not the correct line

    unsigned num_free_vars1 = scan_num_free_vars(line1);
    unsigned num_free_vars2 = scan_num_free_vars(line2);

    Arena arena_operands;
    size_t arena_operands_bytes =
        (s1 + s2) * (sizeof(operand_block) + sizeof(ArrayListSchema) + sizeof(ArrayListDependencyPair) + sizeof(SetSchema)) +
        2 * (num_free_vars1 + num_free_vars2) * (sizeof(char *)) + strlen(line1) + strlen(line2) +
        2 * (num_free_vars1 + num_free_vars2) * (sizeof(unsigned)) +
        (num_free_vars1 * s1 + num_free_vars2 * s2) * (sizeof(unsigned)) +
        s1 * s2 * (sizeof(ArrayListSchema) + sizeof(ArrayListDependencyPair) + sizeof(SetSchema)) +
        500000 * (sizeof(Schema) +
        2 * s1 * s2 * sizeof(PairUInt)
    );
    init_arena(&arena_operands, arena_operands_bytes);

    ArrayListCharPtr free_vars1 = scan_free_vars(line1, num_free_vars1, &arena_operands);
    ArrayListCharPtr free_vars2 = scan_free_vars(line2, num_free_vars2, &arena_operands);
    free(line1);
    free(line2);
    ArrayListCharPtr computed_free_vars3 = final_free_vars_ordering(free_vars1, free_vars2, &arena_operands);

    // NOTE: compute free_var_positions1/2
    // NOTE: no resizing risk with these ArrayLists
    ArrayListUInt free_var_positions1 = create_array_list_uint_arena(computed_free_vars3.size, &arena_operands);
    ArrayListUInt free_var_positions2 = create_array_list_uint_arena(computed_free_vars3.size, &arena_operands);
    foreach_in_arraylist(CharPtr, free_v, computed_free_vars3)
    {
        // NOTE: special value for not found = free_vars3.size
        unsigned pos = find_in_array_list_char_ptr(free_vars1, *free_v);
        if (pos == free_vars1.size)
        {
            pos = computed_free_vars3.size;
        }
        unsafe_add_to_array_list(free_var_positions1, pos);

        pos = find_in_array_list_char_ptr(free_vars2, *free_v);
        if (pos == free_vars2.size)
        {
            pos = computed_free_vars3.size;
        }
        unsafe_add_to_array_list(free_var_positions2, pos);
    }

    operand_block *obs1 = allocate(&arena_operands, s1 * sizeof(operand_block));
    operand_block *obs2 = allocate(&arena_operands, s2 * sizeof(operand_block));
    ArrayListSchema *set_schemas1 = allocate(&arena_operands, s1 * sizeof(*set_schemas1));
    ArrayListSchema *set_schemas2 = allocate(&arena_operands, s2 * sizeof(*set_schemas2));
    ArrayListDependencyPair *dependencies_array1 = allocate(&arena_operands, s1 * sizeof(*dependencies_array1));
    ArrayListDependencyPair *dependencies_array2 = allocate(&arena_operands, s2 * sizeof(*dependencies_array2));

    // NOTE: arena_operands is used only for Schemas and DependencyPairs, not OperandBlocks
    unsigned max_rows1 = 0;
    for (size_t i = 0; i < s1; i++)
    {
        read_operand_block(stream_M1, obs1 + i, set_schemas1 + i, dependencies_array1 + i, &arena_operands);
        max_rows1 = MAX(max_rows1, obs1[i].r);
    }

    unsigned max_rows2 = 0;
    for (size_t i = 0; i < s2; i++)
    {
        read_operand_block(stream_M2, obs2 + i, set_schemas2 + i, dependencies_array2 + i, &arena_operands);
        max_rows2 = MAX(max_rows2, obs2[i].r);
    }

    // NOTE: important to adapt set_schemas2 adding the max_v found in set_schemas1 because the variables are logically independent!
    //  We are calculating the max variable among ALL set_schemas1, which is going to be summed to all set_schemas2. More optimal,
    //  less operations to perform.
    //  Another option would be to calculate the max_v1 for every set_schema pair inside the loop, to avoid "wasting" unused vars.
    Variable max_v1 = 0;
    for (size_t i = 0; i < s1; ++i)
    {
        max_v1 = MAX(max_v1, max_v_in_set_schema(set_schemas1[i]));
    }
    for (size_t i = 0; i < s2; ++i)
    {
        increment_variables_in_set_schema(set_schemas2[i], max_v1);
        increment_variables_in_set_dependencies(dependencies_array2[i], max_v1);
    }

    // NOTE: we normalize the operand blocks' schemas outside the loop, not once per resultant fragment (block combination)
    SetSchema *normalized_set_schemas1 = allocate(&arena_operands, s1 * sizeof(*normalized_set_schemas1));
    SetSchema *normalized_set_schemas2 = allocate(&arena_operands, s2 * sizeof(*normalized_set_schemas2));
    // NOTE: normalized uses longest_dependency which needs to know the sizes of the schemas, so we precalculate them.
    // NOTE: we also calculate the depths of the normalized schemas because we will instantiate iterators over them when calculating
    //  column index mappings.
    // NOTE: we wrap the arraylist of schemas to calculate the entire size of all the schemas.
    // NOTE: max_operand_depth to calculate the size of the Arena for Schema iterators, and the other for the row to mapping side mapping Arena.
    unsigned max_normalized_operand_depth = 0;
    unsigned max_normalized_operand_set_size1 = 0;
    unsigned min_normalized_operand_set_size1 = UINT32_MAX;
    for (unsigned i = 0; i < s1; ++i)
    {
        ArrayListSchema *set_schema = set_schemas1 + i;
        foreach_in_arraylistptr(Schema, s, set_schema) { calculate_schema_size(s); }

        ArrayListDependencyPair *dependencies = dependencies_array1 + i;
        foreach_in_arraylistptr(DependencyPair, pair, dependencies)
        {
            foreach_in_arraylist(Schema, s, pair->schemas)
            {
                calculate_schema_size(s);
            }
        }

        SetSchema *normalized = normalized_set_schemas1 + i;
        ArrayListSchema *normalized_list = &normalized->list;
        *normalized_list = normalized_set_schema(*set_schema, *dependencies, &arena_operands);
        foreach_in_arraylistptr(Schema, s, normalized_list)
        {
            calculate_schema_depth(s);
            max_normalized_operand_depth = MAX(max_normalized_operand_depth, s->depth);
        }

        normalized->size = 0;
        foreach_in_arraylistptr(Schema, s, normalized_list) { normalized->size += s->size; }
        max_normalized_operand_set_size1 = MAX(max_normalized_operand_set_size1, normalized->size);
        min_normalized_operand_set_size1 = MIN(min_normalized_operand_set_size1, normalized->size);
    }
    unsigned max_normalized_operand_set_size2 = 0;
    unsigned min_normalized_operand_set_size2 = UINT32_MAX;
    for (unsigned i = 0; i < s2; ++i)
    {
        ArrayListSchema *set_schema = set_schemas2 + i;
        foreach_in_arraylistptr(Schema, s, set_schema) { calculate_schema_size(s); }

        ArrayListDependencyPair *dependencies = dependencies_array2 + i;
        foreach_in_arraylistptr(DependencyPair, pair, dependencies)
        {
            foreach_in_arraylist(Schema, s, pair->schemas)
            {
                calculate_schema_size(s);
            }
        }

        SetSchema *normalized = normalized_set_schemas2 + i;
        ArrayListSchema *normalized_list = &normalized->list;
        *normalized_list = normalized_set_schema(*set_schema, *dependencies, &arena_operands);
        foreach_in_arraylistptr(Schema, s, normalized_list)
        {
            calculate_schema_depth(s);
            max_normalized_operand_depth = MAX(max_normalized_operand_depth, s->depth);
        }

        normalized->size = 0;
        foreach_in_arraylistptr(Schema, s, normalized_list) { normalized->size += s->size; }
        max_normalized_operand_set_size2 = MAX(max_normalized_operand_set_size2, normalized->size);
        min_normalized_operand_set_size2 = MIN(min_normalized_operand_set_size2, normalized->size);
    }

    Arena debug_arena;
    init_arena(&debug_arena, 10000 * sizeof(Schema));

    ArrayListPairUInt nonexistent_common_schemas_block_ids = create_array_list_pair_uint_arena(s1 * s2, &arena_operands);
    ArrayListSchema *common_set_schemas = allocate(&arena_operands, s1 * s2 * sizeof(*common_set_schemas));
    ArrayListDependencyPair *common_dependencies_array = allocate(&arena_operands, s1 * s2 * sizeof(*common_dependencies_array));
    SetSchema *normalized_common_set_schemas = allocate(&arena_operands, s1 * s2 * sizeof(*normalized_common_set_schemas));
    // NOTE: loop to calculate the common set schemas and normalize them.
    unsigned max_normalized_common_set_size = 0;
    unsigned max_normalized_common_depth = 0;
    for (unsigned t1 = 1; t1 <= s1; ++t1)
    {
        for (unsigned t2 = 1; t2 <= s2; ++t2)
        {

            // NOTE: calculate common schema
            ArrayListSchema set_schema1 = set_schemas1[t1 - 1];
            ArrayListDependencyPair dependencies1 = dependencies_array1[t1 - 1];

            ArrayListSchema set_schema2 = set_schemas2[t2 - 1];
            ArrayListDependencyPair dependencies2 = dependencies_array2[t2 - 1];

            if (global_print_debugging)
            {
                printf("SS1 - ");
                println_set_schema(set_schema1, PRINT_VISUALLY);
                printf("Dependencies 1:\n");
                println_set_dependencies(dependencies1, PRINT_VISUALLY);
                printf("SS2 - ");
                println_set_schema(set_schema2, PRINT_VISUALLY);
                printf("Dependencies 2:\n");
                println_set_dependencies(dependencies2, PRINT_VISUALLY);
            }

            ArrayListSchema *computed_common_set_schema = common_set_schemas + (t1 - 1) * s2 + (t2 - 1);
            ArrayListDependencyPair *computed_common_dependencies = common_dependencies_array + (t1 - 1) * s2 + (t2 - 1);
            bool exists = common_set_schema_free_vars_baseline(
                set_schema1, dependencies1, free_var_positions1,
                set_schema2, dependencies2, free_var_positions2,
                computed_common_set_schema, computed_common_dependencies,
                &arena_operands);
            if (!exists)
            {
                PairUInt p = {.a = t1, .b = t2};
                unsafe_add_to_array_list(nonexistent_common_schemas_block_ids, p);
            }

            if (global_print_debugging)
            {
                printf("CSS - ");
                println_set_schema(*computed_common_set_schema, PRINT_VISUALLY);
                printf("Common Dependencies:\n");
                println_set_dependencies(*computed_common_dependencies, PRINT_VISUALLY);
            }

            // NOTE: normalized uses longest_dependency which needs to know the sizes of the schemas, so we precalculate them.
            foreach_in_arraylistptr(Schema, s, computed_common_set_schema) { calculate_schema_size(s); }
            foreach_in_arraylistptr(DependencyPair, pair, computed_common_dependencies)
            {
                foreach_in_arraylist(Schema, s, pair->schemas)
                {
                    calculate_schema_size(s);
                }
            }

            // NOTE: calculate normalized set schemas. Sizes of schemas updated.
            ArrayListSchema list_normalized_common_set_schema = normalized_set_schema(*computed_common_set_schema, *computed_common_dependencies, &arena_operands);

            if (global_print_debugging)
            {
                ArrayListSchema list_normalized_set_schema1 = normalized_set_schemas1[t1 - 1].list;
                ArrayListSchema list_normalized_set_schema2 = normalized_set_schemas2[t2 - 1].list;
                printf("NCSS - ");
                println_set_schema(list_normalized_common_set_schema, PRINT_VISUALLY);
                printf("NSS1 - ");
                println_set_schema(list_normalized_set_schema1, PRINT_VISUALLY);
                printf("NSS2 - ");
                println_set_schema(list_normalized_set_schema2, PRINT_VISUALLY);
            }

            // NOTE: calculate depths once to create iterator over Schemas
            foreach_in_arraylist(Schema, s, list_normalized_common_set_schema)
            {
                calculate_schema_depth(s);
                max_normalized_common_depth = MAX(max_normalized_common_depth, s->depth);
            }

            // NOTE: wrap to calculate size only in one place
            SetSchema *normalized_common_set_schema = normalized_common_set_schemas + (t1 - 1) * s2 + (t2 - 1);
            normalized_common_set_schema->list = list_normalized_common_set_schema;
            normalized_common_set_schema->size = 0;
            foreach_in_arraylist(Schema, s, normalized_common_set_schema->list) { normalized_common_set_schema->size += s->size; }
            max_normalized_common_set_size = MAX(max_normalized_common_set_size, normalized_common_set_schema->size);
        }
    }

    // NOTE: we can precalculate the starting indices of each term (that correspond to the free vars) using the sizes of the normalized schemas
    //  before entering the resulting blocks' loop.
    unsigned *starting_col_indices_array = allocate(&arena_operands, sizeof(*starting_col_indices_array) * (free_vars1.size * s1 + free_vars2.size * s2));
    unsigned *starting_col_indices_array1 = starting_col_indices_array;
    unsigned *starting_col_indices_array2 = starting_col_indices_array + free_vars1.size * s1;

    unsigned *starting_indices = starting_col_indices_array1;
    for (unsigned i = 0; i < s1; ++i)
    {
        ArrayListSchema normalized = normalized_set_schemas1[i].list;
        starting_column_indexes(normalized, starting_indices);
        starting_indices += free_vars1.size;
    }
    for (unsigned i = 0; i < s2; ++i)
    {
        ArrayListSchema normalized = normalized_set_schemas2[i].list;
        starting_column_indexes(normalized, starting_indices);
        starting_indices += free_vars2.size;
    }

    Arena arena_result;
    init_arena(&arena_result,
               (max_rows1 + max_rows2) * (max_normalized_common_set_size * sizeof(unsigned) + 2 * sizeof(unsigned *)) +
                   (max_normalized_common_set_size) * sizeof(unsigned));

    // NOTE: cleans to this arena are done in the mapping_column_indexes_side functions
    Arena schema_iterator_arena;
    init_arena(&schema_iterator_arena, sizeof(SchemaIteratorNode) * (max_normalized_operand_depth + max_normalized_common_depth));

    // NOTE: at most we will have as many variables as the number of columns and at most as many new virtual columns as the sum
    //  of the number of new columns (if any repeated variable, we will have less virtual columns, because its virtuals will be repeated too)
    Arena row_vars_to_extending_cols_arena;
    unsigned num_bytes_pointers1 = sizeof(unsigned *) * max_normalized_operand_set_size1;
    unsigned num_bytes_pointers2 = sizeof(unsigned *) * max_normalized_operand_set_size2;
    unsigned num_bytes_virtual_columns1 = sizeof(unsigned) * (max_normalized_common_set_size - min_normalized_operand_set_size1);
    unsigned num_bytes_virtual_columns2 = sizeof(unsigned) * (max_normalized_common_set_size - min_normalized_operand_set_size2);
    init_arena(&row_vars_to_extending_cols_arena, MAX(num_bytes_pointers1 + num_bytes_virtual_columns1, num_bytes_pointers2 + num_bytes_virtual_columns2));

    // NOTE: read the first result block
    ArrayListCharPtr free_vars3;
    result_block rb;
    ArrayListSchema common_set_schema;
    ArrayListDependencyPair common_dependencies;
    read_first_result_block(stream_M3, &rb, &common_set_schema, &common_dependencies, &free_vars3, &arena_result, &debug_arena);

    // NOTE: calculate and check final free vars only once! Since it corresponds to the entire M3!
    bool ok_free_vars = equal_array_lists_char_ptr(free_vars3, computed_free_vars3);
    printf("ok_free_vars = %u\n", ok_free_vars);
    assert(ok_free_vars);

    ArrayListPairUInt nonexistent_resulting_block_ids = create_array_list_pair_uint_arena(s1 * s2, &arena_operands);
    for (unsigned t1 = 1; t1 <= s1; ++t1)
    {
        for (unsigned t2 = 1; t2 <= s2; ++t2)
        {
            PairUInt p = {.a = t1, .b = t2};
            unsafe_add_to_array_list(nonexistent_resulting_block_ids, p);
        }
    }

    // NOTE: resulting fragments' loop
    for (unsigned count_t1 = 1; count_t1 <= s1; ++count_t1)
    {
        for (unsigned count_t2 = 1; count_t2 <= s2; ++count_t2)
        {
            unsigned t1 = rb.t1;
            unsigned t2 = rb.t2;

            ArrayListSchema computed_common_set_schema = common_set_schemas[(t1 - 1) * s2 + (t2 - 1)];
            ArrayListDependencyPair computed_common_dependencies = common_dependencies_array[(t1 - 1) * s2 + (t2 - 1)];

            remove_element_from_array_list_pair_uint(&nonexistent_resulting_block_ids, (PairUInt){.a = t1, .b = t2});

            // NOTE: common schema exists so no fragment was skipped
            printf("Resultant fragment: %d-%d (%s)\n", t1, t2, rb.lineal_lineal ? "Linear" : "Non-linear");

            // NOTE: check correct common schema and dependencies!
            {
                // NOTE: variables are identified from 1 to n in the resulting common schema in the file
                SetVariables read_vars = create_set_variables_defsize();
                variables_in_set_schema(common_set_schema, &read_vars);
                unsigned num_read_vars = read_vars.num_variables;
                free_set_variables(read_vars);

                size_t num_bytes_for_mapping = (1 + num_read_vars) * sizeof(Variable);
                Variable *mapping = allocate(&debug_arena, num_bytes_for_mapping);
                memset(mapping, 0, num_bytes_for_mapping);

                bool ok_set_schemas = equivalent_set_schemas(common_set_schema, computed_common_set_schema, mapping);
                bool ok_dependendencies = equivalent_set_dependencies(common_dependencies, computed_common_dependencies, mapping);

                if (global_print_debugging)
                {
                    printf("File CSS - ");
                    println_set_schema(common_set_schema, PRINT_VISUALLY);
                    printf("File Deps - ");
                    println_set_dependencies(common_dependencies, PRINT_VISUALLY);
                    foreach_in_arraylist(Schema, s, common_set_schema) { calculate_schema_size(s); }
                    foreach_in_arraylist(DependencyPair, p, common_dependencies)
                    {
                        foreach_in_arraylist(Schema, s, p->schemas)
                        {
                            calculate_schema_size(s);
                        }
                    }
                    ArrayListSchema normalized = normalized_set_schema(common_set_schema, common_dependencies, &debug_arena);
                    printf("File NCSS - ");
                    println_set_schema(normalized, PRINT_VISUALLY);
                }

                printf("ok_set_schemas = %u\nok_dependencies = %u\n", ok_set_schemas, ok_dependendencies);
                assert(ok_set_schemas);
                // TODO(instance): some empty (<>) dependencies are removed in certain instances, why? Anyways, when normalizing
                //  not having those empty dependencies is equivalent to having them...
            }

            SetSchema normalized_common_set_schema = normalized_common_set_schemas[(t1 - 1) * s2 + (t2 - 1)];
            SetSchema normalized_set_schema1 = normalized_set_schemas1[t1 - 1];
            SetSchema normalized_set_schema2 = normalized_set_schemas2[t2 - 1];

            unsigned *starting_col_indices1 = starting_col_indices_array1 + (t1 - 1) * free_vars1.size;
            unsigned *starting_col_indices2 = starting_col_indices_array2 + (t2 - 1) * free_vars2.size;

            unsigned row_len1 = rb.c1;
            unsigned row_len2 = rb.c2;
            assert(normalized_set_schema1.size == row_len1);
            assert(normalized_set_schema2.size == row_len2);

            operand_block *ob1 = obs1 + t1 - 1;
            operand_block *ob2 = obs2 + t2 - 1;

            mgu_schema computed_mapping;
            computed_mapping.n_common = normalized_common_set_schema.size;
            unsigned num_cols1 = normalized_set_schema1.size;
            computed_mapping.new_a = computed_mapping.n_common - num_cols1;
            unsigned num_cols2 = normalized_set_schema2.size;
            computed_mapping.new_b = computed_mapping.n_common - num_cols2;

            unsigned num_bytes = sizeof(*computed_mapping.common_columns) * computed_mapping.n_common;
            computed_mapping.common_columns = allocate(&arena_result, num_bytes);
            for (unsigned i = 0; i < computed_mapping.n_common; ++i)
            {
                computed_mapping.common_columns[i] = i;
            }

            unsigned mapping_side_size = computed_mapping.n_common * sizeof(unsigned);

            if (rb.lineal_lineal)
            {
                unsigned *mapping_sides = allocate(&arena_result, 2 * mapping_side_size);
                unsigned *mappingL = mapping_sides;
                unsigned *mappingR = mapping_sides + computed_mapping.n_common;

                assert(ob1->r > 0 && ob2->r > 0);

                if (global_print_debugging)
                {
                    printf("Free var positions 1 = ");
                    println_array_list_uint(free_var_positions1);
                    printf("Starting col indices 1 = ");
                    println_array_uints(starting_col_indices1, free_vars1.size);
                    printf("Free var positions 2 = ");
                    println_array_list_uint(free_var_positions2);
                    printf("Starting col indices 2 = ");
                    println_array_uints(starting_col_indices2, free_vars2.size);
                }

                mapping_column_indexes_side_lineal(
                    normalized_set_schema1, free_var_positions1, normalized_common_set_schema,
                    starting_col_indices1, ob1->terms[0].row, mappingL, &schema_iterator_arena);

                mapping_column_indexes_side_lineal(
                    normalized_set_schema2, free_var_positions2, normalized_common_set_schema,
                    starting_col_indices2, ob2->terms[0].row, mappingR, &schema_iterator_arena);

                // Only one mapping in linear result block
                mgu_schema *mapping = rb.ms;
                computed_mapping.common_L = mappingL;
                computed_mapping.common_R = mappingR;
                bool ok_mappings = equal_mgu_schemas(mapping, &computed_mapping);
                printf("ok_mappings = %u\n\n", ok_mappings);
                assert(ok_mappings);
            }
            else
            {
                // NOTE: the calculation of mappingL/R is independent of one another. We can precompute them in two linear loops instead of a quadratic nested loop.
                unsigned *mapping_sides = allocate(&arena_result, (ob1->r + ob2->r) * mapping_side_size);
                unsigned *mapping_side = mapping_sides;

                // NOTE: function symbol differences don't affect to the resulting mapping, the mapping is determined by the placement of variables (0s and negatives),
                //  and the original and common schemas. In practice, most of the mapping sides are equal, so we are going to use a HashMap to determine if the mapping
                //  for an equivalent row was already computed.
                unsigned saved_calls1 = 0; // NOTE: for debugging
                // NOTE: resizing risk (very improbable with our data)
                MapRowToMappingSide map1 = create_map_row_to_mapping_side_arena(ob1->r, normalized_set_schema1.size, normalized_common_set_schema.size, &arena_result);
                for (unsigned i = 0; i < ob1->r; ++i, mapping_side += computed_mapping.n_common)
                {
                    int *row = ob1->terms[i].row;
                    RowToMappingSide *pair = get_pair_in_map_row_to_mapping_side(map1, row);
                    if (pair)
                    {
                        ++saved_calls1;
                    }
                    else
                    {
                        mapping_column_indexes_side(
                            normalized_set_schema1, free_var_positions1, normalized_common_set_schema,
                            starting_col_indices1, row, mapping_side, &row_vars_to_extending_cols_arena,
                            &schema_iterator_arena);
                        clear_arena(&row_vars_to_extending_cols_arena);
                        insert_to_map_row_to_mapping_side_arena(&map1, row, mapping_side, &arena_result);
                    }
                }

                unsigned saved_calls2 = 0;
                // NOTE: resizing risk (very improbable with our data)
                MapRowToMappingSide map2 = create_map_row_to_mapping_side_arena(ob2->r, normalized_set_schema2.size, normalized_common_set_schema.size, &arena_result);
                for (unsigned i = 0; i < ob2->r; ++i, mapping_side += computed_mapping.n_common)
                {
                    int *row = ob2->terms[i].row;
                    RowToMappingSide *pair = get_pair_in_map_row_to_mapping_side(map2, row);
                    if (pair)
                    {
                        ++saved_calls2;
                    }
                    else
                    {
                        mapping_column_indexes_side(
                            normalized_set_schema2, free_var_positions2, normalized_common_set_schema,
                            starting_col_indices2, row, mapping_side, &row_vars_to_extending_cols_arena,
                            &schema_iterator_arena);
                        clear_arena(&row_vars_to_extending_cols_arena);
                        insert_to_map_row_to_mapping_side_arena(&map2, row, mapping_side, &arena_result);
                    }
                }

                // Change the mgu_schemas sides and compare
                // One mapping per row pairs in non-linear result block
                for (unsigned i = 0; i < ob1->r; ++i)
                {
                    RowToMappingSide *row_to_ms = get_pair_in_map_row_to_mapping_side(map1, ob1->terms[i].row);
                    assert(row_to_ms);
                    computed_mapping.common_L = row_to_ms->mapping_side;
                    // computed_mapping.common_L = mapping_sides + i*computed_mapping.n_common;
                    for (unsigned j = 0; j < ob2->r; ++j)
                    {
                        // printf("Rows: %d-%d (1-based)\n", i+1, j+1);

                        mgu_schema *mapping = rb.terms[i * rb.r2 + j].ms;

                        RowToMappingSide *row_to_ms = get_pair_in_map_row_to_mapping_side(map2, ob2->terms[j].row);
                        assert(row_to_ms);
                        computed_mapping.common_R = row_to_ms->mapping_side;
                        // computed_mapping.common_R = mapping_sides + (ob1->r + j)*computed_mapping.n_common;

                        bool ok_mappings = equal_mgu_schemas(mapping, &computed_mapping);

                        // printf("ok_mappings = %u\n\n", ok_mappings);
                        assert(ok_mappings);
                    }
                }
                printf("ok_mappings = 1\n\n");
            }

            clear_arena(&arena_result);
            clear_arena(&debug_arena);
            free_result_block(&rb);
            read_next_result_block(stream_M3, &rb, &common_set_schema, &common_dependencies, &arena_result);
        }
    }

    assert(onerdered_equal_array_lists_pair_uint(nonexistent_common_schemas_block_ids, nonexistent_resulting_block_ids));

    for (size_t i = 0; i < s1; i++)
    {
        free_operand_block(&obs1[i]);
    }
    for (size_t i = 0; i < s2; i++)
    {
        free_operand_block(&obs2[i]);
    }

    free_arena(&arena_operands);
    free_arena(&arena_result);
    free_arena(&debug_arena);
    free_arena(&schema_iterator_arena);
    free_arena(&row_vars_to_extending_cols_arena);

    fclose(stream_M1);
    fclose(stream_M2);
    fclose(stream_M3);
}

void test_mapping_obtention(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <folder> [verbose]\n", argv[0]);
        return;
    }

    char *folder_path = argv[1]; // AGT002+1
    //char *folder_path = "data/experimentation_matrices/AGT004+2";
    //char *folder_path = "data/experimentation_matrices/AGT/AGT006+2.p";
    //char *folder_path = "data/experimentation_matrices/ITP/ITP018+5.p";
    // NOTE: ignore char *folder_path = "data/experimentation_matrices/SEV/SEV437+1.p";

    global_print_debugging = argc > 2;

    DIR *dir = opendir(folder_path);
    if (!dir)
    {
        perror("Error opening directory");
        return;
    }

    var_dict = create_dictionary(501);
    symbols_to_ids = create_dictionary(501);

    struct dirent *entry;
    char path_m1[PATH_MAX], path_m2[PATH_MAX], path_m3[PATH_MAX], base[PATH_MAX];

    // 2. Iterate through files (similar to find -type f)
    while ((entry = readdir(dir)) != NULL)
    {
        size_t len = strlen(entry->d_name);

        // 3. Check if filename ends with "M1.csv"
        if (len > 6 && strcmp(entry->d_name + len - 6, "M1.csv") == 0)
        {

            // Build absolute/relative path to M1
            snprintf(path_m1, sizeof(path_m1), "%s/%s", folder_path, entry->d_name);

            // Get the "base" (remove M1.csv)
            strncpy(base, path_m1, strlen(path_m1) - 6);
            base[strlen(path_m1) - 6] = '\0';

            if (
                // Skip passed tests: AGT002+1
                // strstr(base, "test0016") ||
                // strstr(base, "test0015") ||
                // strstr(base, "test0001") ||
                // strstr(base, "test0005") ||
                // strstr(base, "test0009") ||
                // strstr(base, "test0017") ||
                // strstr(base, "test0011") ||

                // Skip passed tests: AGT004+2
                // strstr(base, "test0001") ||
                // strstr(base, "test0004") ||

                // Skip passed tests: AGT006+2.p
                // strstr(base, "test0002") ||

                // Skip passed tests: ITP018+5.p
                // strstr(base, "test0361") ||
                // strstr(base, "test0458") ||                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         ^

                // Skip passed tests: SEV437+1.p
                // NOTE: strangely, resultant fragments are not in the expected order. For example, 2-2 comes after 3-1. Furthermore, fragment
                //  5-5 is strange: the normalized common schema has a size of 68, but the header of the fragment states that rows have 70 columns! (cannot be
                //  checked, as none of the row-pairs unifiy...)
                // strstr(base, "test0003") ||
                // ==> We are going to ignore this test!!!

                false)
            {
                continue;
            }

            // 4. Construct M2 and M3 paths
            snprintf(path_m2, sizeof(path_m2), "%sM2.csv", base);
            snprintf(path_m3, sizeof(path_m3), "%sM3.csv", base);

            // 5. Check if M2 and M3 exist (access F_OK is like [[ -f ]])
            if (access(path_m2, F_OK) == 0 && access(path_m3, F_OK) == 0)
            {
                printf("%s\n", base);

                test_mapping_obtention_(path_m1, path_m2, path_m3);

                clear(var_dict);
                clear(symbols_to_ids);
                next_symbol_id = 1;
            }
            else
            {
                printf("Skipping %s: M2 or M3 missing\n", base);
            }
        }
    }

    free_dictionary(symbols_to_ids);
    free_dictionary(var_dict);
    closedir(dir);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END TEST MAPPING OBTENTION //////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    // test_set_variables();
    // test_typeof_or_auto_type();
    // test_arraylist_ints();
    // test_schema_management(argc, argv);
    test_mapping_obtention(argc, argv);
    return 0;
}
