#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <stdio.h>

/* Structs */
typedef struct L2 L2;
typedef struct L3 L3;


struct L2 {
    int ind, count, by;
    L3 *head, *tail;  
};

struct L3 {
    int ind;
    L3 *next;  
};


typedef struct {
    unsigned n_common;         // Number of columns in common between two main terms. 
    unsigned *common_columns;  // 1D array holding the indexes of the common columns. NOTE: Columns will be stored from idx 1
    unsigned *common_L;        // 1D array holding the indexes of the common columns in the left main term. NOTE: Columns will be stored from idx 1
    unsigned *common_R;        // 1D array holding the indexes of the common columns in the right main term. NOTE: Columns will be stored from idx 1
    unsigned new_a;            // Number of new columns added to left term
    unsigned new_b;            // Number of new columns added to right term
} mgu_schema;

typedef struct {
    unsigned n;     // Number of exceptions within the exception block
    unsigned m;     // Number of columns for all exceptions within the exception block
    mgu_schema *ms; // Represents the mapping between two operand main terms' columns
    int *mat;       // 1D matrix of size n*m containing the exceptions
} exception_block;

typedef struct {
    unsigned c;                  // Number of columns in the main term
    int *row;                    // 1D array containing the values of the main term
    unsigned e;                  // Number of exception blocks for the main term
    mgu_schema *ms;              // Represents the mapping between two operand main terms' columns. Do not alloc nor initialize this in main_term declaration
    exception_block *exceptions; // Array with e exception blocks for the main term
} main_term;

typedef struct {
    unsigned r;       // Number of main terms
    unsigned c;       // Number of columns for all main terms within the block
    main_term *terms; // r main_term array, represents a block within an operand matrix
} operand_block;

typedef struct {
    unsigned t1;        // ID of the operand block of M1
    unsigned t2;        // ID of the operand block of M2
    unsigned r1;        // Number of main terms in t1
    unsigned r2;        // Number of main terms in t2
    unsigned r;         // Number of unified main terms (not in csv file header, computed after the file is read)
    unsigned c1;        // Number of columns for main terms in t1 
    unsigned c2;        // Number of columns for main terms in t2 
    unsigned c;         // Number of columns for all main terms within the result block
    main_term *terms;   // r1*r2 main_term array, represents a block within an operand matrix
    unsigned *valid;    // 1D array of r1*r2 elements that give info about the main terms, 0 for unified, 1 for subsumed by exception, and 2 for not unifiable
    mgu_schema *ms;     // Represents the mapping between two operand main terms' columns
    bool lineal_lineal; // Indicates if a result block is composed of two lineal operant matrix blocks, so that all main terms in the resulting block can share the mgu_schema and save a lot of memory 
} result_block;

/* Functions */
exception_block create_null_exception_block();
exception_block create_empty_exception_block(unsigned n, unsigned m);
exception_block create_exception_block(unsigned n, unsigned m, mgu_schema *ms, int* mat);
void free_exception_block(exception_block* eb);
void print_exception_block(exception_block* eb, unsigned matrix_idx, unsigned exc_blk_idx);

main_term create_null_main_term();
main_term create_empty_main_term(unsigned c, unsigned e);
main_term create_main_term(unsigned c, unsigned* row, unsigned e, exception_block* exceptions);
void free_main_term(main_term* mt);
void print_main_term(main_term* mt, unsigned matrix_idx, int verbosity);

operand_block create_null_operand_block();
operand_block create_empty_operand_block(unsigned r, unsigned c);
operand_block create_operand_block(unsigned r, unsigned c, main_term* terms);
void free_operand_block(operand_block* ob);
void print_operand_block(operand_block* ob, unsigned matrix_idx, int verbosity);

result_block create_null_result_block();
result_block create_empty_result_block(unsigned r1, unsigned r2, unsigned c1, unsigned c2, unsigned c, mgu_schema *ms);
result_block create_result_block(unsigned t1, unsigned t2, unsigned r1, unsigned r2, unsigned c1, unsigned c2, unsigned c, main_term* terms, unsigned* valid, mgu_schema *ms);
void free_result_block(result_block* rb);
void print_result_block(result_block* rb, int verbosity);

mgu_schema* create_empty_mgu_schema(const unsigned n_common);
mgu_schema* create_mgu_schema(const unsigned m, unsigned* columns, unsigned* mappings_L, unsigned* mappings_R);
mgu_schema* create_mgu_from_mapping(unsigned *mapping, const unsigned n, const unsigned n_L, const unsigned n_R); 
mgu_schema* deep_copy_mgu_schema(const mgu_schema* ms);
void free_mgu_schema(mgu_schema* ms);
void print_mgu_schema(mgu_schema* ms);
void print_mgu_compact(mgu_schema *ms);

L2 create_L2_empty();
L2 create_L2(int ind_in, int count_in, int by_in, L3* head_in, L3* tail_in);
L3* create_L3_empty();
L3* create_L3(int ind_in, L3* next_in);
void free_L2(L2 node);
void free_L3(L3* node);
L2 copy_L2(L2 node);
L3* copy_L3(L3* node);

int timespec_subtract (struct timespec *result, struct timespec *x, struct timespec *y);

#endif