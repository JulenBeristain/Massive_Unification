#ifndef ARRAYLIST_H
#define ARRAYLIST_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "arena.h"

// TODO: make an extension for VSCode that expands macros automatically inplace, removing the macro
//  use, substituting (or commenting and appending) it by the text that appears in the "Expands to" 
//  section when you hover over it, and calling to the clang.formatter.

// NOTE: Arenas don't behave nicely with ArrayLists when the latters resize. All the previous memory used
//  by the ArrayList to store the elements becomes garbage when the ArrayList takes a new greater chunk
//  from the Arena. Therefore, if a lot of resizing happens, the risk of running out of memory in the Arena
//  increases, as well as the amount of unused memory.
//
//  Therefore, avoid using dynamic structures that can resize with Arenas, or prevent the resizing from happening.
//  In the case of ArrayLists: 1) give great initial capacity (or just enough if it can be precomputed) taken from 
//  the Arena. 2) just use malloc (preferably with just enough capacity to avoid resizing).
//
//  Additionally, to mix the use of malloc and Arenas for the same ArrayList we would need an extra flag to know if
//  the previous allocation was done by malloc or not. If that was the case, before taking memory from the arena in 
//  a resizing allocation, we would need to free the malloced memory. DON'T DO THIS, very error prone...


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DECLARATION OF ARRAYLIST TYPES MACRO ////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: for user defined types, I use Pascal Case. For default C types, use typedef. F.ex., int -> Int
#define DECLARE_ARRAYLIST_TYPE(Type)                \
    typedef struct ArrayList##Type ArrayList##Type; \
    struct ArrayList##Type {                        \
        uint32_t size;                              \
        uint32_t capacity;                          \
        Type* array;                                \
    };

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ENUMS FOR RETURNING CODES FOR ARRAYLIST FUNCTIONS ///////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: the functions simply return ints. These codes are used for every type of ArrayList.
typedef enum ArrayListAddReturnCode { NOT_RESIZED, RESIZED, CONTAINED } ArrayListAddReturnCode;
typedef enum ArrayListRemoveReturnCode { OUT_OF_BOUNDS, SUCCESSFUL_REMOVAL } ArrayListRemoveReturnCode;
typedef enum ArrayListGetReturnCode { INVALID_INDEX, VALID_INDEX } ArrayListGetReturnCode;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FOR EACH LOOP MACROS ////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define foreach_in_arraylist(Type, valptr, list)                                                    \
    for(Type *valptr = (list).array, *_end = (list).array + (list).size; valptr < _end; ++valptr)   \

#define foreach_in_arraylistptr(Type, valptr, listptr)                                                          \
    for(Type *valptr = (listptr)->array, *_end = (listptr)->array + (listptr)->size; valptr < _end; ++valptr)   \

#define foreach_in_arraylists(Type, iter_short, iter_long, list_short, list_long) \
    for(Type *iter_short = (list_short).array, *iter_long = (list_long).array, \
             *_end = (list_short).array + (list_short).size; \
    iter_short < _end; \
    ++iter_short, ++iter_long)

#define foreach_in_arraylistptrs(Type, iter_short, iter_long, list_short, list_long) \
    for(Type *iter_short = (list_short)->array, *iter_long = (list_long)->array, \
             *_end = (list_short)->array + (list_short)->size; \
    iter_short < _end; \
    ++iter_short, ++iter_long)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MACROS FOR DECLARATION OF ARRAYLIST FUNCTIONS (and definition of static inline functions) ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_ARRAYLIST_CREATE(Type, type)                        \
    ArrayList##Type create_array_list_##type(uint32_t capacity);    \
    ArrayList##Type create_array_list_##type##_defcapacity();

// NOTE: we don't have the version with default capacity for arraylists whose underlying array is allocated in an arena
//  because it's a bad idea to use an array whose runtime size isn't precomputable and thus will probably be resized.
#define DECLARE_ARRAYLIST_CREATE_ARENA(Type, type)                                      \
    ArrayList##Type create_array_list_##type##_arena(uint32_t capacity, Arena *arena);

// NOTE: only for arraylists whose internal array was allocated with malloc. Only the internal array is freed.
// NOTE: defining the version that receives the pointer to the arraylist is pointless, as arraylists are only 16B.
#define DEFINE_ARRAYLIST_FREE(Type, type)                                                   \
    static inline void free_array_list_##type(ArrayList##Type list){ free(list.array); }

