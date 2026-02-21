#ifndef ARRAYLIST_H
#define ARRAYLIST_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "arena.h"

// TODO: make an extension for VSCode that expands macros automatically inplace, removing the macro
//  use, substituting (or commenting and appending) it by the text that appears in the "Expands to" 
//  section when you hover over it, and calling to the clang.formatter.

// TODO: we can have a version get_pointer that receives an element and searches if an equivalent is found in the arraylist,
//  and in that case it returns a pointer to it. This operation has the same complications as the remove_element one...

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MACROS FOR DECLARATION OF ARRAYLIST FUNCTIONS (and definition of static inline functions) ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_ARRAYLIST_CREATE(Type, type)                        \
    ArrayList##Type create_array_list_##type(uint32_t capacity);

#define DECLARE_ARRAYLIST_CREATE_DEFCAPACITY(Type, type)            \
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


// NOTE: list by value because it isn't modified. When list.size returned, elem not found. ElemType parametirized so
//  in the definition we can use equal functions that receive the element either as a pointer or by value.
#define DECLARE_ARRAYLIST_FIND(Type, type, ElemType) \
    unsigned find_in_array_list_##type(ArrayList##Type list, ElemType elem);

#define DECLARE_ARRAYLIST_CONTAINS(Type, type, ElemType) \
    bool contains_array_list_##type(ArrayList##Type list, ElemType elem);

#define DECLARE_ARRAYLIST_ADD_NO_REPEATED(Type, type, ElemType) \
    int add_no_repeated_to_array_list_##type(ArrayList##Type *list, ElemType elem);

#define DECLARE_ARRAYLIST_ADD_NO_REPEATED_ARENA(Type, type, ElemType) \
    int add_no_repeated_to_array_list_##type##_arena(ArrayList##Type *list, ElemType elem, Arena *arena);

#define DECLARE_ARRAYLIST_EXTEND_NO_REPEATED(Type, type) \
    int extend_no_repeated_array_list_##type(ArrayList##Type *list_to_extend, ArrayList##Type list, ElemType elem);

#define DECLARE_ARRAYLIST_EXTEND_NO_REPEATED_ARENA(Type, type) \
    int extend_no_repeated_array_list_##type##_arena(ArrayList##Type *list_to_extend, ArrayList##Type list, ElemType elem, Arena *arena);


#define DECLARE_ARRAYLIST_REMOVE_INDEX(Type, type)                                          \
    int remove_index_from_array_list_##type(ArrayList##Type *list, uint32_t index);

// NOTE: mixing passing by value the element to remove and having an arraylist of pointers doesn't make sense, because
//  if a type is small enough so that passing by value is more efficient, then it would also be more interesting to have
//  an arraylist of values instead of pointers. Anyways, although ElemType == Type in most of cases, giving that possibility
//  could be interesting, because pointers can be an instrument to implement some optimizations like caching repeated nodes...
//  The equals function used in the definition has two possibilities too: it can receive the elements by value or by pointer...
#define DECLARE_ARRAYLIST_REMOVE_ELEMENT(Type, type, ElemType)                              \
    int remove_element_from_array_list_##type(ArrayList##Type *list, ElemType element);


#define DECLARE_ARRAYLIST_GET(Type, type)                                                   \
    int get_from_array_list_##type(ArrayList##Type list, uint32_t index, Type *result);


// NOTE: in the definition, the print function could use a print element function that receives a value or a pointer to it.
// NOTE: as arraylists are only 16B and printing doesn't modify it, adding a version that receives a pointer doesn't make
//  much sense.
// TODO: other versions with separator parameters and so on?
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
#define DEFINE_ARRAYLIST_CREATION_ARENA(Type,type)                                      \
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

#define DEFINE_ARRAYLIST_ADD_ARENA(Type, type)                                              \
int add_to_array_list_##type##_arena(ArrayList##Type *list, Type element, Arena *arena){    \
    int code = NOT_RESIZED;                                                                 \
    if (list->size == list->capacity){                                                      \
        if(list->capacity){ list->capacity *= 2; }                                          \
        else              { list->capacity  = 1; }                                          \
        unsigned num_bytes = list->capacity * sizeof(*list->array);                         \
        void *new_array = allocate(arena, num_bytes);                                       \
        memcpy(new_array, list->array, num_bytes);                                          \
        list->array = new_array;                                                            \
        code = RESIZED;                                                                     \
    }                                                                                       \
    list->array[list->size] = element;                                                      \
    ++list->size;                                                                           \
    return code;                                                                            \
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EXTENSION ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO_YA: keep cleaning from here...

