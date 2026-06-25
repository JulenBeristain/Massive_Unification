/**
 * @file core.c
 * @brief Entry point and core logic for the massive unification system.
 *
 * Responsibilities:
 *   - CSV parsing for operand matrices (M1, M2) and result matrices (M3)
 *   - Unifier computation (unifier_rows, correct_unifier, unifier_matrices)
 *   - Unifier application (apply_unifier_left, reorder_unified)
 *   - Pairwise block intersection and correctness verification (matrix_intersection)
 *   - Timing and summary reporting (main)
 *
 * Compile-time flags:
 *   -DCOM or -DAGT  Select the symbol set whose perfect hash is compiled in.
 *   -DNO_PERF_HASH  Disable the perfect hash and fall back to a runtime dictionary.
 *   -DDEBUG         Enable extra diagnostic helpers (print_L2_lst, print_mapping,
 *                   print_mat_values, print_unifier_list, compare_mgus).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>

#include "dictionary.h"
#include "structures.h"
#include "Schemas/schemas.h"
#include "Schemas/arena.h"
#include "Schemas/cache_mapping.h"
#include "Schemas/utils.h"

/* Buffer large enough to hold any int printed as a decimal string. */
#define ROW_STR_SIZE (snprintf(NULL, 0, "%d", INT_MAX) + 1)

/* ------------------------------------------------------------------
 * Globals
 * ------------------------------------------------------------------ */

/** Maps CSV variable names to their column indices during parsing. Cleared per row. */
static Dictionary *var_dict;

/** Maps variable column indices (as strings) to substituted indices during unifier application. */
static Dictionary *unif_dict;

/** Runtime symbol-to-id dictionary used when the perfect hash is disabled. */
static Dictionary *symbols_to_ids;

/** When non-zero, print verbose diagnostic and timing information to stdout. */
static int verbose = 0;

/** Accumulated elapsed time for file I/O, unifier computation, and unification application. */
static struct timespec 
    read_file_elapsed, 
    unifiers_elapsed_l, unification_elapsed_l, 
    unifiers_elapsed_nl, unification_elapsed_nl, 
    unification_elapsed_le,
    unification_elapsed_lm,
    unification_elapsed_nle,
    unification_elapsed_nlm;

static inline void reset_global_timers(){
    read_file_elapsed = (struct timespec){}; 
    unifiers_elapsed_l = (struct timespec){};
    unification_elapsed_l = (struct timespec){};
    unifiers_elapsed_nl = (struct timespec){};
    unification_elapsed_nl = (struct timespec){};
    unification_elapsed_le = (struct timespec){};
    unification_elapsed_lm = (struct timespec){};
    unification_elapsed_nle = (struct timespec){};
    unification_elapsed_nlm = (struct timespec){};
}

/** Set to false as soon as any result block comparison fails. */
static bool global_correct = true;

/** Total number of matrix block intersections performed. */
static unsigned global_count = 0;

/** Number of intersections whose computed result differed from M3. */
static unsigned global_incorrect = 0;


/**
 * @brief Adds two timespec values: result = t1 + t2.
 * Normalises tv_nsec to < 1 000 000 000.
 */
static void timespec_add(struct timespec *result,
                         const struct timespec *t1,
                         const struct timespec *t2) {
    result->tv_sec  = t1->tv_sec  + t2->tv_sec;
    result->tv_nsec = t1->tv_nsec + t2->tv_nsec;
    if (result->tv_nsec >= 1000000000L) {
        result->tv_sec++;
        result->tv_nsec -= 1000000000L;
    }
}

/* ================================================================== */
/* Utility / debug helpers                                             */
/* ================================================================== */

/*
 * The functions below are compiled only when -DDEBUG is passed.
 * They are never called in a normal build; the guard keeps compilers
 * from emitting unused-function warnings in release builds.
 */
#ifdef DEBUG

/**
 * @brief Prints the internal L2 substitution list as a table, for debugging.
 * @param lst  Array of 2×n L2 entries (columns 0..n-1 from M1, n..2n-1 from M2).
 * @param n    Number of variable pairs (half the array length).
 *
 * Outputs three header rows (ind, count, by) and one row per depth level of
 * the L3 chains.
 */
static void print_L2_lst(L2 *lst, unsigned n) {
    /* Determine the maximum L3 chain depth. */
    unsigned max_depth = 0;
    for (unsigned i = 0; i < 2 * n; i++) {
        unsigned depth = 0;
        for (L3 *tmp = lst[i].head; tmp != NULL; tmp = tmp->next) depth++;
        if (depth > max_depth) max_depth = depth;
    }

    int *ind     = (int *)malloc(2 * n * sizeof(int));
    int *count   = (int *)malloc(2 * n * sizeof(int));
    int *by      = (int *)malloc(2 * n * sizeof(int));
    int *indices = (int *)malloc(max_depth * 2 * n * sizeof(int));
    memset(indices, -1, max_depth * 2 * n * sizeof(int));

    for (unsigned i = 0; i < 2 * n; i++) {
        ind[i]   = lst[i].ind;
        count[i] = lst[i].count;
        by[i]    = lst[i].by;
        unsigned idx = 0;
        for (L3 *tmp = lst[i].head; tmp != NULL; tmp = tmp->next)
            indices[max_depth * i + idx++] = tmp->ind;
    }

    printf("------------\n");
    printf("ind:\t");
    for (unsigned i = 0; i < 2 * n; i++) { printf("%d", ind[i]); if (i + 1 < 2 * n) printf(","); }
    printf("\ncount:\t");
    for (unsigned i = 0; i < 2 * n; i++) { printf("%d", count[i]); if (i + 1 < 2 * n) printf(","); }
    printf("\nby:\t");
    for (unsigned i = 0; i < 2 * n; i++) {
        int v = by[i];
        if (v == -1) printf("X"); else printf("%d", v);
        if (i + 1 < 2 * n) printf(",");
    }
    printf("\n");
    for (unsigned i = 0; i < max_depth; i++) {
        printf("\t");
        for (unsigned j = 0; j < 2 * n; j++) {
            int v = indices[max_depth * j + i];
            if (v == -1) printf("X"); else printf("%d", v);
            if (j + 1 < 2 * n) printf(",");
        }
        printf("\n");
    }

    free(ind); free(count); free(by); free(indices);
}

/**
 * @brief Prints a flat n×m integer matrix row by row, for debugging.
 * @param mat  Row-major integer array of size n×m.
 */
static void print_mat_values(int *mat, int n, int m) {
    for (int i = 0; i < n; i++) {
        printf("[");
        for (int j = 0; j < m; j++) printf("%d ", mat[i * m + j]);
        printf("]\n");
    }
}

/**
 * @brief Prints n_tl column-mapping pairs in "X-Y" format, for debugging.
 * @param map  Array of 2×n_tl unsigned values: [X0,Y0, X1,Y1, …].
 *             A value of 0 is printed as "_" (fresh variable placeholder).
 */
static void print_mapping(unsigned n_tl, unsigned *map) {
    for (unsigned i = 0; i < n_tl; i++) {
        unsigned left = map[2 * i], right = map[2 * i + 1];
        if (left  == 0) printf("_"); else printf("%u", left);
        printf("-");
        if (right == 0) printf("_"); else printf("%u", right);
        if (i + 1 < n_tl) printf(",");
    }
    printf("\n");
}

/**
 * @brief Prints one unifier vector, for debugging.
 * @param unifier  Array with layout [n_subs, x0,y0, …, x(m-1),y(m-1), idxA, idxB].
 *                 n_subs is the number of actual substitutions; idxA/idxB are
 *                 the 0-based operand row indices (printed as 1-based).
 * @param m        Number of result columns (determines where idxA/idxB are stored).
 */
static void print_unifier(unsigned *unifier, unsigned m) {
    unsigned n_elem = unifier[0] * 2;
    unsigned idxA   = unifier[1 + 2 * m];
    unsigned idxB   = unifier[1 + 2 * m + 1];
    printf("%u elements: [", n_elem);
    for (unsigned i = 0; i < n_elem; i += 2)
        printf("%u<-%u,", unifier[1 + i], unifier[1 + i + 1]);
    printf("],\t\tidxA: %u, idxB: %u\n", idxA + 1, idxB + 1);
}

/**
 * @brief Prints all unifiers in the array, followed by the total count.
 * @param unifier_size  Number of unsigned slots per unifier entry (= 1 + 2*m + 2).
 */
static void print_unifier_list(unsigned *unifiers, unsigned unif_count, unsigned m) {
    unsigned unifier_size = 1 + 2 * m + 2;
    for (unsigned i = 0; i < unif_count; i++)
        print_unifier(&unifiers[i * unifier_size], m);
    printf("Number of unifiers: %u\n", unif_count);
}

/**
 * @brief Prints one row of m integers as "[x0,x1,…]", for debugging.
 * Used by compare_mgus().
 */
static void print_mat_line(int *row, int m) {
    printf("[");
    for (int j = 0; j < m; j++) printf("%d,", row[j]);
    printf("]\n");
}

#endif /* DEBUG */

/* ================================================================== */
/* Comparison helpers (used for result verification)                   */
/* ================================================================== */

/**
 * @brief Compares two main_term structures for equality.
 *
 * Checks column count, exception count, row values, and each exception block's
 * matrix contents in order.
 *
 * @return 1 if equal; 0 otherwise (also prints a diagnostic when verbose).
 */
static int compare_main_terms(main_term *mt1, main_term *mt2) {
    if (mt1->c != mt2->c || mt1->e != mt2->e) {
        if (verbose) printf("compare_main_terms: dimension mismatch\n");
        return 0;
    }
    for (unsigned i = 0; i < mt1->c; i++) {
        if (mt1->row[i] != mt2->row[i]) {
            if (verbose) {
                printf("compare_main_terms: row mismatch at column %u\n", i);
                printf("my_rb:\t"); print_main_term(mt1, 3, 0);
                printf("rb   :\t"); print_main_term(mt2, 3, 0);
            }
            return 0;
        }
    }
    for (unsigned i = 0; i < mt1->e; i++) {
        unsigned n = mt1->exceptions[i].n;
        unsigned m = mt1->exceptions[i].m;
        for (unsigned j = 0; j < n; j++) {
            for (unsigned k = 0; k < m; k++) {
                if (mt1->exceptions[i].mat[j * m + k] != mt2->exceptions[i].mat[j * m + k]) {
                    printf("compare_main_terms: exception mismatch at block %u, row %u, col %u\n",
                           i, j, k);
                    printf("my_rb:\t"); print_exception_block(&mt1->exceptions[i], 3, i + 1);
                    printf("rb   :\t"); print_exception_block(&mt2->exceptions[i], 3, i + 1);
                    return 0;
                }
            }
        }
    }
    return 1;
}

/**
 * @brief Compares the computed result_block @p rb1 against the reference @p rb2.
 *
 * Verifies all dimension fields, then checks each valid slot's main_term.
 * Operand blocks @p ob1 and @p ob2 are used only for verbose diagnostics.
 *
 * @return 1 if all fields match; 0 otherwise.
 */
static int compare_results(result_block *rb1, result_block *rb2,
                            operand_block *ob1, operand_block *ob2) {
    if (rb1->c1 != rb2->c1 || rb1->c2 != rb2->c2 || rb1->c  != rb2->c ||
        rb1->r1 != rb2->r1 || rb1->r2 != rb2->r2 || rb1->r  != rb2->r) {
        if (verbose) {
            printf("compare_results: dimension mismatch\n");
            printf("computed: c1=%u c2=%u c=%u r1=%u r2=%u r=%u t1=%u t2=%u\n",
                   rb1->c1, rb1->c2, rb1->c, rb1->r1, rb1->r2, rb1->r, rb1->t1, rb1->t2);
            printf("expected: c1=%u c2=%u c=%u r1=%u r2=%u r=%u t1=%u t2=%u\n",
                   rb2->c1, rb2->c2, rb2->c, rb2->r1, rb2->r2, rb2->r, rb2->t1, rb2->t2);
        }
        return 0;
    }
    for (unsigned i = 0; i < rb1->r; i++) {
        if (rb1->valid[i] != rb2->valid[i]) {
            if (verbose) {
                printf("compare_results: validity mismatch at term %u (%u-%u): "
                       "computed=%u expected=%u\n",
                       i, i / rb1->r2 + 1, i % rb1->r2 + 1,
                       rb1->valid[i], rb2->valid[i]);
            }
            return 0;
        }
        /* Only unified terms (valid==0) have populated main_term data. */
        if (rb1->valid[i] == 0) {
            if (!compare_main_terms(&rb1->terms[i], &rb2->terms[i])) {
                if (verbose) {
                    printf("compare_results: main_term mismatch at term %u (%u-%u)\n",
                           i, i / rb1->r2 + 1, i % rb1->r2 + 1);
                    printf("mt1:\t"); print_main_term(&ob1->terms[i / rb1->r2], 1, 1);
                    printf("mt2:\t"); print_main_term(&ob2->terms[i % rb1->r2], 2, 1);
                    printf("computed:\t"); print_main_term(&rb1->terms[i], 3, 0);
                    printf("expected:\t"); print_main_term(&rb2->terms[i], 3, 0);
                    if (rb2->ms) print_mgu_schema(rb2->ms);
                    if (rb2->ms) print_mgu_compact(rb2->ms);
                }
                return 0;
            }
        }
    }
    return 1;
}


/* ================================================================== */
/* CSV parsing                                                         */
/* ================================================================== */

/**
 * @brief Reads the block-count header line from a matrix file.
 *
 * Expects a line matching "%%% BEGIN: Matrix MX (S) %%%" and a following
 * "Free variables" line.  Stores the block count in @p s.
 *
 * @return 0 on success; 1 on failure.
 */
int read_num_blocks(FILE *stream, unsigned *s) {
    char  *line = NULL;
    size_t len  = 0;

    if (getline(&line, &len, stream) == -1)   { free(line); return 1; }
    if (!strstr(line, "BEGIN"))               { free(line); return 1; }

    char *e = strchr(line, '(');
    if (!e) { free(line); return 1; }
    char *endptr;
    long num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1) { free(line); return 1; }
    *s = (unsigned)num;

    free(line);
    return 0;
}

/**
 * @brief Reads a "% BEGIN: Matrix subsetX.Y (n,m)" header and extracts n and m.
 * Exits on any parse error.
 */
void read_dimensions(FILE *stream, unsigned *n, unsigned *m) {
    char  *line = NULL;
    size_t len  = 0;
    char  *e, *endptr;
    long   num;

    getline(&line, &len, stream);
    if (!strstr(line, "BEGIN")) {
        fprintf(stderr, "read_dimensions: expected BEGIN header\n");
        exit(EXIT_FAILURE);
    }

    e = strchr(line, '(');
    if (!e) { fprintf(stderr, "read_dimensions: missing '('\n"); exit(EXIT_FAILURE); }
    num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1) { fprintf(stderr, "read_dimensions: could not parse n\n"); exit(EXIT_FAILURE); }
    *n = (unsigned)num;

    e = strchr(endptr, ',');
    if (!e) { fprintf(stderr, "read_dimensions: missing ','\n"); exit(EXIT_FAILURE); }
    num = strtol(e + 1, &endptr, 10);
    if (endptr == e + 1) { fprintf(stderr, "read_dimensions: could not parse m\n"); exit(EXIT_FAILURE); }
    *m = (unsigned)num;

    free(line);
}

size_t next_symbol_id = FIRST_FUNCTION_SYMBOL_POSITIVE;

static inline bool is_original_variable(char *tok) {
    // NOTE: important to not consider prefix's NULL terminator for len.
    char *prefix = "$ORIG_";
    unsigned prefix_len = 6;
    return strncmp(tok, prefix, prefix_len) == 0;
}

/**
 * @brief Parses one CSV data line into an integer row, resolving constants and variables.
 *
 * Encoding conventions:
 *   - Constant token (starts with lowercase or digit): looked up in the symbol table;
 *     its positive integer id is stored in row[col].
 *   - Variable token (starts with uppercase): first occurrence → row[col] = 0;
 *     subsequent occurrences → row[col] = -(first_occurrence_column).
 *
 * @param line        Modifiable string of comma-separated tokens.
 * @param row         Output integer array (caller-allocated, length = number of tokens).
 * @param skip_first  When true, the first token (exception count) is skipped before
 *                    row values are read.
 */
void read_line(char *line, int *row, bool skip_first) {
    char *tok = strtok(line, ",");
    if (skip_first) tok = strtok(NULL, ",\n");

    int col = 1;
    clear(var_dict);
    while (tok) {
        if (!isupper(tok[0])) {
            /* Constant symbol: resolve to a positive integer id. */
            struct nlist *token_id = lookup(symbols_to_ids, tok);
            if (token_id) {
                row[col - 1] = token_id->defn;
            } else {
                install(symbols_to_ids, tok, (int)next_symbol_id);
                row[col - 1] = (int)next_symbol_id++;
            }

        } else {
            /* Variable: encode first occurrence as 0, repeats as negative back-reference. */
            if (lookup(var_dict, tok) == NULL) {
                row[col - 1] = is_original_variable(tok) ? FIRST_ORIGINAL_VAR_APPEARENCE : FIRST_EXTENDING_VAR_APPEARENCE;
                install(var_dict, tok, col);
            } else {
                row[col - 1] = -(lookup(var_dict, tok)->defn);
            }
        }
        tok = strtok(NULL, ",\n");
        col++;
    }
}

/**
 * @brief Parses a "X-Y,X-Y,…" mapping string into a flat unsigned array.
 *
 * "_" on either side of a pair is stored as 0 (denotes a fresh variable).
 *
 * @param line     Modifiable string.
 * @param n_pairs  Number of expected X-Y pairs.
 * @param mapping  Output array of length 2×n_pairs.
 */
static void get_mapping(char *line, unsigned n_pairs, unsigned *mapping) {
    char    *tok   = strtok(line, " ,\n");
    unsigned count = 0;
    while (tok != NULL && count < n_pairs * 2) {
        char *dash = strchr(tok, '-');
        if (dash) {
            *dash = '\0';
            mapping[count++] = (strcmp(tok,     "_") == 0) ? 0 : (unsigned)atoi(tok);
            mapping[count++] = (strcmp(dash + 1,"_") == 0) ? 0 : (unsigned)atoi(dash + 1);
        }
        tok = strtok(NULL, " ,\n");
    }
}

/**
 * @brief Reads @p mt->e exception blocks from @p stream into @p mt.
 *
 * For each exception block:
 *   1. Read dimensions (n, m).
 *   2. Allocate an empty exception_block.
 *   3. Skip the unflattened schema line.
 *   4. Read the mapping line and build the mgu_schema.
 *   5. Optionally skip the flattened schema (operand files only).
 *   6. Read n exception rows, adjusting for result-file format when @p result is true.
 *   7. Skip the END marker line.
 *
 * @param result  When true, input is in result-file format (extra unifier lines present).
 */
