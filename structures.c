#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "structures.h"

// ===<<< BEGIN EXCEPTION BLOCK >>>=== //
exception_block create_null_exception_block() {
    exception_block eb;
    eb.n = 0;
    eb.m = 0;
    eb.ms  = NULL;
    eb.mat = NULL;
    return eb;
}

exception_block create_empty_exception_block(unsigned n, unsigned m) {
    exception_block eb;
    eb.n = n;
    eb.m = m;
    eb.ms = NULL;
    // printf("n,m: %u, %u\n",n,m); // Check
    if (n!=0)
    {
        eb.mat = (int*)malloc(n * m * sizeof(int));

        if (!eb.mat) {
            fprintf(stderr, "Failed to allocate exception_block\n");
            free(eb.mat);
            exit(EXIT_FAILURE);
        }
    }
    else eb.mat = NULL;
    return eb;
}

exception_block create_exception_block(unsigned n, unsigned m, mgu_schema *ms, int* mat) {
    exception_block eb = create_empty_exception_block(n,m);
    eb.ms = deep_copy_mgu_schema(ms);
    if (n!=0) memcpy(eb.mat, mat, n * m * sizeof(int));
    return eb;
}

void free_exception_block(exception_block* eb) {
    if (eb) {
        // print_exception_block(eb,3,0); // Check
        if (eb->ms) 
        {
            free_mgu_schema(eb->ms);
            eb->ms = NULL;
        }
        free(eb->mat);
        eb->mat = NULL;
    }
}

void print_exception_block(exception_block* eb, unsigned matrix_idx, unsigned exc_blk_idx) {
    printf("%% BEGIN: Exception subset%u.%u (%u,%u)\n",matrix_idx,exc_blk_idx,eb->n,eb->m);
    // print_mgu_schema(eb->ms); // Too much
    print_mgu_compact(eb->ms);

    for (unsigned i = 0; i < eb->n; i++) {
        printf("Row %u: ",i);
        for (unsigned j = 0; j < eb->m - 1; j++)
            printf("%d,", eb->mat[i * eb->m + j]);
        printf("%d\n", eb->mat[i * eb->m + eb->m - 1]);
    }
    printf("%% END: Exception subset%u.%u\n",matrix_idx,exc_blk_idx);
}
// ===<<< END EXCEPTION BLOCK >>>=== //

// ===<<< BEGIN MAIN TERM >>>=== //
main_term create_null_main_term() {
    main_term mt;
    mt.c = 0;
    mt.e = 0;
    mt.row = NULL;
    mt.exceptions = NULL;
    mt.ms = NULL;
    return mt;
}

main_term create_empty_main_term(unsigned c, unsigned e) {
    main_term mt;
    mt.c = c;
    mt.e = e;
    mt.row = (int*)malloc(c * sizeof(int));
    if (e!=0) mt.exceptions = (exception_block*)malloc(e * sizeof(exception_block));
    else mt.exceptions = NULL;
    
    if (((e!=0 && (!mt.row || !mt.exceptions))) || (e==0 && !mt.row)) {
        fprintf(stderr, "Failed to allocate main_term\n");
        free(mt.row);
        free(mt.exceptions);
        exit(EXIT_FAILURE);
    }

    for (size_t aux = 0; aux < c; aux++)
    {
        mt.row[aux] = 0;
    }

    for (unsigned i = 0; i < e; ++i) {
        mt.exceptions[i] = create_null_exception_block();
    }

    mt.ms = NULL;

    return mt;
}

main_term create_main_term(unsigned c, unsigned* row, unsigned e, exception_block* exceptions) {
    main_term mt = create_empty_main_term(c,e);

    memcpy(mt.row, row, c * sizeof(int));
    for (unsigned i = 0; i < e; i++) {
        mt.exceptions[i] = exceptions[i];
    }
    return mt;
}