// TODO: add this to the macro that is used to define each arraylist type's functions
#define DEFINE_ARRAYLIST_EXTENSION(Name, name, type)                                                                        \
int extend_array_list_##name(ArrayList##Name *list_to_extend, ArrayList##Name *list){                                       \
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
    foreach_in_arraylistptr(type, elemptr, list){                                                                           \
        add_to_array_list_##name(list_to_extend, *elemptr);                                                                 \
    }                                                                                                                       \
                                                                                                                            \
    return code;                                                                                                            \
}

// TODO: add this to the macro that is used to define each arraylist type's functions
#define DEFINE_ARRAYLIST_EXTENSION_ARENA(Name, name, type)                                                                  \
int extend_array_list_##name##_arena(ArrayList##Name *list_to_extend, ArrayList##Name *list, Arena *arena){                 \
    int code = NOT_RESIZED;                                                                                                 \
                                                                                                                            \
    unsigned extended_size = list_to_extend->size + list->size;                                                             \
    if(extended_size > list_to_extend->capacity){                                                                           \
        list_to_extend->capacity = 2 * extended_size;                                                                       \
        list_to_extend->array = allocate(arena, list_to_extend->capacity * sizeof(*list_to_extend->array));                 \
        code = RESIZED;                                                                                                     \
    }                                                                                                                       \
                                                                                                                            \
    foreach_in_arraylistptr(type, elemptr, list){                                                                           \
        add_to_array_list_##name##_arena(list_to_extend, *elemptr, arena);                                                  \
    }                                                                                                                       \
                                                                                                                            \
    return code;                                                                                                            \
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// REMOVING ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: actually, this macro can be used with arraylists of pointers too, if we can ensure that the free of the object
//  is done elsewhere...
// TODO: change the names of these macros to make that clearer...
#define DEFINE_ARRAYLIST_REMOVAL_INDEX(Name, name, type)                                    \
    int remove_index_from_array_list_##name(ArrayList##Name *list, uint32_t index){         \
        if (index >= list->size){                                                           \
            return OUT_OF_BOUNDS;                                                           \
        }                                                                                   \
        --list->size;                                                                       \
        type *removed_ptr = list->array + index;                                            \
        uint32_t num_shifted_elements = list->size - index;                                 \
        memmove(removed_ptr, removed_ptr + 1, num_shifted_elements * sizeof(*list->array)); \
        return SUCCESSFUL_REMOVAL;                                                          \
    }                                                                                       \
                                                                                            

// NOTE: be careful with dangling pointers...
#define DEFINE_ARRAYLIST_OF_POINTERS_REMOVAL_INDEX(Name, name, type)                        \
    int remove_index_from_array_list_##name(ArrayList##Name *list, uint32_t index){         \
        if (index >= list->size){                                                           \
            return OUT_OF_BOUNDS;                                                           \
        }                                                                                   \
        --list->size;                                                                       \
        type *removed_ptr = list->array + index;                                            \
        free(*removed_ptr);                                                                 \
        uint32_t num_shifted_elements = list->size - index;                                 \
        memmove(removed_ptr, removed_ptr + 1, num_shifted_elements * sizeof(*list->array)); \
        return SUCCESSFUL_REMOVAL;                                                          \
    }                                                                                           
                                                                                            
// NOTE: remove index checks if we haven't found the element (so index == list->size)
#define DEFINE_ARRAYLIST_REMOVAL_VALUE(Name, name, type, equal_function)                    \
    int remove_element_from_array_list_##name(ArrayList##Name *list, type element){         \
        type *iter = list->array;                                                           \
        type *end = list->array + list->size;                                               \
        while((iter < end) && (!equal_function(*iter, element))){                           \
            ++iter;                                                                         \
        }                                                                                   \
        uint32_t index = iter - list->array;                                                \
                                                                                            \
        return remove_index_from_array_list_##name(list, index);                            \
    }

