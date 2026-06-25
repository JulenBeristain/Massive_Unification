#ifndef POSTPROCESSING_TO_MNF_H
#define POSTPROCESSING_TO_MNF_H

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CONTAINER OF ///////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h> // offsetof

// #define typeof_member(T, m)	typeof(((T*)0)->m)

#define container_of(member_ptr, type_struct, member_name) \
	((type_struct *)((void *)(member_ptr) - offsetof(type_struct, member_name)))

#define container_of_const(member_ptr, type_struct, member_name)				\
	_Generic(member_ptr,							\
		const typeof(*(member_ptr)) *: ((const type_struct *)container_of(member_ptr, type_struct, member_name)),\
		default: ((type_struct *)container_of(member_ptr, type_struct, member_name))	\
	)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// INTRUSIVE LIST (Doubly linked) /////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct IntrusiveListDouble IntrusiveListDouble, *IntrusiveListDoublePtr;
struct IntrusiveListDouble {
    IntrusiveListDouble *prev, *next;  
};


#define INTRUSIVE_LIST_INIT(list_var_name) \
    { &(list_var_name), &(list_var_name) }

#define INTRUSIVE_LIST(list_var_name) \
    IntrusiveList list_var_name = INTRUSIVE_LIST_INIT(list_var_name)

static inline void init_intrusive_list_double(IntrusiveListDouble *list) {
	list->next = list;
	list->prev = list;
}


static inline void __list_add(IntrusiveListDouble *new_elem, IntrusiveListDouble *prev, IntrusiveListDouble *next) {
    next->prev = new_elem;
	new_elem->next = next;
	new_elem->prev = prev;
	prev->next = new_elem;
}
// NOTE: add after the specified head, as the new first element (stacks)
static inline void intrusive_list_double_add(IntrusiveListDouble *head, IntrusiveListDouble *new_elem) {
	__list_add(new_elem, head, head->next);
}
// NOTE: add before the specified head, as the new last element (queues)
static inline void intrusive_list_double_add_tail(IntrusiveListDouble *head, IntrusiveListDouble *new_elem) {
	__list_add(new_elem, head->prev, head);
}


static inline void __list_del(IntrusiveListDouble *prev, IntrusiveListDouble *next) {
	next->prev = prev;
	prev->next = next;
}
static inline void intrusive_list_double_del(IntrusiveListDouble *entry_list) {
	__list_del(entry_list->prev, entry_list->next);
	// NOTE: entry_list's pointers could be set to NULL if I end up having hard to debug BUGs.
    //  I keep those pointers to be able to use the basic for_each macros even if the current
    //  entry is removed from the list (not when it is moved in the iteration!)
}

static inline void intrusive_list_double_move(IntrusiveListDouble *entry_list, IntrusiveListDouble *head) {
	__list_del(entry_list->prev, entry_list->next);
	intrusive_list_double_add(head, entry_list);
}

static inline void intrusive_list_double_move_tail(IntrusiveListDouble *entry_list, IntrusiveListDouble *head) {
	__list_del(entry_list->prev, entry_list->next);
	intrusive_list_double_add_tail(head, entry_list);
}


static inline int intrusive_list_double_is_first(const IntrusiveListDouble *list, const IntrusiveListDouble *head) {
	return list->prev == head;
}
//#define intrusive_list_is_first(intrusive_list_ptr, head_ptr) \
//    ((intrusive_list_ptr)->prev == (head_ptr))
// NOTE: as macros, I can use these for both IntrusiveLists and IntrusiveListSingles
#define intrusive_list_is_last(intrusive_list_ptr, head_ptr) \
    ((intrusive_list_ptr)->next == (head_ptr))

#define intrusive_list_is_head(intrusive_list_ptr, head_ptr) \
    ((intrusive_list_ptr) == (head_ptr))

#define intrusive_list_is_empty(head_ptr) \
    ((head_ptr)->next == (head_ptr))

#define list_entry_is_head(container_ptr, headptr, list_member_name)				\
    intrusive_list_is_head(&container_ptr->list_member_name, (headptr))


