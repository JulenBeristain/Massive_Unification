#ifndef INTRUSIVE_LIST_H
#define INTRUSIVE_LIST_H

// TODO(YA): postprocessing to recover MNF
// - Check if a row is not well placed in a fragment
//      - Calculate the rows corresponding schema using the schema of the resulting fragment and the row itself.
//      It should be an instantiation (particularization) of fragment's schema, as well as the resulting row is a
//      particularization of the operand rows.
//      - Check if the fragment's schema and the row's schema cumply the strict conditions. If they do, the row is
//      well placed. If not, it has to be moved to another fragment. If there is no more rows in the fragment, 
//      remove it from the matrix too.
//
// - Check if there is a fragment that cumplies the strict conditions with the row's schema.
//      - If it exists, introduce the row to that fragment.
//      - If it doesn't, create a new_elem fragment with the row and the row's schema as the new_elem fragment's schema too.
//
// - We have to perform the previous operations for all rows in every fragment in the resulting matrix.
//
// - Operations to be implemented:
//      - instantiate_schema(row, fragment_schema) -> row_schema [What about the sets of dependencies?]
//      - check_corresponding_fragment(row_schema, fragment_schema) -> bool [Use first_check, unique_dependency_between_vars, and remove_variables_not_in_set_schema_from_dependencies in case of true!]
//      - postprocess(matrix) -> void [matrix to MNF]
//      - remove_main_term(main_term) -> void [With intrusive lists main_term should be enough]
//      - introduce_main_term(fragment, main_term) -> void [Have to decide to introduce it at the beginning or the end]
//      - create_fragment(main_term, row_schema) -> fragment
//      - introduce_fragment(matrix, fragment) -> void
//      - remove_fragment(fragment) -> void [With intrusive lists main_term should be enough]
//
// - Structures to define or modify:
//      - main_term (row): we are going to add a Linux Kernel style intrusive list to them to iterate over them, remove in O(1) while we iterate and introduce in O(1) too (either to the beginning or the end)
//      - fragment: based on operand_block and result_block + intrusive lists -> matrix_block
//      - matrix: we can have a header with metadata + a pointer to the first element in the intrusive list of fragments

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CONTAINER OF ///////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stddef.h> // offsetof

// #define typeof_member(T, m)	typeof(((T*)0)->m)

#define container_of(member_ptr, type_struct, member_name) \
	((type_struct *)((void *)(member_ptr) - offsetof(type_struct, member_name)))

#define container_of_const(member_ptr, type_struct, member_name)				\
	_Generic(member_ptr,							\
		const typeof(*(member_ptr)) *: ((const type *)container_of(member_ptr, type_struct, member_name)),\
		default: ((type *)container_of(member_ptr, type_struct, member_name))	\
	)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// INTRUSIVE LIST (Doubly linked) /////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct IntrusiveList IntrusiveList, *IntrusiveListPtr;
struct IntrusiveList {
    IntrusiveList *prev, *next;  
};


#define INTRUSIVE_LIST_INIT(list_var_name) \
    { &(list_var_name), &(list_var_name) }

#define INTRUSIVE_LIST(list_var_name) \
    IntrusiveList list_var_name = INTRUSIVE_LIST_INIT(list_var_name)

static inline void init_intrusive_list(IntrusiveList *list) {
	list->next = list;
	list->prev = list;
}


static inline void __list_add(IntrusiveList *new_elem, IntrusiveList *prev, IntrusiveList *next) {
    next->prev = new_elem;
	new_elem->next = next;
	new_elem->prev = prev;
	prev->next = new_elem;
}
// NOTE: add after the specified head, as the new first element (stacks)
static inline void intrusive_list_add(IntrusiveList *head, IntrusiveList *new_elem) {
	__list_add(new_elem, head, head->next);
}
// NOTE: add before the specified head, as the new last element (queues)
static inline void intrusive_list_add_tail(IntrusiveList *head, IntrusiveList *new_elem) {
	__list_add(new_elem, head->prev, head);
}


static inline void __list_del(IntrusiveList *prev, IntrusiveList *next) {
	next->prev = prev;
	prev->next = next;
}
static inline void intrusive_list_del(IntrusiveList *entry_list) {
	__list_del(entry_list->prev, entry_list->next);
	// TODO(YA): decide if in our case it would be helpful to init_intrusive_list(entry_list), or put it to NULL, or a POISON address
    //  I think not, because every time a row is removed from a fragment, it has to be moved to another fragment (see
    //  intrusive_list_move[_tail]). We cannot say the same about removed fragments, in that case, we could do something else.
    //  Plus, if we keep entry_list's pointers, we can use the typical for_each macros without the need to use the safe versions.
}


