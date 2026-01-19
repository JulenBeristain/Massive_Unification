#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>

#include "dictionary.h"
#include "structures.h"
#include "perf_hash.h"

// Buffer size for row-string conversion, enough for INT_MAX digits plus null terminator
#define ROW_STR_SIZE (snprintf(NULL, 0, "%d", INT_MAX) + 1)  

// Global dictionary mapping variables in the csv file to their indices during parsing
Dictionary *var_dict;  

// Global dictionary for tracking variable–variable substitutions when applying unifier
Dictionary *unif_dict;  

// Flag to skip header check only on the first read_result_block call
bool first_rb = true;  

// Verbosity flag: if nonzero, print debugging and timing information
int verbose = 0;  

// Accumulated timers for file I/O, unifier computation, and unification application
struct timespec read_file_elapsed, unifiers_elapsed, unification_elapsed;  

// Internal flag for tracing events or logging extra diagnostics if enabled, only used during development
bool chivato = false;  

// Tracks if all unified matrix subsets from M3 file are correct
bool global_correct = true;  

// Total number of matrix intersections performed
unsigned global_count = 0;  

// Count of intersections that failed correctness checks
unsigned global_incorrect = 0;  


// --------------------- UTILS START --------------------- //

/**
 * @brief Prints the internal L2 substitution list for debugging
 * @param lst   Array of L2 entries of length 2*n
 * @param n     Number of variable pairs
 *
 * Computes maximum list depth across all entries
 * Allocates temporary arrays to collect ind, count, by, and chained indices
 * Prints:
 *   ind:    original index for each entry
 *   count:  substitution count flag
 *   by:     target index or X if none
 *   chained rows of subsequent substitutions or X
 * Frees all temporaries before return
 */
void print_L2_lst(L2 *lst, const unsigned n){

    // Chech max depth
    unsigned max_depth = 0;
    for (size_t i = 0; i < 2*n; i++)
    {
        unsigned depth = 0;
        L3 *tmp = lst[i].head;
        while (tmp!=NULL)
        {
            depth++;
            tmp=tmp->next;
        }
        if (depth>max_depth) max_depth = depth;
    }

    // Allocate memory to display L2 list
    int *ind     = (int*)malloc(2*n*sizeof(int));
    int *count   = (int*)malloc(2*n*sizeof(int));
    int *by      = (int*)malloc(2*n*sizeof(int));
    int *indices = (int*)malloc(max_depth*2*n*sizeof(int));
    memset(indices, -1, max_depth*2*n*sizeof(int));

    // Fill the spaces
    for (size_t i = 0; i < 2*n; i++)
    {
        ind[i] = lst[i].ind;
        count[i] = lst[i].count;
        by[i] = lst[i].by;

        unsigned idx = 0;
        L3 *tmp = lst[i].head;
        while (tmp!=NULL)
        {
            indices[max_depth*i+idx] = tmp->ind;
            tmp=tmp->next;
            idx++;
        }
    }

    // Display the L2 list
    printf("------------\n");
    printf("ind:\t");
    for (size_t i = 0; i < 2*n; i++)
    {
        printf("%d",ind[i]);
        if ((i+1)<(2*n)) printf(",");
    }
    printf("\n");
    printf("count:\t");
    for (size_t i = 0; i < 2*n; i++)
    {
        printf("%d",count[i]);
        if ((i+1)<(2*n)) printf(",");
    }
    printf("\n");
    printf("by:\t");
    for (size_t i = 0; i < 2*n; i++)
    {
        int val = by[i];
        if (val==-1) printf("X");
        else         printf("%d",val);
        if ((i+1)<(2*n)) printf(",");
    }
    printf("\n");
    
    for (size_t i = 0; i < max_depth; i++)
    {
        printf("\t");
        for (size_t j = 0; j < 2*n; j++)
        {
            int val = indices[max_depth*j+i];
            if (val==-1) printf("X");
            else         printf("%d",val);
            if ((j+1)<(2*n)) printf(",");
        }
        printf("\n");
    }
    
    // Free all memory:
    free(ind);
    free(count);
    free(by);
    free(indices);
}

/**
 * @brief Adds two timespec values
 * @param result  Pointer to timespec to receive sum
 * @param t1      First timespec to add
 * @param t2      Second timespec to add
 *
 * Computes result = t1 + t2, normalizing tv_nsec to <1e9
 */
void timespec_add(struct timespec *result, const struct timespec *t1, const struct timespec *t2) {
    result->tv_sec = t1->tv_sec + t2->tv_sec;
    result->tv_nsec = t1->tv_nsec + t2->tv_nsec;
    if (result->tv_nsec >= 1000000000L) {
        result->tv_sec++;
        result->tv_nsec -= 1000000000L;
    }
}

/**
 * @brief Prints a row from a 2D integer matrix, used for debugging
 * @param mat  Pointer to int array of size n*m
 * @param n    Number of rows
 * @param m    Number of columns
 *
 * Each row printed as “[x0 x1 … x(m-1)]”.
 */
void print_mat_values(int *mat, int n, int m){
    int i, j;
    int line_len = m;
    
	for (i = 0; i < n; i++)
	{
		printf("[");
		for (j = 0; j < m; j++)
			printf("%d ", mat[i*line_len+j]);
		printf("]\n");
	}
}

/**
 * @brief Prints mapping pairs in “X-Y” format, for debugging
 * @param n_tl  Number of X-Y pairs
 * @param map   Array of length 2*n_tl: [X0,Y0,X1,Y1,...,X(n_tl-1),Y(n_tl-1)]
 *
 * Prints “[X0-Y0,X1-Y1,...,X(n_tl-1)-Y(n_tl-1)]”, using “_” for X=0|Y=0
 */
void print_mapping(unsigned n_tl, unsigned *map) {
    for (unsigned i = 0; i < n_tl; i++) {
        unsigned left = map[2 * i];
        unsigned right = map[2 * i + 1];
        if (left == 0) printf("_");
        else printf("%u", left);
        printf("-");
        if (right == 0) printf("_");
        else printf("%u", right);
        if (i + 1 < n_tl) printf(",");
    }
    printf("\n");
}

/**
 * @brief Prints a unifier
 * @param unifier  Array with format [n_elem, x1,y1, …, xi,yi, idxA, idxB]
 * @param m        Number of columns in resulting term
 *
 * Prints [n_elem,x<-y,...,idxA,idxB], x<-y being 0-based and idxA and idxB being 1-based
 */