void free_main_term(main_term* mt) {
    if (mt) {
        // print_main_term(mt,3,1); // Check

        if (mt->exceptions)
        {
            for (unsigned i = 0; i < mt->e; i++) {free_exception_block(&mt->exceptions[i]);}
            free(mt->exceptions);
            mt->exceptions = NULL;
        }
        free(mt->row);
        mt->row = NULL;

        if (mt->ms) free_mgu_schema(mt->ms);
        mt->ms = NULL;
    }
}

void print_main_term(main_term* mt, unsigned matrix_idx, int verbosity) {
    // Print line
    printf("%u, ",mt->e);
    for (unsigned i = 0; i < mt->c - 1; i++)
        printf("%d,", mt->row[i]);
    printf("%d\n", mt->row[mt->c - 1]);

    // Print exceptions
    if (verbosity)
    {
        for (unsigned i = 0; i < mt->e; i++) {
            print_exception_block(&mt->exceptions[i], matrix_idx, i+1);
        }
    }
}
// ===<<< END MAIN TERM >>>=== //

// ===<<< BEGIN OPERAND BLOCK >>>=== //
operand_block create_null_operand_block() {
    operand_block ob;
    ob.r = 0;
    ob.c = 0;
    ob.terms = NULL;
    return ob;
}

operand_block create_empty_operand_block(unsigned r, unsigned c) {
    operand_block ob;
    ob.r = r;
    ob.c = c;
    ob.terms = (main_term*)malloc(r * sizeof(main_term));

    if (!ob.terms) {
        fprintf(stderr, "Failed to allocate operand_block\n");
        exit(EXIT_FAILURE);
    }
    return ob;
}

operand_block create_operand_block(unsigned r, unsigned c, main_term* terms) {
    operand_block ob = create_empty_operand_block(r,c);
    for (unsigned i = 0; i < r; i++) {
        ob.terms[i] = terms[i];
    }
    return ob;
}

void free_operand_block(operand_block* ob) {
    if (ob) {
        for (unsigned i = 0; i < ob->r; i++)
        {
            free_main_term(&ob->terms[i]);
        }
        free(ob->terms);
        ob->terms = NULL;
    }
}

// Verbosity levels: 0 - Only print basic operand_block info; 1 - Print rows information from each main_term; 2 - Print exceptions from main_term's exception blocks
void print_operand_block(operand_block* ob, unsigned matrix_idx, int verbosity) {
    printf("Operand Block: %u terms, %u columns\n", ob->r, ob->c);
    if (verbosity)
    {
        for (unsigned i = 0; i < ob->r; i++) {
            printf("== Main Term %u ==\n", i);
            print_main_term(&ob->terms[i], matrix_idx, verbosity - 1);
        }
    }
}
// ===<<< END OPERAND BLOCK >>>=== //

// ===<<< BEGIN RESULT BLOCK >>>=== //
result_block create_null_result_block() {
    result_block rb; // NOTE: this function is equivalent to: result_block rb = {}; // A zero-initialization of a struct (the latter is the preferred option...)
    rb.t1 = 0;
    rb.t2 = 0;
    rb.r1 = 0;
    rb.r2 = 0;
    rb.r = 0;
    rb.c1 = 0;
    rb.c2 = 0;
    rb.c = 0;
    rb.terms = NULL;
    rb.valid = NULL;
    rb.ms = NULL;
    rb.lineal_lineal = false;   // NOTE: this was causing non-linear blocks to fail the obtention of the mgu_schema in the parsing phase...
    return rb;
}

result_block create_empty_result_block(unsigned r1, unsigned r2, unsigned c1, unsigned c2, unsigned c, mgu_schema *ms) {
    result_block rb;
    rb.t1 = 0;
    rb.t2 = 0;
    rb.r1 = r1;
    rb.r2 = r2;
    rb.r = r1 * r2;
    rb.c1 = c1;
    rb.c2 = c2;
    rb.c = c;
    rb.ms = deep_copy_mgu_schema(ms);

    rb.terms = (main_term*)malloc(rb.r * sizeof(main_term));
    rb.valid = (unsigned*)malloc(rb.r * sizeof(unsigned));

    if (!rb.terms || !rb.valid) {
        fprintf(stderr, "Failed to allocate result_block\n");
        exit(EXIT_FAILURE);
    }

    for (unsigned i = 0; i < rb.r; ++i) {
        rb.terms[i] = create_null_main_term();  // sets row=NULL, exceptions=NULL
        rb.valid[i] = 2;                        // so free_result_block will skip it
    }
    
    rb.lineal_lineal = false; // False by default
    
    return rb;
}

