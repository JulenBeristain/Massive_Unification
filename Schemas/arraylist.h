#ifndef ARRAYLIST_H
#define ARRAYLIST_H

#include <stdint.h>
#include <stdlib.h>
#include "schemas.h"
#include "arena.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DECLARATION OF ARRAYLIST TYPES MACRO ////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_ARRAYLIST_TYPE(Name, type)          \
    typedef struct ArrayList##Name ArrayList##Name; \
    struct ArrayList##Name {                        \
        uint32_t size;                              \
        uint32_t capacity;                          \
        type* array;                                \
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
#define foreach_in_arraylist(type, valptr, list)                                                    \
    for(type *valptr = (list).array, *_end = (list).array + (list).size; valptr < _end; ++valptr)   \

#define foreach_in_arraylistptr(type, valptr, listptr)                                                          \
    for(type *valptr = (listptr)->array, *_end = (listptr)->array + (listptr)->size; valptr < _end; ++valptr)   \

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DECLARATION OF ARRAYLIST FUNCTIONS MACROS ///////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_ARRAYLIST_FUNCTIONS(Name, name, type)                                        \
    ArrayList##Name create_array_list_##name(uint32_t capacity);                             \
    ArrayList##Name create_array_list_##name##_defsize();                                    \
    static inline void free_array_list_##name(ArrayList##Name list){ free(list.array); }     \
    static inline void clear_array_list_##name(ArrayList##Name *list){ list->size = 0; }     \
    int add_to_array_list_##name(ArrayList##Name *list, type element);                       \
    int remove_index_from_array_list_##name(ArrayList##Name *list, uint32_t index);          \
    int remove_element_from_array_list_##name(ArrayList##Name *list, type element);          \
    int get_from_array_list_##name(ArrayList##Name list, uint32_t index, type *result);      \
    void print_array_list_##name(ArrayList##Name list);

#define DECLARE_ARRAYLIST_OF_POINTERS_FUNCTIONS(Name, name, type)                            \
    ArrayList##Name create_array_list_##name(uint32_t capacity);                             \
    ArrayList##Name create_array_list_##name##_defsize();                                    \
    void free_array_list_##name(ArrayList##Name list);                                       \
    void clear_array_list_##name(ArrayList##Name *list);                                     \
    int add_to_array_list_##name(ArrayList##Name *list, type element);                       \
    int remove_index_from_array_list_##name(ArrayList##Name *list, uint32_t index);          \
    int remove_element_from_array_list_##name(ArrayList##Name *list, type element);          \
    int get_from_array_list_##name(ArrayList##Name list, uint32_t index, type *result);      \
    void print_array_list_##name(ArrayList##Name list);

#define DECLARE_ARRAYLIST_FUNCTIONS_NOT_REMOVAL(Name, name, type)                            \
    ArrayList##Name create_array_list_##name(uint32_t capacity);                             \
    ArrayList##Name create_array_list_##name##_defsize();                                    \
    static inline void free_array_list_##name(ArrayList##Name list){ free(list.array); }     \
    static inline void clear_array_list_##name(ArrayList##Name *list){ list->size = 0; }     \
    int add_to_array_list_##name(ArrayList##Name *list, type element);                       \
    int get_from_array_list_##name(ArrayList##Name list, uint32_t index, type *result);      \
    void print_array_list_##name(ArrayList##Name list);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DECLARATIONS OF ARRAYLIST MACROS ////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_ARRAYLIST(Name, name, type) \
    DECLARE_ARRAYLIST_TYPE(Name, type)      \
    DECLARE_ARRAYLIST_FUNCTIONS(Name, name, type)

// NOTE: for pointer types, it is helpful to have typedefs
#define DECLARE_ARRAYLIST_OF_POINTERS(Name, name, type) \
    DECLARE_ARRAYLIST_TYPE(Name, type)                  \
    DECLARE_ARRAYLIST_OF_POINTERS_FUNCTIONS(Name, name, type)

#define DECLARE_ARRAYLIST_NOT_REMOVAL(Name, name, type)         \
    DECLARE_ARRAYLIST_TYPE(Name, type)                          \
    DECLARE_ARRAYLIST_FUNCTIONS_NOT_REMOVAL(Name, name, type)

//TODO: incorporate these to the macro
#define DECLARE_ARRAYLIST_ADDITION_ARENA(Name, name, type)                                   \
    int add_to_array_list_##name##_arena(ArrayList##Name *list, type element, Arena *arena);

#define DECLARE_ARRAYLIST_EXTENSION(Name, name, type)                                   \
    int extend_array_list_##name(ArrayList##Name *list_to_extend, ArrayList##Name *list);
#define DECLARE_ARRAYLIST_EXTENSION_ARENA(Name, name, type)                                                                  \
    int extend_array_list_##name##_arena(ArrayList##Name *list_to_extend, ArrayList##Name *list, Arena *arena);

#define DECLARE_ARRAYLIST_CREATION_ARENA(Name,name)                                      \
    ArrayList##Name create_array_list_##name##_arena(uint32_t capacity, Arena *arena);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CONCRETE DECLARATIONS OF ARRAYLIST TYPES ////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: we can not have concrete declarations and definitions in arraylist.h/c, because in compile time a cyclic 
