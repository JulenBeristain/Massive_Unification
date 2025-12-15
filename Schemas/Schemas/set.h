#ifndef SET_H
#define SET_H

#include <stddef.h>

/**
 * In this file we declare structs and functions to work with sets of variables (as unsigned > 0).
 * 
 * The set is going to be an array of linked lists (for hash collisions) of unsigned integers (variables).
 */

typedef struct LinkedList LinkedList;
struct LinkedList {
    unsigned v;
    LinkedList *next;
};

typedef struct Set Set;
struct Set {
    size_t num_slots;
    LinkedList *listsVariables;
};

create
free
hash
lookup
install
clear

#endif // SET_H