// Renaming of container_of_const for intrusive lists
#define intrusive_list_entry(list_member_ptr, type_container, list_member_name) \
    container_of_const(list_member_ptr, type_container, list_member_name)

#define intrusive_list_first_entry(head_ptr, type_container, list_member_name) \
    intrusive_list_entry((head_ptr)->next, type_container, list_member_name)

#define intrusive_list_double_last_entry(head_ptr, type_container, list_member_name) \
    intrusive_list_entry((head_ptr)->prev, type_container, list_member_name)

#define intrusive_list_next_entry(current_container_ptr, list_member_name) \
    intrusive_list_entry((current_container_ptr)->list_member_name.next, typeof(*(current_container_ptr)), list_member_name)

#define intrusive_list_double_prev_entry(current_container_ptr, list_member_name) \
	intrusive_list_entry((current_container_ptr)->list_member_name.prev, typeof(*(current_container_ptr)), list_member_name)

// NOTE: if intrusive_list_del doesn't modify the removed entry_list itself, so its next member keeps pointing to the next intrusive list
//  instance, these basic versions could be used instead of the safe versions (if the next pointer is not modified later!).
#define intrusive_list_for_each(intrusive_list_ptr, head_ptr) \
	for (intrusive_list_ptr = (head_ptr)->next; !intrusive_list_is_head(intrusive_list_ptr, (head_ptr)); intrusive_list_ptr = intrusive_list_ptr->next)

#define intrusive_list_for_each_since(intrusive_list_started_ptr, head_ptr) \
	for (; !intrusive_list_is_head(intrusive_list_started_ptr, (head_ptr)); intrusive_list_started_ptr = intrusive_list_started_ptr->next)

// NOTE: exclusive end, until_ptr entry is not included.
#define intrusive_list_for_each_until(intrusive_list_ptr, head_ptr, until_ptr) \
	for (intrusive_list_ptr = (head_ptr)->next; intrusive_list_ptr != (until_ptr); intrusive_list_ptr = intrusive_list_ptr->next)

#define intrusive_list_for_each_safe(intrusive_list_ptr, next_buffer_ptr, head_ptr) \
	for (intrusive_list_ptr = (head_ptr)->next, next_buffer_ptr = intrusive_list_ptr->next; \
	     !intrusive_list_is_head(intrusive_list_ptr, (head_ptr)); \
	     intrusive_list_ptr = next_buffer_ptr, next_buffer_ptr = intrusive_list_ptr->next)


#define intrusive_list_for_each_entry(container_ptr, head_ptr, list_member_name)				\
    for (container_ptr = intrusive_list_first_entry(head_ptr, typeof(*container_ptr), list_member_name);	\
         !list_entry_is_head(container_ptr, head_ptr, list_member_name);			\
         container_ptr = intrusive_list_next_entry(container_ptr, list_member_name))

#define intrusive_list_for_each_entry_since(container_started_ptr, head_ptr, list_member_name)				\
    for (; !list_entry_is_head(container_started_ptr, head_ptr, list_member_name);			\
         container_started_ptr = intrusive_list_next_entry(container_started_ptr, list_member_name))

// NOTE: exclusive end, until_ptr entry is not included.
#define intrusive_list_for_each_entry_until(container_ptr, head_ptr, list_member_name, until_ptr)				\
    for (container_ptr = intrusive_list_first_entry(head_ptr, typeof(*container_ptr), list_member_name);	\
         &container_ptr->list_member_name != &(until_ptr)->list_member_name;			\
         container_ptr = intrusive_list_next_entry(container_ptr, list_member_name))

// NOTE: exclusive end, until_ptr entry is not included.
#define intrusive_list_for_each_entry_since_until(container_started_ptr, head_ptr, list_member_name, until_ptr)				\
    for (; &container_started_ptr->list_member_name != &(until_ptr)->list_member_name;			\
         container_started_ptr = intrusive_list_next_entry(container_started_ptr, list_member_name))

