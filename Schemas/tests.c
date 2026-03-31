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
    if (strstr(line, "% BEGIN") == NULL) {free(line); return 0;} // Not the correct line

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
    for(char *c = line; *c; ++c){
        if(*c == ','){
            ++num_free_vars;
        }
    }

    // NOTE: no resizing risk
    // NOTE: another option would be to use non-arena version, malloc and define the free_pointers for arraylists of charptrs...
    ArrayListCharPtr free_vars = create_array_list_char_ptr_arena(num_free_vars, arena);
    for(char* token = strtok(line, ",\n"); token; token = strtok(NULL, ",\n")){
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
    if (strstr(line, "% BEGIN") == NULL) {
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
        if (strstr(line, "% END") != NULL || strstr(line, "% End") != NULL)
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
        if (strstr(line, "% END: Matrix M1 & M2 + MGU") != NULL) {free(line); return;}
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
        if (strstr(line, "% END") != NULL || strstr(line, "% End") != NULL)
            break;

        // If non-linear block, we have to read the mapping
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
        // Only necessary for non-linear blocks. If linear, it's better to leave the ms of each row (main term) as NULL for
        // the main_term freeing function.
        mgu_schema *ms = rb->terms[row].ms;
        rb->terms[row] = create_empty_main_term(rb->c,e);
        if (!rb->lineal_lineal){
            rb->terms[row].ms = ms;
        }
        
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
        if (strstr(line, "% END: Matrix M1 & M2 + MGU") != NULL) {free(line);}

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
        bool same_i = ms1->common_columns[i] != ms2->common_columns[i] ||
                      ms1->common_L[i] != ms2->common_L[i] ||
                      ms1->common_R[i] != ms2->common_R[i];
        if(same_i)
        {
            return false;
        }
    }

    return true;
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
    printf("M1 blocks: %u - M2 blocks: %u\n",s1,s2); // Check

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

    // NOTE: important to adapt set_schemas2 adding the max_v found in set_schemas1 because the variables are logically independent!
    //  We are calculating the max variable among ALL set_schemas1, which is going to be summed to all set_schemas2. More optimal,
    //  less operations to perform.
    //  Another option would be to calculate the max_v1 for every set_schema pair inside the loop, to avoid "wasting" unused vars.
    Variable max_v1 = 0;
    for(size_t i = 0; i < s1; ++i){
        max_v1 = MAX(max_v1, max_v_in_set_schema(set_schemas1[i]));
    }
    for(size_t i = 0; i < s2; ++i){
        increment_variables_in_set_schema(set_schemas2[i], max_v1);
        increment_variables_in_set_dependencies(dependencies_array2[i], max_v1);
    }

    first_rb = true;
    ArrayListCharPtr free_vars3, computed_free_vars3;
    result_block rb;
    ArrayListSchema common_set_schema;
    ArrayListDependencyPair common_dependencies;
    // NOTE: read_result_block sets first_rb to false
    read_result_block(stream_M3, &rb, &common_set_schema, &common_dependencies, &free_vars3, arena);
    while((rb.t1 < global_starting_t1) || (rb.t1 == global_starting_t1 && rb.t2 < global_starting_t2)){
        free_result_block(&rb);
        read_result_block(stream_M3, &rb, &common_set_schema, &common_dependencies, &free_vars3, arena);
    }

    // NOTE: calculate and check final free vars only once! Since it corresponds to the entire M3!
    computed_free_vars3 = final_free_vars_ordering(free_vars1, free_vars2, arena);
    bool ok_free_vars = equal_array_lists_char_ptr(free_vars3, computed_free_vars3);
    printf("ok_free_vars = %u\n", ok_free_vars);
    assert(ok_free_vars);

    // NOTE: compute free_var_positions1/2
    // NOTE: no resizing risk with these ArrayLists
    ArrayListUInt free_var_positions1 = create_array_list_uint_arena(free_vars3.size, arena);
    ArrayListUInt free_var_positions2 = create_array_list_uint_arena(free_vars3.size, arena);
    foreach_in_arraylist(CharPtr, free_v, free_vars3){
        // NOTE: special value for not found = free_vars3.size
        unsigned pos = find_in_array_list_char_ptr(free_vars1, *free_v);
        if(pos == free_vars1.size){
            pos = free_vars3.size;
        }
        unsafe_add_to_array_list(free_var_positions1, pos);

        pos = find_in_array_list_char_ptr(free_vars2, *free_v);
        if(pos == free_vars2.size){
            pos = free_vars3.size;
        }
        unsafe_add_to_array_list(free_var_positions2, pos);
    }
    
    // NOTE: no resizing risk with this arena
    Arena starting_indices_arena; init_arena(&starting_indices_arena, sizeof(unsigned) * (free_vars1.size + free_vars2.size));

    for(unsigned t1 = global_starting_t1; t1 <= s1; ++t1){
        for(unsigned t2 = global_starting_t2; t2 <= s2; ++t2){

            // NOTE: calculate common schema
            ArrayListSchema set_schema1 = set_schemas1[t1 - 1];
            ArrayListDependencyPair dependencies1 = dependencies_array1[t1 - 1];
            
            ArrayListSchema set_schema2 = set_schemas2[t2 - 1];
            ArrayListDependencyPair dependencies2 = dependencies_array2[t2 - 1];

            if(global_print_debugging){
                printf("SS1 - "); println_set_schema(set_schema1, PRINT_VISUALLY);
                printf("Dependencies 1:\n"); println_set_dependencies(dependencies1, PRINT_VISUALLY);
                printf("SS2 - "); println_set_schema(set_schema2, PRINT_VISUALLY);
                printf("Dependencies 2:\n"); println_set_dependencies(dependencies2, PRINT_VISUALLY);
            }

            ArrayListSchema computed_common_set_schema;
            ArrayListDependencyPair computed_common_dependencies;
            bool exists_common_schema = common_set_schema_free_vars_baseline(
                set_schema1, dependencies1, free_var_positions1,
                set_schema2, dependencies2, free_var_positions2,
                &computed_common_set_schema, &computed_common_dependencies,
                arena);

            if(rb.t1 == t1 && rb.t2 == t2){
                // NOTE: common schema exists so no fragment was skipped
                printf("Resultant fragment: %d-%d (%s)\n", t1, t2, rb.lineal_lineal ? "Linear" : "Non-linear");
                assert(exists_common_schema);

                // NOTE: check correct common schema and dependencies!
                {
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

                    printf("ok_set_schemas = %u\nok_dependencies = %u\n", ok_set_schemas, ok_dependendencies);
                    assert(ok_set_schemas);
                    // TODO(instance): some empty (<>) dependencies are removed in certain instances, why? Anyways, when normalizing
                    //  not having those empty dependencies is equivalent to having them...
                }

                // NOTE: normalized uses longest_dependency which needs to know the sizes of the schemas, so we precalculate them.
                foreach_in_arraylist(Schema, s, computed_common_set_schema) { calculate_schema_size(s); }
                foreach_in_arraylist(Schema, s, set_schema1) { calculate_schema_size(s); }
                foreach_in_arraylist(Schema, s, set_schema2) { calculate_schema_size(s); }
                foreach_in_arraylist(DependencyPair, pair, computed_common_dependencies){
                    foreach_in_arraylist(Schema, s, pair->schemas){
                        calculate_schema_size(s);
                    }
                }
                foreach_in_arraylist(DependencyPair, pair, dependencies1){
                    foreach_in_arraylist(Schema, s, pair->schemas){
                        calculate_schema_size(s);
                    }
                }
                foreach_in_arraylist(DependencyPair, pair, dependencies2){
                    foreach_in_arraylist(Schema, s, pair->schemas){
                        calculate_schema_size(s);
                    }
                }
#ifndef NDEBUG
                // NOTE: can't create iterators to test sizes if depths are not calculate!
                foreach_in_arraylist(Schema, s, computed_common_set_schema) { calculate_schema_depth(s); }
                foreach_in_arraylist(Schema, s, set_schema1) { calculate_schema_depth(s); }
                foreach_in_arraylist(Schema, s, set_schema2) { calculate_schema_depth(s); }

                foreach_in_arraylist(Schema, s, computed_common_set_schema){
                    SchemaIterator it = create_schema_iterator(*s);
                    Schema current;
                    while(schema_iterator_next(&it, &current)){
                        assert(schema_size(current) == current.size);
                    }
                }
                foreach_in_arraylist(Schema, s, set_schema1){
                    SchemaIterator it = create_schema_iterator(*s);
                    Schema current;
                    while(schema_iterator_next(&it, &current)){
                        assert(schema_size(current) == current.size);
                    }
                }
                foreach_in_arraylist(Schema, s, set_schema2){
                    SchemaIterator it = create_schema_iterator(*s);
                    Schema current;
                    while(schema_iterator_next(&it, &current)){
                        assert(schema_size(current) == current.size);
                    }
                }
#endif          
                // NOTE: calculate normalized set schemas. Sizes of schemas updated.
                ArrayListSchema list_normalized_common_set_schema = normalized_set_schema(computed_common_set_schema, computed_common_dependencies, arena);
                ArrayListSchema list_normalized_set_schema1 = normalized_set_schema(set_schema1, dependencies1, arena);
                ArrayListSchema list_normalized_set_schema2 = normalized_set_schema(set_schema2, dependencies2, arena);
                assert(list_normalized_set_schema1.size == free_vars1.size);
                assert(list_normalized_set_schema2.size == free_vars2.size);
                assert(list_normalized_common_set_schema.size == free_vars3.size);
                if(global_print_debugging){
                    printf("NCSS - "); println_set_schema(list_normalized_common_set_schema, PRINT_VISUALLY);
                    printf("NSS1 - "); println_set_schema(list_normalized_set_schema1, PRINT_VISUALLY);
                    printf("NSS2 - "); println_set_schema(list_normalized_set_schema2, PRINT_VISUALLY);
                }

                // NOTE: calculate depths once to create iterator over Schemas
                foreach_in_arraylist(Schema, s, list_normalized_common_set_schema) { calculate_schema_depth(s); }
                foreach_in_arraylist(Schema, s, list_normalized_set_schema1) { calculate_schema_depth(s); }
                foreach_in_arraylist(Schema, s, list_normalized_set_schema2) { calculate_schema_depth(s); }
#ifndef NDEBUG
                foreach_in_arraylist(Schema, s, list_normalized_common_set_schema){
                    SchemaIterator it = create_schema_iterator(*s);
                    Schema current;
                    while(schema_iterator_next(&it, &current)){
                        assert(schema_size(current) == current.size);
                        assert(schema_depth(current) == current.depth);
                    }
                }
                foreach_in_arraylist(Schema, s, list_normalized_set_schema1){
                    SchemaIterator it = create_schema_iterator(*s);
                    Schema current;
                    while(schema_iterator_next(&it, &current)){
                        assert(schema_size(current) == current.size);
                        assert(schema_depth(current) == current.depth);
                    }
                }
                foreach_in_arraylist(Schema, s, list_normalized_set_schema2){
                    SchemaIterator it = create_schema_iterator(*s);
                    Schema current;
                    while(schema_iterator_next(&it, &current)){
                        assert(schema_size(current) == current.size);
                        assert(schema_depth(current) == current.depth);
                    }
                }
#endif          
                // NOTE: wrap to calculate size and depth only in one place
                SetSchema normalized_common_set_schema = { .list = list_normalized_common_set_schema, .size = 0 };
                SetSchema normalized_set_schema1 = { .list = list_normalized_set_schema1, .size = 0 };
                SetSchema normalized_set_schema2 = { .list = list_normalized_set_schema2, .size = 0 };
                foreach_in_arraylist(Schema, s, normalized_common_set_schema.list) { normalized_common_set_schema.size += s->size; }
                foreach_in_arraylist(Schema, s, normalized_set_schema1.list) { normalized_set_schema1.size += s->size; }
                foreach_in_arraylist(Schema, s, normalized_set_schema2.list) { normalized_set_schema2.size += s->size; }

                // NOTE: calculate starting column indices
                clear_arena(&starting_indices_arena);
                unsigned *starting_col_indices1 = starting_column_indexes(list_normalized_set_schema1, &starting_indices_arena);
                unsigned *starting_col_indices2 = starting_column_indexes(list_normalized_set_schema2, &starting_indices_arena);

                unsigned row_len1 = rb.c1;
                unsigned row_len2 = rb.c2;
                assert(set_schema_size(list_normalized_set_schema1) == row_len1);
                assert(set_schema_size(list_normalized_set_schema2) == row_len2);

                operand_block *ob1 = obs1 + t1-1;
                operand_block *ob2 = obs2 + t2-1;

                mgu_schema computed_mapping;
                computed_mapping.n_common = normalized_common_set_schema.size;
                unsigned num_cols1 = normalized_set_schema1.size;
                computed_mapping.new_a = computed_mapping.n_common - num_cols1;
                unsigned num_cols2 = normalized_set_schema2.size;
                computed_mapping.new_b = computed_mapping.n_common - num_cols2;

                unsigned num_bytes = sizeof(*computed_mapping.common_columns) * computed_mapping.n_common;
                computed_mapping.common_columns = malloc(num_bytes);
                CHECK_MALLOC(computed_mapping.common_columns, "test_mapping_obtention_");
                for(unsigned i = 0; i < computed_mapping.n_common; ++i){ computed_mapping.common_columns[i] = i; }

                unsigned mapping_side_size = computed_mapping.n_common * sizeof(unsigned);
                unsigned *mapping_sides = NULL;

                if(rb.lineal_lineal){
                    unsigned *mapping_sides = malloc(2 * mapping_side_size);
                    CHECK_MALLOC(mapping_sides, "test_mapping_obtention_");
                    unsigned *mappingL = mapping_sides;
                    unsigned *mappingR = mapping_sides + computed_mapping.n_common;

                    assert(ob1->r > 0 && ob2->r > 0);
                    
                    mapping_column_indexes_side_lineal(
                        normalized_set_schema1, free_var_positions1, normalized_common_set_schema,
                        starting_col_indices1, ob1->terms[0].row, mappingL);
                    
                    mapping_column_indexes_side_lineal(
                        normalized_set_schema2, free_var_positions2, normalized_common_set_schema,
                        starting_col_indices2, ob2->terms[0].row, mappingR);

                    // Only one mapping in linear result block
                    mgu_schema *mapping = rb.ms;
                    computed_mapping.common_L = mappingL;
                    computed_mapping.common_R = mappingR;
                    bool ok_mappings = equal_mgu_schemas(mapping, &computed_mapping);
                    printf("ok_mappings = %u\n\n", ok_mappings);
                    assert(ok_mappings);

                } else {
                    // NOTE: the calculation of mappingL/R is independent of one another. We can precompute them in two linear loops instead of a quadratic nested loop.
                    
                    unsigned *mapping_sides = malloc((ob1->r + ob2->r) * mapping_side_size);
                    CHECK_MALLOC(mapping_sides, "test_mapping_obtention_");
                    unsigned *mapping_side = mapping_sides;

                    // NOTE: at most we will have as many variables as the number of columns and at most as many new virtual columns as the sum
                    //  of the number of new columns (if any repeated variable, we will have less virtual columns, because its virtuals will be repeated too)
                    Arena row_vars_to_extending_cols_arena; 
                    unsigned num_bytes_pointers1 = sizeof(unsigned*) * num_cols1;
                    unsigned num_bytes_pointers2 = sizeof(unsigned*) * num_cols2;
                    unsigned num_bytes_virtual_columns1 = sizeof(unsigned)*(computed_mapping.new_a);
                    unsigned num_bytes_virtual_columns2 = sizeof(unsigned)*(computed_mapping.new_b);
                    init_arena(&row_vars_to_extending_cols_arena, MAX(num_bytes_pointers1 + num_bytes_virtual_columns1, num_bytes_pointers2 + num_bytes_virtual_columns2));

                    for(unsigned i = 0; i < ob1->r; ++i, mapping_side += computed_mapping.n_common){
                        mapping_column_indexes_side(
                            normalized_set_schema1, free_var_positions1, normalized_common_set_schema,
                            starting_col_indices1, ob1->terms[i].row, mapping_side, &row_vars_to_extending_cols_arena
                        );
                        clear_arena(&row_vars_to_extending_cols_arena);
                    }
                    for(unsigned i = 0; i < ob2->r; ++i, mapping_side += computed_mapping.n_common){
                        mapping_column_indexes_side(
                            normalized_set_schema2, free_var_positions2, normalized_common_set_schema,
                            starting_col_indices2, ob2->terms[i].row, mapping_side, &row_vars_to_extending_cols_arena
                        );
                        clear_arena(&row_vars_to_extending_cols_arena);
                    }

                    free_arena(&row_vars_to_extending_cols_arena);
                    
                    // Change the mgu_schemas sides and compare
                    // One mapping per row pairs in non-linear result block
                    for(unsigned i = 0; i < ob1->r; ++i){
                        computed_mapping.common_L = mapping_sides + i*computed_mapping.n_common;
                        for(unsigned j = 0; j < ob2->r; ++j){
                            printf("Rows: %d-%d (1-based)\n", i+1, j+1);
                            
                            mgu_schema *mapping = rb.terms[ i*rb.r2 + j ].ms;

                            computed_mapping.common_R = mapping_sides + (ob1->r + j)*computed_mapping.n_common;

                            bool ok_mappings = equal_mgu_schemas(mapping, &computed_mapping);
                            
                            printf("ok_mappings = %u\n\n", ok_mappings);
                            assert(ok_mappings);
                        }
                    }

                }
                
                free(mapping_sides);
                free(computed_mapping.common_columns);
                free_result_block(&rb);
                read_result_block(stream_M3, &rb, &common_set_schema, &common_dependencies, &free_vars3, arena);
            }
            else {
                // NOTE: common schema doesn't exist so a fragment was skipped
                printf("Resultant fragment: %d-%d (SKIPPED BECAUSE COMMON SCHEMA DOESN'T EXIST!)\n", t1, t2);
                assert(!exists_common_schema);
            }
        }
    }

    free_arena(&starting_indices_arena);

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

    char *folder_path = argv[1]; // AGT002+1
    //char *folder_path = "data/experimentation_matrices/AGT004+2";
    //char *folder_path = "data/experimentation_matrices/AGT/AGT006+2.p";
    //char *folder_path = "data/experimentation_matrices/ITP/ITP018+5.p";
    //char *folder_path = "data/experimentation_matrices/SEV/SEV437+1.p";

    global_print_debugging = argc > 2;

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

            if(
                // Skip passed tests: AGT002+1
                //strstr(base, "test0016") || 
                //strstr(base, "test0015") ||
                //strstr(base, "test0001") || 
                //strstr(base, "test0005") || 
                //strstr(base, "test0009") ||
                //strstr(base, "test0017") ||
                strstr(base, "test0011") ||
                
                // Skip passed tests: AGT004+2
                //strstr(base, "test0001") || 
                //strstr(base, "test0004") ||

                // Skip passed tests: AGT006+2.p
                //strstr(base, "test0002") ||

                // Skip passed tests: ITP018+5.p
                //strstr(base, "test0361") ||
                //strstr(base, "test0458") || // TODO(instance): similar problem to SEV. I don't see why the computed is problematic... 
                //  It seems that the criteria for extending variables already in the file is different than the one I am using (?)
                //  But not only that, because all the same column indexes are used, but in a different order...
                //1-80,2-81,3-82,4-83,5-1,6-84,7-85,8-86,9-87,10-88,11-89,12-90,13-91,14-92,15-93,16-94,17-95,18-96,19-97,20-98,21-99,22-100,23-101,24-102,25-103,26-104,27-105,28-106,29-107,30-108,31-109,32-110,33-111,34-112,35-113,36-114,37-115,38-116,39-117,40-118,41-119,42-120,43-121,44-122,45-123,46-124,47-125,48-126,49-127,50-128,51-129,52-130,53-131,54-132,55-133,56-134,57-135,58-136,59-137,60-138,61-139,62-140,63-141,64-142,65-143,66-144,67-145,68-146,69-147,70-148,71-149,72-150,73-151,74-152,75-153,76-154,77-155,78-156,79-157,80-158,81-159,82-160,83-161,84-162,85-163,86-164,87-165,88-166,89-167,90-168,91-169,92-2,93-3,94-4,95-5,107-6,108-7,109-8,110-9,111-10,112-11,113-12,114-13,115-14,116-15,117-16,118-17,96-18,97-19,119-20,120-21,121-22,122-23,123-24,124-25,125-26,126-27,127-28,128-29,98-30,99-31,100-32,129-33,130-34,131-35,132-36,133-37,134-38,101-39,102-40,103-41,135-42,136-43,137-44,138-45,139-46,140-47,141-48,142-49,143-50,144-51,145-52,146-53,147-54,148-55,149-56,150-57,151-58,152-59,153-60,154-61,155-62,156-63,157-64,158-65,159-66,160-67,161-68,162-69,163-70,104-71,105-72,106-73,164-74,165-75,166-76,167-77,168-78,169-79
                //1-80,2-81,3-82,4-83,5-1,6-84,7-85,8-86,9-87,10-88,11-89,12-90,13-91,14-92,15-93,16-94,17-95,18-96,19-97,20-98,21-99,22-100,23-101,24-102,25-103,26-104,27-105,28-106,29-107,30-108,31-109,32-110,33-111,34-112,35-113,36-114,37-115,38-116,39-117,40-118,41-119,42-120,43-121,44-122,45-123,46-124,47-125,48-126,49-127,50-128,51-129,52-130,53-131,54-132,55-133,56-134,57-135,58-136,59-137,60-138,61-139,62-140,63-141,64-142,65-143,66-144,67-145,68-146,69-147,70-148,71-149,72-150,73-151,74-152,75-153,76-154,77-155,78-156,79-157,80-158,81-159,82-160,83-161,84-162,85-163,86-164,87-165,88-166,89-167,90-168,91-169,92-2,93-3,94-4,95-5,96-6,97-7,98-8,99-9,100-10,101-11,102-12,103-13,104-14,105-15,106-16,107-17,108-18,109-19,110-20,111-21,112-22,113-23,114-24,115-25,116-26,117-27,118-28,119-29,120-30,121-31,122-32,123-33,124-34,125-35,126-36,127-37,128-38,129-39,130-40,131-41,132-42,133-43,134-44,135-45,136-46,137-47,138-48,139-49,140-50,141-51,142-52,143-53,144-54,145-55,146-56,147-57,148-58,149-59,150-60,151-61,152-62,153-63,154-64,155-65,156-66,157-67,158-68,159-69,160-70,161-71,162-72,163-73,164-74,165-75,166-76,167-77,168-78,169-79
                //                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  ^


                // Skip passed tests: SEV437+1.p
                // TODO(instance): lineal-lineal (1-1) result block passed. In the first non-lineal block's first pair of rows mismatch found,
                //  but I think that both mappings might be equivalent (at least I don't see why the computed one is incorrect...)
                //  Re-test it when it's integrated with core...
                //1-1,63-2,64-3,65-4,66-5,67-6,68-7,2-8,3-9,4-10,5-11,6-45,69-46,70-47,71-48,72-49,73-50,74-51,75-52,76-53,77-54,78-55,79-56,7-12,8-13,9-45,80-46,81-47,82-48,83-49,84-50,85-51,86-52,87-53,88-54,89-55,90-56,10-14,11-15,12-57,13-58,14-59,15-60,16-61,17-62,18-63,19-64,20-65,21-66,22-67,23-68,24-16,25-69,26-70,27-71,28-72,29-73,30-74,31-75,32-76,33-77,34-78,35-79,36-80,37-17,38-45,39-46,40-47,41-48,42-49,43-50,44-51,45-52,46-53,47-54,48-55,49-56,50-18,51-19,52-20,53-21,91-22,92-23,93-24,54-25,55-26,94-27,95-28,96-29,97-30,98-31,56-32,57-33,58-34,59-35,99-36,100-37,60-38,61-39,62-40,101-41,102-42,103-43,104-44
                //1-1,63-2,64-3,65-4,66-5,67-6,68-7,2-8,3-9,4-10,5-11,6-45,7-46,8-47,9-48,10-49,11-50,12-51,13-52,14-53,15-54,16-55,17-56,18-12,19-13,20-45,21-46,22-47,23-48,24-49,25-50,26-51,27-52,28-53,29-54,30-55,31-56,32-14,33-15,34-57,35-58,36-59,69-60,70-61,71-62,72-63,73-64,74-65,75-66,76-67,77-68,78-16,79-69,80-70,81-71,82-72,83-73,84-74,85-75,86-76,87-77,88-78,89-79,90-80,37-17,38-45,39-46,40-47,41-48,42-49,43-50,44-51,45-52,46-53,47-54,48-55,49-56,50-18,51-19,52-20,53-21,91-22,92-23,93-24,54-25,55-26,94-27,95-28,96-29,97-30,98-31,56-32,57-33,58-34,59-35,99-36,100-37,60-38,61-39,62-40,101-41,102-42,103-43,104-44
                //                                                         ^
                //strstr(base, "test0003") ||

                false)
            {
                continue;
            }

            // 4. Construct M2 and M3 paths
            snprintf(path_m2, sizeof(path_m2), "%sM2.csv", base);
            snprintf(path_m3, sizeof(path_m3), "%sM3.csv", base);

            // 5. Check if M2 and M3 exist (access F_OK is like [[ -f ]])
            if (access(path_m2, F_OK) == 0 && access(path_m3, F_OK) == 0) {
                printf("%s\n", base);
                
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