// NOTE: this should only be called if the objects have been allocated with malloc only in the arraylist of pointers,
//  and not anywhere else (if not, dangling pointers).
#define DECLARE_ARRAYLIST_FREE_POINTERS(Type, type)      \
    void free_pointers_array_list_##type(ArrayList##Type list);


#define DEFINE_ARRAYLIST_CLEAR(Type, type)                                                  \
    static inline void clear_array_list_##type(ArrayList##Type *list){ list->size = 0; }

// NOTE: this should only be called if the objects have been allocated with malloc only in the arraylist of pointers,
//  and not anywhere else (if not, dangling pointers).
#define DECLARE_ARRAYLIST_CLEAR_POINTERS(Type, type) \
    void clear_pointers_array_list_##type(ArrayList##Type *list);


// NOTE: the element has the same type as the underlying array in the list. The appending is by definition done by
//  copying. If the size of Type is big and copying was expensive, then simply use arraylists of pointers to Type
//  to start with; i.e., ElemTypePtr as Type (starting from the arraylist type declaration).
#define DECLARE_ARRAYLIST_ADD(Type, type)                                                   \
    int add_to_array_list_##type(ArrayList##Type *list, Type element);

#define DECLARE_ARRAYLIST_ADD_ARENA(Type, type)                                             \
    int add_to_array_list_##type##_arena(ArrayList##Type *list, Type element, Arena *arena);


// NOTE: the list that is used to extend the other is passed by value since it isn't modified and only occupies 16B
#define DECLARE_ARRAYLIST_EXTEND(Type, type)                                                \
    int extend_array_list_##type(ArrayList##Type *list_to_extend, ArrayList##Type list);

#define DECLARE_ARRAYLIST_EXTEND_ARENA(Type, type)                                                              \
    int extend_array_list_##type##_arena(ArrayList##Type *list_to_extend, ArrayList##Type list, Arena *arena);


// NOTE: list by value because it isn't modified. When list.size returned, elem not found.
// NOTE: no need to have distinct versions because the equal_func in the definition will always follow the same
//  criteria than the ArrayList: if the type is small, by value. If the type is big, by pointer.
#define DECLARE_ARRAYLIST_FIND(Type, type) \
    unsigned find_in_array_list_##type(ArrayList##Type list, Type elem);

#define DEFINE_ARRAYLIST_CONTAINS(Type, type)                                           \
    static inline bool contains_array_list_##type(ArrayList##Type list, Type elem){     \
        return list.size != find_in_array_list_##type(list, elem);                      \
    }


#define DECLARE_ARRAYLIST_ADD_NO_REPEATED(Type, type) \
    int add_no_repeated_to_array_list_##type(ArrayList##Type *list, Type elem);

#define DECLARE_ARRAYLIST_ADD_NO_REPEATED_ARENA(Type, type) \
    int add_no_repeated_to_array_list_##type##_arena(ArrayList##Type *list, Type elem, Arena *arena);

#define DECLARE_ARRAYLIST_EXTEND_NO_REPEATED(Type, type) \
    int extend_no_repeated_array_list_##type(ArrayList##Type *list_to_extend, ArrayList##Type list);

#define DECLARE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(Type, type) \
    int extend_no_repeated_array_list_##type##_arena(ArrayList##Type *list_to_extend, ArrayList##Type list, Arena *arena);


#define DECLARE_ARRAYLIST_REMOVE_INDEX(Type, type)                                          \
    int remove_index_from_array_list_##type(ArrayList##Type *list, uint32_t index);
// NOTE: this version should only be called if the only place where the pointers to the allocated memory exist is in the
//  arraylist itself, to avoid dangling pointers.
#define DECLARE_ARRAYLIST_REMOVE_POINTER_INDEX(Type, type)                                  \
    int remove_pointer_index_from_array_list_##type(ArrayList##Type *list, uint32_t index);

// NOTE: mixing passing by value the element to remove and having an arraylist of pointers doesn't make sense, because
//  if a type is small enough so that passing by value is more efficient, then it would also be more interesting to have
//  an arraylist of values instead of pointers. Pointers can be an instrument to implement some optimizations like caching 
//  repeated nodes, but even in that case, as each node has to appear only once in memory, and in other places the code will
//  work using pointers, the other functions like equal, print, and so on should also work with pointers.
#define DECLARE_ARRAYLIST_REMOVE_ELEMENT(Type, type)                                \
    int remove_element_from_array_list_##type(ArrayList##Type *list, Type element);

