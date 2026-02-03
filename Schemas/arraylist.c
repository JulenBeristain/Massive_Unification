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
 * TODO: be careful with dangling pointers. If, for example, schemas
 *       are freed starting from the root of the "tree", then all 
 *       these pointers in the dependency list will be dangling
 *       (on the other hand, we are going to do a change from 
 *       ArrayList to HashSet there...). We should have to versions
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

int add_not_repeated_to_array_list_schema(ArrayListSchema *list, Schema element){
    foreach_in_arraylistptr(Schema, itptr, list){
        if(equal_schemas(itptr, &element)){
            return CONTAINED;
        }
    }
    return add_to_array_list_schema(list, element);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EXTENSION ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: add this to the macro that is used to define each arraylist type's functions
#define DEFINE_ARRAYLIST_EXTENSION(Name, name, type)                                                                        \
int extend_array_list_##name(ArrayList##Name *list_to_extend, ArrayList##Name *list){                                        \
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

DEFINE_ARRAYLIST_EXTENSION(Schema, schema, Schema);
DEFINE_ARRAYLIST_EXTENSION(DependencyPair, dependency_pair, DependencyPair);

int extend_not_repeated_array_list_schema(ArrayListSchema *list_to_extend, ArrayListSchema *list){
    int code = NOT_RESIZED; 
  
    unsigned maximum_extended_size = list_to_extend->size + list->size; 
    if(maximum_extended_size > list_to_extend->capacity){
        list_to_extend->capacity = 2 * maximum_extended_size; 
        list_to_extend->array = realloc(list_to_extend->array, list_to_extend->capacity * sizeof(*list_to_extend->array));
        if (list_to_extend->array == NULL){
            perror("extend_array_list: realloc failed to allocate memory");
            exit(EXIT_FAILURE);
        } 
        code = RESIZED; 
    } 

    foreach_in_arraylistptr(Schema, elemptr, list){
        add_not_repeated_to_array_list_schema(list_to_extend, *elemptr);
    }

    return code; 
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

// ArrayList of pointers to Schemas ------------------------------------------------------------------------------

DEFINE_ARRAYLIST_OF_POINTERS(SchemaPtr, schema_ptr, SchemaPtr, equal_schemas, print_schema) //TODO: ensure new print version is called here
//----------------------------------------------------------------------------------------------------------------

// ArrayList of Schemas ------------------------------------------------------------------------------------------

DEFINE_ARRAYLIST_EQ_PRINT_POINTERS(Schema, schema, Schema, equal_schemas, print_schema) //TODO: ensure new print version is called here
//----------------------------------------------------------------------------------------------------------------

// ArrayList of DependencyPairs ----------------------------------------------------------------------------------
DEFINE_ARRAYLIST_CREATION(DependencyPair, dependency_pair)
DEFINE_ARRAYLIST_ADDITION(DependencyPair, dependency_pair, DependencyPair)
DEFINE_ARRAYLIST_GETTING(DependencyPair, dependency_pair, DependencyPair)
DEFINE_PRINT_ARRAYLIST(DependencyPair, dependency_pair, print_dependency_pair)
//----------------------------------------------------------------------------------------------------------------