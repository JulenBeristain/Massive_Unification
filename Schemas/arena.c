#include "arena.h"
#include "utils.h"
#include "hash_to_pointers.h"
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
    CHECK_MALLOC(memory_block);

    memory_block->memory = malloc(block_size);
    CHECK_MALLOC(memory_block->memory);
    
    memory_block->size = block_size;
    memory_block->next = NULL;
    return memory_block;
}

// NOTE: we expect size_in_bytes to be a big value, to be able to perform a lot of allocations with a single memory block,
//  hopefully avoiding to allocate further blocks.
void init_arena_chained(Arena *arena, size_t size_in_bytes){
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
void free_arena_chained(Arena arena){
    MemoryBlock *current = arena.memory_blocks;
    while(current){
        free(current->memory);
        MemoryBlock *next = current->next;
        free(current);
        current = next;
    }
}

void clear_arena_chained(Arena *arena){
    arena->current_block = arena->memory_blocks;
    arena->next_free_position = 0;
}

// NOTE: we expect that num_bytes will be much smaller than the size of a memory block. Nonetheless,
//  we manage even that case gracefully.
// If num_bytes is 0, NULL is returned.

void *allocate_chained_(Arena *arena, size_t num_bytes){
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

void *allocate_chained(Arena *arena, size_t num_bytes){
    if(num_bytes == 0){ return NULL; }
    return allocate_chained_(arena, num_bytes);
}

void *callocate_chained(Arena *arena, size_t num_bytes){
    if(num_bytes == 0){ return NULL; }
    void *mem = allocate_(arena, num_bytes);
    SET_TO_ZERO(mem, num_bytes);
    return mem;
}


#include "../postprocessing_to_mnf.h"

void init_schemas_arena(SchemasArena *arena, size_t size_in_bytes) {
    arena->memory = malloc(size_in_bytes);
    CHECK_MALLOC(arena->memory);
    arena->size = size_in_bytes;
    arena->next_free_position = 0;
}

// TODO(YA): RENAME THE SCHEMAS ARENA, BECAUSE WE CHANGE THE APPROXIMATION TO HANDLE THE MEMORY OF SCHEMAS!!!

void *allocate_schemas_arena(SchemasArena *arena, size_t num_bytes) {
    assert(arena->memory != NULL); // PRE: arena already initialized

    // Align the pointer that we will return for faster memory access.
    // NOTE: since malloc could return a 8-aligned starting address for the memory in arena, it is
    //  not enough to align the next_free_position of arena, we have to check with the final returning address.
    uintptr_t current_ptr = (uintptr_t)arena->memory + arena->next_free_position;
    uintptr_t aligned_ptr = align_forward(current_ptr, DEFAULT_ALIGNMENT);
    size_t new_offset = aligned_ptr - (uintptr_t)arena->memory + num_bytes;
    
    if(new_offset > arena->size){
        // TODO(YA): should traverse all Schemas in the program and shallow copy with a HashSet of addresses of Schemas
        //  the current active Schemas.
#if 0
        // create the new arena
        ;;;

        extern HashMapStringToPointer matrices;
        HashSetPointers addresses_of_copied_schemas = create_hash_set_pointers_defnumbuckets();

        // TODO(FUT): would be helpful to know if the Arena is resizing while postprocessing some matrix to MNF. If that is
        //  not the case, we know that BlockRows are not pointing to their denormalized schemas. Furthermore, we could identify
        //  which matrix/matrices (if parallelized) are being postprocessed, and travers their BlockRows only... 
        bool is_called_while_postprocessing = true;

        foreach_in_hashmap_string_to_pointer(matrices, pair) {
            Matrix *matrix = pair->ptr;

            Block *block;
            intrusive_list_for_each_entry(block, &matrix->head_for_blocks, matrix_pos) {
                block->schema;
                block->dependencies;
                block->normalized_schema;

                intrusive_list_
            
                // TODO(YA): what happens if the SchemasArena is filled when calculating Schemas of rows!? We need to traverse
                //  through them too! Therefore, we need to store that information in BlockRow!!!

                // TODO(YA): not enough to simply copy the Schemas in the old Arena memory, because the pointers change!
                //  No solamente necesitamos hash set de addresses the Schemas ya vistos, sino que hay que hacer un mapeo
                //  address viejo a address nuevo!
            }
        }

        // free the previous arena
        ;;;
#else
        assert(false);
#endif
    }
    else { // There is enough space
        arena->next_free_position = new_offset;
        return aligned_ptr;
    }
}

void *callocate_schemas_arena(SchemasArena *arena, size_t num_bytes) {
    if(num_bytes == 0){ return NULL; }
    void *mem = allocate_schemas_arena(arena, num_bytes);
    SET_TO_ZERO(mem, num_bytes);
    return mem;
}