#define DECLARE_ARRAYLIST_REMOVE_POINTER_ELEMENT(Type, type)                                \
    int remove_pointer_element_from_array_list_##type(ArrayList##Type *list, Type element);


#define DECLARE_ARRAYLIST_GET(Type, type)                                                   \
    int get_from_array_list_##type(ArrayList##Type list, uint32_t index, Type *result);


// NOTE: in the definition, the print function will follow the same convention of the ArrayList's Type, value or pointer
#define DECLARE_ARRAYLIST_PRINT_SEPARATORS_1(Type, type, ArgType1, Arg1)        \
    void print_separators_array_list_##type(                                    \
        ArrayList##Type list, char opening_brace, char closing_brace,           \
        char *elem_separator, bool one_line_per_elem, unsigned tab_num,         \
        ArgType1 Arg1);

#define DECLARE_ARRAYLIST_PRINT_SEPARATORS(Type, type)                   \
    void print_separators_array_list_##type(                             \
        ArrayList##Type list, char opening_brace, char closing_brace,    \
        char *elem_separator, bool one_line_per_elem, unsigned tab_num); \

#define DECLARE_ARRAYLIST_PRINT(Type, type)             \
    void print_array_list_##type(ArrayList##Type list);

#define DECLARE_ARRAYLIST_PRINTLN(Type, type)           \
    void println_array_list_##type(ArrayList##Type list);


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MACROS FOR ARRAYLIST FUNCTION DEFINITIONS ///////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CREATE //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ARRAYLIST_CREATE(Type, type)                                     \
    ArrayList##Type create_array_list_##type(uint32_t capacity){                \
        ArrayList##Type list;                                                   \
        list.size = 0;                                                          \
        list.capacity = capacity;                                               \
        if(capacity){                                                           \
            list.array = malloc(capacity * sizeof(*list.array));                \
            if (list.array == NULL){                                            \
                perror("create_array_list: malloc failed to allocate memory");  \
                exit(EXIT_FAILURE);                                             \
            }                                                                   \
        }                                                                       \
        else { list.array = NULL; }                                             \
        return list;                                                            \
    }                                                                           \
                                                                                \
    ArrayList##Type create_array_list_##type##_defcapacity(){                   \
        enum { DEFAULT_ARRAY_LIST_SIZE = 10 };                                  \
        return create_array_list_##type(DEFAULT_ARRAY_LIST_SIZE);               \
    }

// NOTE: not default capacity for Arena because we are only allocating from it when there is no resizing
#define DEFINE_ARRAYLIST_CREATE_ARENA(Type, type)                                       \
    ArrayList##Type create_array_list_##type##_arena(uint32_t capacity, Arena *arena){  \
        ArrayList##Type list;                                                           \
        list.size = 0;                                                                  \
        list.capacity = capacity;                                                       \
        list.array = allocate(arena, capacity * sizeof(*list.array));                   \
        return list;                                                                    \
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FREE and CLEAR //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: the free and clear of ArrayLists that don't call to 'free' after a malloc per each
//  member are defined as static inlines.

/**
 * NOTE: the size and capacity that were remaining are not important. 
 *       After a free, the ArrayList shouldn't be used anymore.
 */
#define DEFINE_ARRAYLIST_FREE_POINTERS(Type, type)              \
    void free_pointers_array_list_##type(ArrayList##Type list){ \
        foreach_in_arraylist(Type, iter, list){                 \
            free(*iter);                                        \
        }                                                       \
        free(list.array);                                       \
    }

#define DEFINE_ARRAYLIST_CLEAR_POINTERS(Type, type)                 \
    void clear_pointers_array_list_##type(ArrayList##Type *list){   \
        foreach_in_arraylistptr(Type, iter, list){                  \
            free(*iter);                                            \
        }                                                           \
        list->size = 0;                                             \
    }                                                         

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ADDITION ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define unsafe_add_to_array_list(list, elem) (list).array[(list).size++] = (elem)
#define unsafe_add_to_array_list_ptr(listptr, elem) (listptr)->array[(listptr)->size++] = (elem)

#define DEFINE_ARRAYLIST_ADD(Type, type)                                            \
int add_to_array_list_##type(ArrayList##Type *list, Type element){                  \
    int code = NOT_RESIZED;                                                         \
    if (list->size == list->capacity){                                              \
        if(list->capacity){ list->capacity *= 2; }                                  \
        else              { list->capacity  = 1; }                                  \
        list->array = realloc(list->array, list->capacity * sizeof(*list->array));  \
        if (list->array == NULL){                                                   \
            perror("add_to_array_list: realloc failed to allocate memory");         \
            exit(EXIT_FAILURE);                                                     \
        }                                                                           \
        code = RESIZED;                                                             \
    }                                                                               \
    list->array[list->size] = element;                                              \
    ++list->size;                                                                   \
    return code;                                                                    \
}