void print_unifier(unsigned *unifier, unsigned m){
    unsigned i;
    unsigned n_elem = unifier[0]*2;
    unsigned idxA = unifier[1+2*m];
    unsigned idxB = unifier[1+2*m+1];
    
    printf("%d elements: [",n_elem);
	for (i = 0; i < n_elem; i+=2)
	{
        printf("%u<-%u,", unifier[1+i],unifier[1+i+1]);
	}
    printf("],\t\tidxA: %u, idxB: %u\n",idxA+1, idxB+1);
}

/**
 * @brief Prints a list of unifiers, for debugging
 * @param unifiers    Array of unifier vectors concatenated
 * @param unif_count  Number of unifiers in the array
 * @param m           Number of columns in resulting term
 *
 * Iterates and calls print_unifier for each entry, then prints total count
 */
void print_unifier_list(unsigned *unifiers, unsigned unif_count, unsigned m){

    unsigned i, unifier_size = 1+(2*m)+2;
    for (i=0; i<unif_count; i++)
    {
        print_unifier(&unifiers[i*unifier_size],m);
    }
    printf("Number of unifiers: %d\n",unif_count);
}

/**
 * @brief Prints one row of a matrix as “[x0,x1,…]”, for debugging
 * @param row  Pointer to int array of length m
 * @param m    Number of columns
 */
void print_mat_line(int *row, int m){
    int j;
    printf("[");
    for (j = 0; j < m; j++)
        printf("%d,", row[j]);
    printf("]\n");
}


/**
 * @brief Compares two MGU matrices for equality
 * @param my_mgu     Pointer to first matrix array of size n*m
 * @param other_mgu  Pointer to second matrix array of size n*m
 * @param n          Number of rows.
 * @param m          Number of columns
 * @return 1 if all entries match; 0 if any differ plus printing context for debugging
 */
int compare_mgus(int *my_mgu, int *other_mgu, int n, int m){
    int same = 1;
    int n_elems = n*m;
    int i;

    for (i = 0; i < n_elems; i++)
    {
        if (my_mgu[i] != other_mgu[i])
        {
            int row = i/m;
            int col = i%m;
            printf("\t"); print_mat_line(&my_mgu[row*m],m);
            printf("\t"); print_mat_line(&other_mgu[row*m],m);
            printf("\tDifferent elements at row %d col %d: %d != %d\n",row,col,my_mgu[i],other_mgu[i]);
            return 0;
        }
    }
    return same; 
}

/**
 * @brief Compares two main_term structures for equality
 * @param mt1  First main_term
 * @param mt2  Second main_term
 * @return 1 if column count, exception count, row values, and exception blocks all match; 0 otherwise plus printing context for debugging
 */
int compare_main_terms(main_term *mt1, main_term *mt2){
    // Quick check
    if ((mt1->c != mt2->c) || (mt1->e != mt2->e))
    {
        if (verbose) {printf("compare_main_terms quick test failed\n");}
       return 0;
    }

    for (size_t i = 0; i < mt1->c; i++)
    {
        if (mt1->row[i]!=mt2->row[i])
        {
            if (verbose) {printf("compare_main_terms row test failed at element in column %lu\n",i);}
            if (verbose) {printf("my_rb:\t"); print_main_term(mt1,3,0);}
            if (verbose) {printf("rb   :\t"); print_main_term(mt2,3,0);}
            return 0;
        }
    }

    for (size_t i = 0; i < mt1->e; i++)
    {
        unsigned n = mt1->exceptions[i].n;
        unsigned m = mt1->exceptions[i].m;

        for (size_t j = 0; j < n; j++)
        {
            for (size_t k = 0; k < m; k++)
            {
                if (mt1->exceptions[i].mat[j*m+k] != mt2->exceptions[i].mat[j*m+k])
                {
                    printf("compare_main_terms exception test failed at exception block %lu, at exception %lu in column %lu\n",i,j,k);
                    printf("my_rb:\t"); print_exception_block(&mt1->exceptions[i],3,i+1);
                    printf("rb   :\t"); print_exception_block(&mt2->exceptions[i],3,i+1);
                    return 0;
                }
            }
            
        }
        
    }
    return 1;
}

/**
 * @brief Compares two result_block contents against expected operand_blocks
 * @param rb1   First result_block, the one computed
 * @param rb2   Second result_block, the one read from csv
 * @param ob1   First operand_block used in comparison context
 * @param ob2   Second operand_block used in comparison context
 * @return 1 if all metadata (dimensions, counts) and each valid main_term (rows and exceptions) match; 0 otherwise plus printing context for debugging
 */
int compare_results(result_block *rb1, result_block *rb2, operand_block *ob1, operand_block *ob2){
    
    // Quick check
    if ((rb1->c1 != rb2->c1) || (rb1->c2 != rb2->c2) || (rb1->c != rb2->c) || 
    (rb1->r1 != rb2->r1) || (rb1->r2 != rb2->r2) || (rb1->r != rb2->r)) 
    {
        if (verbose) printf("compare_results quick test failed\n");
        if (verbose) printf("rb1: c1=%d, c2=%d, c=%d, r1=%d, r2=%d, r=%d, t1=%d, t2=%d\n",
            rb1->c1, rb1->c2, rb1->c, rb1->r1, rb1->r2, rb1->r, rb1->t1, rb1->t2);
        if (verbose) printf("rb2: c1=%d, c2=%d, c=%d, r1=%d, r2=%d, r=%d, t1=%d, t2=%d\n",
            rb2->c1, rb2->c2, rb2->c, rb2->r1, rb2->r2, rb2->r, rb2->t1, rb2->t2);
        return 0;
    }


    // Iterate the main terms, and check the rows, exceptions bloks, etc
    for (unsigned i = 0; i < rb1->r; i++)
    {
        if (rb1->valid[i] != rb2->valid[i]) {
            if (verbose) {printf("compare_results valid test failed at term %u (%u-%u)\n",i, i/rb1->r2+1, i%rb1->r2+1);}
            if (verbose) {printf("my valid: %u, csv valid: %u\n",rb1->valid[i], rb2->valid[i]);}
            if (verbose) {print_mgu_schema(rb2->terms[i].ms);print_mgu_compact(rb2->terms[i].ms);}
            return 0;}
        if (rb1->valid[i] == 0) // Only in this case, otherwise rows and exceptions will be empty
        {
            if (!compare_main_terms(&rb1->terms[i], &rb2->terms[i]))
            {
                if (verbose) {printf("compare_results main term test failed at term %u (%u-%u)\n",i, i/rb1->r2+1, i%rb1->r2+1);}
                if (verbose) {printf("mt1:\t"); print_main_term(&ob1->terms[i/rb1->r2],1,1);}
                if (verbose) {printf("mt2:\t"); print_main_term(&ob2->terms[i%rb1->r2],2,1);}
                if (verbose) {printf("mt3  (me):\t"); print_main_term(&rb1->terms[i],3,0);}
                if (verbose) {printf("mt3 (csv):\t"); print_main_term(&rb2->terms[i],3,0);}
                if (verbose) {print_mgu_schema(rb2->terms[i].ms);print_mgu_compact(rb2->terms[i].ms);}
                return 0;
            }

        }
    }
    return 1;
}
// ---------------------- UTILS END ---------------------- //