#define DEFINE_ARRAYLIST_REMOVAL_VALUE_WITH_POINTER(Name, name, type, equal_function)       \
    int remove_element_from_array_list_##name(ArrayList##Name *list, type *element){        \
        type *iter = list->array;                                                           \
        type *end = list->array + list->size;                                               \
        while((iter < end) && (!equal_function(iter, element))){                            \
            ++iter;                                                                         \
        }                                                                                   \
        uint32_t index = iter - list->array;                                                \
                                                                                            \
        return remove_index_from_array_list_##name(list, index);                            \
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// GETTING /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: for an unsafe get, simply use list[.|->]array[index].
#define DEFINE_ARRAYLIST_GETTING(Name, name, type)                                      \
int get_from_array_list_##name(ArrayList##Name list, uint32_t index, type *result){     \
    if(index >= list.size){                                                             \
        return INVALID_INDEX;                                                           \
    }                                                                                   \
    *result = list.array[index];                                                        \
    return VALID_INDEX;                                                                 \
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PRINTING FOR DEBUGGING //////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_PRINT_ARRAYLIST(Name, name, print_function)  \
    void print_array_list_##name(ArrayList##Name list){     \
        printf("[");                                        \
        if(list.size){ print_function(list.array[0]); }     \
        for(size_t i = 1; i < list.size; ++i){              \
            printf(", ");                                   \
            print_function(list.array[i]);                  \
        }                                                   \
        printf("]");                                        \
    }

#define DEFINE_PRINT_ARRAYLIST_WITH_POINTER(Name, name, print_function)     \
    void print_array_list_##name(ArrayList##Name list){                     \
        printf("[");                                                        \
        if(list.size){ print_function(&list.array[0]); }                    \
        for(size_t i = 1; i < list.size; ++i){                              \
            printf(", ");                                                   \
            print_function(&list.array[i]);                                 \
        }                                                                   \
        printf("]");                                                        \
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMPLETE DEFINES ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ARRAYLIST(Name, name, type, equal_function, print_function)  \
    DEFINE_ARRAYLIST_CREATION(Name, name)                                   \
    DEFINE_ARRAYLIST_ADDITION(Name, name, type)                             \
    DEFINE_ARRAYLIST_GETTING(Name, name, type)                              \
    DEFINE_ARRAYLIST_REMOVAL_INDEX(Name, name, type)                        \
    DEFINE_ARRAYLIST_REMOVAL_VALUE(Name, name, type, equal_function)        \
    DEFINE_PRINT_ARRAYLIST(Name, name, print_function)

#define DEFINE_ARRAYLIST_EQ_PRINT_POINTERS(Name, name, type, equal_function, print_function)    \
    DEFINE_ARRAYLIST_CREATION(Name, name)                                                       \
    DEFINE_ARRAYLIST_ADDITION(Name, name, type)                                                 \
    DEFINE_ARRAYLIST_GETTING(Name, name, type)                                                  \
    DEFINE_ARRAYLIST_REMOVAL_INDEX(Name, name, type)                                            \
    DEFINE_ARRAYLIST_REMOVAL_VALUE_WITH_POINTER(Name, name, type, equal_function)               \
    DEFINE_PRINT_ARRAYLIST_WITH_POINTER(Name, name, print_function)

#define DEFINE_ARRAYLIST_OF_POINTERS(Name, name, type, equal_function, print_function)  \
    DEFINE_ARRAYLIST_CREATION(Name, name)                                               \
    DEFINE_ARRAYLIST_OF_POINTERS_DELETION(Name, name, type)                             \
    DEFINE_ARRAYLIST_ADDITION(Name, name, type)                                         \
    DEFINE_ARRAYLIST_GETTING(Name, name, type)                                          \
    DEFINE_ARRAYLIST_OF_POINTERS_REMOVAL_INDEX(Name, name, type)                        \
    DEFINE_ARRAYLIST_REMOVAL_VALUE(Name, name, type, equal_function)                    \
    DEFINE_PRINT_ARRAYLIST(Name, name, print_function)

// TODO: define a helper resizing function!!! resizing = realloc + COPY_THE_ELEMENTS!!!

#endif // ARRAYLIST_H