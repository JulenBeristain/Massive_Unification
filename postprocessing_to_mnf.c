#include "postprocessing_to_mnf.h"
#include "Schemas/utils.h"

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
void postprocess_to_mnf(Matrix *matrix) {
    
    // NOTE: arena to store temporarily the denormalized schemas and dependencies of each row. They have to be included in the
    //  matrix's schemas arena only when we know they become the schemas and dependencies of a new block. It stores the temporal
    //  common schemas too.
    Arena denormalized_arena; init_arena_defcapacity(&denormalized_arena);

    // TODO(YA2-OPT): we can skip the linear block... is_linear_block =  num_distinct_schema_variables(block.set_schema) == 0
    Block *block;
    intrusive_list_for_each_entry(block, &matrix->head_for_blocks, matrix_pos) {
        BlockRow *block_row;
        intrusive_list_for_each_entry(block_row, &block->head_for_rows, block_pos) {
            // Check if row is not well placed in block
            // Calculate the row's corresponding set schema.
            int *row = block_row->row;
            SetSchema *row_normalized_set_schema = block->normalized_schema;
            // TODO(YA3): review the memory management in the long run...
            ArrayListSchema *row_set_schema = allocate(&denormalized_arena, sizeof(*row_set_schema));
            SetDependencies *row_dependencies = allocate(&denormalized_arena, sizeof(*row_dependencies));
            denormalized_set_schema(*row_normalized_set_schema, row, row_set_schema, row_dependencies, &denormalized_arena);
        
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
            ArrayListSchema *common_set_schema = allocate(&denormalized_arena, sizeof(*common_set_schema));
            ArrayListDependencyPair *common_dependencies = allocate(&denormalized_arena, sizeof(*common_dependencies));
            bool is_row_in_corresponding_block = common_set_schema_strict_baseline(
                *row_set_schema, *row_dependencies,
                *block->schema, *block->dependencies,
                common_set_schema, common_dependencies,
                &denormalized_arena);

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
                        &denormalized_arena);
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
                            &denormalized_arena);
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
                    // TODO(YA): be careful! Moving might invalidate the standard non-safe way of iterating over Intrusive Lists!
                    move_row_to_block(current_block, block_row);
                    block->c--;
                    
                    // If the diminished row has no more rows left, it is removed from the Matrix.
                    // TODO(YA3): When Blocks won't be in the Arena, we should free their memory too!!! NOT really. Once the postprocessing
                    //  is finished, the Matrix is immutable. So we will only waste sizeof(*block) = 64 Bytes per each block that has lost
                    //  all its rows. We don't expect this case to be incredibly common + it's preferable to simplify the memory management
                    //  and do less operations regarding it than wasting 64 Bytes.
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
                    // TODO(YA): see how to manage the resulting extended_row to avoid using one extended row per row in the block + avoid also
                    //  multiple copies of rows back to the actual arrays in the block's rows...
                    // TODO(YA): think about efficient ways of managing single lifetimes per matrix. Would be great to have a method similar to Arenas to 
                    //  avoid allocating each little data in the matrix... One Arena per Matrix enough? --> How would we disard old data... I think Arenas
                    //  are not appropriate for long-run persistent data in applications, more used in games to store per-frame data...

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

    free_arena(denormalized_arena);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DEBUGGING AND TESTING ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE: these equal functions are only used for testing and debugging, they are not (at least right now) inherently interesting operations that will
//  be used with other operations.

typedef struct {
    EqualMatricesResultType type;
    unsigned mismatch_index;
} EqualBlockRowsResult;
#define EQUAL_BLOCK_ROWS_RESULT(Type, Index) (EqualBlockRowsResult){ .type = (Type), .mismatch_index = (Index) }

EqualBlockRowsResult equal_block_rows(BlockRow *block_row1, BlockRow *block_row2) {
    if (block_row1->c != block_row2->c) {
        return EQUAL_BLOCK_ROWS_RESULT(DIMENSION_MISMATCH, 0);
    }
    unsigned c = block_row1->c;

    for (unsigned i = 0; i < c; ++i) {
        if (block_row1->row[i] != block_row2->row[i]) {
            return EQUAL_BLOCK_ROWS_RESULT(ELEMENT_MISMATCH, i);
        }
    }
    return EQUAL_BLOCK_ROWS_RESULT(EQUAL, 0);
}


// TODO(YA2): as well as we have encapsulated the Variable *mapping when comparing Schemas and Dependencies, we should encapsulate the renaming of 
//  schema variables in its own operation too (for example, in common schema, where it's necessary to distinguish variables of the operand schemas
//  since they are ontologically different and if their values are the same, they are considered the same variable!). I.e., we could have a mapping
//  when a renaming happens, and after the operation is performed, we can rename the variables again to the prior values (or simply rename using
//  1,2,3... with a mapping constructed in that final phase...).


typedef struct {
    EqualMatricesResultType type;
    unsigned solitary_block_row;
} EqualBlocksResult;
#define EQUAL_BLOCKS_RESULT(Type, Index) (EqualBlocksResult){ .type = (Type), .solitary_block_row = (Index) }

// NOTE: block comparison doesn't take into account the order of the rows.
EqualBlocksResult equal_blocks(Block *b1, Block *b2) {
    
    if (!equivalent_set_schemas_and_dependencies_ignoring_empties(*b1->schema, *b2->schema, *b1->dependencies, *b2->dependencies)) {
        return EQUAL_BLOCKS_RESULT(NOT_EQUIVALENT_SCHEMAS_AND_DEPENDENCIES, 0);
    }
    unsigned size1 = set_schema_size(b1->normalized_schema), size2 = set_schema_size(b2->normalized_schema);
    if ((size1 != size2) || !equal_set_schemas(b1->normalized_schema->list, b2->normalized_schema->list)){
        return EQUAL_BLOCKS_RESULT(NOT_EQUAL_NORMALIZED_SCHEMAS, 0);
    }
    if (b1->c != b2->c) {
        return EQUAL_BLOCKS_RESULT(DIMENSION_MISMATCH, 0);
    }
    if (b1->r != b2->r) {
        return EQUAL_BLOCKS_RESULT(DIMENSION_MISMATCH, 0);
    }
    unsigned r = b1->r;

    // NOTE: each row in b2 has to match a single row in b1!
    bool *considered_rows_in_b2 = calloc(r, sizeof(*considered_rows_in_b2));
    CHECK_CALLOC(considered_rows_in_b2);

    BlockRow *block_row1;
    unsigned index1 = 0;
    intrusive_list_for_each_entry(block_row1, &b1->head_for_rows, block_pos) {
        unsigned index2 = 0;
        
        BlockRow *block_row2;
        intrusive_list_for_each_entry(block_row2, &b2->head_for_rows, block_pos) {
            if (!considered_rows_in_b2[index2]) {
                EqualBlockRowsResult result = equal_block_rows(block_row1, block_row2);
                if (result.type == EQUAL) {
                    considered_rows_in_b2[index2] = true;
                    break;
                }
                ++index2;
            }
        }

        bool not_found_corresponding_block = index2 == r;
        if (not_found_corresponding_block) {
            free(considered_rows_in_b2);
            return EQUAL_BLOCKS_RESULT(NOT_CORRESPONDING_ROW, index1);
        }

        ++index1;
    }

    free(considered_rows_in_b2);
    return EQUAL_BLOCKS_RESULT(EQUAL, 0);
}

// NOTE: matrix comparison doesn't take into account the order of the blocks and the rows within blocks.
EqualMatricesResult equal_matrices(Matrix *m1, Matrix *m2) {
    
    if(!equal_array_lists_char_ptr(m1->free_vars, m2->free_vars)) {
        return EQUAL_MATRICES_RESULT(NOT_EQUAL_FREEVARS, 0);
    }
    
    if (m1->b != m2->b) {
        return EQUAL_MATRICES_RESULT(DIMENSION_MISMATCH, 0);
    }
    unsigned b = m1->b;

    // NOTE: each block in m2 has to match a single block in m1!
    bool *considered_blocks_in_m2 = calloc(b, sizeof(*considered_blocks_in_m2));
    CHECK_CALLOC(considered_blocks_in_m2);

    Block *block1;
    unsigned index1 = 0;
    intrusive_list_for_each_entry(block1, &m1->head_for_blocks, matrix_pos) {
        unsigned index2 = 0;
        
        Block *block2;
        intrusive_list_for_each_entry(block2, &m2->head_for_blocks, matrix_pos) {
            if (!considered_blocks_in_m2[index2]) {
                EqualBlocksResult result = equal_blocks(block1, block2);
                if (result.type == EQUAL) {
                    considered_blocks_in_m2[index2] = true;
                    break;
                }
                ++index2;
            }
        }

        bool not_found_corresponding_block = index2 == b;
        if (not_found_corresponding_block) {
            free(considered_blocks_in_m2);
            return EQUAL_MATRICES_RESULT(NOT_CORRESPONDING_BLOCK, index1);
        }

        ++index1;
    }

    free(considered_blocks_in_m2);
    return EQUAL_MATRICES_RESULT(EQUAL, 0);
}



// TODO(YA-DEBUG-TESTING): would be great if we had loading and writing functions for some format of matrix_files (the ones of the last tests?)
//  See if we need different versions depending on operand and resultant matrices...

/**
 * @brief Reads one operand matrix from @p stream into @p ob.
 *
 * Expects the layout:
 *   <n_vars_with_deps>, [dep lines ×n_vars_with_deps], <flattened schema>,
 *   <main_term rows>…, "%% END"
 *
 * Exits on format errors.
 */
static void read_block_content(FILE *stream, Block *block, Arena *matrix_arena, Arena *schemas_arena) {
    // Read unflatened schema and the set of dependencies
    read_set_schema_with_dependencies(stream, block->schema, block->dependencies, schemas_arena);
    block->normalized_schema = allocate(schemas_arena, sizeof(*block->normalized_schema));
    block->normalized_schema->list = normalized_set_schema(*block->schema, *block->dependencies, schemas_arena);
    set_schema_size(block->normalized_schema);

    char   *line = NULL;
    size_t  len  = 0;

    /* Skip the flattened schema line. */
    getline(&line, &len, stream);

    /* Read main_term rows until the END marker or the expected row count is reached. */
    ssize_t read;
    unsigned row = 0;
    while ((read = getline(&line, &len, stream)) != -1 && row < block->r) {
        if (strstr(line, "% END") || strstr(line, "% End")) break;

        /* First token is the exception-block count. */
        char *line_copy = strdup(line);
        unsigned e = (unsigned)strtoul(strtok(line_copy, ","), NULL, 10);
        free(line_copy);

        BlockRow *block_row = allocate(matrix_arena, sizeof(*block_row));
        block_row->c = block->c;
        intrusive_list_add(&block->head_for_rows, &block_row->block_pos);
        // TODO(YA): review what the final boolean was for and how it differs from operand to resultant blocks
        read_line(line, block_row->row, true);

        // TODO(YA): make read_exception_blocks accessible
        if (e) read_exception_blocks(stream, block_row, false);

        row++;
    }

    free(line);
}


/**
 * @brief Reads one complete operand block (header + matrix) from @p stream.
 * @return Populated operand_block.
 */
void read_block(FILE *stream, Block *block, Arena *matrix_arena, Arena *schemas_arena) {
    // TODO(YA): make read_dimensions visible + make it return some value instead of exiting!!!
    read_dimensions(stream, &block->r, &block->c);
    
    init_intrusive_list(&block->head_for_rows);
    
    read_block_content(stream, block, matrix_arena, schemas_arena);
}


ReadMatrixResultType read_matrix(char *filename, Matrix *matrix) {
    FILE *stream = fopen(filename, "r");

    if (!stream) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        if (stream) fclose(stream);
        return RM_FILE_NOT_OPENED;
    }

    /* Read block counts from M1 and M2 headers. */
    // TODO(YA): make accessible the read_num_blocks function
    if (read_num_blocks(stream, &matrix->b)){
        return RM_NOT_BLOCK_COUNT;
    }

    // Read the row identifying the columns' free variables
    {
        char *line = NULL;
        size_t len = 0;
        if (getline(&line, &len, stream) == -1) { 
            free(line);
            return RM_NOT_FREE_VARS; 
        }
        // TODO(YA): make accessible scan_num_free_vars
        unsigned num_free_vars = scan_num_free_vars(line);
    
        // TODO(FUT): we could put the size information in the header of the file, to adjust the size of Memory Blocks...
        //  Right now we only know matrix.b
        init_arena(&matrix->blocks_arena, KILOBYTES(4));
        init_arena(&matrix->schemas_arena, KILOBYTES(4));
    
        // TODO(YA): make scan_free_vars accessible
        ArrayListCharPtr free_vars = scan_free_vars(line, num_free_vars, &matrix->blocks_arena);
        free(line);
    }

    init_intrusive_list(&matrix->head_for_blocks);
    for (unsigned i = 0; i < matrix->b; ++i) {
        Block *block = allocate(&matrix->blocks_arena, sizeof(*block));
        
        intrusive_list_add(&matrix->head_for_blocks, &block->matrix_pos);

        read_block(stream, block, &matrix->blocks_arena, &matrix->schemas_arena);
    }
    
    fclose(stream);
    return RM_SUCCESS;
}