// --------------------- READING FILE START --------------------- //
/**
 * @brief Reads the number of blocks from the first line containing “%%% BEGIN: Matrix MX (S) %%%”.
 * @param stream  Pointer to input FILE
 * @param s       Pointer to unsigned where parsed number is stored
 * @return 0 on success; 1 on failure
 * 
 * Reads one line via getline(). On any error (EOF, missing tokens, parse failure), returns 1
 */
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

/**
 * @brief Reads two unsigned dimensions (n, m) from a “% BEGIN: Matrix subsetX.Y (n,m)
 * @param stream  Pointer to input FILE
 * @param n       Pointer to unsigned to receive first number
 * @param m       Pointer to unsigned to receive second number
 * @return void  Exits on any parse error or missing tokens
 */
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

/**
 * @brief Parses one CSV data line into integer row values, handling constants and variables
 * @param line        Modifiable string containing comma-separated tokens
 * @param row         Pointer to int array where parsed values are stored
 * @param skip_first  If true, skip first token before parsing row entries (for exceptions or not)
 * @return void
 * 
 * Tokenizes input on commas/newlines. If token begins with uppercase letter, treats as variable:
 *   - If unseen, installs in var_dict with current column index, row[col]=0
 *   - If seen, row[col]= –(definition index)
 * If token begins lowercase/digit, looks up constant value in symbol table and writes s->value
 * Clears var_dict before iteration, because variables are independent from row to row
 */
void read_line(char *line, int *row, bool skip_first) {
    char *tok = strtok(line, ",");
    if (skip_first) tok = strtok(NULL, ",\n");

    int col = 1;
    clear(var_dict);
    while (tok) {
        if (!isupper(tok[0])) { // If no uppercase appears, it is a constant
            const struct Symbol* s = get_value(tok, strlen(tok));
            row[col-1] = s->value;
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

/**
 * @brief Parses mapping pairs from a “X-Y” list into an array
 * @param line      Modifiable string containing pairs separated by commas
 * @param n_pairs   Number of expected pairs
 * @param mapping   Pointer to unsigned array of length n_pairs*2 to fill with parsed ints
 * @return void
 * 
 * Tokenizes on space/comma/newline. For each token containing ‘-’, splits into first/second
 * Converts each side to unsigned via atoi, treating “_” as zero, although with last format of test files no "_" should happen
 * Stores sequentially in mapping[]
 */
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

/**
 * @brief Reads exception blocks from stream into main_term structures. NOT USED FOR NOW, OUTDATED
 * @param stream  Pointer to input FILE
 * @param mt      Pointer to main_term containing e exception count and exception pointers
 * @param result  If true, input follows result-file format (skips extra unifier lines)
 * @return void  Exits on any parse error
 * 
 * For each exception 0..e-1:
 *   - read_dimensions n,m
 *   - allocate empty exception_block of size n×m
 *   - skip unflatened schema line
 *   - read mapping line, build mgu schema
 *   - optionally skip flattened schema
 *   - for each of n rows:
 *       read line, adjust pointer if result format, call read_line
 *       if result format, skip unifier line
 *   - skip “END” line
 */
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

/**
 * @brief Reads one operand matrix (with exception blocks) from stream
 * @param stream  Pointer to input FILE
 * @param ob      Pointer to operand_block to populate (r rows, c columns)
 * @return void  Exits on EOF or parse error
 * 
 */
void read_operand_matrix(FILE *stream, operand_block *ob) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    unsigned row = 0;

    // Skip unflatened schema
    getline(&line, &len, stream);

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

/**
 * @brief Reads result matrix subsets and their unification terms from stream
 * @param stream  Pointer to input FILE
 * @param rb      Pointer to result_block to fill: indices, dimension, terms, valid flag
 * @return void  Exits on parse error
 * 
 * Reads header “% BEGIN: Matrix subset t1-t2 (r1-r2,c1-c2,c)”
 * Result matrix subsets that combine two lineal operand matrix subsets have one unique mapping common to all unified rows
 * For convenience, it is red once but copied to each result_block.main_term's struct. Not memory efficient, but makes treatment later homogeneous 
 */
void read_result_matrix(FILE *stream, result_block *rb) {
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

    bool not_first_subset;
    if (rb->t1 == rb->t2 && rb->t1 == 1) not_first_subset = false;
    else                                 not_first_subset = true;

    // Info for all possible main terms from the unification of M1 block (r1) and M2 block (r2)
    rb->r = rb->r1*rb->r2;
    rb->terms = (main_term*)malloc(rb->r * sizeof(main_term));
    rb->valid = (unsigned*) malloc(rb->r * sizeof(unsigned));
    for (size_t aux = 0; aux < rb->r; aux++)
    {
        rb->valid[aux] = 0;
    }
    

    // Skip largest schema
    getline(&line, &len, stream);

    // Get mapping info
    bool first_is_non_lineal = false;
    unsigned *mapping = (unsigned*)malloc(rb->c*2*sizeof(unsigned));
    if (!not_first_subset) {
        getline(&line, &len, stream);
        if (strchr(line, '-') == NULL) {
            not_first_subset   = true;
            first_is_non_lineal = true;
        } else {
            get_mapping(line, rb->c, mapping);
            rb->ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);
            rb->lineal_lineal = true;
        }
    }

    // Skip flattened schema
    if (!first_is_non_lineal) getline(&line, &len, stream);

    // Iterate the main term rows
    unsigned row = 0;
    while ((read = getline(&line, &len, stream)) != -1 && row < rb->r) {
        
        // If end of matrix reached, exit
        if (strstr(line, "END") != NULL || strstr(line, "End") != NULL)
            break;
        
        // Get the mapping for each line, but initialize mgu_schema later
        if (not_first_subset)
        {
            // Make a modifiable copy of the line to trim off the 'Mapping X-Y:'
            char *line_ptr = line;  
            line_ptr = strchr(line_ptr, ':') + 2;
            get_mapping(line_ptr, rb->c, mapping);
            getline(&line, &len, stream); // For reading line info
        }

        // Inspect if line is unifiable, subsumed or not unifiable
        if (strstr(line, "subsumed by exception") != NULL)
        {
            rb->valid[row] = 1;
            continue;
        }
        else if (strstr(line, "not unifiable") != NULL)
        {
            rb->terms[row] = create_null_main_term();
            rb->terms[row].ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);
            rb->valid[row] = 2;
            row++;
            continue;
        }

        // If unifiable, read number of exception blocks
        unsigned d1, d2, e;
        if (sscanf(line, "Row %u-%u: %u", &d1, &d2, &e) != 3 &&
            sscanf(line, "Rows %u-%u: %u", &d1, &d2, &e) != 3) 
        {
            if (verbose) printf("Line is: '%s'\n",line);
            fprintf(stderr, "Could not read number of exception blocks in row\n");
            free(line);
            exit(EXIT_FAILURE);
        }

        // Initialize the exception blocks
        rb->terms[row] = create_empty_main_term(rb->c,e);
        if (rb->valid[row]!=1) rb->valid[row] = 0;

        // Get a pointer to the main term for easier working
        main_term *mt = &(rb->terms[row]);

        // Read the rest of the line
        read_line(line, mt->row, true);

        // Skip the unifier line
        getline(&line, &len, stream);

        // Read one by one the exception blocks
        if (e) read_exception_blocks(stream, mt, true);

        // Add mapping to main_term only if the result_block is not formed between two lineal matrix subset operands, so mgu_schema cannot be reused
        if (rb->lineal_lineal) {
            mt->ms = NULL;
        } else {
            mt->ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);
        }

        // Increment the row by 1
        row++;

    }

    free(line);
    free(mapping);
}