result_block create_result_block(unsigned t1, unsigned t2, unsigned r1, unsigned r2, unsigned c1, unsigned c2, unsigned c, main_term* terms, unsigned* valid, mgu_schema *ms) {
    result_block rb = create_empty_result_block(r1, r2, c1, c2, c, ms);
    rb.t1 = t1;
    rb.t2 = t2;

    for (unsigned i = 0; i < rb.r; i++) {
        rb.terms[i] = terms[i];
        rb.valid[i] = valid[i];
    }

    return rb;
}

void free_result_block(result_block* rb) {
    if (rb) {
        // print_result_block(rb,1); // Check
        for (unsigned i = 0; i < rb->r; i++)
            if (rb->valid[i]!=2) free_main_term(&rb->terms[i]);

        free(rb->terms);
        free(rb->valid);
        free_mgu_schema(rb->ms);
        rb->ms = NULL;
    }
}

// Verbosity levels: 0 - Only print basic result_block info; 1 - Print rows information from each main_term; 2 - Print exceptions from main_term's exception blocks
void print_result_block(result_block* rb, int verbosity) {
    printf("Result Block: t1=%u, t2=%u | %u x %u terms (%u unified), c1=%u, c2=%u, c=%u\n",
           rb->t1, rb->t2, rb->r1, rb->r2, rb->r, rb->c1, rb->c2, rb->c);

    if (verbosity) {
        for (unsigned i = 0; i < rb->r; i++) {
            // printf("== Result Term [%u][%u] (valid=%u) ==\n", i / rb->r2, i % rb->r2, rb->valid[i]);
            if (rb->valid[i]<=1) 
            {
                if (rb->valid[i]==1) printf("Rows %u-%u subsumed by exception\n",i/rb->r2+1, i%rb->r2+1);
                printf("Row %u-%u: ",i/rb->r2+1, i%rb->r2+1);
                print_main_term(&rb->terms[i], 3, verbosity - 1);
            }
            else printf("Rows %u-%u not unifiable\n",i/rb->r2+1, i%rb->r2+1);
        }
    }
}
// ===<<< END RESULT BLOCK >>>=== //

// ===<<< BEGIN MGU SCHEMA >>>=== //
mgu_schema* create_empty_mgu_schema(const unsigned n_common) {
    mgu_schema *ms = malloc(sizeof(mgu_schema));
    if (!ms) {
        fprintf(stderr, "Unable to allocate memory for mgu_schema\n");
        exit(EXIT_FAILURE);
    }

    ms->n_common = n_common;
    ms->common_columns = n_common ? (unsigned*)malloc(n_common * sizeof(unsigned)) : NULL;
    ms->common_L       = n_common ? (unsigned*)malloc(n_common * sizeof(unsigned)) : NULL;
    ms->common_R       = n_common ? (unsigned*)malloc(n_common * sizeof(unsigned)) : NULL;

    // Initialize all values, good thing for uninitialized value jumping
    if (ms->common_columns) memset(ms->common_columns, 0, n_common         * sizeof(unsigned));
    if (ms->common_L)       memset(ms->common_L,       0, n_common         * sizeof(unsigned));
    if (ms->common_R)       memset(ms->common_R,       0, n_common         * sizeof(unsigned));
    
    return ms;
}