static inline void intrusive_list_move(IntrusiveList *entry_list, IntrusiveList *head) {
	__list_del(entry_list->prev, entry_list->next);
	intrusive_list_add(head, entry_list);
}

static inline void intrusive_list_move_tail(IntrusiveList *entry_list, IntrusiveList *head) {
	__list_del(entry_list->prev, entry_list->next);
	intrusive_list_add_tail(head, entry_list);
}


// Renaming of container_of_const for intrusive lists
#define intrusive_list_entry(list_member_ptr, type_container, list_member_name) \
    container_of_const(list_member_ptr, type_container, list_member_name)

#define intrusive_list_first_entry(head_ptr, type_container, list_member_name) \
    intrusive_list_entry((head_ptr)->next, type_container, list_member_name)

#define intrusive_list_last_entry(head_ptr, type_container, list_member_name) \
    intrusive_list_entry((head_ptr)->prev, type_container, list_member_name)

#define intrusive_list_next_entry(current_container_ptr, list_member_name) \
    intrusive_list_entry((current_container_ptr)->list_member_name.next, typeof(*(current_container_ptr)), list_member_name)


static inline int intrusive_list_is_first(const IntrusiveList *list, const IntrusiveList *head) {
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


// NOTE: if intrusive_list_del doesn't modify the removed entry_list itself, so its next member keeps pointing to the next intrusive list
//  instance, the safe versions could be removed.
#define intrusive_list_for_each(intrusive_list_ptr, head_ptr) \
	for (intrusive_list_ptr = (head_ptr)->next; !intrusive_list_is_head(intrusive_list_ptr, (head_ptr)); intrusive_list_ptr = intrusive_list_ptr->next)

#define intrusive_list_for_each_safe(intrusive_list_ptr, next_buffer_ptr, head_ptr) \
	for (intrusive_list_ptr = (head_ptr)->next, next_buffer_ptr = intrusive_list_ptr->next; \
	     !intrusive_list_is_head(intrusive_list_ptr, (head_ptr)); \
	     intrusive_list_ptr = next_buffer_ptr, next_buffer_ptr = intrusive_list_ptr->next)

#define intrusive_list_for_each_entry(container_ptr, head_ptr, list_member_name)				\
    for (container_ptr = intrusive_list_first_entry(head_ptr, typeof(*container_ptr), list_member_name);	\
         !list_entry_is_head(container_ptr, head_ptr, list_member_name);			\
         container_ptr = intrusive_list_next_entry(container_ptr, list_member_name))

#define intrusive_list_for_each_entry_safe(container_ptr, next_buffer_ptr, head_ptr, list_member_name)			\
	for (container_ptr = intrusive_list_first_entry(head_ptr, typeof(*container_ptr), list_member_name),	\
		next_buffer_ptr = intrusive_list_next_entry(container_ptr, list_member_name);			\
	     !list_entry_is_head(container_ptr, head_ptr, list_member_name); 			\
	     container_ptr = next_buffer_ptr, next_buffer_ptr = intrusive_list_next_entry(next_buffer_ptr, list_member_name))


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// INTRUSIVE LIST (Singly Linked) /////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO(OPT): see what kind of intrusive list is more efficient in terms of time.

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
	// TODO(YA): decide if in our case it would be helpful to init_intrusive_list(list), or put it to NULL, or a POISON address
    //  I think not, because every time a row is removed from a fragment, it has to be moved to another fragment (see
    //  intrusive_list_single_move). We cannot say the same about removed fragments, in that case, we could do something else.
    //  Plus, if we keep entry_list's pointers, we can use the typical for_each macros (+ prev_ptr logic) without the need to use the safe versions.
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// STRUCTURES /////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO(YA): define these types properly

typedef struct BlockRow BlockRow, *BlockRowPtr;
struct BlockRow {
    int *data;
    IntrusiveList fragment_pos;
};

typedef struct Block Block, *BlockPtr;
struct Block {
    int *data;
    IntrusiveList matrix_pos;
};

typedef struct Matrix Matrix, *MatrixPtr;
struct Matrix {
    int *data;
};

#endif