/**
 * @brief Reads an operand block from a CSV-formatted stream
 * @param stream   FILE pointer positioned at the first line of an operand block header
 *                 Header format: “% BEGIN: Matrix subsetX.Y (n,m)”
 * @return         Populated operand_block containing r*c main terms, each with exception blocks
 *
 */
 operand_block read_operand_block(FILE *stream) {

    // Read operand block dimensions
    unsigned r, c;
    read_dimensions(stream, &r, &c);

    // Create the operand_block structure for later populating it
    operand_block ob = create_empty_operand_block(r, c);

    // Read the operand block and fill the struct
    read_operand_matrix(stream, &ob);

    return ob;
}

/**
 * @brief Reads a result block from a CSV-formatted stream
 * @param stream   FILE pointer positioned at the first line of a result block
 *                 May begin with “END: Matrix M1 & M2 + MGU” if first_rb flag set
 * @return         Populated result_block containing subset indices, term count, mappings, and exception data
 *
 * On first invocation (first_rb==true), reads and skips the matrix header line
 * Detects early “END” marker and returns a null result_block if present
 * Delegates content parsing to read_result_matrix()
 */
result_block read_result_block(FILE *stream) {

    // Create the operand_block structure for later populating it
    result_block rb = create_null_result_block();

    char *line = NULL;
    size_t len = 0;

    // Skip matrix header
    if (first_rb) {
        first_rb=false; 
        getline(&line, &len, stream);
        if (strstr(line, "END: Matrix M1 & M2 + MGU") != NULL) {free(line); return rb;}
    }

    // Read the operand block and fill the struct
    read_result_matrix(stream, &rb);

    free(line);
    return rb;
}

// ---------------------- READING FILE END ---------------------- //

// --------------------- CORE START --------------------- //
/**
 * @brief Treats one variable-to-variable or constant-to-variable binding entry for unification, 'dumb' half of unifier calculation
 * @param row_a         Integer array for row A
 * @param indexA        Zero-based column index in row_a
 * @param row_b         Integer array for row B
 * @param indexB        Zero-based column index in row_b
 * @param unifier       Unsigned array to record binding pairs; entries start at unifier[1+indexUnifier] (first element is the number of substitutions)
 * @param indexUnifier  Offset into unifier array (pairs occupy two slots each)
 * @param cA            Number of original columns in row_a
 * @param n_cA          Number of newly added columns in row_a
 * @param cB            Number of original columns in row_b
 * @return 0 if substitution added or constants match; 1 if both entries constant and unequal (not-unifiable)
 *
 * If indexA refers to a new-variable column in row_a, adds (A←B) substitution
 * Else if indexB refers to a new-variable column in row_b, adds (B←A)
 * Otherwise retrieves a=row_a[indexA], b=row_b[indexB]
 *   - If both positive and unequal, returns 1 (not unifiable)
 *   - If a>0, b≤0, records (b←a)
 *   - If a≤0, records (a←b) (this could be changed for unification speed-up, will be studied later)
 */
int unifier_a_b(int *row_a, const unsigned indexA, int *row_b, const unsigned indexB, unsigned *unifier, const unsigned indexUnifier, const unsigned cA, const unsigned n_cA, const unsigned cB){

    const unsigned m1 = cA + n_cA; // The number of columns in row_a is the old number of columns plus the new columns

    // A is from the new columns of row_a, that is, always a variable. Add (a<-b) to the unifier, don't care about B
    if (indexA >= cA)
    {
        unifier[1+indexUnifier]   = indexA;
        unifier[1+indexUnifier+1] = indexB + m1; 
        return 0;
    } 
    // Else, B is from the new columns of row_b, so always a variable. Add (b<-a) to the unifier, don't care about A
    else if (indexB >= cB)
    {
        unifier[1+indexUnifier]   = indexB + m1;
        unifier[1+indexUnifier+1] = indexA; 
        return 0;
    }

    // If they are from the old columns, get elements
    const int a = row_a[indexA];
    const int b = row_b[indexB];

    // A is constant
	if (a>0 && b > 0 && a!=b) // B is constant too and they don't match
		return 1;
	else if (a>0 && b <= 0)   // B is constant, add to unifier (b<-a)
    {
        unifier[1+indexUnifier]   = indexB + m1;
        unifier[1+indexUnifier+1] = indexA; 
    }
	else if (a <= 0) // A is variable and we don't care about B, add to unifier (a<-b)
	{
        unifier[1+indexUnifier]   = indexA;
        unifier[1+indexUnifier+1] = indexB + m1; 
	}

	return 0;
}