void mgu_schema_fix(mgu_schema *ms)
{
    unsigned n        = ms->n_common;
    unsigned left_cols  = n - ms->new_a;
    unsigned right_cols = n - ms->new_b;

    // These arrays track “old -> new” assignments for fakes
    unsigned *oldL = malloc(ms->new_a * sizeof *oldL);
    unsigned *newL = malloc(ms->new_a * sizeof *newL);
    unsigned *oldR = malloc(ms->new_b * sizeof *oldR);
    unsigned *newR = malloc(ms->new_b * sizeof *newR);
    unsigned countL = 0, countR = 0;

    for (unsigned i = 1; i <= n; ++i) {
        unsigned lx = ms->common_L[i];
        if (lx > left_cols) {
            // Fake on left-lookup or assign
            unsigned j;
            for (j = 0; j < countL; ++j) {
                if (oldL[j] == lx) {
                    ms->common_L[i] = newL[j];
                    break;
                }
            }
            if (j == countL) {
                // First time we see this fake
                unsigned assigned = left_cols + countL + 1;
                oldL[countL] = lx;
                newL[countL] = assigned;
                ms->common_L[i] = assigned;
                ++countL;
            }
        }

        unsigned ry = ms->common_R[i];
        if (ry > right_cols) {
            // Fake on right-lookup or assign 
            unsigned j;
            for (j = 0; j < countR; ++j) {
                if (oldR[j] == ry) {
                    ms->common_R[i] = newR[j];
                    break;
                }
            }
            if (j == countR) {
                unsigned assigned = right_cols + countR + 1;
                oldR[countR] = ry;
                newR[countR] = assigned;
                ms->common_R[i] = assigned;
                ++countR;
            }
        }
    }

    free(oldL);
    free(newL);
    free(oldR);
    free(newR);
}



/**
 * Creates a mgu_schema object from a list of 2*n unsigneds, which contains the mapping between columns of left-right terms, in X-Y pairs.
 * Left-Right is the same as A-B in this context
 *
 * @param n     Number of pairs in mapping, or same as number of columns in the result of unifying the two terms
 * @param n_L   Number of columns in left term
 * @param n_R   Number of colums in rigth term
 * @return      A pointer to the mgu_schema struct, initialized with correct values according to the mapping
 */
mgu_schema* create_mgu_from_mapping(unsigned *mapping, const unsigned n, const unsigned n_L, const unsigned n_R) {

    mgu_schema *ms = create_empty_mgu_schema(n);

    for (unsigned i=0; i<n; i++)
    {
        unsigned x = mapping[2*i];
        unsigned y = mapping[2*i+1];
        ms->common_columns[i] = i;
        ms->common_L[i] = x;
        ms->common_R[i] = y;
    }

    // printf("n: %u, n_L: %u, n_R: %u\n",n,n_L,n_R); // Check
    ms->new_a = n-n_L;
    ms->new_b = n-n_R;

    // mgu_schema_fix(ms);

    return ms;
}

mgu_schema* deep_copy_mgu_schema(const mgu_schema* ms) {
    if (!ms) return NULL;

    mgu_schema* result = create_empty_mgu_schema(ms->n_common);

    memcpy(result->common_columns, ms->common_columns,   ms->n_common           * sizeof(unsigned));
    memcpy(result->common_L,       ms->common_L,         ms->n_common           * sizeof(unsigned));
    memcpy(result->common_R,       ms->common_R,         ms->n_common           * sizeof(unsigned));
    result->new_a = ms->new_a;
    result->new_b = ms->new_b;

    return result;
}

void free_mgu_schema(mgu_schema* ms) {
    if (ms) {
        if (ms->common_columns) free(ms->common_columns);
        if (ms->common_L) free(ms->common_L);
        if (ms->common_R) free(ms->common_R);
        free(ms);
    }
}

