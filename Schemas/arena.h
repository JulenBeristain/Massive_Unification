#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>
#include "utils.h"

// NOTE: if initialized with enough size_in_bytes, even a simple linear arena would work perfectly, and that
//  the two variants will work equally. In our case, we don't expect surprisingly big single special allocations,
//  so we will keep this version that gives more flexibility and robustness than the simple linear arena.

// NOTE: in this implementation we have a chain of MemoryBlocks. We always point to the last block that is being
//  used, we don't come back to blocks before it unless the Arena is cleared or it pops to a prior state. 
//  Blocks can have different sizes. Since at each time take into account a single block (the last that has at 
//  least an allocation, unless a big allocation was requested with no enough space in any previous block) we 
//  only have a single position index.

typedef struct MemoryBlock MemoryBlock, *MemoryBlockPtr;
struct MemoryBlock {
    size_t size;
    MemoryBlock *next;
};
static inline void *memory_block_start (MemoryBlock *mem_block) {
    return address_after_struct(mem_block);
}

typedef struct {
    MemoryBlock *memory_blocks;
    MemoryBlock *current_block;
    size_t next_free_position;  //NOTE: as we will be only in a block at a time, a unique index is enough!
} ChainedArena, *ChainedArenaPtr;

void init_chained_arena(ChainedArena *arena, size_t size_in_bytes);
static inline void init_chained_arena_defcapacity(ChainedArena *arena) { 
    init_chained_arena(arena, KILOBYTES(4) - sizeof(MemoryBlock));
}
void free_chained_arena(ChainedArena arena);
void clear_chained_arena(ChainedArena *arena);
void *allocate_chained_arena(ChainedArena *arena, size_t num_bytes);
void *callocate_chained_arena(ChainedArena *arena, size_t num_bytes);

typedef struct {
    MemoryBlock *current_block;
    size_t next_free_position;
} ChainedArenaState;

static inline ChainedArenaState register_state_chained_arena(ChainedArena *arena) {
    return (ChainedArenaState){ .current_block = arena->current_block, .next_free_position = arena->next_free_position };
}

static inline void pop_to_state_chained_arena(ChainedArena *arena, ChainedArenaState state) {
    arena->current_block = state.current_block;
    arena->next_free_position = state.next_free_position;
}

//NOTE: we could implement pop(size) and pop_to(position) too, but I prefer pop_to_state. The other ones require
//  more manual careful usage, while pop_to_state only needs to be paired with the correct register_state.


typedef struct {
    size_t size;
    size_t next_free_position;
} LinearArena;
static inline void *linear_arena_memory (LinearArena *arena) {
    return address_after_struct(arena);
}

LinearArena *create_linear_arena(size_t size_in_bytes);
static inline void create_linear_arena_defcapacity() { 
    create_linear_arena(KILOBYTES(4) - sizeof(LinearArena));
 }

// NOTE: don't use the Arena after freeing!
static inline void free_linear_arena(LinearArena *arena) {
    free(arena);
}

static inline void clear_linear_arena(LinearArena *arena) {
    arena->next_free_position = 0;
}

void *allocate_linear_arena(LinearArena *arena, size_t num_bytes);
void *callocate_linear_arena(LinearArena *arena, size_t num_bytes);

typedef struct {
    size_t next_free_position;
} LinearArenaState;

static inline LinearArenaState register_state_linear_arena(LinearArena *arena) {
    return (LinearArenaState){ .next_free_position = arena->next_free_position };
}

static inline void pop_to_state_linear_arena(LinearArena *arena, LinearArenaState state) {
    arena->next_free_position = state.next_free_position;
}


// Default Arena is a ChainedArena
typedef ChainedArena Arena;
typedef ChainedArenaState ArenaState;

#define init_arena(arena, size_in_bytes)    \
    _Generic((arena),                       \
        ChainedArena*: init_chained_arena   \
    )(arena, size_in_bytes)

#define init_arena_defcapacity(arena)    \
    _Generic((arena),                                   \
        ChainedArena*: init_chained_arena_defcapacity   \
    )(arena)

#define free_arena(arena)                   \
    _Generic((arena),                       \
        ChainedArena: free_chained_arena,   \
        LinearArena*: free_linear_arena    \
    )(arena)

#define clear_arena(arena)                  \
    _Generic((arena),                       \
        ChainedArena*: clear_chained_arena, \
        LinearArena*: clear_linear_arena   \
    )(arena)

#define allocate(arena, num_bytes)              \
    _Generic((arena),                           \
        ChainedArena*: allocate_chained_arena,  \
        LinearArena*: allocate_linear_arena    \
    )(arena, num_bytes)

#define callocate(arena, num_bytes)             \
    _Generic((arena),                           \
        ChainedArena*: callocate_chained_arena, \
        LinearArena*: callocate_linear_arena   \
    )(arena, num_bytes)

#define register_state_arena(arena) \
    _Generic((arena),                                   \
        ChainedArena*: register_state_chained_arena,    \
        LinearArena*: register_state_linear_arena       \
    )(arena)


#define pop_to_state_arena(arena, state) \
    _Generic((arena),                                  \
        ChainedArena*: pop_to_state_chained_arena,     \
        LinearArena*: pop_to_state_linear_arena        \
    )(arena, state)


#define PUSH_STRUCT(arena, T) allocate((arena), sizeof(T))
#define PUSH_STRUCT_ZERO(arena, T) callocate((arena), sizeof(T))
#define PUSH_ARRAY(arena, T, n) allocate((arena), sizeof(T) * (n))
#define PUSH_ARRAY_ZERO(arena, T, n) callocate((arena), sizeof(T) * (n))

#endif