/**
 * @brief Builds a full unifier for two main_term rows based on a mgu_schema
 * @param mt1       Pointer to first main_term 
 * @param mt2       Pointer to second main_term
 * @param ms        Pointer to mgu_schema listing common column pairs (in this version, all columns are common)
 * @param unifier   Unsigned array sized at least 2*ms->n_common to receive bindings (worst-case that is the max number of substitutions)
 * @return 0 if rows are unifiable; 1 upon first constant mismatch (easiest tell for not-unifiable)
 *
 * Iterates over each common column index pair in ms->common_L and common_R
 * Calls unifier_a_b() to handle individual binding or detect conflict
 * Halts and returns 1 on any non-unifiable pair; returns 0 otherwise
 */
int unifier_rows(main_term *mt1, main_term *mt2, mgu_schema *ms, unsigned *unifier){
    const unsigned m = ms->n_common;
    for (unsigned i = 0; i < m; i++)
    {
        if (unifier_a_b(mt1->row, ms->common_L[i]-1, mt2->row, ms->common_R[i]-1, unifier, 2*i, mt1->c, ms->new_a, mt2->c)) return 1; // If it is not unifiable already, do not bother continuing
    }
    
	return 0;
}

/**
 * @brief Verifies and simplifies a raw unifier array for two rows
 * @param mt1       First main_term
 * @param mt2       Second main_term
 * @param ms        Schema defining common and new columns
 * @param unifier   Array of length 1+2*m where m = ms->n_common, containing the 'raw' unifier (produced by first dumb step)
 * @return 0 if unifier is valid; -1 else
 *
 * Builds temporary lists to track variable substitutions
 * Normalizes repeated-variable indices
 * Merges constant-to-variable and variable-to-variable bindings
 * Removes redundant bindings, writes final substitution count at unifier[0]
 */
int correct_unifier(main_term *mt1, main_term *mt2, mgu_schema *ms, unsigned *unifier){
    
    const unsigned c1 = mt1->c;
    const unsigned c2 = mt2->c;
    const unsigned m  = ms->n_common;
    const int *row_a = mt1->row;
    const int *row_b = mt2->row;
    unsigned lst_length = m*2;
    unsigned i;

    L2 *lst = (L2*) malloc (lst_length*sizeof(L2));
    for (i=0;i<lst_length;i++){
        lst[i] = create_L2_empty();
    }

    // For each element pair, get indexes and perform (x<-y)
    for (i=0; i<2*m; i+=2)
    {
        unsigned x = unifier[1+i]; 
        unsigned y = unifier[1+i+1];
        int val_y;
        int val_x; 

        if (x == y && x == 0) continue; // Empty case

        // If any of the two elements is a repeated variable, get real index
        if (x < c1 && row_a[x] < 0)
            x = -(row_a[x]+1);
        else if (x >= m && (x-m) < c2 && row_b[x-m] < 0) 
            x = -(row_b[x-m]+1) + m;
        if (y < c1 && row_a[y] < 0) 
            y = -(row_a[y]+1);
        else if (y >= m && (y-m) < c2 && row_b[y-m] < 0)
            y = -(row_b[y-m]+1) + m; 


        // If any of them was substituted before, get the corresponding elements
        if (lst[x].count > 0) x = lst[x].by;
        if (lst[y].count > 0) y = lst[y].by;
        if (x==y) continue; 
        // No need to get real index again, the substitution is done for the real index

        // Get the value of x and y
        if (x < c1) val_x = row_a[x];
        else if (x < m) val_x = 0;
        else if ((x-m) < c2) val_x = row_b[(x-m)];
        else val_x = 0;

        if (y < c1) val_y = row_a[y];
        else if (y < m) val_y = 0;
        else if ((y-m) < c2) val_y = row_b[(y-m)];
        else val_y = 0;


        // If both constants 
        if (val_x > 0 && val_y > 0 && val_x!=val_y) {for(size_t aux=0;aux<lst_length;aux++) free_L2(lst[aux]); free(lst);return -1;} // And don't match
        else if (val_x > 0 && val_y > 0) continue;             // And match

        // If x is constant and y is variable
        if (val_x > 0 && val_y == 0) // (y<-x)
        {   
            // make the replacement on y
            lst[y].count = 1;
            lst[y].by    = x;
            lst[y].ind   = y;


            // Update all variables replaced by y to be replaced by x
            L3 *current = lst[y].head;
            while (current != NULL)
            {
                lst[current->ind].by = x;
                current = current->next;
            }

            // At end of X's replacement list, add Y and Y's replacement list (if any)
            L3* newNode = create_L3(y, NULL);

            // Appending it to X’s list (or make it head if empty)
            if (lst[x].head == NULL) {
                lst[x].head = newNode;
                lst[x].tail = newNode;
            }
            else {
                lst[x].tail->next = newNode;
                lst[x].tail       = newNode;
            }

            // Appending Y's chain
            if (lst[y].head != NULL) {
                newNode->next = lst[y].head;
                lst[x].tail   = lst[y].tail;
            }

            // Clear Y’s list
            lst[y].head = NULL;
            lst[y].tail = NULL;
        }
        // If x is variable
        else // (x<-y)
        {
            // Make the replacement on x
            lst[x].count = 1;
            lst[x].by    = y;
            lst[x].ind   = x;

            // Update all variables replaced by x to be replaced by y
            L3 *current = lst[x].head;
            while (current != NULL)
            {
                lst[current->ind].by = y;
                current = current->next;
            }


            // At end of Y's replacement list, add X and X's replacement list (if any)
            L3* newNode = create_L3(x, NULL);

            // Appending it to Y’s list (or make it head if empty)
            if (lst[y].head == NULL) {
                lst[y].head = newNode;
                lst[y].tail = newNode;
            }
            else {
                lst[y].tail->next = newNode;
                lst[y].tail       = newNode;
            }

            // Appending X's chain
            if (lst[x].head != NULL) {
                newNode->next = lst[x].head;
                lst[y].tail   = lst[x].tail;
            }

            // Clearing X’s list
            lst[x].head = NULL;
            lst[x].tail = NULL;
        }
    }

    int last_unifier = 0;
    unsigned n_substitutions = 0;

    // For each element, add the substitutions to the unifier
    for (i=0; i<lst_length; i++)
    {
        int y = i;
        int x;
        if (lst[y].count == 0 && lst[y].head)
        {
            // Get substitutions (x <- y)
            L3* current = lst[y].head;
            while (current != NULL)
            {
                x = current->ind;
                unifier[1+last_unifier]   = x;
                unifier[1+last_unifier+1] = y;
                current = current->next;
                n_substitutions++;
                last_unifier+=2;
            }
        }
    }
    unifier[0] = n_substitutions;


    // Free lst
    for ( i = 0; i < lst_length; i++) free_L2(lst[i]);
    free(lst);
    
    return 0;
}