#define intrusive_list_for_each_entry_safe(container_ptr, next_buffer_ptr, head_ptr, list_member_name)			\
	for (container_ptr = intrusive_list_first_entry(head_ptr, typeof(*container_ptr), list_member_name),	\
		next_buffer_ptr = intrusive_list_next_entry(container_ptr, list_member_name);			\
	     !list_entry_is_head(container_ptr, head_ptr, list_member_name); 			\
	     container_ptr = next_buffer_ptr, next_buffer_ptr = intrusive_list_next_entry(next_buffer_ptr, list_member_name))

// NOTE: the reverse for each loop are exclusive for IntrusiveListDoubles, because we need the prev pointers.
#define intrusive_list_double_for_each_entry_reverse(container_ptr, head_ptr, list_member_name)			\
	for (container_ptr = intrusive_list_double_last_entry(head_ptr, typeof(*container_ptr), list_member_name);		\
	     !list_entry_is_head(container_ptr, head_ptr, list_member_name); 			\
	     container_ptr = intrusive_list_double_prev_entry(container_ptr, list_member_name))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// INTRUSIVE LIST (Singly Linked) /////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: see what kind of intrusive list is more efficient in terms of time.

// NOTE: I think that singly linked intrusive lists are interesting in our use case, because we save a pointer per each
//  row (we have a lot of them in resulting matrices), we don't care about the order of the rows (we can arbitrarily add
//  rows only at the beginning of the other fragment, no need for tail additions), and since we remove or move rows while 
//  we are iterating over the fragments we can easily get the previous node's pointer to delete in O(1) time (without the 
//  need of having a prev pointer in each node).

// NOTE: if we needed to add to the tail, we could modify the Head to be an IntrusiveList (with next and prev pointers),
//  without having to add a prev pointer to each node.

typedef struct IntrusiveListSingle IntrusiveListSingle, *IntrusiveListSinglePtr;
struct IntrusiveListSingle {
    IntrusiveListSingle *next;
};


// NOTE: initialization for circular intrusive lists, even with single links.
#define INTRUSIVE_LIST_SINGLE_INIT(list_var_name) \
    { &(list_var_name) }

#define INTRUSIVE_LIST(list_var_name) \
    IntrusiveList list_var_name = INTRUSIVE_LIST_INIT(list_var_name)

static inline void init_intrusive_list_single(IntrusiveListSingle *list) {
	list->next = list;
}


static inline void intrusive_list_single_add(IntrusiveListSingle *head, IntrusiveListSingle *new_elem) {
    new_elem->next = head->next;
    head->next = new_elem;
}


static inline void __list_del_single(IntrusiveListSingle *prev, IntrusiveListSingle *next) {
	prev->next = next;
}
static inline void intrusive_list_single_del(IntrusiveListSingle *entry_list, IntrusiveListSingle *prev) {
	__list_del_single(prev, entry_list->next);
    // NOTE: entry_list's pointers could be set to NULL if I end up having hard to debug BUGs.
    //  I keep those pointers to be able to use the basic for_each macros even if the current
    //  entry is removed from the list (not when it is moved in the iteration!)
}


static inline void intrusive_list_single_move(IntrusiveListSingle *entry_list, IntrusiveListSingle *prev, IntrusiveListSingle *head) {
	__list_del_single(prev, entry_list->next);
	intrusive_list_single_add(head, entry_list);
}

static inline int intrusive_list_single_is_first(const IntrusiveListSingle *prev, const IntrusiveListSingle *head) {
	return prev == head;
}

// NOTE: we can't have for_each macros for IntrusiveListSingles. The prev_ptr would be initialized to head, but it advances conditionally,
//  only when the current entry hasn't been removed. Therefore, we can't define a simple for macro. We can use the previous intrusive list
//  for each macros with extra logic to handle a prev_ptr for deletions.

// NOTE: generic macros and typedef to toggle which IntrusiveList to use
typedef IntrusiveListDouble IntrusiveList, *IntrusiveListPtr;

#define init_intrusive_list(list) _Generic((list),          \
    IntrusiveListDoublePtr: init_intrusive_list_double,     \
    IntrusiveListSinglePtr: init_intrusive_list_single      \
)(list)