void read_exception_blocks(FILE *stream, main_term *mt, bool result) {
    char  *line = NULL;
    size_t len  = 0;

    for (unsigned i = 0; i < mt->e; i++) {
        unsigned n, m;
        read_dimensions(stream, &n, &m);

        mt->exceptions[i] = create_empty_exception_block(n, m);
        exception_block *eb = &mt->exceptions[i];

        /* Skip unflattened schema. */
        getline(&line, &len, stream);

        /* Read the column mapping. */
        getline(&line, &len, stream);
        unsigned *mapping = (unsigned *)malloc(eb->m * 2 * sizeof(unsigned));
        get_mapping(line, eb->m, mapping);
        eb->ms = create_mgu_from_mapping(mapping, eb->m, mt->c, eb->m);
        free(mapping);

        /* Operand files contain a flattened schema line; result files do not. */
        if (!result) getline(&line, &len, stream);

        for (unsigned j = 0; j < n; j++) {
            getline(&line, &len, stream);
            char *line_ptr = line;
            if (result) line_ptr = strchr(line_ptr, ':') + 2;
            read_line(line_ptr, &eb->mat[j * m], false);
            if (result) getline(&line, &len, stream); /* Skip unifier line. */
        }

        /* Skip END marker. */
        getline(&line, &len, stream);
    }

    free(line);
}

/**
 * @brief Reads one operand matrix from @p stream into @p ob.
 *
 * Expects the layout:
 *   <n_vars_with_deps>, [dep lines ×n_vars_with_deps], <flattened schema>,
 *   <main_term rows>…, "%% END"
 *
 * Exits on format errors.
 */
static void read_operand_matrix(FILE *stream, operand_block *ob, SetSchema *set_schema, SetDependencies *dependencies, Arena *arena) {
    
    // Read unflatened schema and the set of dependencies
    read_set_schema_with_dependencies(stream, set_schema, dependencies, arena);

    char   *line = NULL;
    size_t  len  = 0;

    /* Skip the flattened schema line. */
    getline(&line, &len, stream);

    /* Read main_term rows until the END marker or the expected row count is reached. */
    ssize_t read;
    unsigned row = 0;
    while ((read = getline(&line, &len, stream)) != -1 && row < ob->r) {
        if (strstr(line, "% END") || strstr(line, "% End")) break;

        /* First token is the exception-block count. */
        char *line_copy = strdup(line);
        unsigned e = (unsigned)strtoul(strtok(line_copy, ","), NULL, 10);
        free(line_copy);

        ob->terms[row] = create_empty_main_term(ob->c, e);
        read_line(line, ob->terms[row].row, true);

        if (e) read_exception_blocks(stream, &ob->terms[row], false);

        row++;
    }

    free(line);
}

/**
 * @brief Reads the content of one result matrix block from @p stream into @p rb.
 *
 * Expects the header "% BEGIN: Matrix subset t1-t2 (r1-r2,c1-c2,c)".
 *
 * Linear result blocks (both operand blocks are linear) carry a single shared
 * mgu_schema.  Non-linear blocks carry one mgu_schema per result row, stored
 * in the corresponding main_term.
 *
 * Exits on format errors.
 */
static void read_result_matrix(
    FILE *stream, 
    result_block *rb, SetSchema *common_set_schema, SetDependencies *common_dependencies, 
    Arena *arena
) {
    char  *line = NULL;
    size_t len  = 0;

    /* Read block header. */
    getline(&line, &len, stream);
    int matched = sscanf(line, "%% BEGIN: Matrix subset %u-%u (%u-%u,%u-%u,%u)",
                         &rb->t1, &rb->t2,
                         &rb->r1, &rb->r2,
                         &rb->c1, &rb->c2,
                         &rb->c);
    if (matched != 7) {
        if (strstr(line, "END: Matrix M1 & M2 + MGU")) { free(line); return; }
        fprintf(stderr, "read_result_matrix: malformed header (matched %d fields): '%s'\n",
                matched, line);
        free(line);
        exit(EXIT_FAILURE);
    }

    rb->r     = rb->r1 * rb->r2;
    rb->terms = (main_term *)malloc(rb->r * sizeof(main_term));
    rb->valid = (unsigned  *)malloc(rb->r * sizeof(unsigned));
    for (unsigned i = 0; i < rb->r; i++) rb->valid[i] = 0;

    read_set_schema_with_dependencies(stream, common_set_schema, common_dependencies, arena);

    /* Read the (possibly absent) general mapping line. */
    unsigned *mapping = (unsigned *)malloc(rb->c * 2 * sizeof(unsigned));
    getline(&line, &len, stream);
    if (strchr(line, '-')) {
        /* A general mapping is present → linear block. */
        rb->lineal_lineal = true;
        get_mapping(line, rb->c, mapping);
        rb->ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);
        /* Skip the flattened schema line. */
        getline(&line, &len, stream);
    }
    /* If no general mapping, the flattened schema has already been consumed above. */

    /* Iterate result rows. */
    unsigned row = 0;
    ssize_t  read;
    while ((read = getline(&line, &len, stream)) != -1 && row < rb->r) {
        if (strstr(line, "% END") || strstr(line, "% End")) break;

        if (rb->lineal_lineal) {
            rb->terms[row].ms = NULL;

        } else {
            /* Non-linear block: each row has its own mapping. */
            unsigned row1, row2, offset;
            if (sscanf(line, "Mapping %u-%u: %n", &row1, &row2, (int *)&offset) != 2) {
                fprintf(stderr, "read_result_matrix: missing per-row mapping in block %u-%u\n",
                        rb->t1, rb->t2);
                free(line); exit(EXIT_FAILURE);
            }
            get_mapping(line + offset, rb->c, mapping);
            rb->terms[row].ms = create_mgu_from_mapping(mapping, rb->c, rb->c1, rb->c2);
            getline(&line, &len, stream);
        }

        if (strstr(line, "subsumed by exception")) {
            rb->valid[row++] = 1;
            continue;
        }
        if (strstr(line, "not unifiable")) {
            rb->valid[row++] = 2;
            continue;
        }

        /* Unified row: read exception count, row values, and exception blocks. */
        unsigned d1, d2, e;
        if (sscanf(line, "Row %u-%u: %u",  &d1, &d2, &e) != 3 &&
            sscanf(line, "Rows %u-%u: %u", &d1, &d2, &e) != 3) {
            fprintf(stderr, "read_result_matrix: could not parse row header\n");
            free(line); exit(EXIT_FAILURE);
        }

        /* Preserve per-row ms pointer before (re-)initialising the main_term. */
        mgu_schema *ms_save  = rb->terms[row].ms;
        rb->terms[row]       = create_empty_main_term(rb->c, e);
        rb->terms[row].ms    = ms_save;

        read_line(line, rb->terms[row].row, true);
        getline(&line, &len, stream); /* Skip the unifier line. */
        if (e) read_exception_blocks(stream, &rb->terms[row], true);

        row++;
    }

    free(line);
    free(mapping);
}

/**
 * @brief Reads one complete operand block (header + matrix) from @p stream.
 * @return Populated operand_block.
 */
void read_operand_block(FILE *stream, operand_block *ob, SetSchema *set_schema, SetDependencies *dependencies, Arena *arena) {
    unsigned r, c;
    read_dimensions(stream, &r, &c);
    *ob = create_empty_operand_block(r, c);
    read_operand_matrix(stream, ob, set_schema, dependencies, arena);
}

/* ================================================================== */
/* Helper functions to read Free Variable information                 */
/* ================================================================== */

