#include "postprocessing_to_mnf.h"

// TODO(YA): add this module to the building task


// TODO(YA): postprocessing to recover MNF
// - Check if a row is not well placed in a fragment
//      - Check if the fragment's schema and the row's schema cumply the strict conditions. If they do, the row is
//      well placed. If not, it has to be moved to another fragment. If there is no more rows in the fragment, 
//      remove it from the matrix too (or change the fragments schema to this row's one, no need to delete a 
//      one row fragment to create another one).
//
// - Check if there is a fragment that cumplies the strict conditions with the row's schema.
//      - If it exists, introduce the row to that fragment ==> Have to extend all the rows in the fragment according to the new common schema! (could do some optimizations looking if the new common's size is the same as the previous one and only the new row needs to be extended...)
//      - If it doesn't, create a new_elem fragment with the row and the row's schema as the new_elem fragment's schema too.
//
// - We have to perform the previous operations for all rows in every fragment in the resulting matrix.
//
// - Operations to be implemented:
//      - check_corresponding_fragment(row_schema, fragment_schema) -> bool [Use first_check, unique_dependency_between_vars, and remove_variables_not_in_set_schema_from_dependencies in case of true!]
//      - postprocess(matrix) -> void [matrix to MNF]

// TODO(YA3): see if we need to accept more than one type of Arenas or if we create a local Arena for intermediate operations... The thing is that each Matrix
//  should have its own lifetime, and therefore, its memory (with everything it points to) too...
void postprocess_to_mnf(Matrix *matrix, Arena *arena) {
    

    // TODO(YA2-OPT): we can skip the linear block...
    Block *block;
    intrusive_list_for_each_entry(block, &matrix->head_for_blocks, matrix_pos) {
        BlockRow *block_row;
        intrusive_list_for_each_entry(block_row, &block->head_for_rows, block_pos) {
            // Check if row is not well placed in block
            // Calculate the row's corresponding set schema.
            int *row = block_row->row;
            SetSchema *row_normalized_set_schema = block->normalized_schema;
            // TODO(YA3): review the memory management in the long run...
            ArrayListSchema *row_set_schema = allocate(arena, sizeof(*row_set_schema));
            SetDependencies *row_dependencies = allocate(arena, sizeof(*row_dependencies));
            denormalized_set_schema(*row_normalized_set_schema, row, row_set_schema, row_dependencies, arena);
        
            // TODO(YA4-Clean): the logic is repeated for the current block and the rest of the blocks, I think they can be compacted... But NO if the 
            //  optimization below is implemented! (Then the operations on the Block that contains the Row will be distinct indeed...)
            // Compare row's schema and dependencies with the block's ones
            // TODO(YA3): before matrix_intersection, when calculating common schemas, we can use the strict version storing the fact that the "soft" result complies the strict conditions or not
            //  Why? Because if it doesn't comply the strict conditions, we know that we will have to move every single row from the Block to other blocks...
            //  --> That way, in the case the resulting block schema cumplies the strict conditions, we don't need to calculate the whole common schema with the row schema,
            //  we only need to check the first_condition, since we know that the row_schema doesn't have dependencies with schemas with schema-variables in them...
            // TODO(YA2): taking into account that the lifetime of the matrix is unique, here we should use a non-arena version that returns a pointer to the object in the heap directly... 
            //  That means that whenever Blocks' schemas and dependencies are updated, the old ones have to be freed too!!! 
            // TODO(YA2): currently, as we see, using Arenas is no good due to the amount of calls to common_set_schema_strict that might give invalid results...
            ArrayListSchema *common_set_schema = allocate(arena, sizeof(*common_set_schema));
            ArrayListDependencyPair *common_dependencies = allocate(arena, sizeof(*common_dependencies));
            bool is_row_in_corresponding_block = common_set_schema_strict_baseline(
                *row_set_schema, *row_dependencies,
                *block->schema, *block->dependencies,
                common_set_schema, common_dependencies,
                arena);

            // If we need to move the Row, we have to see if already exists a valid block
            if (!is_row_in_corresponding_block) {

                // Find valid block
                bool is_valid_block_found = false;
                Block *current_block = intrusive_list_entry(&block->matrix_pos.next, Block, matrix_pos); 
                intrusive_list_for_each_entry_since(current_block, &matrix->head_for_blocks, matrix_pos) {
                    is_valid_block_found = common_set_schema_strict_baseline(
                        *row_set_schema, *row_dependencies,
                        *current_block->schema, *current_block->dependencies,
                        common_set_schema, common_dependencies,
                        arena);
                    if (is_valid_block_found) {
                        break;
                    }
                }
                if (!is_valid_block_found) {
                    intrusive_list_for_each_entry_until(current_block, &matrix->head_for_blocks, matrix_pos, block) {
                        is_valid_block_found = common_set_schema_strict_baseline(
                            *row_set_schema, *row_dependencies,
                            *current_block->schema, *current_block->dependencies,
                            common_set_schema, common_dependencies,
                            arena);
                        if (is_valid_block_found) {
                            break;
                        }
                    }
                }
                // NOTE: if is_valid_block_found, current_block is the block that the row is going to move to, and in 
                //  common_set_schema and common_dependencies we have the valid combination.

                // If found,.
                //  Have to set the new block schema and dependencies to the common ones (watch out the normalized!)
                if (is_valid_block_found) {
                    // Insert the row into the valid block, updating the row counts of both blocks
                    move_row_to_block(current_block, block_row);
                    block->c--;
                    
                    // If the diminished row has no more rows left, it is removed from the Matrix.
                    // TODO(YA3): When Blocks won't be in the Arena, we should free their memory too!!!
                    if (block->c == 0) {
                        remove_block_from_matrix(matrix, block);
                    }

                    current_block->schema = common_set_schema;
                    current_block->dependencies = common_dependencies;
                    // TODO(YA): have to decide if we want to have the normalized schema always available... NOTE that currently that is an assumption
                    //  at the beginning of the second inner loop --> To extend the rows we need the normalized one...
                    current_block->normalized_schema = ;
                    unsigned old_c = current_block->c;
                    current_block->c = ;

                    // Have to extend all the rows in the block according to the new common schema! 
                    // (could do some optimizations looking if the new common's size is the same as the previous one and only the new row needs to be extended...)
                    // TODO(YA2): should look at the common schema to see if it is lineal, to determine which version of extend_row has to be called
                    // TODO(YA!!!): have to see how to manage free_var_positions, starting_col_indices, free_vars... At least free_vars should be stored
                    //      in the matrix...
                    
                    if (old_c == current_block->c) {
                        
                        if (the moved row has different size too)

                        extend_row(
                            SetSchema normalized_set_schema, ArrayListUInt free_var_positions, 
                            SetSchema normalized_common_set_schema,
                            unsigned *starting_col_indices, int *row,
                            int *extended_row,
                            Arena *row_vars_arena,
                            Arena *schema_iterator_arena);

                    } else {

                        BlockRow *current_row;
                        intrusive_list_for_each_entry(current_row, &current_block->head_for_rows, block_pos) {
                            extend_row(
                            SetSchema normalized_set_schema, ArrayListUInt free_var_positions, 
                            SetSchema normalized_common_set_schema,
                            unsigned *starting_col_indices, int *row,
                            int *extended_row,
                            Arena *row_vars_arena,
                            Arena *schema_iterator_arena);
                        }

                    }


                } else {
                    // Else, if only one row in the current block, change the block's schemas (NOTE: the normalized one is the same!)
                    //  If more than one remaining, create a new block with a single row.

                    if (block->r == 1) {
                        block->schema = common_set_schema;
                        block->dependencies = common_dependencies;
                    } else {
                        assert(block->r > 1);

                        // TODO(YA2): shouldn't use an Arena to store the new Block...
                        Block *new_block = allocate(arena, sizeof(*new_block));

                        init_block_with_deleted_row(new_block, 1, row_set_schema, row_dependencies, block_row, matrix);
                    }

                }
            }
        }
    }
}