#define intrusive_list_add(head, new_elem) _Generic((head), \
    IntrusiveListDoublePtr: intrusive_list_double_add,      \
    IntrusiveListSinglePtr: init_intrusive_list_single      \
)(head, new_elem)

// NOTE: if single list pass prev as a second argument
#define intrusive_list_del(entry_list, ...) _Generic((entry_list), \
    IntrusiveListDoublePtr: intrusive_list_double_del,       \
    IntrusiveListSinglePtr: intrusive_list_single_del        \
)(entry_list, ##__VA_ARGS__)

// NOTE: if double, second param head; if single, second param prev, third param head
#define intrusive_list_move(entry_list, ...) _Generic((entry_list), \
    IntrusiveListDoublePtr: intrusive_list_double_move,       \
    IntrusiveListSinglePtr: intrusive_list_single_move        \
)(entry_list, ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// STRUCTURES /////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: rename block_pos ==> pos_in_block or position_of_row/row_s_position/row_position (same with matrix)

typedef struct {
    unsigned c;                     // Number of columns in the main term
//    unsigned e;                   // Number of exception blocks for the main term
    int *row;                       // 1D array containing the values of the main term
    IntrusiveList block_pos;        // The intrusive list that stablishes the position of this main term in its block.
//    exception_block *exceptions;  // Array with e exception blocks for the main term (TODO: we may need to modify to not have the array, but intrusive lists of exception blocks)
} BlockRow, *BlockRowPtr;

#include "Schemas/schemas.h"

typedef struct {
    unsigned r;                         // Number of main terms
    unsigned c;                         // Number of columns for all main terms within the block
    IntrusiveList head_for_rows;        // Anchor point for the rows of this block. NOTE: we only add resulting rows that have been unified (valid[i,j] = 0)
    IntrusiveList matrix_pos;           // The intrusive list that stablishes the position of this block in its matrix.
    SetSchema *set_schema;              // The common set schema of the rows in the block. NOTE: we can deduce the block is lineal if it contains any schema variable.
    SetSchema *normalized_set_schema;   // set_schema normalized. All these schemas and dependencies are stored in the Matrix's memory.
    SetDependencies *dependencies;      // The set of dependencies associated to the schema.
} Block, *BlockPtr;

// TODO: could store a pointer to free_vars so free_vars is allocated in the arena, alongside its contents for better locality.
typedef struct {
    unsigned b;                     // Number of blocks in the matrix
    IntrusiveList head_for_blocks;  // Anchor point for the blocks of this matrix.
    ArrayListCharPtr free_vars;     // Free var information (strings and array of pointers stored in the arena)
    Arena arena;                    // Arena to manage all the memory necessary for the matrix's blocks, rows and final deepcopied schemas and dependencies
} Matrix, *MatrixPtr;



typedef struct {
    unsigned op1;
    unsigned op2;
    bool is_in_corresponding_block;

    unsigned c;                     // Number of columns in the main term
//    unsigned e;                   // Number of exception blocks for the main term
    int *row;                       // 1D array containing the values of the main term
    IntrusiveList block_pos;        // The intrusive list that stablishes the position of this main term in its block.
//    exception_block *exceptions;  // Array with e exception blocks for the main term (TODO: we may need to modify to not have the array, but intrusive lists of exception blocks)
} TestingBlockRow, *TestingBlockRowPtr;

typedef struct {
    unsigned op1;
    unsigned op2;
    
    unsigned r;                         // Number of main terms
    unsigned c;                         // Number of columns for all main terms within the block
    IntrusiveList head_for_rows;        // Anchor point for the rows of this block. NOTE: we only add resulting rows that have been unified (valid[i,j] = 0)
    IntrusiveList matrix_pos;           // The intrusive list that stablishes the position of this block in its matrix.
    SetSchema *set_schema;              // The common set schema of the rows in the block. NOTE: we can deduce the block is lineal if it contains any schema variable.
    SetSchema *normalized_set_schema;   // set_schema normalized. All these schemas and dependencies are stored in the Matrix's memory.
    SetDependencies *dependencies;      // The set of dependencies associated to the schema.
} TestingBlock, *TestingBlockPtr;



// NOTE: we introduce at the beginning because we don't care about the order, and it is possible with both kinds of IntrusiveLists.
static inline void add_row_to_block(Block *block, BlockRow *row) {
    intrusive_list_add(&block->head_for_rows, &row->block_pos);
    block->r++;
}

static inline void move_row_to_block(Block *block, BlockRow *row) {
    intrusive_list_move(&row->block_pos, &block->head_for_rows);
    block->r++;
}

static inline void remove_block(Block *block) {
    intrusive_list_del(&block->matrix_pos);
}
static inline void remove_block_from_matrix(Matrix *matrix, Block *block) {
    remove_block(block);
    matrix->b--;
}

// NOTE: we introduce at the beginning because we don't care about the order, and it is possible with both kinds of IntrusiveLists.
static inline void add_block_to_matrix(Matrix *matrix, Block *block) {
    intrusive_list_add(&matrix->head_for_blocks, &block->matrix_pos);
    matrix->b++;
}

static inline void init_empty_matrix(Matrix *matrix) {
    matrix->b = 0;
    init_intrusive_list(&matrix->head_for_blocks);
    matrix->free_vars = EMPTY_ARRAYLIST(CharPtr);
    // TODO: when testing with real instances, we can take here a more appropriate value to reduce the number of Memory Blocks
    init_arena(&matrix->arena, KILOBYTES(4));
}
// NOTE: arena_size used as reference. The real size is the smallest greater power of 2 between 4kB and 4MB.
static inline void init_empty_matrix_with_arena_size(Matrix *matrix, size_t arena_size) {
    matrix->b = 0;
    init_intrusive_list(&matrix->head_for_blocks);
    matrix->free_vars = EMPTY_ARRAYLIST(CharPtr);

    arena_size = CLAMP(smallest_greater_power_of_2(arena_size), KILOBYTES(4), MEGABYTES(4));
    init_arena(&matrix->arena, arena_size);
}


// NOTE: after freeing, shouldn't use the same Matrix variable without proper reinitialization
static inline void free_matrix_v (Matrix m) {
    free_arena(m.arena);
}
static inline void free_matrix_p (Matrix *m) { 
    free_arena(m->arena); 
}
#define free_matrix(m)              \
    _Generic((m),				    \
		Matrix *: free_matrix_p,    \
		Matrix:   free_matrix_v 	\
	)(m)

// NOTE: after clearing, you can use the same Matrix variable. It keeps the same memory usage as before.
static inline void clear_matrix(Matrix *m){
    clear_arena(&m->arena);
    m->b = 0;
    m->free_vars = EMPTY_ARRAYLIST(CharPtr);
    init_intrusive_list(&m->head_for_blocks);
}


void postprocess_to_mnf(Matrix *matrix, Arena *operation_arena, Arena scratch_arena);

typedef enum { 
    DIMENSION_MISMATCH, 
    ELEMENT_MISMATCH, 
    EQUAL,
    NOT_EQUIVALENT_SCHEMAS_AND_DEPENDENCIES,
    NOT_EQUAL_NORMALIZED_SCHEMAS,
    NOT_CORRESPONDING_ROW,
    NOT_CORRESPONDING_BLOCK, 
    NOT_EQUAL_FREEVARS,
} EqualMatricesResultType;

typedef struct {
    EqualMatricesResultType type;
    unsigned solitary_block;
} EqualMatricesResult;
#define EQUAL_MATRICES_RESULT(Type, Index) (EqualMatricesResult){ .type = (Type), .solitary_block = (Index) }

EqualMatricesResult equal_matrices(Matrix *m1, Matrix *m2);


typedef enum {
    RM_FILE_NOT_OPENED, 
    RM_NOT_BLOCK_COUNT,
    RM_NOT_FREE_VARS, 
    RM_SUCCESS 
} ReadMatrixResultType;

ReadMatrixResultType read_matrix(char *filename, Matrix *matrix);

ReadMatrixResultType read_matrix_3(char *M3_file, Matrix *m3);

void check_with_m3(Matrix *computed_m3, Matrix *m3);
void check_with_m3_file(Matrix *computed_m3, char *M3_file);

#endif