/**
 * @brief Computes all valid unifiers between two operand matrices
 * @param ob1        Pointer to first operand_block
 * @param ob2        Pointer to second operand_block
 * @param rb         Pointer to result_block with schemas for each pair
 * @param unifiers   Preallocated array to receive unifier vectors. Size for worst-case (ob1.r*ob2.r), could resize for memory optimization
 * @return Number of unifiers found
 *
 * Allocates a temporary unifier buffer of size 1+2*m+2
 * Iterates over all row pairs (i,j)
 * Calls unifier_rows and correct_unifier for each pair
 * On success, appends unifier plus row indices (i,j) into unifiers
 */
unsigned unifier_matrices(operand_block *ob1, operand_block *ob2, result_block *rb, unsigned *unifiers){

    unsigned i, j, *unifier;
    int code;

    unsigned last_unifier = 0;
    const unsigned m  = rb->c;
    const unsigned unifier_size = 1+(2*m)+2;

    unifier = (unsigned*) malloc (unifier_size*sizeof(unsigned));

    for (i=0; i<ob1->r; i++)
    {
        for (j=0; j<ob2->r; j++)
        {
            memset(unifier,0,unifier_size*sizeof(unsigned));  
            unsigned index_mt = i*rb->r2+j;
            mgu_schema *schema_holder = rb->lineal_lineal ? rb->ms : rb->terms[index_mt].ms;
            code = unifier_rows(&ob1->terms[i], &ob2->terms[j], schema_holder, unifier);
            if (code != 0) continue; // Rows cannot be unified


            code = correct_unifier(&ob1->terms[i], &ob2->terms[j], schema_holder, unifier);
            if (code != 0) continue; // Rows cannot be unified

            
            unifier[1+(2*m)]   = i;
            unifier[1+(2*m)+1] = j;
            memcpy(&unifiers[last_unifier*unifier_size],unifier,unifier_size*sizeof(unsigned));
            last_unifier++;
        }
    }

    // Free the extra space
    // unifiers = realloc(unifiers,last_unifier*unifier_size*sizeof(int));
    free(unifier);
    return last_unifier;
}



/**
 * @brief Applies a unifier to merge left and right main_terms into a resulting main_term
 * @param mt1       Left main_term
 * @param mt2       Right main_term
 * @param mt3       Destination main_term to fill
 * @param unifier   Array with format [n_subs, x1,y1, x2,y2, …]
 * @return void
 *
 * Copies mt1->row into mt3->row. By the nature of unification, applying the unifier to any row gives same result
 * This is optimal to handle new 'fake' columns
 * Iterates each binding (x←y):
 *   - If y is constant, replaces all x references with y
 *   - If y is variable, updates variable occurrences using a dictionary
 * Clears dictionary after application
 */
void apply_unifier_left(main_term *mt1, main_term *mt2, main_term *mt3, unsigned *unifier){
    
    const unsigned n = unifier[0]*2;
    const unsigned c1 = mt1->c;
    const unsigned c2 = mt2->c;
    const unsigned m  = mt3->c; 
    unsigned i, j;
    unsigned x, y; 
    int val_y; // To perform (x<-y), x is ALWAYS a variable
    bool same_row = false;

    // Stuff needed if y is a variable
    int length = (int)log10(2*m) + 2;
    char *y_str = (char *)malloc(length * sizeof(char));
    
    // Copy the left row to the result row
    memcpy(mt3->row, mt1->row, c1*sizeof(int));
    int *row_a = mt3->row;
    const int *row_b = mt2->row;

    for (i = 1; i < n; i+=2)
    {
        x = unifier[i];
        y = unifier[i+1];
        
        if (x < m) // Only apply changes to left row
        {
            if (y < m) {val_y = row_a[y]; same_row=true;}
            // else if (y < m) val_y = 0; // don't need this since row_a now is of size m, with the new columns already set to 0 on creation
            else if ((y-m) < c2) val_y = row_b[y-m];
            else val_y = 0;

            if (val_y > 0) // y is a constant, substitute all x references for the constant in y
            {
                row_a[x] = val_y;
                for (j=0;j<m;j++) if (row_a[j]==(int)(-(x+1))) row_a[j] = val_y;
            }
            else // y is a variable, so it can get tricky
            {
                snprintf(y_str, length, "%d", y);
                struct nlist *entry = lookup(unif_dict,y_str);
                if (entry==NULL) // First appearance of y, do not substitute anything, but add appearance of y linked with x
                {
                    // If some variable is substituted by a variable previous to itself, need to put it as reference 
                    if (same_row && x>y) // in this case we interchange x and y
                    {
                        row_a[x] = -(y+1); 
                        same_row=false;
                        snprintf(y_str, length, "%d", x);
                        install(unif_dict,y_str,y);
                    } 
                    else if (same_row)
                    {
                        row_a[y] = -(x+1);
                        same_row=false;
                        install(unif_dict,y_str,x);
                    }
                    else install(unif_dict,y_str,x);
                }
                else // Not first appearance: need to point all x references to previous (x<-y) [effectively (x<-(-z))]
                {
                    int z = entry->defn;
                    row_a[x] = -(z+1);
                    for (j=0;j<m;j++) if (row_a[j]==(int)(-(x+1))) row_a[j] = -(z+1);
                }
            }
            same_row = false;
        }
    }

    clear(unif_dict); // Clean dictionary
    free(y_str);
}


/**
 * @brief Reorders variables in a unified main_term to 'canonical' column order, same order as the test file one
 * @param mt        Pointer to unified main_term
 * @param ms        Schema containing common_L mapping
 * @return void
 *
 * Detects duplicates, adjusts backward references.
 * Performs necesary passes to correct chained references
 * Writes reordered values back into mt->row
 */