//  unknowing of types happens.
// TODO: move here, to .h, the macros used to define functions, and delete .c altogether

//DECLARE_ARRAYLIST_OF_POINTERS(SchemaPtr, schema_ptr, SchemaPtr)


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MACROS FOR ARRAYLIST FUNCTION DEFINITIONS ///////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CREATION ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ARRAYLIST_CREATION(Name,name)                        \
                                                                    \
    ArrayList##Name create_array_list_##name(uint32_t capacity){    \
        ArrayList##Name list;                                       \
        list.size = 0;                                              \
        list.capacity = capacity;                                   \
        if(capacity){                                               \
            list.array = malloc(capacity * sizeof(*list.array));    \
            if (list.array == NULL){                                \
                perror("malloc failed to allocate memory");         \
                exit(EXIT_FAILURE);                                 \
            }                                                       \
        }                                                           \
        else { list.array = NULL; }                                 \
        return list;                                                \
    }                                                               \
                                                                    \
    ArrayList##Name create_array_list_##name##_defsize(){           \
        enum { DEFAULT_ARRAY_LIST_SIZE = 10 };                      \
        return create_array_list_##name(DEFAULT_ARRAY_LIST_SIZE);   \
    }
//TODO: defsize is misleading, in reallity, it's default capacity

// NOTE: not default capacity for Arena because we are only allocating from it when there is no resizing
#define DEFINE_ARRAYLIST_CREATION_ARENA(Name,name)                                      \
    ArrayList##Name create_array_list_##name##_arena(uint32_t capacity, Arena *arena){  \
        ArrayList##Name list;                                                           \
        list.size = 0;                                                                  \
        list.capacity = capacity;                                                       \
        list.array = allocate(arena, capacity * sizeof(*list.array));                   \
        return list;                                                                    \
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DELETION ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: the deletion of ArrayLists of non-pointer types are defined in the .h as
//  static inlines.

/**
 * NOTE: the size and capacity that were remaining are not important. 
 *       After a free, the ArrayList shouldn't be used more.
 * TODO: be careful with dangling pointers. If, for example, schemas
 *       are freed starting from the root of the "tree", then all 
 *       these pointers in the dependency list will be dangling
 *       (on the other hand, we are going to do a change from 
 *       ArrayList to HashSet there...). We should have two versions
 *       for freeing an ArrayList of pointers: on where free is
 *       called upon each pointer (for the cases where malloc 
 *       was called with the pointer in the array itself) and 
 *       the other where free isn't called (for when we only
 *       have references to already malloced structures elsewhere),
 *       this sounds something like the ownership-borrowing concepts
 *       in Rust...
 */
#define DEFINE_ARRAYLIST_OF_POINTERS_DELETION(Name, name, type) \
                                                                \
    void free_array_list_##name(ArrayList##Name list){          \
        foreach_in_arraylist(type, iter, list){                 \
            free(iter);                                         \
        }                                                       \
        free(list.array);                                       \
    }                                                           \
                                                                \
    void clear_array_list_##name(ArrayList##Name *list){        \
        foreach_in_arraylistptr(type, iter, list){              \
            free(iter);                                         \
        }                                                       \
        list->size = 0;                                         \
    }                                                           

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// ADDITION ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: if we have an ArrayList of values of some type (i.e., the type is not a pointer type to some other concrete one), is because
//  the type is small enough (and we don't have further algorithms, like caching, to take advantage of the use of pointers) that is
//  more efficient to have all the values directly consecutive in memory (for cache locality).
// TODO: nonetheless, could be interesting to have an add version that receives a pointer to the element and makes a copy of it 
//  in the array...

#define DEFINE_ARRAYLIST_ADDITION(Name, name, type)                                 \
int add_to_array_list_##name(ArrayList##Name *list, type element){                  \
    int code = NOT_RESIZED;                                                         \
    if (list->size == list->capacity){                                              \
        if(list->capacity){ list->capacity *= 2; }                                  \
        else              { list->capacity  = 1; }                                  \
        list->array = realloc(list->array, list->capacity * sizeof(*list->array));  \
        if (list->array == NULL){                                                   \
            perror("realloc failed to allocate memory");                            \
            exit(EXIT_FAILURE);                                                     \
        }                                                                           \
        code = RESIZED;                                                             \
    }                                                                               \
    list->array[list->size] = element;                                              \
    ++list->size;                                                                   \
    return code;                                                                    \
}

#define DEFINE_ARRAYLIST_ADDITION_ARENA(Name, name, type)                                   \
int add_to_array_list_##name##_arena(ArrayList##Name *list, type element, Arena *arena){    \
    int code = NOT_RESIZED;                                                                 \
    if (list->size == list->capacity){                                                      \
        if(list->capacity){ list->capacity *= 2; }                                          \
        else              { list->capacity  = 1; }                                          \
        list->array = allocate(arena, list->capacity * sizeof(*list->array));               \
        code = RESIZED;                                                                     \
    }                                                                                       \
    list->array[list->size] = element;                                                      \
    ++list->size;                                                                           \
    return code;                                                                            \
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EXTENSION ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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