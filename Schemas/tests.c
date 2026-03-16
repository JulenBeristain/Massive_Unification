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
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

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
        // NOTE: a unique dependency set would be enough for the three set_schemas the computing code works with: 
        //  two operand set_schemas and a computed common set schema, because in the end we are interested in the 
        //  union of all the dependencies.
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

        bool computed_common_schema_exists = common_set_schema_strict_baseline(set_schema1, dependencies1, 
            set_schema2, dependencies2, &computed_common_set_schema, &computed_common_dependencies, arena);

        if(common_schema_exists != computed_common_schema_exists){
            printf("Test=%u - common_schema_exists=%u - computed_common_schema_exists=%u\n", 
                    test_number, common_schema_exists, computed_common_schema_exists);
        }
        else if(common_schema_exists){
            // Compare the computed common schema and its dependencies with the read ones
            
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
    
    free_arena(&arena);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// TEST MAPPING OBTENTION //////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int read_num_blocks(FILE *stream, unsigned *s) {
    char *line = NULL;
    size_t len = 0;

    if (getline(&line, &len, stream) == -1) return 0; // Failed to read the line
    if (strstr(line, "BEGIN") == NULL) {free(line); return 0;} // Not the correct line

    char *endptr;
    char *e = strchr(line, '(');
    if (!e) {free(line); return 1;} // Not the correct line
    int num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1) {free(line); return 1;}

    *s = (unsigned)num;

    free(line);
    return 0;
}

ArrayListCharPtr read_free_vars(FILE *stream, Arena *arena){
    char *line = NULL;
    size_t len = 0;
    
    // Read the row identifying the columns' free variables
    if (getline(&line, &len, stream) == -1) exit(1); // Failed to read the line
    if (strstr(line, "Free") == NULL) {free(line); exit(1);} // Not the correct line

    unsigned num_free_vars = 1;
    for(char *c = line; c; ++c){
        if(*c == ','){
            ++num_free_vars;
        }
    }

    // NOTE: no resizing risk
    // NOTE: another option would be to use non-arena version, malloc and define the free_pointers for arraylists of charptrs...
    ArrayListCharPtr free_vars = create_array_list_char_ptr_arena(num_free_vars, arena);
    for(char* token = strtok(line, ","); token; token = strtok(NULL, ",")){
        char *var_str = allocate(arena, strlen(token) + 1);
        strcpy(var_str, token);
        unsafe_add_to_array_list(free_vars, var_str);
    }

    return free_vars;
}