void reorder_unified(main_term *mt, mgu_schema *ms)
{
    // This can be handy since the main term needs to be the resulting main term, not the operand. This assertion does not guarantee that though
    unsigned c = mt->c;
    assert(c==ms->n_common);

    // Reordering is needed, taking a look at common_L on ms and updating reference indices
    int *after = (int*)malloc(c*sizeof(int));
    int *before = mt->row;
    int *before_after = (int*)malloc(c*sizeof(int));
    memset(before_after,-1,c*sizeof(int));

    bool *duplicated = (bool*)malloc(c*sizeof(bool));
    memset(duplicated,0,c*sizeof(bool));

    // Need a first pass to correct the state that apply_unifier_left ended in
    for (unsigned i = 0; i < c; i++)
    {
        int ref = before[i];
        unsigned reference = -(ref+1);
        if (ref<0 && before[reference]!=0)
        {
            before[i] = before[reference];
        }
    }

    for (unsigned i_after = 0; i_after < c; i_after++)
    {
        unsigned i_before = ms->common_L[i_after]-1;
        // If the before column appears more than once in ms.common_L
        if (before_after[i_before] != -1) 
        {
            if (before[i_before]>0) after[i_after] = before[i_before]; // If the column is a constant, just copy the constant
            else                    after[i_after] = -(before_after[i_before]+1); // If not, reference first appearance
            duplicated[i_after] = true;
        }
        // if not, just reorder
        else
        {
            after[i_after] = before[i_before];
            before_after[i_before] = i_after;
        } 
    }
    
    int *after2 = (int*)malloc(c*sizeof(int));
    memcpy(after2,after,c*sizeof(int));
    

    // Need an additional pass to correct new reference index positions
    for (unsigned i = 0; i < c; i++)
    {
        int ref = after[i];
        
        // Only if it is a reference to a variable or constant
        if (ref<0)
        {
            // If this column was duplicated, it is already correct
            if (duplicated[i]) continue;
            unsigned reference = -(ref+1); // Get the index it is pointing to, this index is in the before array
            unsigned current_idx = (unsigned)before_after[reference]; // With this we know the corresponding index in the after array
            // When reordering, the first appearance can move to the right of a reference
            if (current_idx < i) {after2[i] = -(current_idx+1);} // The first appearance is still the first appearance
            else // The first appearance became a reference                 
            {
                // In this case, the swap happened before (multiple references), point to the first appearance
                if (after2[current_idx] < 0 ) after2[i] = after2[current_idx];
                // In this case, it is the first swap, make the reference the first appearance and viceversa
                else {after2[current_idx] = -(i+1); after2[i] = 0;}
            }
        }
    }

    // Need a last pass to correct indices, same logic as the first pass, but now with the corrected reference indices
    for (unsigned i = 0; i < c; i++)
    {
        int ref = after2[i];
        unsigned reference = -(ref+1);
        if (ref<0 && after2[reference] != 0)
        {
            after2[i] = after2[reference];
        }
    }

    memcpy(before,after2,c*sizeof(int));
    free(after);
    free(after2);
    free(before_after);
    free(duplicated);
    return;
}

/**
 * @brief Performs intersection of two operand blocks against a reference result block, measures timing
 * @param ob1       Pointer to first operand_block
 * @param ob2       Pointer to second operand_block
 * @param rb        Pointer to reference result_block containing expected results
 * @return void     Prints timing and correctness to stdout/stderr if verbose; updates global timing counters
 *
 * Steps:
 *   1. Record start time for unifier computation
 *   2. Allocate buffer for all unifiers: size = ob1->r * ob2->r * (1+2*rb->c+2)
 *   3. Call unifier_matrices() to populate unifiers and count
 *   4. Record end time for unifier computation
 *   5. Record start time for unification application
 *   6. Create an empty result_block my_rb sized ob1->r×ob2->r (worst-case)
 *   7. For each computed unifier:
 *        a. Extract operand indices i,j
 *        b. Initialize a new main_term in my_rb at index i*my_rb.r2+j
 *        c. apply_unifier_left() then reorder_unified() to build unified term
 *        d. Mark my_rb.valid for that position as unified
 *   8. Record end time for unification application
 *   9. Compare my_rb against rb via compare_results(); update global_correct counts
 *  10. Accumulate elapsed times into global timing variables
 *  11. Free allocated temporary result_block
 */
void matrix_intersection(operand_block *ob1, operand_block *ob2, result_block *rb){

    struct timespec start_unifiers, start_unification;
    struct timespec end_unifiers, end_unification;     
    struct timespec elapsed, elapsed2;   
    
    // ----- Calculate unifiers start ----- //
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_unifiers);
    
	unsigned *unifiers = NULL;
    unsigned unifier_size = 1+(2*rb->c)+2;
    unifiers = (unsigned*) malloc (ob1->r*ob2->r*unifier_size*sizeof(unsigned));
    unsigned unif_count = unifier_matrices(ob1, ob2, rb, unifiers);

    clock_gettime(CLOCK_MONOTONIC_RAW, &end_unifiers);
    // ----- Calculate unifiers end ----- //
    
    // ----- Perform unification start ----- //
    if (verbose) printf("\tApplying all unifiers . . . \n");
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_unification);
    result_block my_rb = create_empty_result_block(ob1->r,ob2->r,ob1->c,ob2->c,rb->c,rb->ms);
    my_rb.t1 = 1;
    my_rb.t2 = 1;
    unsigned i, ind_A, ind_B;
    for (i=0; i<my_rb.r; i++) my_rb.valid[i] = 2;
    for (i=0; i<unif_count; i++)
    {
        ind_A = unifiers[i*unifier_size+unifier_size-2];
        ind_B = unifiers[i*unifier_size+unifier_size-1];
        unsigned index_mt = ind_A*my_rb.r2+ind_B;

        main_term *mt = &my_rb.terms[index_mt];
        *mt = create_empty_main_term(my_rb.c, ob1->terms[ind_A].e + ob2->terms[ind_B].e);
        apply_unifier_left(&ob1->terms[ind_A], &ob2->terms[ind_B], mt, &unifiers[i*unifier_size]);
        mgu_schema *schema_holder = rb->lineal_lineal ? rb->ms : rb->terms[index_mt].ms;
        reorder_unified(mt, schema_holder);
        my_rb.valid[index_mt] = 0;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &end_unification);

    if (verbose) printf("\tApplied all unifiers\n");
    // ----- Perform unification end ----- //

    // ----- Check correctness start ---- //
    if (verbose) printf("\tComparing unification results. . . \n");
    int same_int = compare_results(&my_rb,rb,ob1,ob2);
    if (!same_int) {global_correct = false; global_incorrect++; }
    global_count++;

    if (same_int  && verbose) printf("Unification is correct :)\n");
    if (!same_int && verbose) printf("Unification is NOT correct :(\n");
    // ----- Check correctness end   ---- //

    // Accumulate the times
    timespec_subtract(&elapsed, &end_unifiers, &start_unifiers);
    timespec_add(&unifiers_elapsed, &unifiers_elapsed, &elapsed);

    timespec_subtract(&elapsed2, &end_unification, &start_unification);
    timespec_add(&unification_elapsed, &unification_elapsed, &elapsed2);

    // Free memory
    free(unifiers);
    free_result_block(&my_rb);
    
}
// --------------------- CORE END --------------------- //