// NOTE: no need for a not_arena version, because realloc already copies the elements.
// NOTE: this is a helper function. We don't need a declaration macro for a .h file.
#define DEFINE_ARRAYLIST_RESIZE_ARENA(Type, type)                                                   \
void resize_array_list_##type##_arena(ArrayList##Type *list, unsigned new_capacity, Arena *arena){  \
    assert(list->capacity < new_capacity);                                                          \
    unsigned content_num_bytes = list->capacity * sizeof(*list->array);                             \
    list->capacity = new_capacity;                                                                  \
    unsigned alloc_num_bytes = new_capacity * sizeof(*list->array);                                 \
    void *new_array = allocate(arena, alloc_num_bytes);                                             \
    memcpy(new_array, list->array, content_num_bytes);                                              \
    list->array = new_array;                                                                        \
}

#define DEFINE_ARRAYLIST_ADD_ARENA(Type, type)                                              \
int add_to_array_list_##type##_arena(ArrayList##Type *list, Type element, Arena *arena){    \
    int code = NOT_RESIZED;                                                                 \
    if (list->size == list->capacity){                                                      \
        unsigned new_capacity = list->capacity ? list->capacity * 2 : 1;                    \
        resize_array_list_##type##_arena(list, new_capacity, arena);                        \
        code = RESIZED;                                                                     \
    }                                                                                       \
    list->array[list->size] = element;                                                      \
    ++list->size;                                                                           \
    return code;                                                                            \
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EXTENSION ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ARRAYLIST_EXTEND(Type, type)                                                                                 \
int extend_array_list_##type(ArrayList##Type *list_to_extend, ArrayList##Type list){                                        \
    int code = NOT_RESIZED;                                                                                                 \
                                                                                                                            \
    unsigned extended_size = list_to_extend->size + list->size;                                                             \
    if(extended_size > list_to_extend->capacity){                                                                           \
        list_to_extend->capacity = 2 * extended_size;                                                                       \
        list_to_extend->array = realloc(list_to_extend->array, list_to_extend->capacity * sizeof(*list_to_extend->array));  \
        if (list_to_extend->array == NULL){                                                                                 \
            perror("extend_array_list: realloc failed to allocate memory");                                                 \
            exit(EXIT_FAILURE);                                                                                             \
        }                                                                                                                   \
        code = RESIZED;                                                                                                     \
    }                                                                                                                       \
                                                                                                                            \
    foreach_in_arraylist(Type, elemptr, list){                                                                              \
        add_to_array_list_##type(list_to_extend, *elemptr);                                                                 \
    }                                                                                                                       \
                                                                                                                            \
    return code;                                                                                                            \
}

#define DEFINE_ARRAYLIST_EXTEND_ARENA(Type, type)                                                           \
int extend_array_list_##type##_arena(ArrayList##Type *list_to_extend, ArrayList##Type list, Arena *arena){  \
    int code = NOT_RESIZED;                                                                                 \
                                                                                                            \
    unsigned extended_size = list_to_extend->size + list.size;                                              \
    if(extended_size > list_to_extend->capacity){                                                           \
        resize_array_list_##type##_arena(list_to_extend, 2 * extended_size, arena);                         \
        code = RESIZED;                                                                                     \
    }                                                                                                       \
                                                                                                            \
    foreach_in_arraylist(Type, elemptr, list){                                                              \
        add_to_array_list_##type##_arena(list_to_extend, *elemptr, arena);                                  \
    }                                                                                                       \
                                                                                                            \
    return code;                                                                                            \
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FIND ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ARRAYLIST_FIND(Type, type, equal_func)                       \
    unsigned find_in_array_list_##type(ArrayList##Type list, Type elem){    \
        unsigned i = 0;                                                     \
        foreach_in_arraylist(Type, iter, list){                             \
            if(equal_func(elem, *iter)){                                    \
                return i;                                                   \
            }                                                               \
            ++i;                                                            \
        }                                                                   \
        return list.size;                                                   \
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SET OPERATIONS //////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ARRAYLIST_ADD_NO_REPEATED(Type, type)                            \
    int add_no_repeated_to_array_list_##type(ArrayList##Type *list, Type elem){ \
        if(contains_array_list_##type(*list, elem)){                            \
            return CONTAINED;                                                   \
        }                                                                       \
        return add_to_array_list_##type(list, elem);                            \
    }