void print_mgu_schema(mgu_schema* ms) {
    printf("common_columns: [");
    for (unsigned i = 0; i < ms->n_common; i++) {
        if (i==(ms->n_common-1)) printf("%u", ms->common_columns[i]);
        else printf("%u, ", ms->common_columns[i]);
    }
    printf("]\n");

    printf("common_L:       [");
    for (unsigned i = 0; i < ms->n_common; i++) {
        if (i==(ms->n_common-1)) printf("%u", ms->common_L[i]);
        else printf("%u, ", ms->common_L[i]);
    }
    printf("]\n");

    printf("common_R:       [");
    for (unsigned i = 0; i < ms->n_common; i++) {
        if (i==(ms->n_common-1)) printf("%u", ms->common_R[i]);
        else printf("%u, ", ms->common_R[i]);
    }
    printf("]\n");

    printf("left term has %u new columns\n",ms->new_a);
    printf("right term has %u new columns\n",ms->new_b);
}

void print_mgu_compact(mgu_schema *ms) {

    // Fill common entries // X-Y schema
    for (unsigned i = 0; i < ms->n_common; i++) {
        unsigned left  = ms->common_L[i];
        unsigned right = ms->common_R[i];
        printf("%u-%u", left, right);
        if (i + 1 < ms->n_common) printf(",");
    }
    printf("\n");
}

bool equal_mgu_schemas(mgu_schema *ms1, mgu_schema *ms2){
    if(ms1->n_common != ms2->n_common || ms1->new_a != ms2->new_a || ms1->new_b != ms2->new_b){
        return false;
    }

    for(unsigned i = 0; i < ms1->n_common; ++i){
        bool not_same_i = ms1->common_columns[i] != ms2->common_columns[i] ||
                      ms1->common_L[i] != ms2->common_L[i] ||
                      ms1->common_R[i] != ms2->common_R[i];
        if(not_same_i)
        {
            return false;
        }
    }

    return true;
}

// ===<<< END MGU SCHEMA >>>=== //

// ===<<< BEGIN L2, L3 >>>=== //
L2 create_L2_empty(){
    L2 node;
    node.ind   = -1;
    node.by    = -1;
    node.count = 0;
    node.head  = NULL;
    node.tail  = NULL;
    return node;
}

L2 create_L2(int ind_in, int count_in, int by_in, L3* head_in, L3* tail_in){
    L2 node;
    node.ind   = ind_in;
    node.count = count_in;
    node.by    = by_in;
    node.head  = head_in;
    node.tail  = tail_in;
    return node;
}

L3* create_L3_empty(){
    L3* node   = (L3*) malloc(sizeof(L3));
    node->ind  = -1;
    node->next = NULL;
    return node;
}

L3* create_L3(int ind_in, L3* next_in){
    L3* node   = (L3*) malloc(sizeof(L3));
    node->ind  = ind_in;
    node->next = next_in;
    return node;
}

void free_L2(L2 node){
    if (node.head) free_L3(node.head);
}

void free_L3(L3* node){
    if (!node)      return;
    if (node->next) free_L3(node->next);
    free(node);
}

L2 copy_L2(L2 node) {
    L2 new_node = create_L2(node.ind, node.count, node.by, NULL, NULL);
    if (node.head) 
    {
        new_node.head = copy_L3(node.head);
        L3 *current = new_node.head;
        while (current->next != NULL) {current = current->next;}
        new_node.tail = current;
    } 
    else {new_node.head = new_node.tail = NULL;}
    return new_node;
}

L3* copy_L3(L3* node) {
    if (!node) return NULL;
    L3* new_node = create_L3(node->ind, NULL);
    if (node->next) new_node->next = copy_L3(node->next);
    return new_node;
}
// ===<<< END L2, L3 >>>=== //


int timespec_subtract (struct timespec *result, struct timespec *x, struct timespec *y)
{
    if (x->tv_nsec < y->tv_nsec)
    {
        int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
        y->tv_nsec -= 1000000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_nsec - y->tv_nsec > 1000000000)
    {
        int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
        y->tv_nsec += 1000000000 * nsec;
        y->tv_sec -= nsec;
    }

    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_nsec = x->tv_nsec - y->tv_nsec;

    return x->tv_sec < y->tv_sec;
}