unsigned scan_num_free_vars(char *line){
    unsigned num_free_vars = 1;
    for(char *c = line; *c; ++c){
        if(*c == ','){
            ++num_free_vars;
        }
    }
    return num_free_vars;
}
ArrayListCharPtr scan_free_vars(char *line, unsigned num_free_vars, Arena *arena){
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
ArrayListCharPtr read_free_vars(FILE *stream, Arena *arena){
    char *line = NULL;
    size_t len;
    
    if (getline(&line, &len, stream) == -1) { free(line); exit(1); } // Failed to read the line
    if (strstr(line, "Free") == NULL) { free(line); exit(1); } // Not the correct line

    unsigned num_free_vars = scan_num_free_vars(line);

    // NOTE: no resizing risk
    // NOTE: another option would be to use non-arena version, malloc and define the free_pointers for arraylists of charptrs...
    ArrayListCharPtr free_vars = create_array_list_char_ptr_arena(num_free_vars, arena);
    for(char* token = strtok(line, ",\n"); token; token = strtok(NULL, ",\n")){
        char *var_str = allocate(arena, strlen(token) + 1);
        strcpy(var_str, token);
        unsafe_add_to_array_list(free_vars, var_str);
    }

    free(line);
    return free_vars;
}

/**
 * @brief Reads one complete result block from @p stream.
 *
 * On the first call, reads and skips the global matrix file header.  Subsequent
 * calls resume directly from the next block.
 *
 * @return Populated result_block, or a null result_block (t1==0) when no more
 *         blocks are available.
 */
static void read_first_result_block(
    FILE *stream, result_block *rb, 
    SetSchema *common_set_schema, SetDependencies *common_dependencies, 
    ArrayListCharPtr *free_vars,
    Arena *arena, Arena *debug_arena
) {
    // Create the operand_block structure for later populating it
    *rb = create_null_result_block();

    char *line = NULL;
    size_t len = 0;

    // Skip matrix header
    getline(&line, &len, stream);
    if (strstr(line, "% END: Matrix M1 & M2 + MGU") != NULL) {free(line);}

    *free_vars = read_free_vars(stream, debug_arena);
    
    // Read the operand block and fill the struct
    read_result_matrix(stream, rb, common_set_schema, common_dependencies, arena);

    free(line);
}
static void read_next_result_block(
    FILE *stream, result_block *rb, 
    SetSchema *common_set_schema, SetDependencies *common_dependencies, 
    Arena *arena
) {
    // Create the operand_block structure for later populating it
    *rb = create_null_result_block();

    // Read the operand block and fill the struct
    read_result_matrix(stream, rb, common_set_schema, common_dependencies, arena);
}


/* ================================================================== */
/* Unifier computation                                                 */
/* ================================================================== */

/**
 * @brief Handles one column-pair binding for unifier_rows().
 *
 * Given column indices @p indexA (in row_a) and @p indexB (in row_b), records
 * the appropriate substitution in @p unifier at position @p indexUnifier.
 *
 * Encoding in the unifier:
 *   - Columns 0..cA-1 refer to original M1 columns.
 *   - Columns cA..m-1 refer to fresh M1 columns (always variables).
 *   - Columns m..m+cB-1 (stored as index+m) refer to original M2 columns.
 *   - Columns m+cB..2m-1 (stored as index+m) refer to fresh M2 columns.
 *
 * @param row_a, row_b       Integer rows being unified.
 * @param indexA, indexB     0-based column indices into each row.
 * @param unifier            Output array; binding written at slots [1+indexUnifier] and [1+indexUnifier+1].
 * @param cA, n_cA, cB       Original and fresh column counts for row_a and row_b.
 * @return 0 on success; 1 if the pair is not unifiable (two distinct function symbols).
 */
static int unifier_a_b(int *row_a, unsigned indexA,
                       int *row_b, unsigned indexB,
                       unsigned *unifier, unsigned indexUnifier,
                       unsigned cA, unsigned n_cA, unsigned cB) {
    const unsigned m1 = cA + n_cA;

    /*
     * Columns beyond the original range are fresh variables introduced by the
     * mgu_schema. They are always treated as variables regardless of the row
     * value, so the binding is recorded immediately without inspecting the row.
     */
    if (indexA >= cA) {
        /* Fresh column on the left side → record (A←B). */
        unifier[1 + indexUnifier]     = indexA;
        unifier[1 + indexUnifier + 1] = indexB + m1;
        return 0;
    }
    if (indexB >= cB) {
        /* Fresh column on the right side → record (B←A). */
        unifier[1 + indexUnifier]     = indexB + m1;
        unifier[1 + indexUnifier + 1] = indexA;
        return 0;
    }

    const int a = row_a[indexA];
    const int b = row_b[indexB];

    if (is_symbol(a) && is_symbol(b) && a != b) return 1; /* Two distinct constants: not unifiable. */
    if (is_symbol(a) && is_variable(b)) {
        /* A is a constant, B is a variable → record (B←A). */
        unifier[1 + indexUnifier]     = indexB + m1;
        unifier[1 + indexUnifier + 1] = indexA;
    } else if (is_variable(a)) {
        /* A is a variable → record (A←B). */
        unifier[1 + indexUnifier]     = indexA;
        unifier[1 + indexUnifier + 1] = indexB + m1;
    }
    /* If both are equal constants, no substitution is needed; the slot stays {0,0}. */
    return 0;
}

/**
 * @brief Builds a raw unifier for a pair of main_term rows using their mgu_schema.
 *
 * Iterates over each common column pair defined in @p ms, calling unifier_a_b()
 * for each.  The raw unifier may contain redundant or variable-to-variable
 * bindings that are resolved by correct_unifier().
 *
 * @param unifier  Output array (caller allocates 2×ms->n_common slots starting at unifier[1]).
 * @return 0 if the rows are potentially unifiable; 1 if a constant clash was detected.
 */
static int unifier_rows(main_term *mt1, main_term *mt2,
                        mgu_schema *ms, unsigned *unifier) {
    for (unsigned i = 0; i < ms->n_common; i++) {
        /*
         * common_L/R are 1-based; subtract 1 to get the 0-based column index.
         */
        assert(ms->common_L[i] > 0 && ms->common_R[i] > 0);
        if (unifier_a_b(mt1->row, ms->common_L[i] - 1,
                        mt2->row, ms->common_R[i] - 1,
                        unifier, 2 * i, mt1->c, ms->new_a, mt2->c))
            return 1;
    }
    return 0;
}

/**
 * @brief Refines the raw unifier produced by unifier_rows() into a minimal substitution set.
 *
 * Algorithm:
 *   1. Build a union-find-like L2 array (one entry per extended column index).
 *   2. Normalise repeated-variable references so each variable is represented by
 *      its first occurrence.
 *   3. For each binding pair (x←y):
 *        - If both are constants and equal, skip.
 *        - If both are constants and distinct, return -1 (not unifiable).
 *        - If one is a constant and the other a variable, record (variable←constant)
 *          and propagate to all variables already aliased to the variable.
 *        - If both are variables, make one point to the other and merge their alias lists.
 *   4. Collect the final substitutions: for each column that is not substituted by
 *      anything but has an alias list, emit (alias←column) pairs.
 *   5. Write the substitution count into unifier[0].
 *
 * @param unifier  In/out array; layout on input: 2×n slots starting at unifier[1],
 *                 each pair (x,y) meaning "x should become y".  On output: unifier[0]
 *                 holds the number of valid substitutions, and the first 2×count slots
 *                 contain the simplified bindings.
 * @return 0 if the unifier is valid; -1 if a contradiction was found.
 */
static int correct_unifier(main_term *mt1, main_term *mt2,
                            mgu_schema *ms, unsigned *unifier) {
    const unsigned c1  = mt1->c;
    const unsigned c2  = mt2->c;
    const unsigned m   = ms->n_common;
    const int *row_a   = mt1->row;
    const int *row_b   = mt2->row;

    /*
     * The extended index space has 2*m slots:
     *   0..m-1     → extended M1 columns
     *   m..2*m-1   → extended M2 columns (stored as original_index + m in the raw unifier)
     */
    unsigned lst_length = m * 2;
    unsigned i;

    L2 *lst = (L2 *)malloc(lst_length * sizeof(L2));
    for (i = 0; i < lst_length; i++) lst[i] = create_L2_empty();

    for (i = 0; i < 2 * m; i += 2) {
        unsigned x  = unifier[1 + i];
        unsigned y  = unifier[1 + i + 1];
        int val_x, val_y;

        if (x == 0 && y == 0) continue; /* Empty / equal-constant pair. */

        /* Resolve repeated variables to their first occurrence (negative back-reference). */
        if (x < c1 && row_a[x] < 0)
            x = (unsigned)(-(row_a[x] + 1));
        else if (x >= m && (x - m) < c2 && row_b[x - m] < 0)
            x = (unsigned)(-(row_b[x - m] + 1)) + m;

        if (y < c1 && row_a[y] < 0)
            y = (unsigned)(-(row_a[y] + 1));
        else if (y >= m && (y - m) < c2 && row_b[y - m] < 0)
            y = (unsigned)(-(row_b[y - m] + 1)) + m;

        /* Follow any existing substitution pointers. */
        if (lst[x].count > 0) x = (unsigned)lst[x].by;
        if (lst[y].count > 0) y = (unsigned)lst[y].by;
        if (x == y) continue; /* Already aliased; nothing to do. */

        /* Retrieve actual values. */
        if      (x < c1)        val_x = row_a[x];
        else if (x < m)         val_x = 1;           /* Fresh M1 variable. */
        else if ((x - m) < c2)  val_x = row_b[x - m];
        else                    val_x = 1;

        if      (y < c1)        val_y = row_a[y];
        else if (y < m)         val_y = 1;
        else if ((y - m) < c2)  val_y = row_b[y - m];
        else                    val_y = 1;

        if (is_symbol(val_x) && is_symbol(val_y) && val_x != val_y) {
            /* Constant clash → not unifiable. */
            for (unsigned k = 0; k < lst_length; k++) free_L2(lst[k]);
            free(lst);
            return -1;
        }
        if (is_symbol(val_x) && is_symbol(val_y)) continue; /* Equal constants; nothing to do. */

        if (is_symbol(val_x) && is_variable(val_y)) {
            /* x is a constant, y is a variable → substitute y with x. */
            lst[y].count = 1;
            lst[y].by    = (int)x;
            lst[y].ind   = (int)y;
            /* Propagate: all variables previously pointing to y now point to x. */
            for (L3 *cur = lst[y].head; cur != NULL; cur = cur->next)
                lst[cur->ind].by = (int)x;
            /* Move y and its chain to x's alias list. */
            L3 *node = create_L3((int)y, NULL);
            if (!lst[x].head) { lst[x].head = lst[x].tail = node; }
            else              { lst[x].tail->next = node; lst[x].tail = node; }
            if (lst[y].head) { node->next = lst[y].head; lst[x].tail = lst[y].tail; }
            lst[y].head = lst[y].tail = NULL;
        } else {
            /* x is a variable (or both variables) → substitute x with y. */
            lst[x].count = 1;
            lst[x].by    = (int)y;
            lst[x].ind   = (int)x;
            for (L3 *cur = lst[x].head; cur != NULL; cur = cur->next)
                lst[cur->ind].by = (int)y;
            L3 *node = create_L3((int)x, NULL);
            if (!lst[y].head) { lst[y].head = lst[y].tail = node; }
            else              { lst[y].tail->next = node; lst[y].tail = node; }
            if (lst[x].head) { node->next = lst[x].head; lst[y].tail = lst[x].tail; }
            lst[x].head = lst[x].tail = NULL;
        }
    }

    /* Collect final substitutions from the alias lists. */
    int      last_unifier   = 0;
    unsigned n_substitutions = 0;
    for (i = 0; i < lst_length; i++) {
        if (lst[i].count == 0 && lst[i].head) {
            for (L3 *cur = lst[i].head; cur != NULL; cur = cur->next) {
                unifier[1 + last_unifier]     = (unsigned)cur->ind;
                unifier[1 + last_unifier + 1] = i;
                n_substitutions++;
                last_unifier += 2;
            }
        }
    }
    unifier[0] = n_substitutions;

    for (i = 0; i < lst_length; i++) free_L2(lst[i]);
    free(lst);
    return 0;
}

/**
 * @brief Computes all pairwise unifiers for two operand blocks.
 *
 * For each pair (i, j) of main_terms from @p ob1 and @p ob2:
 *   1. Calls unifier_rows() to produce a raw binding list.
 *   2. Calls correct_unifier() to refine it.
 *   3. On success, appends the unifier plus operand row indices (i, j) to @p unifiers.
 *
 * @param unifiers  Pre-allocated output array (worst-case: ob1->r × ob2->r entries,
 *                  each of size 1 + 2*rb->c + 2 unsigned slots).
 * @return Number of valid unifiers found.
 */
static unsigned unifier_matrices(operand_block *ob1, operand_block *ob2,
                                  result_block *rb, mgu_schema *schemas, unsigned *unifiers) {
    const unsigned m            = rb->c;
    const unsigned unifier_size = 1 + 2 * m + 2;
    unsigned       last_unifier = 0;

    unsigned *unifier = (unsigned *)malloc(unifier_size * sizeof(unsigned));

    for (unsigned i = 0; i < ob1->r; i++) {
        for (unsigned j = 0; j < ob2->r; j++) {
            memset(unifier, 0, unifier_size * sizeof(unsigned));
            unsigned index_mt    = i * rb->r2 + j;
            mgu_schema *schema   = rb->lineal_lineal ? schemas : schemas + index_mt;

            if (unifier_rows(&ob1->terms[i], &ob2->terms[j], schema, unifier) != 0)
                continue;
            if (correct_unifier(&ob1->terms[i], &ob2->terms[j], schema, unifier) != 0)
                continue;

            /* Store operand row indices at the end of the unifier slot. */
            unifier[1 + 2 * m]     = i;
            unifier[1 + 2 * m + 1] = j;
            memcpy(&unifiers[last_unifier * unifier_size], unifier,
                   unifier_size * sizeof(unsigned));
            last_unifier++;
        }
    }

    free(unifier);
    return last_unifier;
}


/* ================================================================== */
/* Unifier application                                                 */
/* ================================================================== */

/**
 * @brief Applies a unifier to produce the unified main_term @p mt3.
 *
 * Strategy:
 *   1. Copy @p mt1->row into @p mt3->row (the left operand determines the
 *      base layout; fresh columns are already zeroed by create_empty_main_term).
 *   2. For each substitution (x←y) in the unifier where x belongs to the
 *      left-side extended row:
 *        - If y is a constant: set row[x] = val_y and replace all back-references
 *          to x with the constant.
 *        - If y is a variable: use unif_dict to track the canonical representative
 *          for y; set row[x] accordingly and handle cross-row variable aliasing.
 *   3. Clear unif_dict after all substitutions.
 *
 * @param mt1     Left operand main_term.
 * @param mt2     Right operand main_term.
 * @param mt3     Output main_term (must already be allocated via create_empty_main_term).
 * @param unifier Substitution array: [n_subs, x0,y0, x1,y1, …].
 */
static void apply_unifier_left(main_term *mt1, main_term *mt2,
                                main_term *mt3, unsigned *unifier) {
    const unsigned n  = unifier[0] * 2;
    const unsigned c1 = mt1->c;
    const unsigned c2 = mt2->c;
    const unsigned m  = mt3->c;

    /* Buffer for converting a column index to its string key in unif_dict. */
    int    length = (int)log10((double)(2 * m)) + 2;
    char  *y_str  = (char *)malloc((size_t)length * sizeof(char));

    /* Copy the left row; columns c1..m-1 (fresh) remain zero. */
    memcpy(mt3->row, mt1->row, c1 * sizeof(int));
    int       *row_a = mt3->row;
    const int *row_b = mt2->row;

    for (unsigned i = 1; i < n; i += 2) {
        unsigned x = unifier[i];
        unsigned y = unifier[i + 1];
        bool same_row = false;

        if (x >= m) continue; /* x belongs to the right side; skip for left-row application. */

        int val_y;
        if (y < m)             { val_y = row_a[y]; same_row = true; }
        else if ((y - m) < c2) { val_y = row_b[y - m]; }
        else                   { val_y = 1; } /* Fresh variable on right side. */

        if (is_symbol(val_y) > 0) {
            /* y is a constant: replace x and all its back-references. */
            row_a[x] = val_y;
            for (unsigned j = 0; j < m; j++)
                if (row_a[j] == (int)(-(x + 1))) row_a[j] = val_y;
        } else {
            /* y is a variable: track via unif_dict to handle repeated occurrences. */
            snprintf(y_str, (size_t)length, "%d", y);
            struct nlist *entry = lookup(unif_dict, y_str);

            if (!entry) {
                /*
                 * First occurrence of y.  When both x and y are in the same
                 * (left) row, ensure the variable with the smaller index is
                 * the canonical representative so that later reordering is
                 * consistent.
                 */
                if (same_row && x > y) {
                    /* x > y: make x point to y (i.e. x ← -( y+1 )). */
                    row_a[x] = -(int)(y + 1);
                    snprintf(y_str, (size_t)length, "%d", x);
                    install(unif_dict, y_str, (int)y);
                } else if (same_row) {
                    /* x < y: make y point to x. */
                    row_a[y] = -(int)(x + 1);
                    install(unif_dict, y_str, (int)x);
                } else {
                    install(unif_dict, y_str, (int)x);
                }
            } else {
                /*
                 * y was already seen.  Make x point to the same canonical
                 * representative (z) as y, and update any existing references to x.
                 */
                int z    = entry->defn;
                row_a[x] = -(z + 1);
                for (unsigned j = 0; j < m; j++)
                    if (row_a[j] == (int)(-(x + 1))) row_a[j] = -(z + 1);
            }
        }
    }

    clear(unif_dict);
    free(y_str);
}

/**
 * @brief Reorders a unified main_term's row into canonical column order.
 *
 * After apply_unifier_left(), the row values are indexed relative to the
 * extended M1 layout.  This function remaps them to the result column order
 * defined by ms->common_L, fixes back-reference indices, and resolves any
 * residual chained references.
 *
 * @param mt  Unified main_term to reorder in place.
 * @param ms  Schema whose common_L array defines the before→after column mapping.
 */
static void reorder_unified(main_term *mt, mgu_schema *ms) {
    unsigned c = mt->c;
    assert(c == ms->n_common);

    int *before      = mt->row;
    int *after       = (int  *)malloc(c * sizeof(int));
    int *before_after = (int  *)malloc(c * sizeof(int));
    bool *duplicated  = (bool *)malloc(c * sizeof(bool));
    memset(before_after, -1, c * sizeof(int));
    memset(duplicated,    0,  c * sizeof(bool));

    /*
     * Pass 1: resolve single-level chained references in the before array.
     * After apply_unifier_left(), it is possible that before[i] = -( ref+1 )
     * where before[ref] != 0 (the reference target was itself substituted).
     * This pass collapses one level of indirection.
     */
    for (unsigned i = 0; i < c; i++) {
        int ref = before[i];
        if (ref < 0 && !is_var_first_appearence(before[-(ref + 1)]))
            before[i] = before[-(ref + 1)];
    }

    /*
     * Pass 2: permute columns according to common_L.
     * common_L[i] is the 1-based before-index for result column i.
     * If a before-column appears more than once, the second occurrence
     * is recorded as a reference to the first appearance in the after array.
     */
    for (unsigned i_after = 0; i_after < c; i_after++) {
        unsigned i_before = ms->common_L[i_after] - 1; /* 0-based */
        if (before_after[i_before] != -1) {
            /* Already seen: reference or copy the constant. */
            after[i_after] = is_symbol(before[i_before])
                ? before[i_before]
                : -(before_after[i_before] + 1);
            duplicated[i_after] = true;
        } else {
            after[i_after]        = before[i_before];
            before_after[i_before] = (int)i_after;
        }
    }

    /*
     * Pass 3: fix back-reference indices after the permutation.
     * A reference after[i] = -(ref+1) was valid in the before array;
     * the corresponding after-index is before_after[ref].
     * If the first appearance moved to the right of the reference, the
     * canonical position and the reference swap roles.
     */
    int *after2 = (int *)malloc(c * sizeof(int));
    memcpy(after2, after, c * sizeof(int));

    for (unsigned i = 0; i < c; i++) {
        if (after[i] >= 0 || duplicated[i]) continue;

        unsigned ref         = (unsigned)(-(after[i] + 1));
        unsigned current_idx = (unsigned)before_after[ref];

        if (current_idx < i) {
            after2[i] = -(int)(current_idx + 1);
        } else {
            if (after2[current_idx] < 0) {
                /* Already swapped in a previous iteration; follow the chain. */
                after2[i] = after2[current_idx];
            } else {
                /* First swap: make current_idx reference i, and i become the canonical 0. */
                // TODO(YA-TEST 3): check if the assertion is right, and we always have the first apparition (0 or 1) in temp...
                //  I think the code still works even if temp swaps a symbol, so the assertion could be temp >= 0 ...
                int temp = after2[current_idx];
                assert(is_var_first_appearence(temp));
                after2[current_idx] = -(int)(i + 1);
                after2[i]           = temp;
            }
        }
    }

    /*
     * Pass 4: resolve any residual single-level chained references in after2,
     * using the same logic as Pass 1.
     */
    for (unsigned i = 0; i < c; i++) {
        int ref = after2[i];
        if (ref < 0 && !is_var_first_appearence(after2[-(ref + 1)]))
            after2[i] = after2[-(ref + 1)];
    }

    memcpy(before, after2, c * sizeof(int));
    free(after); free(after2); free(before_after); free(duplicated);
}


/* ================================================================== */
/* Matrix intersection                                                 */
/* ================================================================== */

/**
 * @brief Computes and verifies the pairwise intersection of two operand blocks.
 *
 * Steps:
 *   1. Compute all unifiers for the ob1×ob2 pair via unifier_matrices().
 *   2. Apply each unifier via apply_unifier_left() + reorder_unified() to
 *      build a computed result_block.
 *   3. Compare the computed block against the reference @p rb from M3.
 *   4. Accumulate timing into the global time counters.
 *
 * @param ob1  First operand block (from M1).
 * @param ob2  Second operand block (from M2).
 * @param rb   Reference result block read from M3.
 */
static void matrix_intersection(operand_block *ob1, operand_block *ob2,
                                 result_block *rb, mgu_schema *schemas) {
    struct timespec start_unifiers, end_unifiers;
    struct timespec start_unification, end_unification;
    struct timespec elapsed, elapsed2;

    /* --- Compute unifiers --- */
    clock_gettime(CLOCK_MONOTONIC, &start_unifiers);

    unsigned unifier_size = 1 + 2 * rb->c + 2;
    unsigned *unifiers    = (unsigned *)malloc(ob1->r * ob2->r * unifier_size * sizeof(unsigned));
    unsigned  unif_count  = unifier_matrices(ob1, ob2, rb, schemas, unifiers);

    clock_gettime(CLOCK_MONOTONIC, &end_unifiers);

    /* --- Apply unifiers --- */
    if (verbose) printf("\tApplying %u unifiers…\n", unif_count);
    clock_gettime(CLOCK_MONOTONIC, &start_unification);

    result_block my_rb = create_empty_result_block(ob1->r, ob2->r,
                                                    ob1->c, ob2->c,
                                                    rb->c, rb->lineal_lineal ? schemas : NULL);
    my_rb.t1 = 1;
    my_rb.t2 = 1;
    for (unsigned i = 0; i < my_rb.r; i++) my_rb.valid[i] = 2;

    for (unsigned i = 0; i < unif_count; i++) {
        unsigned ind_A    = unifiers[i * unifier_size + unifier_size - 2];
        unsigned ind_B    = unifiers[i * unifier_size + unifier_size - 1];
        unsigned index_mt = ind_A * my_rb.r2 + ind_B;

        main_term *mt = &my_rb.terms[index_mt];
        *mt = create_empty_main_term(my_rb.c,
                                     ob1->terms[ind_A].e + ob2->terms[ind_B].e);
        apply_unifier_left(&ob1->terms[ind_A], &ob2->terms[ind_B], mt,
                           &unifiers[i * unifier_size]);

        mgu_schema *schema = rb->lineal_lineal ? schemas : schemas + index_mt;
        reorder_unified(mt, schema);
        my_rb.valid[index_mt] = 0;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_unification);
    if (verbose) printf("\tUnifiers applied\n");

    /* --- Verify correctness --- */
    if (verbose) printf("\tComparing against reference…\n");
    int correct = compare_results(&my_rb, rb, ob1, ob2);
    if (!correct) { global_correct = false; global_incorrect++; }
    global_count++;
    if (verbose) printf("Unification is %scorrect\n", correct ? "" : "NOT ");

    /* --- Accumulate timings --- */
    timespec_subtract(&elapsed, &end_unifiers, &start_unifiers);
    timespec_subtract(&elapsed2, &end_unification, &start_unification);
    if (rb->lineal_lineal) {
        timespec_add(&unifiers_elapsed_l, &unifiers_elapsed_l, &elapsed);
        timespec_add(&unification_elapsed_l, &unification_elapsed_l, &elapsed2);
    } else {
        timespec_add(&unifiers_elapsed_nl, &unifiers_elapsed_nl, &elapsed);
        timespec_add(&unification_elapsed_nl, &unification_elapsed_nl, &elapsed2);
    }

    free(unifiers);
    free_result_block(&my_rb);
}


/**
 * Has to return the position of the unifying class in the resulting row that corresponds to var, while at the
 * same time flattens the unification tree in the class partition implicit in row. Doing it by recursion removes
 * the need to handle memory manually.
 * 
 * The position is returned as a positive 0-based unsigned.
 */
static unsigned find_and_flatten(int *row, int var){
    assert(var < 0);
    unsigned old_pos = -(var + 1);
    if(row[old_pos] >= 0){
        return old_pos;
    }

    unsigned pos = find_and_flatten(row, row[old_pos]);
    row[old_pos] = -((int)(pos) + 1);

    return pos;
}

static inline unsigned find(int *row, int var) {
    assert(var < 0);
    unsigned pos = -(var + 1);
    while (row[pos] < 0) {
        pos = -(row[pos] + 1);
    }
    return pos;
}

static void matrix_intersection_with_extended_rows(
    operand_block *ob1, operand_block *ob2,
    unsigned len_extended_row, int *extended_rows, 
    result_block *rb
){
    struct timespec start_unification, end_unification, elapsed;

    /* --- Compute unifiers --- */
    // NOTE: no need to compute unifiers (column mappings)

    /* --- Apply unifiers --- */
    if (verbose) printf("\tmatrix intersection with extended rows\n");
    clock_gettime(CLOCK_MONOTONIC, &start_unification);

    unsigned num_rows1 = ob1->r;
    unsigned num_rows2 = ob2->r;
    unsigned num_rows3 = num_rows1 * num_rows2;
    result_block my_rb = { .r1 = num_rows1, .r2 = num_rows2, .r = num_rows3, .c1 = ob1->c, .c2 = ob2->c, .c = len_extended_row };
    my_rb.terms = malloc(num_rows3 * sizeof(*my_rb.terms));
    CHECK_MALLOC(my_rb.terms);
    my_rb.valid = calloc(num_rows3, sizeof(*my_rb.valid)); // NOTE: 0 - unified
    CHECK_CALLOC(my_rb.valid);

    // NOTE: with each pair of rows we calculate the resulting unified row using a partition-like data structure with path compression.

    // TODO(ORG): move arraylist of uints to another file, not in schemas.c/h

    int *extended_rows2 = extended_rows + (num_rows1 * len_extended_row);
    int *row_a = extended_rows;
    for(unsigned i = 0; i < num_rows1; ++i, row_a += len_extended_row){
        int *row_b = extended_rows2;
        for(unsigned j = 0; j < num_rows2; ++j, row_b += len_extended_row){
            
            int *resulting_row = malloc(sizeof(*resulting_row) * len_extended_row);
            CHECK_MALLOC(resulting_row);
            
            unsigned index_mt = i * my_rb.r2 + j;
            my_rb.terms[index_mt] = (main_term){ .c = len_extended_row, .row = resulting_row };

            // NOTE: unification
            unsigned k = 0;
            for(; k < len_extended_row; ++k){
                int a = row_a[k];
                int b = row_b[k];

                if (is_symbol(a) && is_symbol(b)) {
                    if (a == b) {
                        resulting_row[k] = a;
                    } else {
                        break;
                    }

                } else if (is_symbol(a) || is_symbol(b)) {
                    int sym, var;
                    if (is_symbol(a)) {
                        sym = a;
                        var = b;
                    } else {
                        sym = b;
                        var = a;
                    }

                    if (is_var_first_appearence(var)) {
                        resulting_row[k] = sym;
                    } else {
                        unsigned pos = find_and_flatten(resulting_row, var);
                        int class = is_symbol(resulting_row[pos]) ? resulting_row[pos] : -((int)(pos) + 1);

                        if (is_symbol(class)) {
                            if (sym != class) {
                                break;
                            }
                        } else {
                            resulting_row[-(class + 1)] = sym;
                        }

                        resulting_row[k] = sym;
                    }
                    
                } else {
                    // NOTE: both are variables
                    if (is_var_first_appearence(a) && is_var_first_appearence(b)) {
                        resulting_row[k] = 0;

                    } else if (is_var_first_appearence(a) || is_var_first_appearence(b)) {
                        int repeated = is_var_repeated_appearence(a) ? a : b;
                        unsigned pos = find_and_flatten(resulting_row, repeated);
                        
                        resulting_row[k] = -((int)(pos) + 1);
                    
                    } else {
                        unsigned pos_a = find_and_flatten(resulting_row, a);
                        int class_a = is_symbol(resulting_row[pos_a]) ? resulting_row[pos_a] : -((int)(pos_a) + 1);

                        unsigned pos_b = find_and_flatten(resulting_row, b);
                        int class_b = is_symbol(resulting_row[pos_b]) ? resulting_row[pos_b] : -((int)(pos_b) + 1);

                        if (is_symbol(class_a) && is_symbol(class_b)) {
                            if (class_a == class_b) {
                                // NOTE: no need to change references, because both partitions' root is already a function symbol
                                resulting_row[k] = class_a;
                            } else {
                                break;
                            }

                        } else {
                            // NOTE: the variable that appears first --> lowest column in absolute value --> greatest number
                            // NOTE: the case where one of the two classes is a function symbol is treated in the same way too.
                            if (class_a > class_b) {
                                resulting_row[k] = class_a;
                                resulting_row[pos_b] = class_a;
                            } else {
                                resulting_row[k] = class_b;
                                resulting_row[pos_a] = class_b;
                            }
                        }
                    }
                }
            }

            // NOTE: if some clash was detected, continue with the next pair of rows
            if (k < len_extended_row) {
                my_rb.valid[index_mt] = 2;
                continue;
            }

            // NOTE: there might remain some backreferences to constants, which have to be resolved, as well as
            //  some references to be flattened yet.
            for (k = 1; k < len_extended_row; ++k) {
                if (resulting_row[k] < 0) {
                    unsigned pos = find(resulting_row, resulting_row[k]);
                    resulting_row[k] = is_symbol(resulting_row[pos]) ? resulting_row[pos] : -((int)(pos) + 1);
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_unification);

    /* --- Verify correctness --- */
    if (verbose) printf("\tComparing against reference…\n");
    int correct = compare_results(&my_rb, rb, ob1, ob2);
    if (!correct) { global_correct = false; global_incorrect++; }
    global_count++;
    if (verbose) printf("Unification is %scorrect\n", correct ? "" : "NOT ");

    /* --- Accumulate timings --- */

    timespec_subtract(&elapsed, &end_unification, &start_unification);
    timespec_add(&unification_elapsed_nle, &unification_elapsed_nle, &elapsed);

    free_result_block(&my_rb);
}

static void matrix_intersection_with_extended_rows_lineal(
    operand_block *ob1, operand_block *ob2,
    unsigned len_extended_row, int *extended_rows, 
    result_block *rb
){
    struct timespec start_unification, end_unification, elapsed;

    /* --- Compute unifiers --- */
    // NOTE: no need to compute unifiers (column mappings)

    /* --- Apply unifiers --- */
    if (verbose) printf("\tmatrix intersection with extended rows lineal\n");
    clock_gettime(CLOCK_MONOTONIC, &start_unification);

    unsigned num_rows1 = ob1->r;
    unsigned num_rows2 = ob2->r;
    unsigned num_rows3 = num_rows1 * num_rows2;
    result_block my_rb = { .r1 = num_rows1, .r2 = num_rows2, .r = num_rows3, .c1 = ob1->c, .c2 = ob2->c, .c = len_extended_row };
    my_rb.terms = malloc(num_rows3 * sizeof(*my_rb.terms));
    CHECK_MALLOC(my_rb.terms);
    my_rb.valid = calloc(num_rows3, sizeof(*my_rb.valid)); // NOTE: 0 - unified
    CHECK_CALLOC(my_rb.valid);

    // NOTE: this might be a better alternative for future parallelization
    //for(unsigned i = 0; i < num_rows1; ++i){
    //    int *row_a = extended_rows + i * len_extended_row;
    //    for(unsigned j = 0; j < num_rows2; ++j){
    //        int *row_b = extended_rows + (num_rows1 + j) * len_extended_row;
    
    int *extended_rows2 = extended_rows + (num_rows1 * len_extended_row);
    int *row_a = extended_rows;
    for(unsigned i = 0; i < num_rows1; ++i, row_a += len_extended_row){
        int *row_b = extended_rows2;
        for(unsigned j = 0; j < num_rows2; ++j, row_b += len_extended_row){

            int *resulting_row = malloc(sizeof(*resulting_row) * len_extended_row);
            CHECK_MALLOC(resulting_row);
            
            unsigned index_mt = i * my_rb.r2 + j;
            my_rb.terms[index_mt] = (main_term){ .c = len_extended_row, .row = resulting_row };

            // NOTE: unification
            for(unsigned k = 0; k < len_extended_row; ++k){
                int a = row_a[k];
                int b = row_b[k];
                //assert(a >= 0 && b >= 0);

                if (is_variable(a) && is_variable(b)) {
                    if (a == 0 || b == 0) {
                        resulting_row[k] = 0;
                    } else {
                        resulting_row[k] = 1;
                    }
                }
                else if (is_symbol(a) && is_variable(b)) {
                    resulting_row[k] = a;
                } else if (is_variable(a) && is_symbol(b)) {
                    resulting_row[k] = b;
                } else if (a == b) {
                    resulting_row[k] = a;
                } else {
                    my_rb.valid[index_mt] = 2;
                    break;
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_unification);

    /* --- Verify correctness --- */
    if (verbose) printf("\tComparing against reference…\n");
    int correct = compare_results(&my_rb, rb, ob1, ob2);
    if (!correct) { global_correct = false; global_incorrect++; }
    global_count++;
    if (verbose) printf("Unification is %scorrect\n", correct ? "" : "NOT ");

    /* --- Accumulate timings --- */

    timespec_subtract(&elapsed, &end_unification, &start_unification);
    timespec_add(&unification_elapsed_le, &unification_elapsed_le, &elapsed);

    free_result_block(&my_rb);
}

static unsigned find_and_flatten_unif(int *unification_array, unsigned var_pos){
    if(unification_array[var_pos] >= 0){
        return var_pos;
    }

    unsigned pos = find_and_flatten_unif(unification_array, -unification_array[var_pos]);
    unification_array[var_pos] = -(int)(pos);

    return pos;
}

static void matrix_intersection_with_mappings(
    operand_block *ob1, operand_block *ob2,
    mgu_schema *mappings,
    result_block *rb
){
    
    struct timespec start_unification, end_unification, elapsed;
    
    /* --- Compute unifiers --- */
    // NOTE: no need to compute unifiers (column mappings)
    
    /* --- Apply unifiers --- */
    if (verbose) printf("\tmatrix intersection with mappings\n");
    clock_gettime(CLOCK_MONOTONIC, &start_unification);
    
    unsigned len_extended_row = mappings->n_common;
    unsigned num_rows1 = ob1->r;
    unsigned num_rows2 = ob2->r;
    unsigned num_rows3 = num_rows1 * num_rows2;
    result_block my_rb = { .r1 = num_rows1, .r2 = num_rows2, .r = num_rows3, .c1 = ob1->c, .c2 = ob2->c, .c = len_extended_row };
    my_rb.terms = malloc(num_rows3 * sizeof(*my_rb.terms));
    CHECK_MALLOC(my_rb.terms);
    my_rb.valid = calloc(num_rows3, sizeof(*my_rb.valid)); // NOTE: 0 - unified
    CHECK_CALLOC(my_rb.valid);
    
    // TODO(ORG): move arraylist of uints/ints to another file, not in schemas.c/h
    // NOTE: unification array of 2*len_extended_row + 1, with interpretation 
    //  1..c1 (original vars 1), c1+1..n (virtual vars 1), n+1..n+c2 (original vars 2), n+c2+1..2n (virtual vars 2)
    // - Values in the unification array:
    //  Everyone starts with 0 --> Each index in a separate group, 0 is the value for roots
    //  If positive --> They are in the partition of that constant (another kind of root)
    //  If negative --> They point to the root or to an intermediate node, all of a same partition of variables unified among them
    unsigned num_classes = (2 * len_extended_row + 1);
    unsigned num_bytes_unification_buffer = 2 * num_classes * sizeof(int);
    int *unification_buffer = malloc(num_bytes_unification_buffer);
    CHECK_MALLOC(unification_buffer);
    int *unification_array = unification_buffer;
    int *unification_first_var_appearences_to_resulting_cols = unification_buffer + num_classes;

    mgu_schema *mapping = mappings - 1;
    main_term *mt_a = ob1->terms;
    for(unsigned i = 0; i < num_rows1; ++i, ++mt_a){
        int *row_a = mt_a->row;
        main_term *mt_b = ob2->terms;
        for(unsigned j = 0; j < num_rows2; ++j, ++mt_b){
            int *row_b = mt_b->row;
            ++mapping;

            SET_TO_ZERO(unification_buffer, num_bytes_unification_buffer);

            int *resulting_row = malloc(sizeof(*resulting_row) * len_extended_row);
            CHECK_MALLOC(resulting_row);
            
            // NOTE: we could initialize it outside the loop to simply increment it as we do with mapping. But
            //  to parallelize it in a GPU in the future, this approach is better (for mapping too).
            unsigned index_mt = i * my_rb.r2 + j;
            my_rb.terms[index_mt] = (main_term){ .c = len_extended_row, .row = resulting_row };

            // TODO(CLEAN/OPT): could the following four loops (or at least some subparts) be collapsed into one?

            // NOTE: an extra initial loop is needed to ensure we can distinguish first apparitions of extending and original rows
            //  in the unification_array.
            for(unsigned k = 0; k < len_extended_row; ++k){
                unsigned col_a = mapping->common_L[k];
                unsigned col_b = mapping->common_R[k];

                if (col_a > ob1->c || row_a[col_a - 1] == 1) {
                    unification_array[col_a] = 1;
                }
                if (col_b > ob2->c || row_b[col_b - 1] == 1) {
                    unification_array[col_b + len_extended_row] = 1;
                }
            }
            
            // NOTE: unification. First loop to decide unification classes (or partitions of variables).
            // NOTE: If all variables in a unification class are extending variables that are not unified to a symbol, then the root
            //  is 1. If any of them is an original variable, the root is 0.
            for(unsigned k = 0; k < len_extended_row; ++k){
                unsigned col_a = mapping->common_L[k];
                unsigned col_b = mapping->common_R[k];
                
                // NOTE: if we have a repeated appearence of an original variable (negative index in row), 
                //  we have to "unify" its position in unification_array with the class of the first appearence 
                //  of that variable to be able to focus only on unification_array for all necessary information
                //  about the relations between variables.
                if (col_a <= ob1->c && row_a[col_a - 1] < 0) {
                    unification_array[col_a] = row_a[col_a - 1];
                }
                if (col_b <= ob2->c && row_b[col_b - 1] < 0) {
                    unification_array[col_b + len_extended_row] = row_b[col_b - 1] - len_extended_row;
                }

                bool is_var_a = (col_a > ob1->c) || (row_a[col_a - 1] <= 1);
                bool is_var_b = (col_b > ob2->c) || (row_b[col_b - 1] <= 1);
                if (is_var_a && is_var_b) {
                    // NOTE: Unite both classes with the greatest class as root (first variable, the one with the lowest position)
                    unsigned pos_a = find_and_flatten_unif(unification_array, col_a);
                    unsigned pos_b = find_and_flatten_unif(unification_array, col_b + len_extended_row);
                    unsigned root_value = (unification_array[pos_a] == 0 || unification_array[pos_b] == 0) ? 0 : 1;

                    if (pos_a < pos_b) {
                        int class_a = -(int)(pos_a);
                        unification_array[pos_b] = class_a;
                        unification_array[pos_a] = root_value;
                    } else {
                        int class_b = -(int)(pos_b);
                        unification_array[pos_a] = class_b;
                        unification_array[pos_b] = root_value;
                    }
                }
            }

            // NOTE: Second loop to perform the unification, checking clashes. We have to adjust the classes in 
            //  unification_array in case a variable of a class unifies with a constant. In the case a class 
            //  remains simply as a unification among variables (and therefore it will be reflected by a single
            //  variable in the resulting_row, if it appears there), we have to store in unification_first_var_appearences_to_resulting_cols
            //  the first apparition of that variable in the resulting_row.
            unsigned k = 0;
            for (; k < len_extended_row; ++k) {
                unsigned col_a = mapping->common_L[k];
                unsigned col_b = mapping->common_R[k];
                
                // NOTE: in the case of a function symbol in the original row, a and b will have that value. In the case of a 
                //  virtual or original row variable, they will have the class (negative 1-based int pointing to the root in unification_array),
                //  unless that class has been unified with a symbol in this loop.
                int a;
                if ((col_a <= ob1->c) && (row_a[col_a - 1] > 1)) {
                    a = row_a[col_a - 1];
                } else {
                    unsigned pos = find_and_flatten_unif(unification_array, col_a);
                    a = (unification_array[pos] > 1) ? unification_array[pos] : -(int)(pos);
                }
                int b;
                if ((col_b <= ob2->c) && (row_b[col_b - 1] > 1)) {
                    b = row_b[col_b - 1];
                } else {
                    unsigned pos = find_and_flatten_unif(unification_array, col_b + len_extended_row);
                    b = (unification_array[pos] > 1) ? unification_array[pos] : -(int)(pos);
                }
                
                if (a > 1 && b > 1) {
                    if (a == b) {
                        resulting_row[k] = a;
                    } else {
                        break;
                    }
                } else if (a > 1 || b > 1) {
                    int sym, var_class;
                    if (a > 1) {
                        sym = a;
                        var_class = b;
                    } else {
                        sym = b;
                        var_class = a;
                    }

                    resulting_row[k] = sym;
                    unification_array[-var_class] = sym;
                    // NOTE: in case a corresponding variable was already introduced in the resulting row, we set its
                    //  first appearence to the symbol the class of the variable has unified with. The other apparitions
                    //  of that variable in the resulting row, the negative 1-based backreferences, are resolved in a 
                    //  posterior loop.
                    if (unification_first_var_appearences_to_resulting_cols[-var_class] < 0) {
                        unsigned pos0based = -(unification_first_var_appearences_to_resulting_cols[-var_class] + 1);
                        resulting_row[pos0based] = sym;
                        unification_first_var_appearences_to_resulting_cols[-var_class] = sym;
                    }

                } else {
                    // NOTE: if both are variables, they should be in the same unification class
                    assert(a < 0 && b < 0 && a == b);

                    // NOTE: check if a corresponding resulting variable was introduced in the resulting row
                    bool corresponding_var_in_result = unification_first_var_appearences_to_resulting_cols[-a] != 0;
                    if (corresponding_var_in_result) {
                        resulting_row[k] = unification_first_var_appearences_to_resulting_cols[-a];
                    } else {
                        // NOTE: If anyone of the variables unified in the a var class is an original one, we
                        //  introduce 0. Otherwise, we introduce 1. Those values are in the unification_array thanks
                        //  to the previous loops.
                        resulting_row[k] = unification_array[-a];
                        unification_first_var_appearences_to_resulting_cols[-a] = -(k + 1);
                    }
                }
            }

            if (k < len_extended_row) {
                my_rb.valid[index_mt] = 2;
                continue;
            }
            
            // NOTE: we need a third pass to resolve backreferences to symbols.
            for (k = 1; k < len_extended_row; ++k) {
                if (resulting_row[k] < 0) {
                    unsigned pos = -(resulting_row[k] + 1);
                    int sym = resulting_row[pos];
                    if (sym > 1) {
                        resulting_row[k] = sym;
                    }
                }
            }
        }
    }

    free(unification_buffer);

    clock_gettime(CLOCK_MONOTONIC, &end_unification);

    /* --- Verify correctness --- */
    if (verbose) printf("\tComparing against reference…\n");
    int correct = compare_results(&my_rb, rb, ob1, ob2);
    if (!correct) { global_correct = false; global_incorrect++; }
    global_count++;
    if (verbose) printf("Unification is %scorrect\n", correct ? "" : "NOT ");

    /* --- Accumulate timings --- */

    timespec_subtract(&elapsed, &end_unification, &start_unification);
    timespec_add(&unification_elapsed_nlm, &unification_elapsed_nlm, &elapsed);

    free_result_block(&my_rb);
}

static void matrix_intersection_with_mapping_lineal(
    operand_block *ob1, operand_block *ob2,
    mgu_schema *mapping, 
    result_block *rb
){
    struct timespec start_unification, end_unification, elapsed;

    /* --- Compute unifiers --- */
    // NOTE: no need to compute unifiers (column mappings)

    /* --- Apply unifiers --- */
    if (verbose) printf("\tmatrix intersection with mapping lineal\n");
    clock_gettime(CLOCK_MONOTONIC, &start_unification);

    unsigned num_rows1 = ob1->r;
    unsigned num_rows2 = ob2->r;
    unsigned num_rows3 = num_rows1 * num_rows2;
    result_block my_rb = { .r1 = num_rows1, .r2 = num_rows2, .r = num_rows3, .c1 = ob1->c, .c2 = ob2->c, .c = mapping->n_common };
    my_rb.terms = malloc(num_rows3 * sizeof(*my_rb.terms));
    CHECK_MALLOC(my_rb.terms);
    my_rb.valid = calloc(num_rows3, sizeof(*my_rb.valid)); // NOTE: 0 - unified
    CHECK_CALLOC(my_rb.valid);

    main_term *mt_a = ob1->terms;
    for(unsigned i = 0; i < num_rows1; ++i, ++mt_a){
        int *row_a = mt_a->row;
        main_term *mt_b = ob2->terms;
        for(unsigned j = 0; j < num_rows2; ++j, ++mt_b){
            int *row_b = mt_b->row;
            
            int *resulting_row = malloc(sizeof(*resulting_row) * mapping->n_common);
            CHECK_MALLOC(resulting_row);
            
            unsigned index_mt = i * my_rb.r2 + j;
            my_rb.terms[index_mt] = (main_term){ .c = mapping->n_common, .row = resulting_row };

            // NOTE: unification
            for(unsigned k = 0; k < mapping->n_common; ++k){
                unsigned col_a = mapping->common_L[k] - 1;
                unsigned col_b = mapping->common_R[k] - 1;
                
                int a = col_a < ob1->c ? row_a[col_a] : 1;
                int b = col_b < ob2->c ? row_b[col_b] : 1;
                //assert(a >= 0 && b >= 0);

                if (is_variable(a) && is_variable(b)) {
                    if (a == 0 || b == 0) {
                        resulting_row[k] = 0;
                    } else {
                        resulting_row[k] = 1;
                    }
                }
                else if (is_symbol(a) && is_variable(b)) {
                    resulting_row[k] = a;
                } else if (is_variable(a) && is_symbol(b)) {
                    resulting_row[k] = b;
                } else if (a == b) {
                    resulting_row[k] = a;
                } else {
                    my_rb.valid[index_mt] = 2;
                    break;
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end_unification);

    /* --- Verify correctness --- */
    if (verbose) printf("\tComparing against reference…\n");
    int correct = compare_results(&my_rb, rb, ob1, ob2);
    if (!correct) { global_correct = false; global_incorrect++; }
    global_count++;
    if (verbose) printf("Unification is %scorrect\n", correct ? "" : "NOT ");

    /* --- Accumulate timings --- */

    timespec_subtract(&elapsed, &end_unification, &start_unification);
    timespec_add(&unification_elapsed_lm, &unification_elapsed_lm, &elapsed);

    free_result_block(&my_rb);
}




/* ================================================================== */
/* Entry point                                                        */
/* ================================================================== */

// TODO: if the new postprocessed main works and there is no reusable code here, remove this block
//  and its associated functions! 
#if 0

/**
 * @brief Program entry point.
 *
 * Usage: ./c <M1.csv> <M2.csv> <M3.csv> [verbose]
 *
 * Reads three CSV matrix files, performs all pairwise block intersections,
 * checks results against M3, and prints a one-line CSV summary:
 *
 *   <base_path>, M1 blocks N, M2 blocks N, OK|Not OK, wrong/total,
 *   read_time, unif_time, apply_time, total_unif_time, total_time
 *
 * All times are in seconds with nanosecond precision.
 *
 * @return 0 on success; exits with EXIT_FAILURE on file or parse errors.
 */
int main(char *M1_file, char *M2_file, char *M3_file, bool verb) {
    struct timespec start_reading, end_reading;
    struct timespec elapsed;

    var_dict  = create_dictionary(501);
    unif_dict = create_dictionary(501);
    symbols_to_ids = create_dictionary(501);

    verbose = verb;

    FILE *stream_M1 = fopen(M1_file, "r");
    FILE *stream_M2 = fopen(M2_file, "r");
    FILE *stream_M3 = fopen(M3_file, "r");

    if (!stream_M1 || !stream_M2 || !stream_M3) {
        fprintf(stderr, "Error opening files: %s, %s, %s\n", M1_file, M2_file, M3_file);
        if (stream_M1) fclose(stream_M1);
        if (stream_M2) fclose(stream_M2);
        if (stream_M3) fclose(stream_M3);
        return EXIT_FAILURE;
    }

    /* Read block counts from M1 and M2 headers. */
    unsigned s1, s2;
    if (read_num_blocks(stream_M1, &s1))
        fprintf(stderr, "Warning: could not read block count from %s\n", M1_file);
    if (read_num_blocks(stream_M2, &s2))
        fprintf(stderr, "Warning: could not read block count from %s\n", M2_file);
    if (verbose) printf("M1 blocks %u, M2 blocks %u\n", s1, s2);


    // Read the row identifying the columns' free variables
    char *line1 = NULL, *line2 = NULL;
    size_t len1 = 0, len2 = 0;
    if (getline(&line1, &len1, stream_M1) == -1) { free(line1); exit(1); } // Failed to read the line
    if (getline(&line2, &len2, stream_M2) == -1) { free(line2); exit(1); } // Failed to read the line
    
    unsigned num_free_vars1 = scan_num_free_vars(line1);
    unsigned num_free_vars2 = scan_num_free_vars(line2);

    Arena arena_operands;
    size_t arena_operands_bytes = 
        (s1 + s2)*(sizeof(operand_block) + sizeof(ArrayListSchema) + sizeof(ArrayListDependencyPair) + sizeof(SetSchema)) +
        2*(num_free_vars1 + num_free_vars2)*(sizeof(char*)) + strlen(line1) + strlen(line2) +
        2*(num_free_vars1 + num_free_vars2)*(sizeof(unsigned)) +
        (num_free_vars1*s1 + num_free_vars2*s2)*(sizeof(unsigned)) +
        s1*s2 * (sizeof(bool) + sizeof(ArrayListSchema) + sizeof(ArrayListDependencyPair) + sizeof(SetSchema)) +
        500000*(sizeof(Schema));
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
    foreach_in_arraylist(CharPtr, free_v, computed_free_vars3){
        // NOTE: special value for not found = free_vars3.size
        unsigned pos = find_in_array_list_char_ptr(free_vars1, *free_v);
        if(pos == free_vars1.size){
            pos = computed_free_vars3.size;
        }
        unsafe_add_to_array_list(free_var_positions1, pos);

        pos = find_in_array_list_char_ptr(free_vars2, *free_v);
        if(pos == free_vars2.size){
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


    /* --- Read operand blocks --- */
    clock_gettime(CLOCK_MONOTONIC, &start_reading);
    // NOTE: arena_operands is used only for Schemas and DependencyPairs, not OperandBlocks
    unsigned max_rows1 = 0;
    for (size_t i = 0; i < s1; i++) {
        read_operand_block(stream_M1, obs1 + i, set_schemas1 + i, dependencies_array1 + i, &arena_operands);
        max_rows1 = MAX(max_rows1, obs1[i].r);
    }

    unsigned max_rows2 = 0;
    for (size_t i = 0; i < s2; i++) {
        read_operand_block(stream_M2, obs2 + i, set_schemas2 + i, dependencies_array2 + i, &arena_operands);
        max_rows2 = MAX(max_rows2, obs2[i].r);
    }
    clock_gettime(CLOCK_MONOTONIC, &end_reading);
    timespec_subtract(&read_file_elapsed, &end_reading, &start_reading);

    /* --- Set Schemas: variables' independence, normalization and commmon --- */
    struct timespec start_schemas, end_schemas, schemas_elapsed;    
    
    clock_gettime(CLOCK_MONOTONIC, &start_schemas);

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
    for(unsigned i = 0; i < s1; ++i){
        ArrayListSchema *set_schema = set_schemas1 + i;
        foreach_in_arraylistptr(Schema, s, set_schema) { calculate_schema_size(s); }
        
        ArrayListDependencyPair *dependencies = dependencies_array1 + i;
        foreach_in_arraylistptr(DependencyPair, pair, dependencies){
            foreach_in_arraylist(Schema, s, pair->schemas){
                calculate_schema_size(s);
            }
        }

        SetSchema *normalized = normalized_set_schemas1 + i;
        ArrayListSchema *normalized_list = &normalized->list;
        *normalized_list = normalized_set_schema(*set_schema, *dependencies, &arena_operands);
        foreach_in_arraylistptr(Schema, s, normalized_list) {
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
    for(unsigned i = 0; i < s2; ++i){
        ArrayListSchema *set_schema = set_schemas2 + i;
        foreach_in_arraylistptr(Schema, s, set_schema) { calculate_schema_size(s); }
        
        ArrayListDependencyPair *dependencies = dependencies_array2 + i;
        foreach_in_arraylistptr(DependencyPair, pair, dependencies){
            foreach_in_arraylist(Schema, s, pair->schemas){
                calculate_schema_size(s);
            }
        }

        SetSchema *normalized = normalized_set_schemas2 + i;
        ArrayListSchema *normalized_list = &normalized->list;
        *normalized_list = normalized_set_schema(*set_schema, *dependencies, &arena_operands);
        foreach_in_arraylistptr(Schema, s, normalized_list) {
            calculate_schema_depth(s);
            max_normalized_operand_depth = MAX(max_normalized_operand_depth, s->depth);
        }

        normalized->size = 0;
        foreach_in_arraylistptr(Schema, s, normalized_list) { normalized->size += s->size; }
        max_normalized_operand_set_size2 = MAX(max_normalized_operand_set_size2, normalized->size);
        min_normalized_operand_set_size2 = MIN(min_normalized_operand_set_size2, normalized->size);
    }

    bool *exists_common_schema_array = allocate(&arena_operands, s1*s2 * sizeof(bool));
    ArrayListSchema *common_set_schemas = allocate(&arena_operands, s1*s2 * sizeof(*common_set_schemas));
    ArrayListDependencyPair *common_dependencies_array = allocate(&arena_operands, s1*s2 * sizeof(*common_dependencies_array));
    SetSchema *normalized_common_set_schemas = allocate(&arena_operands, s1*s2 * sizeof(*normalized_common_set_schemas));
    // NOTE: loop to calculate the common set schemas and normalize them.
    unsigned max_normalized_common_set_size = 0;
    unsigned max_normalized_common_depth = 0;
    for(unsigned t1 = 1; t1 <= s1; ++t1){
        for(unsigned t2 = 1; t2 <= s2; ++t2){

            // NOTE: calculate common schema
            ArrayListSchema set_schema1 = set_schemas1[t1 - 1];
            ArrayListDependencyPair dependencies1 = dependencies_array1[t1 - 1];
            
            ArrayListSchema set_schema2 = set_schemas2[t2 - 1];
            ArrayListDependencyPair dependencies2 = dependencies_array2[t2 - 1];

            ArrayListSchema *computed_common_set_schema = common_set_schemas + (t1-1)*s2 + (t2-1);
            ArrayListDependencyPair *computed_common_dependencies = common_dependencies_array + (t1-1)*s2 + (t2-1);
            exists_common_schema_array[(t1-1)*s2 + (t2-1)] = common_set_schema_free_vars_baseline(
                set_schema1, dependencies1, free_var_positions1,
                set_schema2, dependencies2, free_var_positions2,
                computed_common_set_schema, computed_common_dependencies,
                &arena_operands);

            // NOTE: normalized uses longest_dependency which needs to know the sizes of the schemas, so we precalculate them.
            foreach_in_arraylistptr(Schema, s, computed_common_set_schema) { calculate_schema_size(s); }
            foreach_in_arraylistptr(DependencyPair, pair, computed_common_dependencies){
                foreach_in_arraylist(Schema, s, pair->schemas){
                    calculate_schema_size(s);
                }
            }

            // NOTE: calculate normalized set schemas. Sizes of schemas updated.
            ArrayListSchema list_normalized_common_set_schema = normalized_set_schema(*computed_common_set_schema, *computed_common_dependencies, &arena_operands);

            // NOTE: calculate depths once to create iterator over Schemas
            foreach_in_arraylist(Schema, s, list_normalized_common_set_schema) {
                calculate_schema_depth(s);
                max_normalized_common_depth = MAX(max_normalized_common_depth, s->depth);
            }

            // NOTE: wrap to calculate size only in one place
            SetSchema *normalized_common_set_schema = normalized_common_set_schemas + (t1-1)*s2 + (t2-1);
            normalized_common_set_schema->list = list_normalized_common_set_schema;
            normalized_common_set_schema->size = 0;
            foreach_in_arraylist(Schema, s, normalized_common_set_schema->list) { normalized_common_set_schema->size += s->size; }
            max_normalized_common_set_size = MAX(max_normalized_common_set_size, normalized_common_set_schema->size);
        }
    }

    // NOTE: we can precalculate the starting indices of each term (that correspond to the free vars) using the sizes of the normalized schemas
    //  before entering the resulting blocks' loop.
    unsigned *starting_col_indices_array = allocate(&arena_operands, sizeof(*starting_col_indices_array) * (free_vars1.size*s1 + free_vars2.size*s2));
    unsigned *starting_col_indices_array1 = starting_col_indices_array;
    unsigned *starting_col_indices_array2 = starting_col_indices_array + free_vars1.size*s1;
    
    unsigned *starting_indices = starting_col_indices_array1;
    for(unsigned i = 0; i < s1; ++i){
        ArrayListSchema normalized = normalized_set_schemas1[i].list;
        starting_column_indexes(normalized, starting_indices);
        starting_indices += free_vars1.size;
    }
    for(unsigned i = 0; i < s2; ++i){
        ArrayListSchema normalized = normalized_set_schemas2[i].list;
        starting_column_indexes(normalized, starting_indices);
        starting_indices += free_vars2.size;
    }

    clock_gettime(CLOCK_MONOTONIC, &end_schemas);
    timespec_subtract(&schemas_elapsed, &end_schemas, &start_schemas);

    Arena arena_result;
    float load_factor = 0.75f;
    size_t num_buckets_hms_rows_to_mapping_sides = (unsigned)((float)(max_rows1) / load_factor) + 1 + (unsigned)((float)(max_rows2) / load_factor) + 1;
    size_t arena_result_bytes = 
        (max_rows1 + max_rows2) * (max_normalized_common_set_size*sizeof(unsigned)) +
        (num_buckets_hms_rows_to_mapping_sides) * (sizeof(RowToMappingSide)) +
        (max_normalized_common_set_size) * sizeof(unsigned) +
        (max_rows1 * max_rows2) * sizeof(mgu_schema) +
        (s1 + s2) * max_normalized_common_set_size * sizeof(int);
    init_arena(&arena_result, arena_result_bytes);

    Arena debug_arena; init_arena(&debug_arena, 10000*sizeof(Schema));

    // NOTE: cleans to this arena are done in the mapping_column_indexes_side functions
    Arena schema_iterator_arena; init_arena(&schema_iterator_arena, sizeof(SchemaIteratorNode) * (max_normalized_operand_depth + max_normalized_common_depth));

    // NOTE: at most we will have as many variables as the number of columns and at most as many new virtual columns as the sum
    //  of the number of new columns (if any repeated variable, we will have less virtual columns, because its virtuals will be repeated too)
    Arena row_vars_arena;
    unsigned num_bytes_pointers1 = sizeof(unsigned*) * max_normalized_operand_set_size1;
    unsigned num_bytes_pointers2 = sizeof(unsigned*) * max_normalized_operand_set_size2;
    unsigned num_bytes_virtual_columns1 = sizeof(unsigned)*(max_normalized_common_set_size - min_normalized_operand_set_size1);
    unsigned num_bytes_virtual_columns2 = sizeof(unsigned)*(max_normalized_common_set_size - min_normalized_operand_set_size2);
    unsigned num_bytes_column_mappings1 = sizeof(int)*(max_normalized_operand_set_size1);
    unsigned num_bytes_column_mappings2 = sizeof(int)*(max_normalized_operand_set_size2);
    init_arena(&row_vars_arena, MAX(num_bytes_pointers1 + num_bytes_virtual_columns1 + num_bytes_column_mappings1,
        num_bytes_pointers2 + num_bytes_virtual_columns2 + num_bytes_column_mappings2));


    /* --- Process result blocks one at a time --- */
    ArrayListCharPtr free_vars3;
    result_block rb;
    ArrayListSchema common_set_schema;
    ArrayListDependencyPair common_dependencies;
    clock_gettime(CLOCK_MONOTONIC, &start_reading);
    read_first_result_block(stream_M3, &rb, &common_set_schema, &common_dependencies, &free_vars3, &arena_result, &debug_arena);
    clock_gettime(CLOCK_MONOTONIC, &end_reading);
    timespec_subtract(&elapsed, &end_reading, &start_reading);
    timespec_add(&read_file_elapsed, &read_file_elapsed, &elapsed);

    // NOTE: calculate and check final free vars only once! Since it corresponds to the entire M3!
    bool ok_free_vars = equal_array_lists_char_ptr(free_vars3, computed_free_vars3);
    if (verbose) printf("ok_free_vars = %u\n", ok_free_vars);
    assert(ok_free_vars);

    if (verbose) print_result_block(&rb, 0);

    struct timespec mapping_elapsed_l = {}, mapping_elapsed_nl = {}, mapping_elapsed_nl_hashopt = {};

    struct timespec start_row_extension, end_row_extension;
    struct timespec row_extention_elapsed_l = {}, row_extention_elapsed_nl = {};

    // NOTE: the amount of calls to mapping side calculation thanks to the HasMap of Row structure optimization in the case of non-linear blocks
    unsigned saved_calls1 = 0, saved_calls2 = 0;

    for(unsigned t1 = 1; t1 <= s1; ++t1){
        for(unsigned t2 = 1; t2 <= s2; ++t2) {

            ArrayListSchema computed_common_set_schema = common_set_schemas[(t1-1)*s2 + (t2-1)];
            ArrayListDependencyPair computed_common_dependencies = common_dependencies_array[(t1-1)*s2 + (t2-1)];
            bool exists_common_schema = exists_common_schema_array[(t1-1)*s2 + (t2-1)];
            
            if(rb.t1 == t1 && rb.t2 == t2){
                // NOTE: common schema exists so no fragment was skipped
                assert(exists_common_schema);
                
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
                    
                    if (verbose) printf("ok_set_schemas = %u\nok_dependencies = %u\n", ok_set_schemas, ok_dependendencies);
                    assert(ok_set_schemas);
                }
                
                struct timespec start_mapping, end_mapping;
                clock_gettime(CLOCK_MONOTONIC, &start_mapping);
                
                // Take arguments to calculate the mappings of column indexes
                SetSchema normalized_common_set_schema = normalized_common_set_schemas[(t1-1)*s2 + (t2-1)];
                SetSchema normalized_set_schema1 = normalized_set_schemas1[t1-1];
                SetSchema normalized_set_schema2 = normalized_set_schemas2[t2-1];

                unsigned *starting_col_indices1 = starting_col_indices_array1 + (t1-1)*free_vars1.size;
                unsigned *starting_col_indices2 = starting_col_indices_array2 + (t2-1)*free_vars2.size;

                unsigned row_len1 = rb.c1;
                unsigned row_len2 = rb.c2;
                assert(normalized_set_schema1.size == row_len1);
                assert(normalized_set_schema2.size == row_len2);

                operand_block *ob1 = obs1 + t1-1;
                operand_block *ob2 = obs2 + t2-1;

                // Calculate the mapping of column indexes
                mgu_schema computed_mapping;
                computed_mapping.n_common = normalized_common_set_schema.size;
                unsigned num_cols1 = normalized_set_schema1.size;
                computed_mapping.new_a = computed_mapping.n_common - num_cols1;
                unsigned num_cols2 = normalized_set_schema2.size;
                computed_mapping.new_b = computed_mapping.n_common - num_cols2;

                unsigned num_bytes = sizeof(*computed_mapping.common_columns) * computed_mapping.n_common;
                computed_mapping.common_columns = allocate(&arena_result, num_bytes);
                for(unsigned i = 0; i < computed_mapping.n_common; ++i){ computed_mapping.common_columns[i] = i; }

                unsigned mapping_side_size = computed_mapping.n_common * sizeof(unsigned);
                mgu_schema *schemas; // = NULL;

                if(rb.lineal_lineal){
                    unsigned *mapping_sides = allocate(&arena_result, 2 * mapping_side_size);
                    unsigned *mappingL = mapping_sides;
                    unsigned *mappingR = mapping_sides + computed_mapping.n_common;

                    assert(ob1->r > 0 && ob2->r > 0);
                    
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
                    if (verbose) printf("ok_mappings = %u\n\n", ok_mappings);
                    //assert(ok_mappings);
                
                    // We only use a schema for lineal blocks
                    schemas = allocate(&arena_result, 1 * sizeof(*schemas));
                    *schemas = computed_mapping;

                    clock_gettime(CLOCK_MONOTONIC, &end_mapping);
                    timespec_subtract(&elapsed, &end_mapping, &start_mapping);
                    timespec_add(&mapping_elapsed_l, &mapping_elapsed_l, &elapsed);

                } else {
                    // NOTE: the calculation of mappingL/R is independent of one another. We can precompute them in two linear loops instead of a quadratic nested loop.
                    unsigned *mapping_sides = allocate(&arena_result, (ob1->r + ob2->r) * mapping_side_size);
                    schemas = allocate(&arena_result, (ob1->r)*(ob2->r) * sizeof(*schemas));

                    clock_gettime(CLOCK_MONOTONIC, &end_mapping);
                    timespec_subtract(&elapsed, &end_mapping, &start_mapping);
                    timespec_add(&mapping_elapsed_nl, &mapping_elapsed_nl, &elapsed);
                    timespec_add(&mapping_elapsed_nl_hashopt, &mapping_elapsed_nl_hashopt, &elapsed);
                
                    // NOTE: measure the alternative for mapping calculation that doesn't use the HashMap for row structure
                    clock_gettime(CLOCK_MONOTONIC, &start_mapping);

                    unsigned *mapping_side = mapping_sides;
                    for(unsigned i = 0; i < ob1->r; ++i, mapping_side += computed_mapping.n_common){
                        mapping_column_indexes_side(
                            normalized_set_schema1, free_var_positions1, normalized_common_set_schema,
                            starting_col_indices1, ob1->terms[i].row, mapping_side, &row_vars_arena,
                            &schema_iterator_arena
                        );
                        clear_arena(&row_vars_arena);    
                    }
                    
                    for(unsigned i = 0; i < ob2->r; ++i, mapping_side += computed_mapping.n_common){
                        mapping_column_indexes_side(
                            normalized_set_schema2, free_var_positions2, normalized_common_set_schema,
                            starting_col_indices2, ob2->terms[i].row, mapping_side, &row_vars_arena,
                            &schema_iterator_arena
                        );
                        clear_arena(&row_vars_arena);
                    }

                    // TODO(CLEAN): instead of copying computed_mapping's header information per mapping_sideL/R combination,
                    //  we could pass the header only once per block to the matrix_intersection functions + the mapping sides information
                    //  (the cleanest way would be to modify the struct mgu_schema, taking into account that it can have more than one mapping
                    //  side depending on the linearity of the block).
                    mgu_schema *schema = schemas;

                    bool ok_mappings = true;
                    for(unsigned i = 0; i < ob1->r; ++i){
                        computed_mapping.common_L = mapping_sides + i*computed_mapping.n_common;
                        for(unsigned j = 0; j < ob2->r; ++j){
                            computed_mapping.common_R = mapping_sides + (ob1->r + j)*computed_mapping.n_common;
                            
                            mgu_schema *mapping = rb.terms[ i*rb.r2 + j ].ms;
                            ok_mappings &= equal_mgu_schemas(mapping, &computed_mapping);

                            // NOTE: We copy the structs (only the pointers) of all the schemas, one per ob1/2 combination
                            *schema = computed_mapping;
                            ++schema;
                        }
                    }

                    clock_gettime(CLOCK_MONOTONIC, &end_mapping);
                    timespec_subtract(&elapsed, &end_mapping, &start_mapping);
                    timespec_add(&mapping_elapsed_nl, &mapping_elapsed_nl, &elapsed);

                    if (verbose) printf("ok_mappings = %u\n\n", ok_mappings);

                    // NOTE: measure the alternative for mapping calculation that uses the HashMap for row structure
                    clock_gettime(CLOCK_MONOTONIC, &start_mapping);

                    // NOTE: function symbol differences don't affect to the resulting mapping, the mapping is determined by the placement of variables (0s and negatives),
                    //  and the original and common schemas. In practice, most of the mapping sides are equal, so we are going to use a HashMap to determine if the mapping
                    //  for an equivalent row was already computed.
                    // NOTE: no resizing risk with a load_factor of 75%
                    mapping_side = mapping_sides;

                    float load_factor = 0.75f;
                    unsigned num_buckets = (unsigned)((float)(ob1->r) / load_factor) + 1;
                    MapRowToMappingSide map1 = create_map_row_to_mapping_side_arena(num_buckets, normalized_set_schema1.size, normalized_common_set_schema.size, &arena_result);
                    for(unsigned i = 0; i < ob1->r; ++i, mapping_side += computed_mapping.n_common){
                        int *row = ob1->terms[i].row;
                        RowToMappingSide *pair = get_pair_in_map_row_to_mapping_side(map1, row);
                        if(pair){
                            ++saved_calls1;
                        } else {
                            mapping_column_indexes_side(
                                normalized_set_schema1, free_var_positions1, normalized_common_set_schema,
                                starting_col_indices1, row, mapping_side, &row_vars_arena,
                                &schema_iterator_arena
                            );
                            clear_arena(&row_vars_arena);
                            insert_to_map_row_to_mapping_side_arena(&map1, row, mapping_side, &arena_result);
                        }
                    }
                    
                    // NOTE: no resizing risk with a load_factor of 75%
                    num_buckets = (unsigned)((float)(ob2->r) / load_factor) + 1;
                    MapRowToMappingSide map2 = create_map_row_to_mapping_side_arena(num_buckets, normalized_set_schema2.size, normalized_common_set_schema.size, &arena_result);
                    for(unsigned i = 0; i < ob2->r; ++i, mapping_side += computed_mapping.n_common){
                        int *row = ob2->terms[i].row;
                        RowToMappingSide *pair = get_pair_in_map_row_to_mapping_side(map2, row);
                        if(pair){
                            ++saved_calls2;
                        } else {
                            mapping_column_indexes_side(
                                normalized_set_schema2, free_var_positions2, normalized_common_set_schema,
                                starting_col_indices2, row, mapping_side, &row_vars_arena,
                                &schema_iterator_arena
                            );
                            clear_arena(&row_vars_arena);
                            insert_to_map_row_to_mapping_side_arena(&map2, row, mapping_side, &arena_result);
                        }
                    }

                    // Change the mgu_schemas sides and compare
                    // One mapping per row pairs in non-linear result block
                    // NOTE: we use the same memory as before.
                    schema = schemas;

                    ok_mappings = true;
                    for(unsigned i = 0; i < ob1->r; ++i){
                        RowToMappingSide *row_to_ms = get_pair_in_map_row_to_mapping_side(map1, ob1->terms[i].row);
                        assert(row_to_ms);
                        computed_mapping.common_L = row_to_ms->mapping_side;
                        //computed_mapping.common_L = mapping_sides + i*computed_mapping.n_common;
                        for(unsigned j = 0; j < ob2->r; ++j){
                            //printf("Rows: %d-%d (1-based)\n", i+1, j+1);
                            
                            mgu_schema *mapping = rb.terms[ i*rb.r2 + j ].ms;

                            RowToMappingSide *row_to_ms = get_pair_in_map_row_to_mapping_side(map2, ob2->terms[j].row);
                            assert(row_to_ms);
                            computed_mapping.common_R = row_to_ms->mapping_side;
                            //computed_mapping.common_R = mapping_sides + (ob1->r + j)*computed_mapping.n_common;

                            ok_mappings &= equal_mgu_schemas(mapping, &computed_mapping);
                            //assert(ok_mappings);

                            // We copy the structs (only the pointers) of all the schemas, one per ob1/2 combination
                            *schema = computed_mapping;
                            ++schema;
                        }
                    }

                    clock_gettime(CLOCK_MONOTONIC, &end_mapping);
                    timespec_subtract(&elapsed, &end_mapping, &start_mapping);
                    timespec_add(&mapping_elapsed_nl_hashopt, &mapping_elapsed_nl_hashopt, &elapsed);
                    
                    if (verbose) printf("ok_mappings = %u\n\n", ok_mappings);
                }

                clock_gettime(CLOCK_MONOTONIC, &start_row_extension);

                unsigned len_extended_row = normalized_common_set_schema.size;
                int *extended_rows = allocate(&arena_result, (ob1->r + ob2->r) * len_extended_row * sizeof(*extended_rows));
                int *extended_row = extended_rows;
                if (rb.lineal_lineal) {
                    for (unsigned i = 0; i < ob1->r; ++i, extended_row += len_extended_row) {
                        extend_row_lineal(normalized_set_schema1, free_var_positions1, normalized_common_set_schema, 
                            starting_col_indices1, ob1->terms[i].row, extended_row, &schema_iterator_arena);
                    }
                    for (unsigned i = 0; i < ob2->r; ++i, extended_row += len_extended_row) {
                        extend_row_lineal(normalized_set_schema2, free_var_positions2, normalized_common_set_schema, 
                            starting_col_indices2, ob2->terms[i].row, extended_row, &schema_iterator_arena);
                    }

                } else {
                    for (unsigned i = 0; i < ob1->r; ++i, extended_row += len_extended_row) {
                        extend_row(normalized_set_schema1, free_var_positions1, normalized_common_set_schema, 
                            starting_col_indices1, ob1->terms[i].row, extended_row, &row_vars_arena,
                            &schema_iterator_arena);
                        clear_arena(&row_vars_arena);
                    }
                    for (unsigned i = 0; i < ob2->r; ++i, extended_row += len_extended_row) {
                        extend_row(normalized_set_schema2, free_var_positions2, normalized_common_set_schema, 
                            starting_col_indices2, ob2->terms[i].row, extended_row, &row_vars_arena,
                            &schema_iterator_arena);
                        clear_arena(&row_vars_arena);
                    }
                }

                clock_gettime(CLOCK_MONOTONIC, &end_row_extension);
                timespec_subtract(&elapsed, &end_row_extension, &start_row_extension);
                if (rb.lineal_lineal) {
                    timespec_add(&row_extention_elapsed_l, &row_extention_elapsed_l, &elapsed);
                } else {
                    timespec_add(&row_extention_elapsed_nl, &row_extention_elapsed_nl, &elapsed);
                }

                
                // Matrix intersection: all variants, original core and alternative versions for lineal or nonlinear with mappings or extended rows
                matrix_intersection(&obs1[rb.t1 - 1], &obs2[rb.t2 - 1], &rb, schemas);
                if (rb.lineal_lineal) {
                    matrix_intersection_with_mapping_lineal(&obs1[rb.t1 - 1], &obs2[rb.t2 - 1], schemas, &rb);
                    matrix_intersection_with_extended_rows_lineal(&obs1[rb.t1 - 1], &obs2[rb.t2 - 1], len_extended_row, extended_rows, &rb);

                } else {
                    matrix_intersection_with_extended_rows(&obs1[rb.t1 - 1], &obs2[rb.t2 - 1], len_extended_row, extended_rows, &rb);
                    matrix_intersection_with_mappings(&obs1[rb.t1 - 1], &obs2[rb.t2 - 1], schemas, &rb);
                }

                clear_arena(&arena_result);
                clear_arena(&debug_arena);
                free_result_block(&rb);

                clock_gettime(CLOCK_MONOTONIC, &start_reading);
                read_next_result_block(stream_M3, &rb, &common_set_schema, &common_dependencies, &arena_result);
                clock_gettime(CLOCK_MONOTONIC, &end_reading);
    
                timespec_subtract(&elapsed, &end_reading, &start_reading);
                timespec_add(&read_file_elapsed, &read_file_elapsed, &elapsed);

                if (verbose) print_result_block(&rb, 0);

            } else {
                // NOTE: common schema doesn't exist so a fragment was skipped
                printf("Resultant fragment: %d-%d (SKIPPED BECAUSE COMMON SCHEMA DOESN'T EXIST!)\n", t1, t2);
                assert(!exists_common_schema);
                clear_arena(&arena_result);
                clear_arena(&debug_arena);
            }
        }
    }

    /* --- Print timing breakdown --- */
    struct timespec mapping_elapsed, mapping_elapsed_hashopt, row_extention_elapsed, unification_elapsed_l_total, unification_lm_total, unification_elapsed_nl_total, unification_nlm_total, unification_le_total, unification_nle_total;
    if (verbose) {
        printf("-------- TIME MEASUREMENTS --------\n");
        printf("File I/O: %ld.%09ld s\n\n", read_file_elapsed.tv_sec, read_file_elapsed.tv_nsec);
        
        printf("----------------\n");
        printf("Schema management: %ld.%09ld s\n\n", schemas_elapsed.tv_sec, schemas_elapsed.tv_nsec);
        
        printf("----------------\n");
        printf("Mapping obtention linear:             %ld.%09ld s\n", mapping_elapsed_l.tv_sec, mapping_elapsed_l.tv_nsec);
        printf("Mapping obtention non-linear:         %ld.%09ld s\n", mapping_elapsed_nl.tv_sec, mapping_elapsed_nl.tv_nsec);
        printf("Mapping obtention non-linear hashopt: %ld.%09ld s\n", mapping_elapsed_nl_hashopt.tv_sec, mapping_elapsed_nl_hashopt.tv_nsec);
        printf("\tSaved calls 1=%u - Saved calls 2=%u - Total=%u\n", saved_calls1, saved_calls2, saved_calls1 + saved_calls2);
        timespec_add(&mapping_elapsed, &mapping_elapsed_l, &mapping_elapsed_nl);
        printf("Mapping obtention total:              %ld.%09ld s\n", mapping_elapsed.tv_sec, mapping_elapsed.tv_nsec);
        timespec_add(&mapping_elapsed_hashopt, &mapping_elapsed_l, &mapping_elapsed_nl_hashopt);
        printf("Mapping obtention total hashopt:      %ld.%09ld s\n\n", mapping_elapsed_hashopt.tv_sec, mapping_elapsed_hashopt.tv_nsec);

        printf("----------------\n");
        printf("Row extention linear:     %ld.%09ld s\n", row_extention_elapsed_l.tv_sec, row_extention_elapsed_l.tv_nsec);
        printf("Row extention non-linear: %ld.%09ld s\n", row_extention_elapsed_nl.tv_sec, row_extention_elapsed_nl.tv_nsec);
        timespec_add(&row_extention_elapsed, &row_extention_elapsed_l, &row_extention_elapsed_nl);
        printf("Row extention total:      %ld.%09ld s\n\n", row_extention_elapsed.tv_sec, row_extention_elapsed.tv_nsec);
        
        printf("----------------\n");
        printf("Original core algorithm:\n");
        printf("Linear:\n");
        printf("Unifier computation: %ld.%09ld s\n", unifiers_elapsed_l.tv_sec, unifiers_elapsed_l.tv_nsec);
        printf("Unifier application: %ld.%09ld s\n", unification_elapsed_l.tv_sec, unification_elapsed_l.tv_nsec);
        timespec_add(&unification_elapsed_l_total, &unifiers_elapsed_l, &unification_elapsed_l);
        printf("Total unification:   %ld.%09ld s\n", unification_elapsed_l_total.tv_sec, unification_elapsed_l_total.tv_nsec);
        timespec_add(&unification_elapsed_l_total, &unification_elapsed_l_total, &mapping_elapsed_l);
        printf("Total:               %ld.%09ld s\n\n", unification_elapsed_l_total.tv_sec, unification_elapsed_l_total.tv_nsec);
        
        printf("Non-linear:\n");
        printf("Unifier computation: %ld.%09ld s\n", unifiers_elapsed_nl.tv_sec, unifiers_elapsed_nl.tv_nsec);
        printf("Unifier application: %ld.%09ld s\n", unification_elapsed_nl.tv_sec, unification_elapsed_nl.tv_nsec);
        timespec_add(&unification_elapsed_nl_total, &unifiers_elapsed_nl, &unification_elapsed_nl);
        printf("Total unification:   %ld.%09ld s\n", unification_elapsed_nl_total.tv_sec, unification_elapsed_nl_total.tv_nsec);
        timespec_add(&unification_elapsed_nl_total, &unification_elapsed_nl_total, &mapping_elapsed_nl);
        printf("Total:               %ld.%09ld s\n\n", unification_elapsed_nl_total.tv_sec, unification_elapsed_nl_total.tv_nsec);
        
        printf("----------------\n");
        printf("Alternative with mappings:\n");
        printf("Linear:\n");
        printf("Core:  %ld.%09ld s\n", unification_elapsed_lm.tv_sec, unification_elapsed_lm.tv_nsec);
        timespec_add(&unification_lm_total, &unification_elapsed_lm, &mapping_elapsed_l);
        printf("Total: %ld.%09ld s\n\n", unification_lm_total.tv_sec, unification_lm_total.tv_nsec);
        
        printf("Non-linear:\n");
        printf("Core:  %ld.%09ld s\n", unification_elapsed_nlm.tv_sec, unification_elapsed_nlm.tv_nsec);
        timespec_add(&unification_nlm_total, &unification_elapsed_nlm, &mapping_elapsed_nl);
        printf("Total: %ld.%09ld s\n\n", unification_nlm_total.tv_sec, unification_nlm_total.tv_nsec);
        
        printf("----------------\n");
        printf("Alternative with extended rows:\n");
        printf("Linear:\n");
        printf("Core:  %ld.%09ld s\n", unification_elapsed_le.tv_sec, unification_elapsed_le.tv_nsec);
        timespec_add(&unification_le_total, &unification_elapsed_le, &mapping_elapsed_l);
        printf("Total: %ld.%09ld s\n\n", unification_le_total.tv_sec, unification_le_total.tv_nsec);
        
        printf("Non-linear:\n");
        printf("Core:  %ld.%09ld s\n", unification_elapsed_nle.tv_sec, unification_elapsed_nle.tv_nsec);
        timespec_add(&unification_nle_total, &unification_elapsed_nle, &mapping_elapsed_nl);
        printf("Total: %ld.%09ld s\n\n", unification_nle_total.tv_sec, unification_nle_total.tv_nsec);
    } else {
        timespec_add(&mapping_elapsed, &mapping_elapsed_l, &mapping_elapsed_nl);
        timespec_add(&mapping_elapsed_hashopt, &mapping_elapsed_l, &mapping_elapsed_nl_hashopt);
        timespec_add(&row_extention_elapsed, &row_extention_elapsed_l, &row_extention_elapsed_nl);
        timespec_add(&unification_elapsed_l_total, &unifiers_elapsed_l, &unification_elapsed_l);
        timespec_add(&unification_elapsed_l_total, &unification_elapsed_l_total, &mapping_elapsed_l);
        timespec_add(&unification_elapsed_nl_total, &unifiers_elapsed_nl, &unification_elapsed_nl);
        timespec_add(&unification_elapsed_nl_total, &unification_elapsed_nl_total, &mapping_elapsed_nl);
        timespec_add(&unification_lm_total, &unification_elapsed_lm, &mapping_elapsed_l);
        timespec_add(&unification_nlm_total, &unification_elapsed_nlm, &mapping_elapsed_nl);
        timespec_add(&unification_le_total, &unification_elapsed_le, &mapping_elapsed_l);
        timespec_add(&unification_nle_total, &unification_elapsed_nle, &mapping_elapsed_nl);
    }

    /* --- Print one-line CSV summary --- */
    if (!verbose) {
        char *FILE = M3_file;
        
        unsigned F1 = s1;
        unsigned L1 = 0;
        unsigned N1 = 0;
        double C1 = 0.0;
        for (unsigned i = 0; i < s1; ++i) {
            operand_block *ob = obs1 + i;
            
            ArrayListSchema set_schema = set_schemas1[i];
            SetVariables vars = create_set_variables_defsize();
            variables_in_set_schema(set_schema, &vars);
            bool is_linear = vars.num_variables == 0;
            free_set_variables(vars);
    
            if (is_linear) {
                L1 += ob->r;
            } else {
                N1 += ob->r;
            }
    
            C1 = incremental_mean(C1, ob->c, i + 1);
        }
    
        unsigned F2 = s2;
        unsigned L2 = 0;
        unsigned N2 = 0;
        double C2 = 0.0;
        for (unsigned i = 0; i < s2; ++i) {
            operand_block *ob = obs2 + i;
            
            ArrayListSchema set_schema = set_schemas2[i];
            SetVariables vars = create_set_variables_defsize();
            variables_in_set_schema(set_schema, &vars);
            bool is_linear = vars.num_variables == 0;
            free_set_variables(vars);
    
            if (is_linear) {
                L2 += ob->r;
            } else {
                N2 += ob->r;
            }
    
            C2 = incremental_mean(C2, ob->c, i + 1);
        }
        
        printf("%s,%u,%u,%u,%f,%u,%u,%u,%f,", FILE, F1, L1, N1, C1, F2, L2, N2, C2);
        printf("%ld.%09ld,", read_file_elapsed.tv_sec, read_file_elapsed.tv_nsec);
        printf("%ld.%09ld,", schemas_elapsed.tv_sec, schemas_elapsed.tv_nsec);
        printf("%ld.%09ld,", mapping_elapsed_l.tv_sec, mapping_elapsed_l.tv_nsec);
        printf("%ld.%09ld,", mapping_elapsed_nl.tv_sec, mapping_elapsed_nl.tv_nsec);
        printf("%ld.%09ld,", mapping_elapsed_nl_hashopt.tv_sec, mapping_elapsed_nl_hashopt.tv_nsec);
        printf("%ld.%09ld,", row_extention_elapsed_l.tv_sec, row_extention_elapsed_l.tv_nsec);
        printf("%ld.%09ld,", row_extention_elapsed_nl.tv_sec, row_extention_elapsed_nl.tv_nsec);
        printf("%ld.%09ld,", unification_elapsed_l_total.tv_sec, unification_elapsed_l_total.tv_nsec);
        printf("%ld.%09ld,", unification_elapsed_nl_total.tv_sec, unification_elapsed_nl_total.tv_nsec);
        printf("%ld.%09ld,", unification_elapsed_lm.tv_sec, unification_elapsed_lm.tv_nsec);
        printf("%ld.%09ld,", unification_elapsed_nlm.tv_sec, unification_elapsed_nlm.tv_nsec);
        printf("%ld.%09ld,", unification_elapsed_le.tv_sec, unification_elapsed_le.tv_nsec);
        printf("%ld.%09ld\n", unification_elapsed_nle.tv_sec, unification_elapsed_nle.tv_nsec);
    }

    /* --- Cleanup --- */
    for (unsigned i = 0; i < s1; i++) free_operand_block(&obs1[i]);
    for (unsigned i = 0; i < s2; i++) free_operand_block(&obs2[i]);
    
    free_arena(&arena_operands);
    free_arena(&arena_result);
    free_arena(&debug_arena);
    free_arena(&schema_iterator_arena);
    free_arena(&row_vars_arena);

    free_dictionary(var_dict);
    free_dictionary(unif_dict);
    free_dictionary(symbols_to_ids);


    fclose(stream_M1);
    fclose(stream_M2);
    fclose(stream_M3);

    return 0;
}

#else

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// POSTPROCESSED MAIN ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Program entry point.
 *
 * Usage: ./c <M1.csv> <M2.csv> <M3.csv> [verbose]
 *
 * Reads three CSV matrix files, performs all pairwise block intersections,
 * checks results against M3, and prints a one-line CSV summary:
 *
 *   <base_path>, M1 blocks N, M2 blocks N, OK|Not OK, wrong/total,
 *   read_time, unif_time, apply_time, total_unif_time, total_time
 *
 * All times are in seconds with nanosecond precision.
 *
 * @return 0 on success; exits with EXIT_FAILURE on file or parse errors.
 */

#include "postprocessing_to_mnf.h"

/**
 * @brief Builds a raw unifier for a pair of main_term rows using their mgu_schema.
 *
 * Iterates over each common column pair defined in @p ms, calling unifier_a_b()
 * for each.  The raw unifier may contain redundant or variable-to-variable
 * bindings that are resolved by correct_unifier().
 *
 * @param unifier  Output array (caller allocates 2×ms->n_common slots starting at unifier[1]).
 * @return 0 if the rows are potentially unifiable; 1 if a constant clash was detected.
 */
static int unifier_block_rows(BlockRow *br1, BlockRow *br2, mgu_schema *ms, unsigned *unifier) {
    for (unsigned i = 0; i < ms->n_common; i++) {
        /*
         * common_L/R are 1-based; subtract 1 to get the 0-based column index.
         */
        assert(ms->common_L[i] > 0 && ms->common_R[i] > 0);
        if (unifier_a_b(br1->row, ms->common_L[i] - 1,
                        br2->row, ms->common_R[i] - 1,
                        unifier, 2 * i, br1->c, ms->new_a, br2->c))
            return 1;
    }
    return 0;
}


/**
 * @brief Refines the raw unifier produced by unifier_rows() into a minimal substitution set.
 *
 * Algorithm:
 *   1. Build a union-find-like L2 array (one entry per extended column index).
 *   2. Normalise repeated-variable references so each variable is represented by
 *      its first occurrence.
 *   3. For each binding pair (x←y):
 *        - If both are constants and equal, skip.
 *        - If both are constants and distinct, return -1 (not unifiable).
 *        - If one is a constant and the other a variable, record (variable←constant)
 *          and propagate to all variables already aliased to the variable.
 *        - If both are variables, make one point to the other and merge their alias lists.
 *   4. Collect the final substitutions: for each column that is not substituted by
 *      anything but has an alias list, emit (alias←column) pairs.
 *   5. Write the substitution count into unifier[0].
 *
 * @param unifier  In/out array; layout on input: 2×n slots starting at unifier[1],
 *                 each pair (x,y) meaning "x should become y".  On output: unifier[0]
 *                 holds the number of valid substitutions, and the first 2×count slots
 *                 contain the simplified bindings.
 * @return 0 if the unifier is valid; -1 if a contradiction was found.
 */
static int correct_block_rows_unifier(BlockRow *br1, BlockRow *br2, mgu_schema *ms, unsigned *unifier) {
    const unsigned c1  = br1->c;
    const unsigned c2  = br2->c;
    const unsigned m   = ms->n_common;
    const int *row_a   = br1->row;
    const int *row_b   = br2->row;

    /*
     * The extended index space has 2*m slots:
     *   0..m-1     → extended M1 columns
     *   m..2*m-1   → extended M2 columns (stored as original_index + m in the raw unifier)
     */
    unsigned lst_length = m * 2;
    unsigned i;

    L2 *lst = (L2 *)malloc(lst_length * sizeof(L2));
    for (i = 0; i < lst_length; i++) lst[i] = create_L2_empty();

    for (i = 0; i < 2 * m; i += 2) {
        unsigned x  = unifier[1 + i];
        unsigned y  = unifier[1 + i + 1];
        int val_x, val_y;

        if (x == 0 && y == 0) continue; /* Empty / equal-constant pair. */

        /* Resolve repeated variables to their first occurrence (negative back-reference). */
        if (x < c1 && row_a[x] < 0)
            x = (unsigned)(-(row_a[x] + 1));
        else if (x >= m && (x - m) < c2 && row_b[x - m] < 0)
            x = (unsigned)(-(row_b[x - m] + 1)) + m;

        if (y < c1 && row_a[y] < 0)
            y = (unsigned)(-(row_a[y] + 1));
        else if (y >= m && (y - m) < c2 && row_b[y - m] < 0)
            y = (unsigned)(-(row_b[y - m] + 1)) + m;

        /* Follow any existing substitution pointers. */
        if (lst[x].count > 0) x = (unsigned)lst[x].by;
        if (lst[y].count > 0) y = (unsigned)lst[y].by;
        if (x == y) continue; /* Already aliased; nothing to do. */

        /* Retrieve actual values. */
        if      (x < c1)        val_x = row_a[x];
        else if (x < m)         val_x = 1;           /* Fresh M1 variable. */
        else if ((x - m) < c2)  val_x = row_b[x - m];
        else                    val_x = 1;

        if      (y < c1)        val_y = row_a[y];
        else if (y < m)         val_y = 1;
        else if ((y - m) < c2)  val_y = row_b[y - m];
        else                    val_y = 1;

        if (is_symbol(val_x) && is_symbol(val_y) && val_x != val_y) {
            /* Constant clash → not unifiable. */
            for (unsigned k = 0; k < lst_length; k++) free_L2(lst[k]);
            free(lst);
            return -1;
        }
        if (is_symbol(val_x) && is_symbol(val_y)) continue; /* Equal constants; nothing to do. */

        if (is_symbol(val_x) && is_variable(val_y)) {
            /* x is a constant, y is a variable → substitute y with x. */
            lst[y].count = 1;
            lst[y].by    = (int)x;
            lst[y].ind   = (int)y;
            /* Propagate: all variables previously pointing to y now point to x. */
            for (L3 *cur = lst[y].head; cur != NULL; cur = cur->next)
                lst[cur->ind].by = (int)x;
            /* Move y and its chain to x's alias list. */
            L3 *node = create_L3((int)y, NULL);
            if (!lst[x].head) { lst[x].head = lst[x].tail = node; }
            else              { lst[x].tail->next = node; lst[x].tail = node; }
            if (lst[y].head) { node->next = lst[y].head; lst[x].tail = lst[y].tail; }
            lst[y].head = lst[y].tail = NULL;
        } else {
            /* x is a variable (or both variables) → substitute x with y. */
            lst[x].count = 1;
            lst[x].by    = (int)y;
            lst[x].ind   = (int)x;
            for (L3 *cur = lst[x].head; cur != NULL; cur = cur->next)
                lst[cur->ind].by = (int)y;
            L3 *node = create_L3((int)x, NULL);
            if (!lst[y].head) { lst[y].head = lst[y].tail = node; }
            else              { lst[y].tail->next = node; lst[y].tail = node; }
            if (lst[x].head) { node->next = lst[x].head; lst[y].tail = lst[x].tail; }
            lst[x].head = lst[x].tail = NULL;
        }
    }

    /* Collect final substitutions from the alias lists. */
    int      last_unifier   = 0;
    unsigned n_substitutions = 0;
    for (i = 0; i < lst_length; i++) {
        if (lst[i].count == 0 && lst[i].head) {
            for (L3 *cur = lst[i].head; cur != NULL; cur = cur->next) {
                unifier[1 + last_unifier]     = (unsigned)cur->ind;
                unifier[1 + last_unifier + 1] = i;
                n_substitutions++;
                last_unifier += 2;
            }
        }
    }
    unifier[0] = n_substitutions;

    for (i = 0; i < lst_length; i++) free_L2(lst[i]);
    free(lst);
    return 0;
}


/**
 * @brief Computes all pairwise unifiers for two operand blocks.
 *
 * For each pair (i, j) of main_terms from @p ob1 and @p ob2:
 *   1. Calls unifier_rows() to produce a raw binding list.
 *   2. Calls correct_unifier() to refine it.
 *   3. On success, appends the unifier plus operand row indices (i, j) to @p unifiers.
 *
 * @param unifiers  Pre-allocated output array (worst-case: ob1->r × ob2->r entries,
 *                  each of size 1 + 2*rb->c + 2 unsigned slots).
 * @return Number of valid unifiers found.
 */
static unsigned unifier_blocks(Block *b1, Block *b2, Block *b3, mgu_schema *schemas, bool is_b3_lineal, unsigned *unifiers) {
    const unsigned m            = b3->c;
    const unsigned unifier_size = 1 + 2 * m + 2;
    unsigned       last_unifier = 0;

    unsigned *unifier = (unsigned *)malloc(unifier_size * sizeof(unsigned));

    unsigned i = ~0;
    BlockRow *br1;
    intrusive_list_for_each_entry(br1, &b1->head_for_rows, block_pos) {
        ++i;

        unsigned j = ~0;
        BlockRow *br2;
        intrusive_list_for_each_entry(br2, &b2->head_for_rows, block_pos) {
            ++j;
            
            memset(unifier, 0, unifier_size * sizeof(unsigned));
            unsigned index_mt    = i * b2->r + j;
            mgu_schema *schema   = is_b3_lineal ? schemas : schemas + index_mt;
        
            if (unifier_block_rows(br1, br2, schema, unifier) != 0)
                continue;
            if (correct_block_rows_unifier(br1, br2, schema, unifier) != 0)
                continue;
        
            // NOTE: we store the addresses of the BlockRows that unify in the new version with intrusive_lists and postprocessing.
            //  We store the index_mt to identify what mgu_schema has been used for this unifier calculation
            unifier[1 + 2*m] = index_mt;
            BlockRow **address = (BlockRow **)(unifier + (1 + 2 * m + 1));
            *address = br1;
            ++address;
            *address = br2;
            
            memcpy(&unifiers[last_unifier * unifier_size], unifier,
                   unifier_size * sizeof(unsigned));
            last_unifier++;
        }
    }

    free(unifier);
    return last_unifier;
}

/**
 * @brief Applies a unifier to produce the unified main_term @p mt3.
 *
 * Strategy:
 *   1. Copy @p mt1->row into @p mt3->row (the left operand determines the
 *      base layout; fresh columns are already zeroed by create_empty_main_term).
 *   2. For each substitution (x←y) in the unifier where x belongs to the
 *      left-side extended row:
 *        - If y is a constant: set row[x] = val_y and replace all back-references
 *          to x with the constant.
 *        - If y is a variable: use unif_dict to track the canonical representative
 *          for y; set row[x] accordingly and handle cross-row variable aliasing.
 *   3. Clear unif_dict after all substitutions.
 *
 * @param mt1     Left operand main_term.
 * @param mt2     Right operand main_term.
 * @param mt3     Output main_term (must already be allocated via create_empty_main_term).
 * @param unifier Substitution array: [n_subs, x0,y0, x1,y1, …].
 */
static void apply_unifier_left_to_block_rows(BlockRow *br1, BlockRow *br2, BlockRow *br3, unsigned *unifier) {
    const unsigned n  = unifier[0] * 2;
    const unsigned c1 = br1->c;
    const unsigned c2 = br2->c;
    const unsigned m  = br3->c;

    /* Buffer for converting a column index to its string key in unif_dict. */
    int    length = (int)log10((double)(2 * m)) + 2;
    char  *y_str  = (char *)malloc((size_t)length * sizeof(char));

    /* Copy the left row; columns c1..m-1 (fresh) remain zero. */
    memcpy(br3->row, br1->row, c1 * sizeof(int));
    int       *row_a = br3->row;
    const int *row_b = br2->row;

    for (unsigned i = 1; i < n; i += 2) {
        unsigned x = unifier[i];
        unsigned y = unifier[i + 1];
        bool same_row = false;

        if (x >= m) continue; /* x belongs to the right side; skip for left-row application. */

        int val_y;
        if (y < m)             { val_y = row_a[y]; same_row = true; }
        else if ((y - m) < c2) { val_y = row_b[y - m]; }
        else                   { val_y = 1; } /* Fresh variable on right side. */

        if (is_symbol(val_y) > 0) {
            /* y is a constant: replace x and all its back-references. */
            row_a[x] = val_y;
            for (unsigned j = 0; j < m; j++)
                if (row_a[j] == (int)(-(x + 1))) row_a[j] = val_y;
        } else {
            /* y is a variable: track via unif_dict to handle repeated occurrences. */
            snprintf(y_str, (size_t)length, "%d", y);
            struct nlist *entry = lookup(unif_dict, y_str);

            if (!entry) {
                /*
                 * First occurrence of y.  When both x and y are in the same
                 * (left) row, ensure the variable with the smaller index is
                 * the canonical representative so that later reordering is
                 * consistent.
                 */
                if (same_row && x > y) {
                    /* x > y: make x point to y (i.e. x ← -( y+1 )). */
                    row_a[x] = -(int)(y + 1);
                    snprintf(y_str, (size_t)length, "%d", x);
                    install(unif_dict, y_str, (int)y);
                } else if (same_row) {
                    /* x < y: make y point to x. */
                    row_a[y] = -(int)(x + 1);
                    install(unif_dict, y_str, (int)x);
                } else {
                    install(unif_dict, y_str, (int)x);
                }
            } else {
                /*
                 * y was already seen.  Make x point to the same canonical
                 * representative (z) as y, and update any existing references to x.
                 */
                int z    = entry->defn;
                row_a[x] = -(z + 1);
                for (unsigned j = 0; j < m; j++)
                    if (row_a[j] == (int)(-(x + 1))) row_a[j] = -(z + 1);
            }
        }
    }

    clear(unif_dict);
    free(y_str);
}

/**
 * @brief Reorders a unified main_term's row into canonical column order.
 *
 * After apply_unifier_left(), the row values are indexed relative to the
 * extended M1 layout.  This function remaps them to the result column order
 * defined by ms->common_L, fixes back-reference indices, and resolves any
 * residual chained references.
 *
 * @param mt  Unified main_term to reorder in place.
 * @param ms  Schema whose common_L array defines the before→after column mapping.
 */
static void reorder_unified_block_row(BlockRow *br, mgu_schema *ms) {
    unsigned c = br->c;
    assert(c == ms->n_common);

    int *before      = br->row;
    int *after       = (int  *)malloc(c * sizeof(int));
    int *before_after = (int  *)malloc(c * sizeof(int));
    bool *duplicated  = (bool *)malloc(c * sizeof(bool));
    memset(before_after, -1, c * sizeof(int));
    memset(duplicated,    0,  c * sizeof(bool));

    /*
     * Pass 1: resolve single-level chained references in the before array.
     * After apply_unifier_left(), it is possible that before[i] = -( ref+1 )
     * where before[ref] != 0 (the reference target was itself substituted).
     * This pass collapses one level of indirection.
     */
    for (unsigned i = 0; i < c; i++) {
        int ref = before[i];
        if (ref < 0 && !is_var_first_appearence(before[-(ref + 1)]))
            before[i] = before[-(ref + 1)];
    }

    /*
     * Pass 2: permute columns according to common_L.
     * common_L[i] is the 1-based before-index for result column i.
     * If a before-column appears more than once, the second occurrence
     * is recorded as a reference to the first appearance in the after array.
     */
    for (unsigned i_after = 0; i_after < c; i_after++) {
        unsigned i_before = ms->common_L[i_after] - 1; /* 0-based */
        if (before_after[i_before] != -1) {
            /* Already seen: reference or copy the constant. */
            after[i_after] = is_symbol(before[i_before])
                ? before[i_before]
                : -(before_after[i_before] + 1);
            duplicated[i_after] = true;
        } else {
            after[i_after]        = before[i_before];
            before_after[i_before] = (int)i_after;
        }
    }

    /*
     * Pass 3: fix back-reference indices after the permutation.
     * A reference after[i] = -(ref+1) was valid in the before array;
     * the corresponding after-index is before_after[ref].
     * If the first appearance moved to the right of the reference, the
     * canonical position and the reference swap roles.
     */
    int *after2 = (int *)malloc(c * sizeof(int));
    memcpy(after2, after, c * sizeof(int));

    for (unsigned i = 0; i < c; i++) {
        if (after[i] >= 0 || duplicated[i]) continue;

        unsigned ref         = (unsigned)(-(after[i] + 1));
        unsigned current_idx = (unsigned)before_after[ref];

        if (current_idx < i) {
            after2[i] = -(int)(current_idx + 1);
        } else {
            if (after2[current_idx] < 0) {
                /* Already swapped in a previous iteration; follow the chain. */
                after2[i] = after2[current_idx];
            } else {
                /* First swap: make current_idx reference i, and i become the canonical 0. */
                // TODO(YA-TEST 3): check if the assertion is right, and we always have the first apparition (0 or 1) in temp...
                //  I think the code still works even if temp swaps a symbol, so the assertion could be temp >= 0 ...
                int temp = after2[current_idx];
                assert(is_var_first_appearence(temp));
                after2[current_idx] = -(int)(i + 1);
                after2[i]           = temp;
            }
        }
    }

    /*
     * Pass 4: resolve any residual single-level chained references in after2,
     * using the same logic as Pass 1.
     */
    for (unsigned i = 0; i < c; i++) {
        int ref = after2[i];
        if (ref < 0 && !is_var_first_appearence(after2[-(ref + 1)]))
            after2[i] = after2[-(ref + 1)];
    }

    memcpy(before, after2, c * sizeof(int));
    free(after); free(after2); free(before_after); free(duplicated);
}


static void block_and(Block *b1, Block *b2, Block *b3, mgu_schema *schemas, bool is_b3_lineal, Arena *arena_for_rows) {
    // TODO: as we see, important change to this matrix_intersection approach having to store pointers. Would be interesting to see how
    //  the other algorithms would need to be changed, and what approach becomes the new more efficient one.
    // NOTE: 2*2 instead of 2 in unifier_size because now, with intrusive list of rows in blocks, we don't have all rows
    //  (or main_terms before) consecutive in memory. Therefore, instead of storing unsigned indices (2*4B=8B), we are going to 
    //  store the pointers to the BlockRows that correspond to that unifier (2*8B=16B = 4*sizeof(unsigned))
    // We also need an extra slot (the 1 prior to 2*2) to store the index_mt that identifies which mgu_schema has been used to
    //  calculate that unifier.
    unsigned unifier_size = 1 + 2*b3->c + 1 + 2*2;
    unsigned *unifiers    = (unsigned *)malloc(b1->r * b2->r * unifier_size * sizeof(unsigned));
    unsigned  unif_count  = unifier_blocks(b1, b2, b3, schemas, is_b3_lineal, unifiers);

    for (unsigned i = 0; i < unif_count; i++) {

        BlockRow **address = (BlockRow **)(unifiers + (i * unifier_size + unifier_size));
        BlockRow *br2 = address[-1];
        BlockRow *br1 = address[-2];
        unsigned index_mt = *(((unsigned *)(address)) - 1);

        BlockRow *br3 = PUSH_SINGLE(arena_for_rows, *br3);
        br3->c = b3->c;
        add_row_to_block(b3, br3);
        br3->row = PUSH_ARRAY(arena_for_rows, *br3->row, br3->c);
                                     
        apply_unifier_left_to_block_rows(br1, br2, br3, &unifiers[i * unifier_size]);

        mgu_schema *schema = is_b3_lineal ? schemas : schemas + index_mt;
        reorder_unified_block_row(br3, schema);
    }

    free(unifiers);
}



Matrix matrix_and(Matrix *m1, Matrix *m2, Arena *operation_arena, Arena scratch_arena) {
    Matrix result; init_empty_matrix(&result);
    
    result.free_vars = final_free_vars_ordering(m1->free_vars, m2->free_vars, &result.arena);

    // NOTE: free var positions isn't stored inside the Matrix structure, since it relates data about free_vars in one matrix
    //  with data about resultant free_vars in the result Matrix. Therefore, their calculation is enclosed in matrix operation functions.
    // NOTE: compute free_var_positions1/2
    // NOTE: no resizing risk with these ArrayLists
    ArrayListUInt free_var_positions1 = create_array_list_uint_arena(result.free_vars.size, operation_arena);
    ArrayListUInt free_var_positions2 = create_array_list_uint_arena(result.free_vars.size, operation_arena);
    foreach_in_arraylist(CharPtr, free_v, result.free_vars){
        // NOTE: special value for not found = free_vars3.size
        unsigned pos = find_in_array_list_char_ptr(m1->free_vars, *free_v);
        if(pos == m1->free_vars.size){
            pos = result.free_vars.size;
        }
        unsafe_add_to_array_list(free_var_positions1, pos);

        pos = find_in_array_list_char_ptr(m2->free_vars, *free_v);
        if(pos == m2->free_vars.size){
            pos = result.free_vars.size;
        }
        unsafe_add_to_array_list(free_var_positions2, pos);
    }

    // NOTE: we can precalculate the starting indices of each term (that correspond to the free vars) using the sizes of the normalized schemas
    //  before entering the resulting blocks' loop.
    unsigned *starting_col_indices_array = PUSH_ARRAY(operation_arena, *starting_col_indices_array, (m1->free_vars.size * m1->b) + (m2->free_vars.size * m2->b));
    unsigned *starting_col_indices_array1 = starting_col_indices_array;
    unsigned *starting_col_indices_array2 = starting_col_indices_array + (m1->free_vars.size * m1->b);
    {
        unsigned *starting_indices = starting_col_indices_array1;
        Block *block;
        intrusive_list_for_each_entry(block, &m1->head_for_blocks, matrix_pos){
            starting_column_indexes(*block->normalized_set_schema, starting_indices);
            starting_indices += m1->free_vars.size;
        }
        intrusive_list_for_each_entry(block, &m2->head_for_blocks, matrix_pos){
            starting_column_indexes(*block->normalized_set_schema, starting_indices);
            starting_indices += m2->free_vars.size;
        }
    }

    // NOTE: loop to calculate the common set schemas, normalize them and calculate each resulting block
    {
        unsigned t1 = 0;
        Block *block1;
        intrusive_list_for_each_entry(block1, &m1->head_for_blocks, matrix_pos) {
            ++t1;
            unsigned t2 = 0;
            Block *block2;
            intrusive_list_for_each_entry(block2, &m2->head_for_blocks, matrix_pos) {
                ++t2;

                // Calculate common schema
                SetSchema *set_schema1 = block1->set_schema;
                SetDependencies *dependencies1 = block1->dependencies;

                SetSchema *set_schema2 = block2->set_schema;
                SetDependencies *dependencies2 = block2->dependencies;

                SetSchema *computed_common_set_schema = NULL;
                SetDependencies *computed_common_dependencies = NULL;

                clear_arena(&scratch_arena);
                ArenaState before_common_schema_state = register_state_arena(operation_arena);

                bool exists_common_schema = common_set_schema_free_vars_baseline(
                    *set_schema1, *dependencies1, free_var_positions1,
                    *set_schema2, *dependencies2, free_var_positions2,
                    computed_common_set_schema, computed_common_dependencies,
                    operation_arena, scratch_arena);

                if (exists_common_schema) {
                    ArenaState before_block3_computation = register_state_arena(&result.arena);

                    Block *block3 = PUSH_SINGLE(&result.arena, *block3);
                    block3->set_schema = computed_common_set_schema;
                    block3->dependencies = computed_common_dependencies;
                    block3->normalized_set_schema = normalized_set_schema_arena(*block3->set_schema, *block3->dependencies, operation_arena);
                    block3->c = set_schema_size(block3->normalized_set_schema);
                    block3->r = 0;
                    init_intrusive_list(&block3->head_for_rows);
                    //block3->matrix_pos later when added to the matrix, if it has at least one row!
                    
                    // Take arguments to calculate the mappings of column indexes
                    SetSchema *normalized_common_set_schema = block3->normalized_set_schema;
                    SetSchema *normalized_set_schema1 = block1->normalized_set_schema;
                    SetSchema *normalized_set_schema2 = block2->normalized_set_schema;

                    unsigned *starting_col_indices1 = starting_col_indices_array1 + (t1-1) * m1->free_vars.size;
                    unsigned *starting_col_indices2 = starting_col_indices_array2 + (t2-1) * m2->free_vars.size;

                    unsigned row_len1 = block1->c;
                    unsigned row_len2 = block2->c;
                    assert(set_schema_size(normalized_set_schema1) == row_len1);
                    assert(set_schema_size(normalized_set_schema1) == row_len2);

                    // Calculate the mapping of column indexes
                    mgu_schema computed_mapping;
                    computed_mapping.n_common = set_schema_size(normalized_common_set_schema);
                    unsigned num_cols1 = set_schema_size(normalized_set_schema1);
                    computed_mapping.new_a = computed_mapping.n_common - num_cols1;
                    unsigned num_cols2 = set_schema_size(normalized_set_schema2);
                    computed_mapping.new_b = computed_mapping.n_common - num_cols2;

                    computed_mapping.common_columns = PUSH_ARRAY(&scratch_arena, *computed_mapping.common_columns, computed_mapping.n_common);
                    for(unsigned i = 0; i < computed_mapping.n_common; ++i){ computed_mapping.common_columns[i] = i; }

                    unsigned mapping_side_size = computed_mapping.n_common * sizeof(unsigned);
                    mgu_schema *schemas; // = NULL;

                    bool block1_is_lineal = set_schema_contains_variables(*block1->set_schema);
                    bool block2_is_lineal = set_schema_contains_variables(*block2->set_schema);
                    bool block3_is_lineal = block1_is_lineal && block2_is_lineal;

                    if(block3_is_lineal){
                        unsigned *mapping_sides = allocate(&scratch_arena, 2 * mapping_side_size);
                        unsigned *mappingL = mapping_sides;
                        unsigned *mappingR = mapping_sides + computed_mapping.n_common;

                        assert(block1->r > 0 && block2->r > 0);
                        
                        BlockRow *first_row1 = intrusive_list_first_entry(&block1->head_for_rows, BlockRow, block_pos);
                        mapping_column_indexes_side_lineal(
                            normalized_set_schema1, free_var_positions1, normalized_common_set_schema,
                            starting_col_indices1, first_row1->row, mappingL, scratch_arena);
                            
                        BlockRow *first_row2 = intrusive_list_first_entry(&block2->head_for_rows, BlockRow, block_pos);
                        mapping_column_indexes_side_lineal(
                            normalized_set_schema2, free_var_positions2, normalized_common_set_schema,
                            starting_col_indices2, first_row2->row, mappingR, scratch_arena);

                        // Only one mapping in linear result block
                        computed_mapping.common_L = mappingL;
                        computed_mapping.common_R = mappingR;
                    
                        // We only use a schema for lineal blocks
                        schemas = PUSH_SINGLE(&scratch_arena, *schemas);
                        *schemas = computed_mapping;

                    } else {
                        // NOTE: the calculation of mappingL/R is independent of one another. We can precompute them in two linear loops instead of a quadratic nested loop.
                        unsigned *mapping_sides = allocate(&scratch_arena, (block1->r + block2->r) * mapping_side_size);
                        schemas = allocate(&scratch_arena, (block1->r)*(block2->r) * sizeof(*schemas));

                        unsigned *mapping_side = mapping_sides;

                        BlockRow *block_row;
                        intrusive_list_for_each_entry(block_row, &block1->head_for_rows, block_pos) {
                            mapping_column_indexes_side(
                                normalized_set_schema1, free_var_positions1, normalized_common_set_schema,
                                starting_col_indices1, block_row->row, mapping_side, scratch_arena
                            );
                            mapping_side += computed_mapping.n_common;
                        }
                        intrusive_list_for_each_entry(block_row, &block2->head_for_rows, block_pos) {
                            mapping_column_indexes_side(
                                normalized_set_schema2, free_var_positions2, normalized_common_set_schema,
                                starting_col_indices2, block_row->row, mapping_side, scratch_arena
                            );
                            mapping_side += computed_mapping.n_common;  
                        }

                        // TODO: instead of copying computed_mapping's header information per mapping_sideL/R combination,
                        //  we could pass the header only once per block to the matrix_intersection functions + the mapping sides information
                        //  (the cleanest way would be to modify the struct mgu_schema, taking into account that it can have more than one mapping
                        //  side depending on the linearity of the block).
                        mgu_schema *schema = schemas;
                        for(unsigned i = 0; i < block1->r; ++i){
                            computed_mapping.common_L = mapping_sides + i*computed_mapping.n_common;
                            for(unsigned j = 0; j < block2->r; ++j){
                                computed_mapping.common_R = mapping_sides + (block1->r + j)*computed_mapping.n_common;

                                // NOTE: We copy the structs (only the pointers) of all the schemas, one per ob1/2 combination
                                *schema = computed_mapping;
                                ++schema;
                            }
                        }
                    }

                    // And operation between blocks
                    // NOTE: since having one row is enough to consider this block as a block that will be stored in the 
                    //  memory of the matrix, we can store the rows directly in the memory of the Matrix.
                    block_and(block1, block2, block3, schemas, block3_is_lineal, &result.arena);

                    if(block3->r) {
                        // NOTE: operation_arena is not restored, so the common schemas, dependencies and normalized schemas are kept
                        //  for postprocessing, after which the definitive deepcopying will happen.
                        // NOTE: result.arena is not restored neither, so the block3 header remains in the matrix's data
                        add_block_to_matrix(&result, block3);
                    } else {
                        // No row in resultant block ==> No resultant block
                        // NOTE: the arenas are restored to avoid wasting memory
                        pop_to_state_arena(operation_arena, before_common_schema_state);
                        pop_to_state_arena(&result.arena, before_block3_computation);
                    }
                } else {
                    // Common Schema doesn't exist
                    pop_to_state_arena(operation_arena, before_common_schema_state);
                }
            }
        }
    }

    return result;
}

int main_postprocessed_(char *M1_file, char *M2_file, char *M3_file, bool verb) {
    var_dict  = create_dictionary(501);
    unif_dict = create_dictionary(501);
    symbols_to_ids = create_dictionary(501);

    verbose = verb;

    Matrix m1;
    ReadMatrixResultType read1 = read_matrix(M1_file, &m1);
    if (read1 != RM_SUCCESS) {
        assert(0);
        exit(1);
    }

    Matrix m2;
    ReadMatrixResultType read2 = read_matrix(M2_file, &m2);
    if (read2 != RM_SUCCESS) {
        assert(0);
        exit(1);
    }

    // TODO: this code is good to test with M4 files!
    Matrix m3;
    ReadMatrixResultType read3 = read_matrix(M3_file, &m3);
    if (read3 != RM_SUCCESS) {
        assert(0);
        exit(1);
    }

    // NOTE: important to adapt set_schemas2 adding the max_v found in set_schemas1 because the variables are logically independent!
    //  We are calculating the max variable among ALL set_schemas1, which is going to be summed to all set_schemas2.
    Variable max_v1 = 0;
    {
        Block *block;
        intrusive_list_for_each_entry(block, &m1.head_for_blocks, matrix_pos) {
            max_v1 = MAX(max_v1, max_v_in_set_schema(*block->set_schema));
        }
        intrusive_list_for_each_entry(block, &m2.head_for_blocks, matrix_pos) {
            increment_variables_in_set_schema(block->set_schema, max_v1);
            increment_variables_in_set_dependencies(*block->dependencies, max_v1);
        }
    }

    Arena operation_arena; init_arena(&operation_arena, sizeof(Schema) * 100000);
    Arena scratch_arena; init_arena_defcapacity(&scratch_arena);
    Matrix computed_m3 = matrix_and(&m1, &m2, &operation_arena, scratch_arena);
    // NOTE: passing operation_arena by value or by pointer is indifferent because it is freed just after the operation. Therefore, we pass by value to 
    //  avoid extra indirections in the function
    postprocess_to_mnf(&computed_m3, &operation_arena, scratch_arena);
    free_arena(scratch_arena);
    free_arena(operation_arena);

    // Normalize and deepcopy the resultant common schemas and dependencies
    {
        Block *block;
        intrusive_list_for_each_entry(block, &computed_m3.head_for_blocks, matrix_pos) {
            block->normalized_set_schema = set_schema_deepcopy(block->normalized_set_schema, &computed_m3.arena);
            block->set_schema = set_schema_deepcopy(block->set_schema, &computed_m3.arena);
            block->dependencies = set_dependencies_deepcopy(block->dependencies, &computed_m3.arena);

            normalize_schema_variables(block->set_schema, block->dependencies, scratch_arena);
        }
    }
    // Set Schema variables in set_schemas2 back to original names, 1..max, subtracting max_v1.
    {
        Block *block;
        intrusive_list_for_each_entry(block, &m2.head_for_blocks, matrix_pos) {
            decrement_variables_in_set_schema(block->set_schema, max_v1);
            decrement_variables_in_set_dependencies(*block->dependencies, max_v1);
        }
    }

    EqualMatricesResult are_equal = equal_matrices(&m3, &computed_m3);
    assert(are_equal.type == EQUAL);

    /* --- Cleanup --- */

    free_matrix(m1);
    free_matrix(m2);
    free_matrix(m3);

    free_dictionary(var_dict);
    free_dictionary(unif_dict);
    free_dictionary(symbols_to_ids);

    return 0;
}


int test_postprocessed_with_M3(char *M1_file, char *M2_file, char *M3_file, bool verb) {
    var_dict  = create_dictionary(501);
    unif_dict = create_dictionary(501);
    symbols_to_ids = create_dictionary(501);

    verbose = verb;

    // TODO: might be more interesting to open all the streams first, to exit as fast as possible if any file is missing/incorrectly named

    Matrix m1;
    ReadMatrixResultType read1 = read_matrix(M1_file, &m1);
    if (read1 != RM_SUCCESS) {
        assert(0);
        exit(1);
    }

    Matrix m2;
    ReadMatrixResultType read2 = read_matrix(M2_file, &m2);
    if (read2 != RM_SUCCESS) {
        assert(0);
        exit(1);
    }

    // NOTE: important to adapt set_schemas2 adding the max_v found in set_schemas1 because the variables are logically independent!
    //  We are calculating the max variable among ALL set_schemas1, which is going to be summed to all set_schemas2.
    Variable max_v1 = 0;
    {
        Block *block;
        intrusive_list_for_each_entry(block, &m1.head_for_blocks, matrix_pos) {
            max_v1 = MAX(max_v1, max_v_in_set_schema(*block->set_schema));
        }
        intrusive_list_for_each_entry(block, &m2.head_for_blocks, matrix_pos) {
            increment_variables_in_set_schema(block->set_schema, max_v1);
            increment_variables_in_set_dependencies(*block->dependencies, max_v1);
        }
    }

    Arena operation_arena; init_arena(&operation_arena, sizeof(Schema) * 100000);
    Arena scratch_arena; init_arena_defcapacity(&scratch_arena);
    Matrix computed_m3 = matrix_and(&m1, &m2, &operation_arena, scratch_arena);
    
    check_with_m3_file(&computed_m3, M3_file);

    free_arena(scratch_arena);
    free_arena(operation_arena);

    /* --- Cleanup --- */

    free_matrix(m1);
    free_matrix(m2);

    free_dictionary(var_dict);
    free_dictionary(unif_dict);
    free_dictionary(symbols_to_ids);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main_dir(char *folder_path, bool verb){
    // NOTE: instances must respect the format. Among other characteristics, the resulting fragments should be
    // in the expected order: 1-1, 1-2, ..., 1-n, 2-1, ... (with holes in case a resulting fragment doesn't exist
    // due to a lack of finite common schema)

    DIR *dir = opendir(folder_path);
    if (!dir) {
        perror("Error opening directory");
        return EXIT_FAILURE;
    }
    
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
                //strstr(base, "test0011") ||
                
                // Skip passed tests: AGT004+2
                //strstr(base, "test0001") || 
                //strstr(base, "test0004") ||

                // Skip passed tests: AGT006+2.p
                //strstr(base, "test0002") ||

                // Skip passed tests: ITP018+5.p
                strstr(base, "test0361") ||
                //strstr(base, "test0458") ||

                false)
            {
                continue;
            }

            // 4. Construct M2 and M3 paths
            snprintf(path_m2, sizeof(path_m2), "%sM2.csv", base);
            snprintf(path_m3, sizeof(path_m3), "%sM3.csv", base);

            // 5. Check if M2 and M3 exist (access F_OK is like [[ -f ]])
            if (access(path_m2, F_OK) == 0 && access(path_m3, F_OK) == 0) {
                if (verb) printf("%s\n", base);
                int code = main_postprocessed_(path_m1, path_m2, path_m3, verb);
                if (verb) printf("\nMain's return code = %d\n\n##########################################################\n", code);

                next_symbol_id = FIRST_FUNCTION_SYMBOL_POSITIVE;
                global_correct = true;
                global_count = 0;
                global_incorrect = 0;
                reset_global_timers();

            } else {
                printf("Skipping %s: M2 or M3 missing\n", base);
            }
        }
    }

    closedir(dir);

    return 0;
}

int main(){
#if 0

    // NOTE: instances must respect the format. Among other characteristics, the resulting fragments should be
    // in the expected order: 1-1, 1-2, ..., 1-n, 2-1, ... (with holes in case a resulting fragment doesn't exist
    // due to a lack of finite common schema)
    CharPtr folder_paths[] = {
        //"data/experimentation_matrices/AGT002+1",
        //"data/experimentation_matrices/AGT004+2",
        //"data/experimentation_matrices/AGT/AGT006+2.p",
        "data/experimentation_matrices/ITP/ITP018+5.p",
    };
    unsigned n = sizeof(folder_paths) / sizeof(folder_paths[0]);

    // If not verbose, print CSV header line
    bool verb = true;
    if (!verb) {
        printf("FILE,F1,L1,N1,C1,F2,L2,N2,C2,File_IO,Schema_Management,Mapping_Linear,Mapping_Nonlinear,Mapping_Nonlinear_Hashopt,Row_Ext_Linear,Row_Ext_Nonlinear,Original_Linear,Original_Nonlinear,Alt_Maps_Lin,Alt_Maps_Nonlin, Alt_Ext_Lin,Alt_Ext_Nonlin\n");
    }

    for (unsigned i = 0; i < n; ++i) {
        main_dir(folder_paths[i], verb);
    }
#else
    char *M1_file = "data/NUM/NUM291+1.p/test0001M1.csv";
    char *M2_file = "data/NUM/NUM291+1.p/test0001M2.csv";
    char *M3_file = "data/NUM/NUM291+1.p/test0001M3.csv";
    bool verb = true;
    test_postprocessed_with_M3(M1_file, M2_file, M3_file, verb);
#endif
}

#endif