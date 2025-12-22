#include "arraylist.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CREATION ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEFINE_ARRAYLIST_CREATION(Name,name)                        \
                                                                    \
    ArrayList##Name create_array_list_##name(uint32_t capacity){    \
        ArrayList##Name list;                                       \
        list.size = 0;                                              \
        list.capacity = capacity;                                   \
        list.array = malloc(capacity * sizeof(*list.array));        \
        if (list.array == NULL){                                    \
            perror("malloc failed to allocate memory");             \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
        return list;                                                \
    }                                                               \
                                                                    \
    ArrayList##Name create_array_list_##name##_defsize(){           \
        enum { DEFAULT_ARRAY_LIST_SIZE = 10 };                      \
        return create_array_list_##name(DEFAULT_ARRAY_LIST_SIZE);   \
    }

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DELETION ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * NOTE: the size and capacity that were remaining are not important. 
 *       After a free, the ArrayList shouldn't be used more.
 * NOTE: be careful with dangling pointers...
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

#define DEFINE_ARRAYLIST_ADDITION(Name, name, type)                                 \
int add_to_array_list_##name(ArrayList##Name *list, type element){                  \
    int code = NOT_RESIZED;                                                         \
    if (list->size == list->capacity){                                              \
        list->capacity *= 2;                                                        \
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// REMOVING ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
        uint32_t num_shifted_elements = list->size - index;                                 \
        memmove(removed_ptr, removed_ptr + 1, num_shifted_elements * sizeof(*list->array)); \
        free(removed_ptr);                                                                  \
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
        printf("]\n");                                      \
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

#define DEFINE_ARRAYLIST_OF_POINTERS(Name, name, type, equal_function, print_function)  \
    DEFINE_ARRAYLIST_CREATION(Name, name)                                               \
    DEFINE_ARRAYLIST_OF_POINTERS_DELETION(Name, name, type)                             \
    DEFINE_ARRAYLIST_ADDITION(Name, name, type)                                         \
    DEFINE_ARRAYLIST_GETTING(Name, name, type)                                          \
    DEFINE_ARRAYLIST_OF_POINTERS_REMOVAL_INDEX(Name, name, type)                        \
    DEFINE_ARRAYLIST_REMOVAL_VALUE(Name, name, type, equal_function)                    \
    DEFINE_PRINT_ARRAYLIST(Name, name, print_function)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CONCRETE DEFINES ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ArrayList of ints ---------------------------------------------------------------------------------------------

// For removing elements by value, we need to have a function to compare the items. It can be defined here
// as the version for ints as an example, or elsewhere (in that case, we would need at least the declaration).
static inline bool equal_ints(int a, int b) { return a == b; }

// The same for printing each element of the arraylist.
static inline void print_int(int i) { printf("%d", i); }

DEFINE_ARRAYLIST(Int, int, int, equal_ints, print_int)
//----------------------------------------------------------------------------------------------------------------