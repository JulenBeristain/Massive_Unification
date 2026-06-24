#include "postprocessing_to_mnf.h"
#include "Schemas/utils.h"
#include <assert.h>

// TODO: change partition_pos to pos_in_partition; header_pos to pos_in_header s/_list
typedef struct {
    Block *block_ptr;
    SetSchema *final_normalized_schema; // NOTE: helps avoiding recalculations of normalized set schemas
    IntrusiveList partition_head;
    IntrusiveList header_pos;
} PartitionListHeader;

typedef struct {
    Block *original_block;
    BlockRow *final_row_of_partition;
    IntrusiveList partition_pos;
} PartitionListNode;

static PartitionListHeader *find_header_that_contains_block(IntrusiveList *headers_head, Block *block) {
    PartitionListHeader *result;
    intrusive_list_for_each_entry(result, headers_head, header_pos) {
        if (result->block_ptr == block) {
            return result;
        }
    }
    return NULL;
}

void postprocess_to_mnf(Matrix *matrix, Arena *operation_arena, Arena scratch_arena) {
    
    // NOTE: is inserting the new blocks to the head of the linked list correct? YES! (Actually, it is even more efficient)
    //  The rows of those new blocks are not going to be checked. Since they come from rows that were already moved from 
    //  another block and who didn't match with any existing block, there is no need to check them once again. Furthermore, 
    //  when looking for a valid block, these new blocks are taken into account indeed, so no problem!

    // TODO: we should avoid iterating over the same row more than once, no? Right now, we can move a row to a posterior block, and check that row
    //  unnecessarily... We can add a flag 'is_moved' (false initialized) to the struct BlockRow. Or since we add rows from the beginning, we can store the
    //  nodes of the starting block rows and do the since version of the for_each loop! --> PROBLEM: what if that is moved? NO PROBLEM, already iterating over
    //  that block. In fact, we don't need to store the first blocks starting node! SHOULD USE THE AFTER_SAFE VERSION!!!

    // Calculate the maximum schema variable in the matrix to use independent values when denormalizing set schemas per row
    unsigned max_schema_v = 0;
    Block *block;
    intrusive_list_for_each_entry(block, &matrix->head_for_blocks, matrix_pos) {
        max_schema_v = MAX(max_schema_v, max_v_in_set_schema(*block->set_schema));
    }
    
    // First pass: move the rows "virtually", updating the common schemas and dependencies of the blocks. Have to store the pointers to the appropriate normalized set
    //  schemas of the rows that were moved (they are the normalized schemas of the blocks that initially contained the moved row), and the rest of the rows that 
    //  are in a block that has received new rows (whose normalized set schema will be the initial normalized set schema of the block). Since blocks and rows are 
    //  traversed in order, the rows in a block that receives new rows are partitioned in order too. I.e., first all the rows from block N, then all the rows from 
    //  block M (M < N), ..., then all the original rows (that were not moved to another block).
    IntrusiveList partition_headers_head; init_intrusive_list(&partition_headers_head);

    intrusive_list_for_each_entry(block, &matrix->head_for_blocks, matrix_pos) {
        
        // TODO: if we knew that if exists any linear block, it would be the first one, we could start directly from the second one, avoiding
        //  having an if-condition in the loop... ==> For this optimization to be valid, we would need to introduce blocks at the tail, not
        //  at the beginning. But that would make that rows in new blocks are analyzed two times. A solution could be to separate the linear
        //  block at the matrix struct with a pointer, updating the following code because some rows can move from a non-linear to the linear
        //  block.
        // NOTE: linearity is preserved, so we know that all rows in the linear block which is the combination of two operand linear blocks
        //  will remain in the resulting linear block.
        bool is_linear = set_schema_contains_variables(*block->set_schema);
        if (is_linear) {
            continue;
        }

        // NOTE: the safe version is necessary because we can move block_rows to other blocks, breaking its intrusive list's next pointer
        BlockRow *block_row, *next_buffer_ptr;
        intrusive_list_for_each_entry_safe(block_row, next_buffer_ptr, &block->head_for_rows, block_pos) {
            // Check if row is not well placed in block
            // Calculate the row's corresponding set schema.
            int *row = block_row->row;
            SetSchema *row_normalized_set_schema = block->normalized_set_schema;

            ArenaState operation_arena_before_denormalization = register_state_arena(operation_arena);

            SetSchema *row_set_schema = PUSH_SINGLE(operation_arena, *row_set_schema);
            SetDependencies *row_dependencies = PUSH_SINGLE(operation_arena, *row_dependencies);
            denormalized_set_schema(max_schema_v, row_normalized_set_schema, row, row_set_schema, row_dependencies, operation_arena, scratch_arena);
        
            // Compare row's and block's set schemas and dependencies
            
            // TODO: the logic is repeated for the current block and the rest of the blocks, I think they can be compacted... But NO if the 
            //  optimization below is implemented! (Then the operations on the Block that contains the Row will be distinct indeed...)
            // vs
            //  Before, in block_and-s, when calculating common schemas, we can use the strict version storing the fact that the "soft" result complies the strict conditions or not
            //  Why? Because if it doesn't comply the strict conditions, we know that we will have to move every single row from the Block to other blocks...
            //  That way, we avoid calling to common_set_schema between the schemas of the row and the schemas of the resulting fragments that do not cumply the
            //  strict conditions, we would only need to iterate through the resulting fragments whose schemas have respected the strict conditions.
            //  --> Additionally, in the case the resulting block schema cumplies the strict conditions, we don't need to calculate the whole common schema with the row schema,
            //  we only need to check the first_condition, since we know that the row_schema doesn't have dependencies with schemas with schema-variables in them...
            //  --> I wouldn't store those flags in the matrices memory, because we must ensure that all Matrices are in MNF once their calculation / parsing is finished...
            //  We will need to allocate them in some temporal arena of the matrix_and function, and pass that information here. That said, if the current Matrix format
            //  is eventually considered a temporal internal format to optimized the postprocessing step, then we could have this information stored in the Matrix's memory.
            
            ArenaState scratch_arena_before_common = register_state_arena(&scratch_arena);
            SetSchema *common_set_schema = PUSH_SINGLE(&scratch_arena, *common_set_schema);
            SetDependencies *common_dependencies = PUSH_SINGLE(&scratch_arena, *common_dependencies);
            bool is_row_in_corresponding_block = common_set_schema_strict_baseline(
                *row_set_schema, *row_dependencies,
                *block->set_schema, *block->dependencies,
                common_set_schema, common_dependencies,
                &scratch_arena);
            pop_to_state_arena(&scratch_arena, scratch_arena_before_common);

            // If we need to move the Row, we have to see if already exists a valid block
            if (is_row_in_corresponding_block) {
                pop_to_state_arena(operation_arena, operation_arena_before_denormalization);
            
            } else {
                // Find valid block
                bool is_valid_block_found = false;
                Block *current_block = intrusive_list_entry(&block->matrix_pos.next, Block, matrix_pos); 
                intrusive_list_for_each_entry_since(current_block, &matrix->head_for_blocks, matrix_pos) {
                    ArenaState operation_arena_before_common = register_state_arena(operation_arena);
                    is_valid_block_found = common_set_schema_strict_baseline(
                        *row_set_schema, *row_dependencies,
                        *current_block->set_schema, *current_block->dependencies,
                        common_set_schema, common_dependencies,
                        operation_arena);
                    if (is_valid_block_found) {
                        break;
                    }
                    pop_to_state_arena(operation_arena, operation_arena_before_common);
                }
                if (!is_valid_block_found) {
                    intrusive_list_for_each_entry_until(current_block, &matrix->head_for_blocks, matrix_pos, block) {
                        ArenaState operation_arena_before_common = register_state_arena(operation_arena);
                        is_valid_block_found = common_set_schema_strict_baseline(
                            *row_set_schema, *row_dependencies,
                            *current_block->set_schema, *current_block->dependencies,
                            common_set_schema, common_dependencies,
                            operation_arena);
                        if (is_valid_block_found) {
                            break;
                        }
                        pop_to_state_arena(operation_arena, operation_arena_before_common);
                    }
                }
                // NOTE: if is_valid_block_found, current_block is the block that the row is going to move to, and in 
                //  common_set_schema and common_dependencies we have the valid combination.

                // Have to set the new block schema and dependencies to the common ones.
                // NOTE: the normalized one is not modified yet because we need the original normalized schemas of rows in the second pass for row extensions
                if (is_valid_block_found) {
                    Block *destination_block = current_block;

                    // Insert the row into the valid block, updating the row counts of both blocks
                    move_row_to_block(destination_block, block_row);
                    block->c--;
                    
                    // If the diminished row has no more rows left, it is removed from the Matrix.
                    
                    // NOTE: here, we could waste some memory in the blocks_arena of the Matrix. It is not so important, since once the postprocessing
                    //  is finished, the Matrix is immutable. So we will only waste sizeof(*block) = 64 Bytes per each block that has lost
                    //  all its rows. We don't expect this case to be common. It's preferable to simplify the memory management
                    //  and do less operations regarding it than wasting 64 Bytes per empty block.
                    // TODO: if we implement the optimization above where we identify the common schemas that don't fulfill the strict conditions
                    //  before calculating block_and, we could store the memory of those blocks that we know will be removed in a temporal memory buffer.
                    //  Even with that, I think having some wasted memory here is not a big deal.
                    if (block->c == 0) {
                        remove_block_from_matrix(matrix, block);
                    }

                    destination_block->set_schema = common_set_schema;
                    destination_block->dependencies = common_dependencies;

                    PartitionListHeader *new_extended_block_s_header = find_header_that_contains_block(&partition_headers_head, destination_block);
                    if (new_extended_block_s_header == NULL) {
                        // NOTE: it is the first time a row is moved to the current destination_block
                        new_extended_block_s_header = PUSH_SINGLE(&scratch_arena, *new_extended_block_s_header);
                        new_extended_block_s_header->block_ptr = destination_block;
                        new_extended_block_s_header->final_normalized_schema = normalized_set_schema_arena(
                            *destination_block->set_schema, 
                            *destination_block->dependencies,
                            operation_arena
                        );
                        intrusive_list_add(&partition_headers_head, &new_extended_block_s_header->header_pos);
                        init_intrusive_list(&new_extended_block_s_header->partition_head);

                        // NOTE: since rows are added to Blocks in reversed order, the last block_row of this partition will be the first one
                        PartitionListNode *node = PUSH_SINGLE(&scratch_arena, *node);
                        node->final_row_of_partition = block_row;
                        node->original_block = block;
                        intrusive_list_add(&new_extended_block_s_header->partition_head, &node->partition_pos);

                    } else {
                        
                        PartitionListNode *last_added = intrusive_list_first_entry(&new_extended_block_s_header->partition_head, PartitionListNode, partition_pos);
                        bool block_s_partition_already_included = last_added->original_block == block;
                        if (!block_s_partition_already_included) {
                            // NOTE: it is the first time a row in the current block is moved to the current destination_block
                            PartitionListNode *node = PUSH_SINGLE(&scratch_arena, *node);
                            node->final_row_of_partition = block_row;
                            node->original_block = block;
                            intrusive_list_add(&new_extended_block_s_header->partition_head, &node->partition_pos);
                        }
                    }

                } else {
                    // Else, if only one row in the current block, change the block's schemas (NOTE: the normalized one is the same!)
                    //  If more than one remaining, create a new block with a single row.

                    if (block->r == 1) {
                        block->set_schema = row_set_schema;
                        block->dependencies = row_dependencies;
                    } else {
                        assert(block->r > 1);

                        Block *new_block = PUSH_SINGLE(&matrix->arena, *new_block);
                        new_block->set_schema = row_set_schema;
                        new_block->dependencies = row_dependencies;
                        new_block->normalized_set_schema = block->normalized_set_schema;
                        
                        new_block->r = 1;
                        new_block->c = set_schema_size(new_block->normalized_set_schema);
                        
                        move_row_to_block(new_block, block_row);
                        block->r--;
                        
                        add_block_to_matrix(matrix, new_block);
                    }
                }
            }
        }
    }

    // Second pass: extend the rows with respect to the final common schemas of each block, and store the final common schemas in the permanent memory of the Matrix.
    PartitionListHeader *header;
    intrusive_list_for_each_entry(header, &partition_headers_head, header_pos) {
        
        Block *block_to_be_extended = header->block_ptr;

        ExtendRowNoFreeVarsFunc extend_row = 
            set_schema_contains_variables(*block_to_be_extended->set_schema) ? extend_row_no_free_vars : extend_row_lineal_no_free_vars;

        SetSchema *new_common_normalized = header->final_normalized_schema;

        block_to_be_extended->c = set_schema_size(new_common_normalized);

        BlockRow *block_row_to_be_extended = intrusive_list_first_entry(&block_to_be_extended->head_for_rows, BlockRow, block_pos);

        PartitionListNode *node;
        intrusive_list_for_each_entry(node, &header->partition_head, partition_pos) {
            SetSchema *original_normalized_schema_of_partition = node->original_block->normalized_set_schema;

            BlockRow *inclusive_end = node->final_row_of_partition;
            BlockRow *exclusive_end = intrusive_list_next_entry(inclusive_end, block_pos);
            
            if (set_schema_size(original_normalized_schema_of_partition) == block_to_be_extended->c) {
                // No need to extend the rows in this partition --> Skip the partition
                block_row_to_be_extended = exclusive_end;
            } else {
                // Have to extend the rows in this partition
                intrusive_list_for_each_entry_since_until(
                    block_row_to_be_extended, &block_to_be_extended->head_for_rows, block_pos, exclusive_end) 
                {
                    int *extended_row = PUSH_ARRAY(&matrix->arena, *extended_row, block_to_be_extended->c);
                    extend_row(
                        original_normalized_schema_of_partition, 
                        new_common_normalized,
                        block_row_to_be_extended->row,
                        extended_row,
                        scratch_arena);
                    block_row_to_be_extended->row = extended_row;
                    // NOTE: as well as blocks that are removed make us waste some memory with their headers, the old rows
                    //  remain in the matrix's memory too
                }
            }
        }

        // Check the rest of the rows, i.e., any row that has remained in its initial block
        SetSchema *original_normalized_schema_of_partition = block_to_be_extended->normalized_set_schema;
        if (set_schema_size(original_normalized_schema_of_partition) != block_to_be_extended->c) {
            assert(set_schema_size(original_normalized_schema_of_partition) < block_to_be_extended->c);

            intrusive_list_for_each_entry_since(block_row_to_be_extended, &block_to_be_extended->head_for_rows, block_pos) {
                int *extended_row = PUSH_ARRAY(&matrix->arena, *extended_row, block_to_be_extended->c);
                extend_row(
                    original_normalized_schema_of_partition, 
                    new_common_normalized,
                    block_row_to_be_extended->row,
                    extended_row,
                    scratch_arena);
                block_row_to_be_extended->row = extended_row;
            }
        }
    }

    // Third pass: update the normalized common schemas to the last ones
    intrusive_list_for_each_entry(header, &partition_headers_head, header_pos) {
        header->block_ptr->normalized_set_schema = header->final_normalized_schema;
    }
}

