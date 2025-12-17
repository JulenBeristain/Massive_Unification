#ifndef LIST_H
#define LIST_H

#include <stdint.h>

//TODO: for ordered set of terms and schemas (set-schemas) could be interesting to implement list
//  behavior instead of simply using arrays. Therefore, we are going to start implementing an array list

//TODO: since we are going to use arraylists of different types, I have to look how to do generic programming
//  in C. But first, I am going to implement all the logic for arraylists of ints.

typedef struct ArrayList ArrayList;
struct ArrayList {
    uint32_t size;
    uint32_t capacity;
    int *array; // TODO:change this for generic programming
};

// Default kind of list
//typedef ArrayList List;
//#define create_list create_array_list
//

#endif // LIST_H