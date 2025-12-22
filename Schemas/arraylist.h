#ifndef ARRAYLIST_H
#define ARRAYLIST_H

#include <stdint.h>
#include <stdlib.h>

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
typedef enum ArrayListAddReturnCode { NOT_RESIZED, RESIZED } ArrayListAddReturnCode;
typedef enum ArrayListRemoveReturnCode { OUT_OF_BOUNDS, SUCCESSFUL_REMOVAL } ArrayListRemoveReturnCode;
typedef enum ArrayListGetReturnCode { INVALID_INDEX, VALID_INDEX } ArrayListGetReturnCode;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FOR EACH LOOP MACROS ////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define foreach_in_arraylist(type, valptr, list) \
    for(type *valptr = (list).array, *_end = (list).array + (list).size; valptr < _end; ++valptr)

#define foreach_in_arraylistptr(type, valptr, listptr) \
    for(type *valptr = (listptr)->array, *_end = (listptr)->array + (listptr)->size; valptr < _end; ++valptr)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DECLARATION OF ARRAYLIST FUNCTIONS MACROS ///////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_ARRAYLIST_FUNCTIONS(Name, name, type)                                        \
    ArrayList##Name create_array_list_##name(uint32_t capacity);                             \
    ArrayList##Name create_array_list_##name##_defsize();                                    \
    static inline void free_array_list_##name(ArrayList##Name list){ free(list.array); }     \
    static inline void clear_array_list_##name(ArrayList##Name *list){ list->size = 0; }     \
    int add_to_array_list_##name(ArrayList##Name *list, int element);                        \
    int remove_index_from_array_list_##name(ArrayList##Name *list, uint32_t index);          \
    int remove_element_from_array_list_##name(ArrayList##Name *list, type element);          \
    int get_from_array_list_##name(ArrayList##Name list, uint32_t index, type *result);      \
    void print_array_list_##name(ArrayList##Name list);

#define DECLARE_ARRAYLIST_OF_POINTERS_FUNCTIONS(Name, name, type)                            \
    ArrayList##Name create_array_list_##name(uint32_t capacity);                             \
    ArrayList##Name create_array_list_##name##_defsize();                                    \
    void free_array_list_##name(ArrayList##Name list);                                       \
    void clear_array_list_##name(ArrayList##Name *list);                                     \
    int add_to_array_list_##name(ArrayList##Name *list, int element);                        \
    int remove_index_from_array_list_##name(ArrayList##Name *list, uint32_t index);          \
    int remove_element_from_array_list_##name(ArrayList##Name *list, type element);          \
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CONCRETE DECLARATIONS OF ARRAYLIST TYPES ////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_ARRAYLIST(Int, int, int)

//TODO: for ordered set of terms and schemas (set-schemas)

#endif // ARRAYLIST_H