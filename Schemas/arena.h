#ifndef ARENA_H
#define ARENA_H

#include <stdlib.h>

// NOTE: another variant would be to not have a current_block, and having a next_free_position per block. When 
//  allocating, we would iterate over all blocks until one with enough memory was found (if any). This would
//  reduce fragmentation (unused memory in the tails of each block) in case of an unexpectedly single big allocation.
//  Note that if initialized with enough size_in_bytes, even a simple linear arena would work perfectly, and that
//  the two variants will work equally. In our case, we don't expect surprisingly big single special allocations,
//  so we will keep this version that gives more flexibility and robustness than the simple linear arena.

// NOTE: in this implementation we have a chain of MemoryBlocks. We always point to the last block that is being
//  used, we don't come back to blocks before it unless the Arena is cleared. Blocks can have different sizes.
//  Since at each time take into account a single block (the last that has at least an allocation, unless a 
//  big allocation was requested with no enough space in any previous block) we only have a single position index.

typedef struct MemoryBlock MemoryBlock, *MemoryBlockPtr;
struct MemoryBlock {
    void *memory;
    size_t size;
    MemoryBlock *next;
};

typedef struct Arena {
    MemoryBlock *memory_blocks;
    MemoryBlock *current_block;
    size_t next_free_position;  //NOTE: as we will be only in a block at a time, a unique index is enough!
} Arena, *ArenaPtr;

void init_arena(Arena *arena, size_t size_in_bytes);
void free_arena(Arena *arena);
void clear_arena(Arena *arena);
void *allocate(Arena *arena, size_t num_bytes);

#endif