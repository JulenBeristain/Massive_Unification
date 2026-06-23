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


typedef struct {
    Block *block_ptr;
    IntrusiveList partition_head;
    IntrusiveList header_pos;
} PartitionListHeader;

typedef struct {
    Block *original_block;
    BlockRow *final_row_of_partition;
    IntrusiveList partition_pos;
} PartitionListNode;

// TODO(YA): try to not use this function. If it is possible, remove it!
static PartitionListHeader *find_header_that_contains_block(IntrusiveList *headers_head, Block *block) {
    PartitionListHeader *result;
    intrusive_list_for_each_entry(result, headers_head, header_pos) {
        if (result->block_ptr == block) {
            return result;
        }
    }
    return NULL;
}

void postprocess_to_mnf(Matrix *matrix, Arena operation_arena) {
    
    // NOTE: arena to store temporarily the denormalized schemas and dependencies of each row. They have to be included in the
    //  matrix's schemas arena only when we know they become the schemas and dependencies of a new block. It stores the temporal
    //  common schemas too.
    Arena denormalized_arena; init_arena_defcapacity(&denormalized_arena);
    
    // NOTE: arena that stores common schemas and dependencies that might be the final Schemas to be stored in the definitive memory
    //  of the matrix.
    Arena commons_arena; init_arena_defcapacity(&commons_arena);

    // TODO(YA3-CLEAN): this shouldn't be necessary in a final version.
    // NOTE: we use commons_arena because its lifetime is the whole function, no calls to clear
    ArrayListUInt free_var_positions = create_array_list_uint_arena(matrix->free_vars.size, &commons_arena);
    free_var_positions.size = matrix->free_vars.size;
    for (unsigned i = 0; i < free_var_positions.size; ++i) {
        free_var_positions.array[i] = i;
    }
    
    // TODO(YA3-OPT-CLEAN): would be cleaner and equally efficient to avoid the passing of these Arenas ???
    Arena row_vars_arena; init_arena_defcapacity(&row_vars_arena);
    Arena schema_iterator_arena; init_arena_defcapacity(&schema_iterator_arena);

    // TODO(YA3-CLEAN): could implement a version of extend_row that doesn't need to take this Arena ???
    // TODO(OPT): could use some kind of caching strategy to avoid recalculations, but taking into account that we can have new blocks and that
    //  a block's normalized common schema might change.
    void *starting_col_indices_memory = malloc(2 * sizeof(unsigned) * matrix->free_vars.size);
    unsigned *starting_col_indices_moved_row = starting_col_indices_memory;
    unsigned *starting_col_indices_destination_block = starting_col_indices_memory + (sizeof(unsigned) * matrix->free_vars.size);

    // NOTE: is inserting the new blocks to the head of the linked list correct? YES! (Actually, it is even more efficient)
    //  The rows of those new blocks are not going to be checked. Since they come from rows that were already moved from 
    //  another block and who didn't match with any existing block, there is no need to check them once again. Furthermore, 
    //  when looking for a valid block, these new blocks are taken into account indeed, so no problem!

    // First pass: move the rows "virtually", updating the common schemas and dependencies of the rows. Have to store the pointers to the appropriate normalized set
    //  schemas of the rows that were moved (the set schemas themselves are the normalized schemas of the blocks that initially contained the moved row, and therefore,
    //  they are stored in the matrix's memory), and the rest of the rows that are in a block that has received new rows (whose normalized set schema will be the 
    //  initial normalized set schema of the block). Since blocks and rows are traversed in order, the rows in a block that receives new rows are partitioned in
    //  order too. I.e., first all the rows from block X, then all the rows from block Y, ..., then all the original rows (that were not moved to another block). 
    //  The starting_column_indexes can be calculated from those normalized set schemas. Important to NOT modify the normalized schemas so we can access the original
    //  normalized schemas in the second pass.
    
    // TODO(OPT-Cache): we could try to cache the starting_column_indexes alongside the normalized set schemas... In fact, the starting column indexes have to be
    //  calculated only with the original normalized set schemas of all the blocks in any case.

    // TODO(YA-OPT): we should avoid iterating over the same row more than once, no? Right now, we can move a row to a posterior block, and check that row
    //  unnecessarily... We can add a flag 'is_moved' (false initialized) to the struct BlockRow. Or since we add rows from the beginning, we can store the
    //  nodes of the starting block rows and do the since version of the for_each loop! --> PROBLEM: what if that is moved? NO PROBLEM, already iterating over
    //  that block. In fact, we don't need to store the first blocks starting node!

    // PLANTEAMIENTO:
    // - Cada vez que se mueva una fila a otro bloque: añadir/activar el bloque destino en el array
    // - Cuando terminamos de iterar las filas de un bloque, meter la entrada de la última fila del bloque (si queda alguno) por el tail
    // - Second Pass iterando sobre los bloques activos en el array y sabiendo que cada partición comparte el mismo normalized set schema y starting_col_indices

    Arena first_pass_arena; init_arena_defcapacity(&first_pass_arena);
    IntrusiveList partition_headers_head; init_intrusive_list(&partition_headers_head);


    Block *block;
    intrusive_list_for_each_entry(block, &matrix->head_for_blocks, matrix_pos) {
        
        // TODO(OPT): if we knew that if exists any linear block, it would be the first one, we could start directly from the second one, avoiding
        //  having an if-condition in the loop...
        // NOTE: linearity is preserved, so we know that all rows in the linear block which is the combination of two operand linear blocks
        //  will remain in the resulting linear block.
        bool is_linear = num_distinct_schema_variables(*block->schema);
        if (is_linear) {
            continue;
        }

        // NOTE: the safe version is necessary because we can move block_rows to other blocks, breaking its intrusive list's next pointer
        BlockRow *block_row, *next_buffer_ptr;
        intrusive_list_for_each_entry_safe(block_row, next_buffer_ptr, &block->head_for_rows, block_pos) {
            //// Check if row is not well placed in block
            // Calculate the row's corresponding set schema.
            int *row = block_row->row;
            SetSchema *row_normalized_set_schema = block->normalized_schema;

            // TODO(YA!): have to see if denormalized introduces schema variables starting from 1 or starting from the maximum variable in 
            //  the schema of the block. Should be not only the latter, but also the maximum number of variables in any schemas of the blocks
            //  in the matrix, to ensure the common schema operation knows that the schema variables are independent. We could do this as 
            //  a second increment_schema_variables call (or passing the initial v argument to denormalize, more efficient). In this case,
            //  I think that we won't need a decrement step because the potentially new schemas stored in the matrix will be the common
            //  results between the denormalized ones and others. NOTE that this is also the case of the schemas that resulted from the 
            //  common schema operations in the matrix_and function ==> COULDN'T THE RENAMING OF OPERAND SCHEMAS IN MATRIX_AND AFFECT TO
            //  THE RESULTING SCHEMAS IN THE MATRIX IF THEY WERE NOT DEEPCOPIED (I think they were deepcopied, but this dependency has to 
            //  be taken into account for future possible optimizations!!!).
            //  Two conclusions:
            //  - Instead of decrementing, we could have a normalize_schema_variables(set_schema, dependencies) operation that renames the 
            //  schema variables to 1..max respecting the equivalences.
            //  - Instead of modifying the actual content of Variable Schemas from operands, we could pass an extra parameter, a mapping or
            //  an unsigned that has to be summed to the schemas variables of the second set schema operands each time they are accessed, 
            //  to avoid the need of normalizing schema variables in the operands. Even then, I think you would still need to normalize
            //  the resulting common schemas. Therefore, you need either to deepcopy, as we do now, OR if we implement immutable schema
            //  optimizations, we would need to consider differing branches also the branches that are structurally equal to a branch of
            //  the second operand, but where the schema variables of that branch had been renamed.
            //  vvv
            // - We can internalize the normalization of the Schemas to the common schema operation. We will calculate the max variable in the
            //  first schema. We can have a counter of the new variables that appear in the result until each time, and a mapping between
            //  schema variables in the second schema and those new variables. That way, the new variables will be max1+count=1, max1+count=2...
            //  each of them associated with the variables in the second schema (1 to max1+count=x, 2 to max1+count=y, and so on). The mapping
            //  is necessary because we need to consider that some schema variables in the second operand might be repeated, so with each of
            //  their apparitions we need to consider the same corresponding new variable. With this helper data, we can work with schemas
            //  in a fully immutable way, no need to normalize the schema variables at the end. WRONG: we would still need to renormalize
            //  because some of the schema variables in the first operand might not appear in the result!
            //  ==> We can have a normalize operation that creates (updating the ref_counts) new branches whenever a normalization needs to
            //  happen. An optimization in this operation would be to modify the VariableSchema's value directly if its refcount is only 1,
            //  instead of cloning the whole branch. Additionally, we could modify/create new branches the least amount of times possible:
            //  first, counting the number of distinct variables in the resulting Schema, so we can respect the values of every variable schema
            //  that remains in that interval, and modifying only the rest (we would need a mapping, also for those that are not modified,
            //  from the old value to the new). NOTE: the refcount of the leaf is only 1 always, no? I wanted to say that the branch is only
            //  referenced once, not only the leaf!!!
            //      We have specified how the normalization will work. Before the normalization, in the common operation, to differ the schema
            //  variables we can take into account the maximum variable in op1, and each time we access a variable in op2, sum the maximum to it.
            //  vvv
            // Have to review how the immutability on trees works... If in our case is not so simple to reference common subbranches, deepcopying and
            //  doing a simple in-place normalization might be the best option. All intermediate nodes have to be copied if a new leaf is necessary, no?
            // ? For renormalization: interesting to have access to an iterable of tree-leaves (since we know that all schema variables are leaves!)

            clear_arena(&denormalized_arena);
            ArrayListSchema *row_set_schema = allocate(&denormalized_arena, sizeof(*row_set_schema));
            SetDependencies *row_dependencies = allocate(&denormalized_arena, sizeof(*row_dependencies));
            denormalized_set_schema(*row_normalized_set_schema, row, row_set_schema, row_dependencies, &denormalized_arena);
        
            // Compare row's schema and dependencies with the block's
            
            // TODO(YA4-Clean): the logic is repeated for the current block and the rest of the blocks, I think they can be compacted... But NO if the 
            //  optimization below is implemented! (Then the operations on the Block that contains the Row will be distinct indeed...)
            
            // TODO(YA3): before block_and-s, when calculating common schemas, we can use the strict version storing the fact that the "soft" result complies the strict conditions or not
            //  Why? Because if it doesn't comply the strict conditions, we know that we will have to move every single row from the Block to other blocks...
            //  That way, we avoid calling to common_set_schema between the schemas of the row and the schemas of the resulting fragments that do not cumply the
            //  strict conditions, we would only need to iterate through the resulting fragments whose schemas have respected the strict conditions.
            //  --> Additionally, in the case the resulting block schema cumplies the strict conditions, we don't need to calculate the whole common schema with the row schema,
            //  we only need to check the first_condition, since we know that the row_schema doesn't have dependencies with schemas with schema-variables in them...
            //  --> I wouldn't store those flags in the matrices memory, because we must ensure that all Matrices are in MNF once their calculation / parsing is finished...
            //  We will need to allocate them in some temporal arena of the matrix_and function, and pass that information here. That said, if the current Matrix format
            //  is eventually considered a temporal internal format to optimized the postprocessing step, then we could have this information stored in the Matrix's memory.
            
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

                //  Have to set the new block schema and dependencies to the common ones (watch out the normalized!)
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
                    // TODO(YA3): if we implement the optimization above where we identify the common schemas that don't fulfill the strict conditions
                    //  before calculating block_and, we could store the memory of those blocks that we know will be removed in a temporal memory buffer.
                    //  Even with that, I think we could have some wasted memory here.
                    if (block->c == 0) {
                        remove_block_from_matrix(matrix, block);
                    }

                    destination_block->schema = set_schema_deepcopy(common_set_schema, &commons_arena);
                    destination_block->dependencies = set_dependencies_deepcopy(common_dependencies, &commons_arena);

                    PartitionListHeader *new_extended_block_s_header = find_header_that_contains_block(&partition_headers_head, destination_block);
                    if (new_extended_block_s_header == NULL) {
                        new_extended_block_s_header = allocate(&first_pass_arena, sizeof(*new_extended_block_s_header));
                        new_extended_block_s_header->block_ptr = destination_block;
                        intrusive_list_add(&partition_headers_head, &new_extended_block_s_header->header_pos);
                        init_intrusive_list(&new_extended_block_s_header->partition_head);

                        PartitionListNode *node = allocate(&first_pass_arena, sizeof(*node));
                        node->final_row_of_partition = block_row;
                        node->original_block = block;
                        intrusive_list_add(&new_extended_block_s_header->partition_head, &node->partition_pos);

                    } else {
                        
                        PartitionListNode *last_added = intrusive_list_first_entry(&new_extended_block_s_header->partition_head, PartitionListNode, partition_pos);
                        bool block_s_partition_already_included = last_added->original_block == block;
                        if (!block_s_partition_already_included) {
                            PartitionListNode *node = allocate(&first_pass_arena, sizeof(*node));
                            node->final_row_of_partition = block_row;
                            node->original_block = block;
                            intrusive_list_add(&new_extended_block_s_header->partition_head, &node->partition_pos);
                        }
                        
                    }


                } else {
                    // Else, if only one row in the current block, change the block's schemas (NOTE: the normalized one is the same!)
                    //  If more than one remaining, create a new block with a single row.

                    if (block->r == 1) {
                        block->schema = set_schema_deepcopy(row_set_schema, &commons_arena);
                        block->dependencies = set_dependencies_deepcopy(row_dependencies, &commons_arena);
                    } else {
                        assert(block->r > 1);

                        Block *new_block = allocate(&matrix->blocks_arena, sizeof(*new_block));
                        new_block->schema = set_schema_deepcopy(row_set_schema, &commons_arena);
                        new_block->dependencies = set_dependencies_deepcopy(row_dependencies, &commons_arena);
                        new_block->normalized_schema = block->normalized_schema;
                        
                        new_block->r = 1;
                        new_block->c = set_schema_size(new_block->normalized_schema);
                        
                        move_row_to_block(new_block, block_row);
                        block->r--;
                        
                        add_block_to_matrix(matrix, new_block);
                    }
                }
            }
        }
    }


    // TODO(YA): finish the second and third passes, and review the TODOs of the postprocess function!!!
    // Second pass: extend the rows with respect to the final common schemas of each block, and store the final common schemas in the permanent memory of the Matrix.
    PartitionListHeader *header;
    intrusive_list_for_each_entry(header, &partition_headers_head, header_pos) {
        
        Block *block_to_be_extended = header->block_ptr;

        // TODO(YA3-OPT): would be interesting to unify the interfaces of both linear and non-linear version to take the pointer to the function
        //  in a single if condition here.
        bool is_linear = num_distinct_schema_variables(*block_to_be_extended->schema);

        // TODO(OPT): a pity to store this temporarily here, and have to be recalculated in the third pass...
        clear_arena(&denormalized_arena);
        SetSchema new_common_normalized = {
            .list = normalized_set_schema(*block_to_be_extended->schema, *block_to_be_extended->dependencies, &denormalized_arena)
        };
        // NOTE: it is interesting to store the new number of columns here, so we can check if we need to recalculate some normalized schemas in the third pass
        block_to_be_extended->c = set_schema_size(&new_common_normalized);

        BlockRow *block_row_to_be_extended = intrusive_list_first_entry(&block_to_be_extended->head_for_rows, BlockRow, block_pos);

        PartitionListNode *node;
        intrusive_list_for_each_entry(node, &header->partition_head, partition_pos) {
            SetSchema *original_normalized_schema_of_partition = node->original_block->normalized_schema;

            BlockRow *inclusive_end = node->final_row_of_partition;
            BlockRow *exclusive_end = intrusive_list_next_entry(inclusive_end, block_pos);
            
            if (set_schema_size(original_normalized_schema_of_partition) == block_to_be_extended->c) {
                // No need to extend the rows in this partition --> Skip the partition
                block_row_to_be_extended = exclusive_end;
            } else {
                // TODO(OPT): right now, we recalculate each time. Wouldn't be interesting to do some kind of precalculation ???
                // Have to extend the rows in this partition
                starting_column_indexes(original_normalized_schema_of_partition->list, starting_col_indices_moved_row);
                
                intrusive_list_for_each_entry_since_until(
                    block_row_to_be_extended, &block_to_be_extended->head_for_rows, block_pos, &exclusive_end->block_pos) 
                {
                    int *extended_row = allocate(&matrix->blocks_arena, sizeof(*extended_row) * block_to_be_extended->c);
                    // TODO(OPT): I let this dispatching as local as possible because I would like to make compatible the interfaces and store the proper function
                    //  in a variable.
                    if (is_linear) {
                        extend_row_lineal(
                            *original_normalized_schema_of_partition, free_var_positions, 
                            new_common_normalized,
                            starting_col_indices_destination_block, block_row_to_be_extended->row,
                            extended_row,
                            &schema_iterator_arena);
                    } else {
                        extend_row(
                            *original_normalized_schema_of_partition, free_var_positions, 
                            new_common_normalized,
                            starting_col_indices_destination_block, block_row_to_be_extended->row,
                            extended_row,
                            &row_vars_arena,
                            &schema_iterator_arena);
                    }
                    block_row_to_be_extended->row = extended_row;
                }
            }
        }
        
        // Check the rest of the rows, i.e., any row that has remained in its initial block
        SetSchema *original_normalized_schema_of_partition = block_to_be_extended->normalized_schema;
        if (set_schema_size(original_normalized_schema_of_partition) != block_to_be_extended->c) {
            assert(set_schema_size(original_normalized_schema_of_partition) < block_to_be_extended->c);

            starting_column_indexes(original_normalized_schema_of_partition->list, starting_col_indices_moved_row);
            
            intrusive_list_for_each_entry_since(block_row_to_be_extended, &block_to_be_extended->head_for_rows, block_pos) {
                int *extended_row = allocate(&matrix->blocks_arena, sizeof(*extended_row) * block_to_be_extended->c);
                // TODO(OPT): I let this dispatching as local as possible because I would like to make compatible the interfaces and store the proper function
                //  in a variable.
                if (is_linear) {
                    extend_row_lineal(
                        *original_normalized_schema_of_partition, free_var_positions, 
                        new_common_normalized,
                        starting_col_indices_destination_block, block_row_to_be_extended->row,
                        extended_row,
                        &schema_iterator_arena);
                } else {
                    extend_row(
                        *original_normalized_schema_of_partition, free_var_positions, 
                        new_common_normalized,
                        starting_col_indices_destination_block, block_row_to_be_extended->row,
                        extended_row,
                        &row_vars_arena,
                        &schema_iterator_arena);
                }
                block_row_to_be_extended->row = extended_row;
            }
        }
    }

    // TODO(OPT): we know which are the destination blocks based on the partition headers. We could do a similar list storing the pointers to the new blocks,
    //  but since this two set of blocks are not exclusive, we should subtract from the latter the blocks in the first. Notice that storing the initial first
    //  block is not enough to identify all new blocks, because in some cases we reuse the memory of the old block when only one block row to be moved remains!
    //  Right now, we simply traverse all blocks, make a new copy of all the common schemas (even if they were not changed) and the normalized ones if they were
    //  changed... ==> WASTE OF MEMORY!
    //      - Conclusion: change the memory managment of Schema to use RefCounted GC.
    //      - Remove even the ArrayListSchema use, just a Schema, no?
    //      - Isn't more convenient a simpler use of malloc for our Postprocessable Matrices that can suffer so many changes to their Rows?
    // Third pass: update the normalized common schemas to the last ones, plus the possible schemas that could be stored in the temporal commons_arena too!
    //  I.e., we need to store the schemas, dependencies and the normalized schemas (when it has increased) of all destination blocks and new blocks
    //  (they are not exclusive).
    intrusive_list_for_each_entry(block, &matrix->head_for_blocks, matrix_pos) {
        block->schema = set_schema_deepcopy(block->schema, &matrix->schemas_arena);
        block->dependencies = set_dependencies_deepcopy(block->dependencies, &matrix->schemas_arena);
        if (block->c != set_schema_size(block->normalized_schema)){
            assert(block->c > set_schema_size(block->normalized_schema));
            SetSchema *normalized = callocate(&matrix->schemas_arena, sizeof(*normalized));
            normalized->list = normalized_set_schema(*block->schema, *block->dependencies, &matrix->schemas_arena);
            block->normalized_schema = normalized;
        }
    }

    free_arena(denormalized_arena);
    free_arena(commons_arena);
    free_arena(row_vars_arena);
    free_arena(schema_iterator_arena);
    free(starting_col_indices_memory);
}

// TODO(FUT): once the matrix is postprocessed to MNF and we know the final result, we could pass the entire matrix to a more coalescing
//  memory access friendly, compact, GPU bandwith friendly format where all data is as compact in memory as possible. That is because the
//  formulas are what they are (i.e., immutable). If that is the idea, the intermediate memory management/layout of the Matrix format loses
//  importance. We can change to a traditional multiple malloc strategy where we ensure we free the memory that we won't be using furthermore
//  to store in the final result of the intermediate format, or if the wasted memory of old rows is not that big, we could simply allocate
//  double the memory (a little more, because rows are extended) because in the worst case each row is extended in the postprocessing step.
//  Then, we would use the actual result to generate the coalesced format and free all the memory of the intermediate format, including the
//  old rows not relevant for the result, and the resulting rows.
// ==> Have to think how many potential versions of each data we can have in the current intermediate format. Each resulting row has only
//  two potential values? One that comes from matrix_and, and the other the single (I HOPE) re-extension in this postprocess_to_mnf ???
// ==> In the case we finally design the final compact format, the parsing and writing functions should only use those formats, not the 
//  intermediate one. The latter should only be an internal format to be able to perform postprocess in a more efficient way.


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