/**
 * @brief Entry point: reads three matrix CSVs, performs intersection tests, reports timing and correctness
 * @param argc      Argument count; expects 4 or 5 (any more than 5 will be irrelevant, but no error)
 * @param argv      Argument array: argv[1]=M1 file, argv[2]=M2 file, argv[3]=M3 file, optional argv[4] enables verbose
 * @return int      Exit code: 0 on normal completion; exits on file errors or parsing failures
 *
 * Sequence:
 *   1. Initialize global timing and dictionaries
 *   2. Open input files
 *   3. Read block counts s1,s2 via read_num_blocks() and print if verbose
 *   4. Allocate arrays obs1[s1], obs2[s2]
 *   5. Measure and invoke read_operand_block() to populate obs1 and obs2
 *   6. Loop: read_result_block() to obtain rb; break on t1==0 (meaning no more result_blocks. Computing how many in the CSV beforehand is expensive)
 *        a. Measure read time
 *        b. print rb if verbose
 *        c. Call matrix_intersection() on selected operand blocks and rb
 *        d. free_result_block(rb)
 *   7. Record total end time
 *   8. Print verbose timing breakdown if enabled
 *   9. Print summary: global_correct status, counts, timing metrics
 */
int main(int argc, char *argv[]){
    struct timespec start_total, start_reading;
    struct timespec end_total, end_reading;     
    struct timespec elapsed;         

    clock_gettime(CLOCK_MONOTONIC_RAW, &start_total);

    var_dict = create_dictionary(501);
    unif_dict = create_dictionary(501);

    char *M1_file = argv[1];
    char *M2_file = argv[2];
    char *M3_file = argv[3];
    if (argc>4) verbose = 1;

    // Open the files and check so
    FILE *stream_M1 = fopen(M1_file, "r");
    FILE *stream_M2 = fopen(M2_file, "r");
    FILE *stream_M3 = fopen(M3_file, "r");

    if (!stream_M1 || !stream_M2 || !stream_M3) {
        fprintf(stderr, "Error opening files %s, %s and %s; exiting\n", M1_file, M2_file, M3_file);
        if (stream_M1) fclose(stream_M1);
        if (stream_M2) fclose(stream_M2);
        if (stream_M3) fclose(stream_M3);
        exit(EXIT_FAILURE);
    }

    // Read number of blocks in M1 and M2
    unsigned s1, s2;
    if (read_num_blocks(stream_M1,&s1)) fprintf(stderr, "Could not read number of blocks in %s\n",M1_file);
    if (read_num_blocks(stream_M2,&s2)) fprintf(stderr, "Could not read number of blocks in %s\n",M2_file);
    printf("M1 blocks %u, M2 blocks %u, ",s1,s2); // Check

    operand_block *obs1 = (operand_block*)malloc(s1*sizeof(operand_block));
    operand_block *obs2 = (operand_block*)malloc(s2*sizeof(operand_block));
    result_block rb;

    clock_gettime(CLOCK_MONOTONIC_RAW, &start_reading);

    // ----- Read file start ----- //
    for (size_t i = 0; i < s1; i++)
    {
        obs1[i] = read_operand_block(stream_M1);
    }

    for (size_t i = 0; i < s2; i++)
    {
        obs2[i] = read_operand_block(stream_M2);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &end_reading);
    timespec_subtract(&read_file_elapsed, &end_reading, &start_reading);    

    // ----- Read file end ----- //

    // ----- Matrix intersection start ----- //
    do {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_reading);
            rb = read_result_block(stream_M3);
        clock_gettime(CLOCK_MONOTONIC_RAW, &end_reading);
        if (rb.t1)
        {
            // verbose = true; // Check
            timespec_subtract(&elapsed, &end_reading, &start_reading);    
            timespec_add(&read_file_elapsed, &read_file_elapsed, &elapsed);
            if (verbose) print_result_block(&rb,0);
            matrix_intersection(&obs1[rb.t1-1],&obs2[rb.t2-1],&rb);
            free_result_block(&rb);
        }
        else break;
    } while (true);

    // ----- Matrix intersection end ----- //

    clock_gettime(CLOCK_MONOTONIC_RAW, &end_total);

    if (verbose) printf("-------- TIME MEASUREMENTS --------\n");
    if (verbose) printf("Time for reading from file:    %ld.%0*ld sec\n",read_file_elapsed.tv_sec, 9, read_file_elapsed.tv_nsec);
    if (verbose) printf("Time for calculating unifiers: %ld.%0*ld sec\n",unifiers_elapsed.tv_sec, 9, unifiers_elapsed.tv_nsec);
    if (verbose) printf("Time for applying unifiers:    %ld.%0*ld sec\n",unification_elapsed.tv_sec, 9, unification_elapsed.tv_nsec);

    if (global_correct) printf("OK, ");
    else printf("Not OK, ");
    printf("%u/%u, ", global_incorrect, global_count);
    printf("%ld.%0*ld, %ld.%0*ld, %ld.%0*ld, ",read_file_elapsed.tv_sec, 9, read_file_elapsed.tv_nsec,unifiers_elapsed.tv_sec, 9, unifiers_elapsed.tv_nsec,unification_elapsed.tv_sec, 9, unification_elapsed.tv_nsec);

    timespec_add(&unifiers_elapsed, &unifiers_elapsed, &unification_elapsed);
    if (verbose) printf("Time for unification total:    %ld.%0*ld sec\n",unifiers_elapsed.tv_sec, 9, unifiers_elapsed.tv_nsec);
    printf("%ld.%0*ld, ",unifiers_elapsed.tv_sec, 9, unifiers_elapsed.tv_nsec);
    timespec_subtract(&elapsed, &end_total, &start_total);
    if (verbose) printf("Total time:                    %ld.%0*ld sec\n",elapsed.tv_sec, 9, elapsed.tv_nsec);
    printf("%ld.%0*ld\n",elapsed.tv_sec, 9, elapsed.tv_nsec);

    // Free memory
        // Free operand blocks
        for (size_t i = 0; i < s1; i++) {
            free_operand_block(&obs1[i]);
        }
        for (size_t i = 0; i < s2; i++) {
            free_operand_block(&obs2[i]);
        }
    free(obs1);
    free(obs2);

    // Free dictionaries
    free_dictionary(var_dict);
    free_dictionary(unif_dict);

    fclose(stream_M1);
    fclose(stream_M2);
    fclose(stream_M3);
    
	return 0;
}