#define DEFINE_ARRAYLIST_ADD_NO_REPEATED_ARENA(Type, type) \
    int add_no_repeated_to_array_list_##type##_arena(ArrayList##Type *list, Type elem, Arena *arena) {  \
        if(contains_array_list_##type(*list, elem)){                                                    \
                return CONTAINED;                                                                       \
        }                                                                                               \
        return add_to_array_list_##type##_arena(list, elem, arena);                                     \
    }

#define DEFINE_ARRAYLIST_EXTEND_NO_REPEATED(Type, type) \
    int extend_no_repeated_array_list_##type(ArrayList##Type *list_to_extend, ArrayList##Type list){                            \
        int code = NOT_RESIZED;                                                                                                 \
                                                                                                                                \
        foreach_in_arraylist(Type, elemptr, list){                                                                              \
            int add_code = add_no_repeated_to_array_list_##type(list_to_extend, *elemptr);                                      \
            if(add_code == RESIZED) {                                                                                           \
                code = RESIZED;                                                                                                 \
            }                                                                                                                   \
        }                                                                                                                       \
                                                                                                                                \
        return code;                                                                                                            \
    }

#define DEFINE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(Type, type) \
    int extend_no_repeated_array_list_##type##_arena(ArrayList##Type *list_to_extend, ArrayList##Type list, Arena *arena){      \
        int code = NOT_RESIZED;                                                                                                 \
                                                                                                                                \
        foreach_in_arraylist(Type, elemptr, list){                                                                              \
            int add_code = add_no_repeated_to_array_list_##type##_arena(list_to_extend, *elemptr, arena);                       \
            if(add_code == RESIZED){                                                                                            \
                code = RESIZED;                                                                                                 \
            }                                                                                                                   \
        }                                                                                                                       \
                                                                                                                                \
        return code;                                                                                                            \
    }


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// REMOVING ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define unsafe_remove_last_in_array_list(type, list) remove_index_from_array_list_##type((&list), list.size - 1) 
#define unsafe_remove_last_in_array_list_ptr(type, listptr) remove_index_from_array_list_##type((listptr), listptr->size - 1) 

#define DEFINE_ARRAYLIST_REMOVE_INDEX(Type, type)                                           \
    int remove_index_from_array_list_##type(ArrayList##Type *list, uint32_t index){         \
        if (index >= list->size){                                                           \
            return OUT_OF_BOUNDS;                                                           \
        }                                                                                   \
        --list->size;                                                                       \
        Type *removed_ptr = list->array + index;                                            \
        uint32_t num_shifted_elements = list->size - index;                                 \
        memmove(removed_ptr, removed_ptr + 1, num_shifted_elements * sizeof(*list->array)); \
        return SUCCESSFUL_REMOVAL;                                                          \
    }

#define DEFINE_ARRAYLIST_REMOVE_POINTER_INDEX(Type, type)                                   \
    int remove_pointer_index_from_array_list_##type(ArrayList##Type *list, uint32_t index){ \
        if (index >= list->size){                                                           \
            return OUT_OF_BOUNDS;                                                           \
        }                                                                                   \
        --list->size;                                                                       \
        Type *removed_ptr = list->array + index;                                            \
        free(*removed_ptr);                                                                 \
        uint32_t num_shifted_elements = list->size - index;                                 \
        memmove(removed_ptr, removed_ptr + 1, num_shifted_elements * sizeof(*list->array)); \
        return SUCCESSFUL_REMOVAL;                                                          \
    }
    
                                                                                            
// NOTE: remove index checks if we haven't found the element (so index == list->size)
#define DEFINE_ARRAYLIST_REMOVE_ELEMENT(Type, type)                                         \
    int remove_element_from_array_list_##type(ArrayList##Type *list, Type element){         \
        uint32_t index = find_in_array_list_##type(*list, element);                         \
        return remove_index_from_array_list_##type(list, index);                            \
    }

#define DEFINE_ARRAYLIST_REMOVE_POINTER_ELEMENT(Type, type)                                 \
    int remove_pointer_element_from_array_list_##type(ArrayList##Type *list, Type element){ \
        uint32_t index = find_in_array_list_##type(*list, element);                         \
        return remove_pointer_index_from_array_list_##type(list, index);                    \
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GETTING /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define unsafe_last_in_array_list(list)         ((list).array[(list).size - 1])
#define unsafe_last_in_array_list_ptr(listptr)  ((listptr)->array[(listptr)->size - 1])
#define last_in_array_list(list)                ((list).size ? &unsafe_last_in_array_list(list) : NULL)
#define last_in_array_list_ptr(listptr)         ((listptr)->size ? &unsafe_last_in_array_list_ptr(listptr) : NULL)

