#include "arena.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define DEFAULT_ALIGNMENT 16 // 16-byte alignment is safe for everything, including SIMD

static inline unsigned num_active_bits(size_t n) {
    unsigned result = 0;
    while(n){
        result += n & 1;
        n >>= 1;
    }
    return result;
}

static inline bool is_power_of_2(size_t n) {
    return num_active_bits(n) == 1;
}

// Helper function to align a size/pointer upward to a power of 2
static inline uintptr_t align_forward(uintptr_t ptr, size_t alignment) {
    assert(is_power_of_2(alignment));
    return (ptr + (alignment - 1)) & ~(alignment - 1);
}

// PRE: size_in_bytes > 0
// Block size is the smaller power of 2 greater or equal than size_in_bytes. In the extreme case that the most significant bit
// is 63, we return the size itself (to avoid returning 0).
static inline size_t calculate_block_size(size_t size_in_bytes){
    unsigned most_significant_1; // Range: [0, 63] - Ensured to initialize because size_in_bytes has at least a bit activated
    unsigned i = MOST_SIGNIFICANT_BIT(size_t);
    while(i){
        if(GET_BIT(size_in_bytes, i)){
            most_significant_1 = i;
            break;
        }
        --i;
    }
    
    if(most_significant_1 == MOST_SIGNIFICANT_BIT(size_t)){
        return size_in_bytes;
    }
    
    bool is_power_of_2 = true;
    while(i){
        --i;
        if(GET_BIT(size_in_bytes, i)){
            is_power_of_2 = false;
            break;
        }
    }

    size_t block_size = is_power_of_2 ? size_in_bytes : (1ul << (most_significant_1 + 1));
    return block_size;
}

// PRE: block_size > 0
static inline MemoryBlock *create_memory_block(size_t block_size){
    MemoryBlock *memory_block = malloc(sizeof(*memory_block));
    if (memory_block == NULL){
        perror("create_memory_block: malloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    memory_block->memory = malloc(block_size);
    if (memory_block->memory == NULL){
        perror("create_memory_block: malloc failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    memory_block->size = block_size;
    memory_block->next = NULL;
    return memory_block;
}

// NOTE: we expect size_in_bytes to be a big value, to be able to perform a lot of allocations with a single memory block,
//  hopefully avoiding to allocate further blocks.
void init_arena(Arena *arena, size_t size_in_bytes){
    if(size_in_bytes == 0){
        SET_TO_ZERO(arena, sizeof(*arena));
        return;
    }
    
    // Set the block size to the smaller power of 2 greater or equal than size_in_bytes
    size_t block_size = calculate_block_size(size_in_bytes);

    // Allocate the first block
    arena->memory_blocks = create_memory_block(block_size);
    
    // Current block to first block and next free position to 0
    arena->current_block = arena->memory_blocks;
    arena->next_free_position = 0;
}

// NOTE: only the contents of the arena are freed. If the arena itself was malloced, it is not freed!
void free_arena(Arena *arena){
    MemoryBlock *current = arena->memory_blocks;
    while(current){
        free(current->memory);
        MemoryBlock *next = current->next;
        free(current);
        current = next;
    }
}

void clear_arena(Arena *arena){
    arena->current_block = arena->memory_blocks;
    arena->next_free_position = 0;
}

// NOTE: we expect that num_bytes will be much smaller than the size of a memory block. Nonetheless,
//  we manage even that case gracefully.
// If num_bytes is 0, NULL is returned.

void *allocate_(Arena *arena, size_t num_bytes){
    MemoryBlock *block = arena->current_block;
    assert(block != NULL); // PRE: arena already initialized

    // Align the pointer that we will return for faster memory access.
    // NOTE: since malloc could return a 8-aligned starting address for the memory in memory blocks, it is
    //  not enough to align the next_free_position of arena, we have to check with the final returning address.
    uintptr_t current_ptr = (uintptr_t)block->memory + arena->next_free_position;
    uintptr_t aligned_ptr = align_forward(current_ptr, DEFAULT_ALIGNMENT);
    size_t new_offset = aligned_ptr - (uintptr_t)block->memory + num_bytes;
    
    if(new_offset > block->size){
        bool already_next_allocated = block->next != NULL;
        if(already_next_allocated){
            arena->current_block = block->next;
            return allocate_(arena, num_bytes);
        }

        // Allocate a new memory block with enough space (if num_bytes less than the current block size, choose that)
        size_t new_block_size;
        if(num_bytes < block->size){
            new_block_size = block->size;
        }
        else if(num_bytes == block->size){
            new_block_size = 2*block->size;
        }
        else {
            new_block_size = calculate_block_size(num_bytes);
        }
        
        ++global_create_memory_block_calls;
        printf("Create Memory Block in Arena when resizing (call=%u)\n", global_create_memory_block_calls);
        MemoryBlock *new_memory_block = create_memory_block(new_block_size);

        // Insert the new block in the Arena and make a recursive call to take the alignment into account.
        block->next = new_memory_block;
        arena->current_block = new_memory_block;
        arena->next_free_position = 0;
        return allocate_(arena, num_bytes);
    }
    else { // There is enough space
        arena->next_free_position = new_offset;
        return aligned_ptr;
    }
}

void *allocate(Arena *arena, size_t num_bytes){
    if(num_bytes == 0){ return NULL; }
    return allocate_(arena, num_bytes);
}

void *callocate(Arena *arena, size_t num_bytes){
    if(num_bytes == 0){ return NULL; }
    void *mem = allocate_(arena, num_bytes);
    SET_TO_ZERO(mem, num_bytes);
    return mem;
}