// TODO: once the matrix is postprocessed to MNF and we know the final result, we could pass the entire matrix to a more coalescing
//  memory access friendly, compact, GPU bandwith friendly format where all data is as compact in memory as possible. That is because the
//  formulas are what they are (i.e., immutable). If that is the idea, the intermediate memory management/layout of the Matrix format loses
//  importance. We can change to a traditional multiple malloc strategy where we ensure we free the memory that we won't be using furthermore
//  to store in the final result of the intermediate format, or if the wasted memory of old rows is not that big, we could simply allocate
//  double the memory (a little more, because rows are extended) because in the worst case each row is extended in the postprocessing step.
//  Then, we would use the actual result to generate the coalesced format and free all the memory of the intermediate format, including the
//  old rows not relevant for the result, and the resulting rows.
// ==> In the case we finally design the final compact format, the parsing and writing functions should only use those formats, not the 
//  intermediate one. The latter should only be an internal format to be able to perform postprocess in a more efficient way.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DEBUGGING AND TESTING ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: although being debugging functions, we could make this cleaner, instead of splitting reading code in two files...
extern ArrayListCharPtr scan_free_vars(char *line, unsigned num_free_vars, Arena *arena);
extern unsigned scan_num_free_vars(char *line);
extern int read_num_blocks(FILE *stream, unsigned *s);
extern void read_dimensions(FILE *stream, unsigned *n, unsigned *m);
extern void read_exception_blocks(FILE *stream, main_term *mt, bool result);
extern void read_line(char *line, int *row, bool skip_first);

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