void read_dimensions(FILE *stream, unsigned *n, unsigned *m) {
    char *line = NULL;
    size_t len = 0;
    char *endptr, *e;
    long int num;

    getline(&line, &len, stream);
    if (strstr(line, "BEGIN") == NULL) {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    }
    
    e = strchr(line, '(');
    if (!e) {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    };

    num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1) {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    }

    *n = num;

    e = strchr(endptr, ',');
    if (!e) {
        fprintf(stderr, "Could not read matrix dimensions\n");
        exit(EXIT_FAILURE);
    }

    num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1) {
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
void read_line(char *line, int *row, bool skip_first) {
    char *tok = strtok(line, ",");
    if (skip_first) tok = strtok(NULL, ",\n");

    int col = 1;
    clear(var_dict);
    while (tok) {
        if (!isupper(tok[0])) { // If no uppercase appears, it is a constant
            next_symbol_id = 1;
            struct nlist *token_id = lookup(symbols_to_ids, tok);
            if(token_id){
                row[col-1] = token_id->defn;
            } else {
                install(symbols_to_ids, tok, next_symbol_id);
                row[col-1] = next_symbol_id++;
            }
        } else { // It is a variable
            if (lookup(var_dict, tok) == NULL) {
                row[col-1] = 0;
                install(var_dict, tok, col);
            } else {
                row[col-1] = -(lookup(var_dict, tok)->defn);
            }
        }
        tok = strtok(NULL, ",\n");
        col++;
    }
}


void get_mapping(char *line, unsigned n_pairs, unsigned *mapping){
    char *tok = strtok(line, " ,\n");
    unsigned count = 0;

    while (tok != NULL && count < n_pairs * 2) {
        char *dash = strchr(tok, '-');
        if (dash) {
            *dash = '\0'; 
            char *first = tok;
            char *second = dash + 1;
            mapping[count++] = (strcmp(first,  "_") == 0) ? 0 : (unsigned)atoi(first);
            mapping[count++] = (strcmp(second, "_") == 0) ? 0 : (unsigned)atoi(second);
        }
        tok = strtok(NULL, " ,\n");
    }
}


void read_exception_blocks(FILE *stream, main_term *mt, const bool result) {
    unsigned n, m;
    unsigned e = mt->e;
    exception_block *eb;
    char *line = NULL;
    size_t len = 0;

    for (unsigned i = 0; i < e; i++)
    {
        // Read dimensions of exception block (subset)
        read_dimensions(stream,&n,&m);

        // Create empty exception block to be populated later
        mt->exceptions[i] = create_empty_exception_block(n,m);
        eb = &(mt->exceptions[i]);

        // Skip unflatened schema
        getline(&line, &len, stream);

        // Read mapping
        getline(&line, &len, stream);
        unsigned *mapping = (unsigned*)malloc(eb->m*2*sizeof(unsigned));
        get_mapping(line,eb->m, mapping);
        eb->ms = create_mgu_from_mapping(mapping, eb->m, mt->c, eb->m);
        free(mapping);

        // Skip flatened schema if reading from operand
        if (!result) getline(&line, &len, stream);

        for (size_t j = 0; j < n; j++)
        {            
            getline(&line, &len, stream);
            char *line_ptr = line;  
            if (result) line_ptr = strchr(line_ptr, ':') + 2;
            read_line(line_ptr, &eb->mat[j*m], false);

            // Skip unifier if reading from result file
            if (result) getline(&line, &len, stream);
        }

        // Skip end exception subset line
        getline(&line, &len, stream);
    }
    
    free(line);
}


void read_operand_matrix(FILE *stream, operand_block *ob, ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies, Arena *arena) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    unsigned row = 0;

    // Read unflatened schema and the set of dependencies
    read_set_schema_with_dependencies(stream, set_schema, dependencies, arena);

    // Skip flatened schema
    getline(&line, &len, stream);

    // Iterate the main term rows
    while ((read = getline(&line, &len, stream)) != -1 && row < ob->r) {
        
        // If end of matrix reached, exit
        if (strstr(line, "END") != NULL || strstr(line, "End") != NULL)
            break;
        
        // Get first token, which tells the number of exception blocks
        char *line_copy = strdup(line);
        char *tok = strtok(line_copy, ",");
        unsigned e = (unsigned)strtoul(tok, NULL, 10);
        free(line_copy);
        
        // Initialize the exception blocks
        ob->terms[row] = create_empty_main_term(ob->c,e);

        // Get a pointer to the main term for easier working
        main_term *mt = &(ob->terms[row]);

        // Read the rest of the line
        read_line(line, mt->row, true);

        // Read one by one the exception blocks
        if (e) read_exception_blocks(stream, mt, false);

        // Increment the row by 1
        row++;
    }

    free(line);
}
void read_operand_block(FILE *stream, operand_block *ob, ArrayListSchema *set_schema, ArrayListDependencyPair *dependencies, Arena *arena) {

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
    Arena *arena
) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    // Read block header
    getline(&line, &len, stream);
    int matched = sscanf(line, "%% BEGIN: Matrix subset %u-%u (%u-%u,%u-%u,%u)",
                         &rb->t1, &rb->t2, &rb->r1, &rb->r2, &rb->c1, &rb->c2, &rb->c);
    
    if (matched != 7) {
        if (strstr(line, "END: Matrix M1 & M2 + MGU") != NULL) {free(line); return;}
        // if (verbose) 
        printf("line: '%s'\n",line);
        fprintf(stderr, "Could not read matrix subset info, matched: %d\n",matched);
        free(line);
        exit(EXIT_FAILURE);
    }

    // Info for all possible main terms from the unification of M1 block (r1) and M2 block (r2)
    rb->r = rb->r1*rb->r2;
    rb->terms = (main_term*)malloc(rb->r * sizeof(main_term));
    rb->valid = (unsigned*) malloc(rb->r * sizeof(unsigned));
    for (size_t aux = 0; aux < rb->r; aux++)
    {
        rb->valid[aux] = 0;
    }

    read_set_schema_with_dependencies(stream, common_set_schema, common_dependencies, arena);

    // If we have a general mapping, it's a linear result block. Then, get mapping info
    unsigned *mapping = (unsigned*)malloc(rb->c*2*sizeof(unsigned));
    getline(&line, &len, stream);
    if(strchr(line, '-')){
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
    while ((read = getline(&line, &len, stream)) != -1 && row < rb->r) {
        
        // If end of matrix reached, exit
        if (strstr(line, "END") != NULL || strstr(line, "End") != NULL)
            break;

        // If non-liner block, we have read the mapping
        if (!rb->lineal_lineal){
            unsigned row1, row2;
            int offset;
            if(sscanf(line, "Mapping %u-%u: %n", &row1, &row2, &offset) != 2){ // NOTE: %n doesn't consume any input so it doesn't increment the returned value!
                printf("line: '%s'\n",line);
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
            //NOTE: es necesario modificar el term? Si no unifican las filas correspondientes, para qué te creas una fila (main_term) vacía, y además el mgu_schema?
            //  si estoy en lo cierto, en compare_results solo comparamos términos si los valid[i] de ambos result_blocks son 0 (han unificado)...
            //  creo que estas dos lineas se podrían ignorar...
            //rb->terms[row] = create_null_main_term();
            //rb->terms[row].ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);
            
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
        mgu_schema *ms = rb->terms[row].ms;
        rb->terms[row] = create_empty_main_term(rb->c,e);
        rb->terms[row].ms = ms;
        //if (rb->valid[row]!=1) rb->valid[row] = 0;  // NOTE: completely unnecessary, no? valid_ was already 0-initialized...

        // Get a pointer to the main term for easier working
        main_term *mt = &(rb->terms[row]);

        // Read the rest of the line
        read_line(line, mt->row, true);

        // Skip the unifier line
        getline(&line, &len, stream);

        // Read one by one the exception blocks
        if (e) read_exception_blocks(stream, mt, true);

        // Increment the row by 1
        ++row;
    }

    free(line);
    free(mapping);
}


bool first_rb = true;
void read_result_block(
    FILE *stream, result_block *rb, 
    ArrayListSchema *common_set_schema, ArrayListDependencyPair *common_dependencies, 
    ArrayListCharPtr *free_vars,
    Arena *arena
) {
    // Create the operand_block structure for later populating it
    *rb = create_null_result_block();

    char *line = NULL;
    size_t len = 0;

    // Skip matrix header
    if (first_rb) {
        first_rb=false; 
        getline(&line, &len, stream);
        if (strstr(line, "END: Matrix M1 & M2 + MGU") != NULL) {free(line);}

        *free_vars = read_free_vars(stream, arena);
    }

    // Read the operand block and fill the struct
    read_result_matrix(stream, rb, common_set_schema, common_dependencies, arena);

    free(line);
}

bool equal_mgu_schemas(mgu_schema *ms1, mgu_schema *ms2){
    if(ms1->n_common != ms2->n_common || ms1->new_a != ms2->new_a || ms1->new_b != ms2->new_b){
        return false;
    }

    for(unsigned i = 0; i < ms1->n_common; ++i){
        if(ms1->common_columns[i] != ms2->common_columns[i] ||
           ms1->common_L[i] != ms2->common_L[i] ||
           ms1->common_R[i] != ms2->common_L[i])
        {
            return false;
        }
    }

    return true;
}

void test_mapping_obtention__(
    ArrayListSchema set_schema1, ArrayListDependencyPair dependencies1, ArrayListCharPtr free_vars1, int *row1, unsigned row_len1,
    ArrayListSchema set_schema2, ArrayListDependencyPair dependencies2, ArrayListCharPtr free_vars2, int *row2, unsigned row_len2,
    ArrayListSchema common_set_schema, ArrayListDependencyPair common_dependencies, ArrayListCharPtr free_vars3, mgu_schema *mapping_cols, Arena *arena
){
    printf("###########################################################################\n");
    printf("### test_mapping_obtention ################################################\n");
    printf("###########################################################################\n");

    // Calculate mgu_schema
    mgu_schema computed_mapping;
    ArrayListSchema computed_common_set_schema;
    ArrayListDependencyPair computed_common_dependencies;
    ArrayListCharPtr computed_free_vars;
    bool exists_common_schema = mapping_column_indexes(
        set_schema1, dependencies1, free_vars1, row1, row_len1,
        set_schema2, dependencies2, free_vars2, row2, row_len2,
        &computed_common_set_schema, &computed_common_dependencies, &computed_free_vars, &computed_mapping, arena);

    // Compare the calculated free_vars for M3
    if(first_rb){
        bool ok_free_vars = equal_array_lists_char_ptr(free_vars3, computed_free_vars);
        printf("ok_free_vars = %u\n", ok_free_vars);
    }

    // Compare the computed common_schema and dependencies with the read one
    // Compare the computed mgu_schema with the read one
    if(exists_common_schema){
        // NOTE: variables are identified from 1 to n in the resulting common schema in the file
        SetVariables read_vars = create_set_variables_defsize(); 
        variables_in_set_schema(common_set_schema, &read_vars);
        unsigned num_read_vars = read_vars.num_variables;
        free_set_variables(read_vars);

        size_t num_bytes_for_mapping = (1 + num_read_vars) * sizeof(Variable);
        Variable *mapping = allocate(arena, num_bytes_for_mapping);
        memset(mapping, 0, num_bytes_for_mapping);

        bool ok_set_schemas = equivalent_set_schemas(common_set_schema, computed_common_set_schema, mapping);
        bool ok_dependendencies = equivalent_set_dependencies(common_dependencies, computed_common_dependencies, mapping);
        bool ok_mappings = equal_mgu_schemas(mapping_cols, &computed_mapping);
        
        printf("ok_set_schemas = %u\nok_dependencies = %u\nok_mappings = %u\n", ok_set_schemas, ok_dependendencies, ok_mappings);
    } else {
        // NOTE: nothing to do. In core, simply common_schema doesn't exist...
    }

    printf("\n");
}

void test_mapping_obtention_(
    char *path_m1, char *path_m2, char *path_m3, 
    Arena *arena)
{
    // Open the files and check so
    FILE *stream_M1 = fopen(path_m1, "r");
    FILE *stream_M2 = fopen(path_m2, "r");
    FILE *stream_M3 = fopen(path_m3, "r");

    if (!stream_M1 || !stream_M2 || !stream_M3) {
        fprintf(stderr, "Error opening files %s, %s and %s; exiting\n", path_m1, path_m2, path_m3);
        if (stream_M1) fclose(stream_M1);
        if (stream_M2) fclose(stream_M2);
        if (stream_M3) fclose(stream_M3);
        exit(EXIT_FAILURE);
    }

    // Read number of blocks in M1 and M2
    unsigned s1, s2;
    if (read_num_blocks(stream_M1,&s1)) fprintf(stderr, "Could not read number of blocks in %s\n", path_m1);
    if (read_num_blocks(stream_M2,&s2)) fprintf(stderr, "Could not read number of blocks in %s\n", path_m2);
    printf("M1 blocks %u, M2 blocks %u, ",s1,s2); // Check

    operand_block *obs1 = (operand_block*)malloc(s1*sizeof(operand_block));
    operand_block *obs2 = (operand_block*)malloc(s2*sizeof(operand_block));
    // NOTE: we could use arenas for these too...
    ArrayListSchema *set_schemas1 = malloc(s1 * sizeof(*set_schemas1));
    ArrayListSchema *set_schemas2 = malloc(s2 * sizeof(*set_schemas2));
    ArrayListDependencyPair *dependencies_array1 = malloc(s1 * sizeof(*dependencies_array1));
    ArrayListDependencyPair *dependencies_array2 = malloc(s2 * sizeof(*dependencies_array2));

    ArrayListCharPtr free_vars1 = read_free_vars(stream_M1, arena);
    ArrayListCharPtr free_vars2 = read_free_vars(stream_M2, arena);
    

    for (size_t i = 0; i < s1; i++)
    {
        read_operand_block(stream_M1, obs1 + i, set_schemas1 + i, dependencies_array1 + i, arena);
    }

    for (size_t i = 0; i < s2; i++)
    {
        read_operand_block(stream_M2, obs2 + i, set_schemas2 + i, dependencies_array2 + i, arena);
    }

    first_rb = true;
    ArrayListCharPtr free_vars3;
    do {
        result_block rb;
        ArrayListSchema common_set_schema;
        ArrayListDependencyPair common_dependencies;
        read_result_block(stream_M3, &rb, &common_set_schema, &common_dependencies, &free_vars3, arena);

        if (rb.t1) {
            // Get arguments
            ArrayListSchema set_schema1 = set_schemas1[rb.t1];
            ArrayListDependencyPair dependencies1 = dependencies_array1[rb.t1];
            unsigned row_len1 = rb.c1;
            int *row1;
            
            ArrayListSchema set_schema2 = set_schemas2[rb.t2];
            ArrayListDependencyPair dependencies2 = dependencies_array2[rb.t2];
            unsigned row_len2 = rb.c2;
            int *row2;

            operand_block *ob1 = obs1 + rb.t1;
            operand_block *ob2 = obs2 + rb.t2;

            mgu_schema *mapping;

            if(rb.lineal_lineal){
                // Only one mapping in linear result block
                assert(ob1->r > 0);
                row1 = ob1->terms[0].row;
                row2 = ob2->terms[0].row;
                mapping = rb.ms;

                // Calculate the mgu_schema and compare it with the one in the file
                test_mapping_obtention__(
                    set_schema1, dependencies1, free_vars1, row1, row_len1,
                    set_schema2, dependencies2, free_vars2, row2, row_len2,
                    common_set_schema, common_dependencies, free_vars3, mapping, arena);
            } else {
                // One mapping per row pairs in non-linear result block
                for(unsigned i = 0; i < ob1->r; ++i){
                    row1 = ob1->terms[i].row;
                    for(unsigned j = 0; j < ob2->r; ++j){
                        row2 = ob2->terms[j].row;
                        mapping = rb.terms[ i*rb.r2 + j ].ms;

                        // Calculate the mgu_schema and compare it with the one in the file
                        test_mapping_obtention__(
                            set_schema1, dependencies1, free_vars1, row1, row_len1,
                            set_schema2, dependencies2, free_vars2, row2, row_len2,
                            common_set_schema, common_dependencies, free_vars3, mapping, arena
                        );
                    }
                }
            }

            free_result_block(&rb);
        }
        else break;
    } while (true);


    for (size_t i = 0; i < s1; i++) {
        free_operand_block(&obs1[i]);
    }
    for (size_t i = 0; i < s2; i++) {
        free_operand_block(&obs2[i]);
    }
    free(obs1);
    free(obs2);

    // NOTE: we could use arenas for these too...
    free(set_schemas1);
    free(set_schemas2);
    free(dependencies_array1);
    free(dependencies_array2);

    fclose(stream_M1);
    fclose(stream_M2);
    fclose(stream_M3);
}

void test_mapping_obtention(int argc, char *argv[]){
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <folder> [verbose]\n", argv[0]);
        return;
    }

    char *folder_path = argv[1];
    //int verbose = (argc > 2); // Check if any second argument exists

    DIR *dir = opendir(folder_path);
    if (!dir) {
        perror("Error opening directory");
        return;
    }


    Arena arena;
    init_arena(&arena, sizeof(Schema) * 100000);
    
    var_dict = create_dictionary(501);
    symbols_to_ids = create_dictionary(501);
    
    
    struct dirent *entry;
    char path_m1[PATH_MAX], path_m2[PATH_MAX], path_m3[PATH_MAX], base[PATH_MAX];
    
    // 2. Iterate through files (similar to find -type f)
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);

        // 3. Check if filename ends with "M1.csv"
        if (len > 6 && strcmp(entry->d_name + len - 6, "M1.csv") == 0) {
            
            // Build absolute/relative path to M1
            snprintf(path_m1, sizeof(path_m1), "%s/%s", folder_path, entry->d_name);
            
            // Get the "base" (remove M1.csv)
            strncpy(base, path_m1, strlen(path_m1) - 6);
            base[strlen(path_m1) - 6] = '\0';

            // 4. Construct M2 and M3 paths
            snprintf(path_m2, sizeof(path_m2), "%sM2.csv", base);
            snprintf(path_m3, sizeof(path_m3), "%sM3.csv", base);

            // 5. Check if M2 and M3 exist (access F_OK is like [[ -f ]])
            if (access(path_m2, F_OK) == 0 && access(path_m3, F_OK) == 0) {
                printf("%s, ", base);
                
                test_mapping_obtention_(
                    path_m1, path_m2, path_m3,
                    &arena);

                clear_arena(&arena);
                clear(var_dict);
                clear(symbols_to_ids);
                next_symbol_id = 1;

            } else {
                printf("Skipping %s: M2 or M3 missing\n", base);
            }
        }
    }

    
    free_dictionary(symbols_to_ids);
    free_dictionary(var_dict);
    free_arena(&arena);
    closedir(dir);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// END TEST MAPPING OBTENTION //////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    //test_set_variables();
    //test_typeof_or_auto_type();
    //test_arraylist_ints();
    //test_schema_management(argc, argv);
    test_mapping_obtention(argc, argv);
    return 0;
}
