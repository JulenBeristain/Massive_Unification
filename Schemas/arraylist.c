#include "arraylist.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// TODO: make an extension for VSCode that expands macros automatically inplace, removing the macro
//  use, substituting it by the text that appears in the "Expands to" section when you hover over it,
//  and calling to the clang.formatter

// NOTE: Arenas don't behave nicely with ArrayLists when the latters resize. All the previous memory used
//  by the ArrayList to store the elements becomes garbage when the ArrayList takes a new greater chunk
//  from the Arena. Therefore, if a lot of resizing happens, the risk of running out of memory in the Arena
//  increases, as well as the amount of unused memory.
//
//  Therefore, avoid using dynamic structures that can resize with Arenas, or prevent the resizing from happening.
//  In the case of ArrayLists: 1) give great initial capacity taken from the Arena. 2) just use malloc (preferably
//  with just enough capacity to avoid resizing).
//
//  Additionally, to mix the use of malloc and Arenas for the same ArrayList we would need an extra flag to know if
//  the previous allocation was done by malloc or not. If that was the case, before taking memory from the arena in 
//  a resizing allocation, we would need to free the malloced memory.



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CONCRETE DEFINES ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ArrayList of pointers to Schemas ------------------------------------------------------------------------------

//TODO: ensure new print version is called here
//DEFINE_ARRAYLIST_OF_POINTERS(SchemaPtr, schema_ptr, SchemaPtr, equal_schemas, print_schema)
//----------------------------------------------------------------------------------------------------------------