// NOTE: for an unsafe get, simply use list[.|->]array[index].
#define DEFINE_ARRAYLIST_GET(Type, type)                                                \
int get_from_array_list_##type(ArrayList##Type list, uint32_t index, Type *result){     \
    if(index >= list.size){                                                             \
        return INVALID_INDEX;                                                           \
    }                                                                                   \
    *result = list.array[index];                                                        \
    return VALID_INDEX;                                                                 \
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PRINTING FOR DEBUGGING //////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: the number at the end (N) is the number of extra parameters the print_function requires. We will have N pairs of
//  type-name macro parameters.
#define DEFINE_ARRAYLIST_PRINT_SEPARATORS_1(Type, type, print_function, ArgType1, Arg1, ...)  \
    void print_separators_array_list_##type(                                    \
        ArrayList##Type list, char opening_brace, char closing_brace,           \
        char *elem_separator, bool one_line_per_elem, unsigned tab_num,         \
        ArgType1 Arg1)                                                          \
    {                                                                           \
        printf("%c", opening_brace);                                            \
        if(one_line_per_elem){ printf("\n"); }                                  \
        char *tabs;                                                             \
        if(one_line_per_elem){                                                  \
            tabs = malloc(tab_num * sizeof('\t') + 1);                          \
            for(unsigned i = 0; i < tab_num; ++i) { tabs[i] = '\t'; }           \
            tabs[tab_num] = '\0';                                               \
        } else {                                                                \
            tabs = malloc(sizeof('\0'));                                        \
            tabs[0] = '\0';                                                     \
        }                                                                       \
        if(list.size){ print_function(list.array[0], Arg1, ##__VA_ARGS__); }    \
        for(unsigned i = 1; i < list.size; ++i){                                \
            printf("%s", elem_separator);                                       \
            if(one_line_per_elem) { printf("\n"); }                             \
            printf("%s", tabs);                                                 \
            print_function(list.array[i], Arg1, ##__VA_ARGS__);                 \
        }                                                                       \
        free(tabs);                                                             \
        if(one_line_per_elem){ printf("\n"); }                                  \
        printf("%c", closing_brace);                                            \
    }

#define DEFINE_ARRAYLIST_PRINT_SEPARATORS(Type, type, print_function, ...)      \
    void print_separators_array_list_##type(                                    \
        ArrayList##Type list, char opening_brace, char closing_brace,           \
        char *elem_separator, bool one_line_per_elem, unsigned tab_num)         \
    {                                                                           \
        printf("%c", opening_brace);                                            \
        if(one_line_per_elem){ printf("\n"); }                                  \
        char *tabs;                                                             \
        if(one_line_per_elem){                                                  \
            tabs = malloc(tab_num * sizeof('\t') + 1);                          \
            for(unsigned i = 0; i < tab_num; ++i) { tabs[i] = '\t'; }           \
            tabs[tab_num] = '\0';                                               \
        } else {                                                                \
            tabs = malloc(sizeof('\0'));                                        \
            tabs[0] = '\0';                                                     \
        }                                                                       \
        if(list.size){ print_function(list.array[0], ##__VA_ARGS__); }          \
        for(unsigned i = 1; i < list.size; ++i){                                \
            printf("%s", elem_separator);                                       \
            if(one_line_per_elem) { printf("\n"); }                             \
            printf("%s", tabs);                                                 \
            print_function(list.array[i], ##__VA_ARGS__);                       \
        }                                                                       \
        free(tabs);                                                             \
        if(one_line_per_elem){ printf("\n"); }                                  \
        printf("%c", closing_brace);                                            \
    }

#define DEFINE_ARRAYLIST_PRINT(Type, type)  \
    void print_array_list_##type(ArrayList##Type list){     \
        print_separators_array_list_##type(                 \
            list, '[', ']', ", ", false, 0                  \
        );                                                  \
    }

// NOTE: useful for the console of the debugger
#define DEFINE_ARRAYLIST_PRINTLN(Type, type)  \
    void println_array_list_##type(ArrayList##Type list){     \
        print_array_list_##type(list);                        \
        printf("\n");                                         \
    }

#endif // ARRAYLIST_H