typedef struct {
    EqualMatricesResultType type;
    unsigned solitary_block_row;
} EqualBlocksResult;
#define EQUAL_BLOCKS_RESULT(Type, Index) (EqualBlocksResult){ .type = (Type), .solitary_block_row = (Index) }

// NOTE: block comparison doesn't take into account the order of the rows.
EqualBlocksResult equal_blocks(Block *b1, Block *b2) {
    
    if (!equivalent_set_schemas_and_dependencies_ignoring_empties(*b1->set_schema, *b2->set_schema, *b1->dependencies, *b2->dependencies)) {
        return EQUAL_BLOCKS_RESULT(NOT_EQUIVALENT_SCHEMAS_AND_DEPENDENCIES, 0);
    }
    unsigned size1 = set_schema_size(b1->normalized_set_schema), size2 = set_schema_size(b2->normalized_set_schema);
    if ((size1 != size2) || !equal_set_schemas(*b1->normalized_set_schema, *b2->normalized_set_schema)){
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



// TODO(YA-TEST): would be great if we had loading and writing functions for some format of matrix_files (the ones of the last tests?)
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
static void read_block_content(FILE *stream, Block *block, Arena *matrix_arena) {
    // Read unflatened schema and the set of dependencies
    read_set_schema_with_dependencies(stream, block->set_schema, block->dependencies, matrix_arena);
    block->normalized_set_schema = normalized_set_schema_arena(*block->set_schema, *block->dependencies, matrix_arena);
    set_schema_size(block->normalized_set_schema);

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

        read_line(line, block_row->row, true);

        // NOTE: obviously, update these when exception blocks are included!!!
        if (e) read_exception_blocks(stream, (main_term *)block_row, false);

        row++;
    }

    free(line);
}


/**
 * @brief Reads one complete operand block (header + matrix) from @p stream.
 * @return Populated operand_block.
 */
void read_block(FILE *stream, Block *block, Arena *matrix_arena) {
    // TODO: read_dimensions shouldn't exit the program, but return a value if the file has an erroneous format!
    read_dimensions(stream, &block->r, &block->c);
    
    init_intrusive_list(&block->head_for_rows);
    
    read_block_content(stream, block, matrix_arena);
}


ReadMatrixResultType read_matrix(char *filename, Matrix *matrix) {
    FILE *stream = fopen(filename, "r");

    if (!stream) {
        fprintf(stderr, "Error opening file: %s\n", filename);
        if (stream) fclose(stream);
        return RM_FILE_NOT_OPENED;
    }

    /* Read block counts from M1 and M2 headers. */
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

        unsigned num_free_vars = scan_num_free_vars(line);

        init_arena(&matrix->arena, KILOBYTES(4));
    
        matrix->free_vars = scan_free_vars(line, num_free_vars, &matrix->arena);
        free(line);
    }

    init_intrusive_list(&matrix->head_for_blocks);
    for (unsigned i = 0; i < matrix->b; ++i) {
        Block *block = PUSH_SINGLE(&matrix->arena, *block);
        
        intrusive_list_add(&matrix->head_for_blocks, &block->matrix_pos);

        read_block(stream, block, &matrix->arena);
    }
    
    fclose(stream);
    return RM_SUCCESS;
}
