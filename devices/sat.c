/** @file sat.c
 *
 *  @author Slinga
 *  @brief Saturn Allocation Table (SAT) parsing. Shared by internal, cartridge, and Action Replay
 *  @bug No known bugs.
 */
#include "sat.h"



/** @brief Bitmap representing blocks in a partition. Each bit represents one block */
unsigned char g_SAT_bitmap[SAT_MAX_BITMAP] = {0};

// block helper functions
static SLINGA_ERROR calc_num_blocks(unsigned int save_size, unsigned int block_size, unsigned int* num_save_blocks);
static SLINGA_ERROR convert_address_to_block_index(const unsigned char* address, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned int skip_bytes, unsigned int* block_index);

// parsing saves
static SLINGA_ERROR copy_metadata(PSAVE_METADATA metadata, const PSAT_START_BLOCK_HEADER save, unsigned int skip_bytes);
static SLINGA_ERROR find_save(const char* filename, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, PSAT_START_BLOCK_HEADER* save_start);
static SLINGA_ERROR read_save_and_metadata(const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, PSAVE_METADATA metadata, const char* filename, unsigned char* buffer, unsigned int size, unsigned int* bytes_read);
static SLINGA_ERROR walk_partition(const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found, unsigned int* used_blocks);

// parsing the SAT table
static SLINGA_ERROR read_sat_table(const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, const PSAT_START_BLOCK_HEADER save_start, unsigned char* bitmap, unsigned int bitmap_size, unsigned int* start_block, unsigned int* start_data_block);
static SLINGA_ERROR read_save_from_sat_table(unsigned char* buffer, unsigned int size, unsigned int* bytes_read, unsigned int start_block, unsigned int start_data_block, const unsigned char* bitmap, unsigned int bitmap_size, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned int skip_bytes);
static SLINGA_ERROR read_sat_table_from_block(unsigned int block_index, unsigned char* bitmap, unsigned int bitmap_size, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned int skip_bytes, unsigned int start_block, unsigned int* start_data_block, unsigned int* written_sat_entries);

// SAT bitmap helpers
static SLINGA_ERROR set_bitmap(unsigned int block_index, unsigned char* bitmap, unsigned int bitmap_size);
static SLINGA_ERROR get_next_block_bitmap(unsigned int block_index, const unsigned char* bitmap, unsigned int bitmap_size, unsigned int* next_block_index);

//
// Functions exposed to Internal, Cartridge, and Action Replay
//

/**
 * @brief Calculate the number of used blocks on the SAT partition
 *
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] used_blocks Number of used blocks on the partition on success
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR sat_get_used_blocks(const unsigned char* partition_buf,
                                 unsigned int partition_size,
                                 unsigned int block_size,
                                 unsigned char skip_bytes,
                                 unsigned int* used_blocks)
{
    // this is just a wrapper for walk_partition()
    return  walk_partition(partition_buf,
                           partition_size,
                           block_size,
                           skip_bytes,
                           NULL,
                           0,
                           NULL,
                           used_blocks);
}

/**
 * @brief List all saves on the SAT partition
 *
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] saves Filled out SAVE_METADATA array on success
 * @param[in] num_sizes size in elements of saves array
 * @param[out] saves_available Number of saves found on device on success
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR sat_list_saves(const unsigned char* partition_buf,
                            unsigned int partition_size,
                            unsigned int block_size,
                            unsigned char skip_bytes,
                            PSAVE_METADATA saves,
                            unsigned int num_saves,
                            unsigned int* saves_available)
{
    // this is just a wrapper for walk_partition()
    return  walk_partition(partition_buf,
                           partition_size,
                           block_size,
                           skip_bytes,
                           saves,
                           num_saves,
                           saves_available,
                           NULL);
}

/**
 * @brief Query metadata for a save on the SAT partition
 *
 * @param[in] filename Save to query
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] metadata Metadata for filename on success
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR sat_query_file(const char* filename,
                            const unsigned char* partition_buf,
                            unsigned int partition_size,
                            unsigned int block_size,
                            unsigned char skip_bytes,
                            PSAVE_METADATA metadata)
{
    // wrapper for sat_read_save
    return read_save_and_metadata(partition_buf,
                         partition_size,
                         block_size,
                         skip_bytes,
                         metadata,
                         filename,
                         NULL,
                         0,
                         NULL);
}

/**
 * @brief Read save data for a save on the SAT partition
 *
 * @param[in] filename Save to query
 * @param[out] buffer data of filename on success
 * @param[in] size size in bytes of the buffer array
 * @param[out] bytes_read number of bytes read on success
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR sat_read_save(const char* filename,
                           unsigned char* buffer,
                           unsigned int size,
                           unsigned int* bytes_read,
                           const unsigned char* partition_buf,
                           unsigned int partition_size,
                           unsigned int block_size,
                           unsigned char skip_bytes)
{
    // wrapper for sat_read_save
    return read_save_and_metadata(partition_buf,
                         partition_size,
                         block_size,
                         skip_bytes,
                         NULL,
                         filename,
                         buffer,
                         size,
                         bytes_read);
}

//
// Block helper functions
//

/**
 * @brief Given a save size, calculate how many blocks it needs.
 *
 * @param[in] save_size How big the save is in bytes
 * @param[in] block_size How big the blocks are on the partition
 * @param[out] num_save_blocks Number of blocks needed to record save_size bytes on success
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR calc_num_blocks(unsigned int save_size, unsigned int block_size, unsigned int* num_save_blocks)
{
    unsigned int total_bytes = 0;
    unsigned int fixed_bytes = 0;
    unsigned int num_blocks = 0;
    unsigned int num_blocks2 = 0;

    if(!save_size || !num_save_blocks || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // block size must be 64-byte aligned
    if((block_size % MIN_BLOCK_SIZE) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    //
    // The stored save consists of:
    // - the save metadata header
    // - the variable length SAT table
    // - the save data itself
    //
    // What makes this tricky to compute is that the SAT table itself is stored
    // on the blocks
    //

    fixed_bytes = sizeof(SAT_START_BLOCK_HEADER) - SAT_TAG_SIZE + save_size;

    // calculate the number of SAT table entries required
    num_blocks = fixed_bytes / (block_size - SAT_TAG_SIZE);

    // +1 for 0x0000 SAT table terminator
    total_bytes = fixed_bytes + ((num_blocks + 1) * sizeof(unsigned short));

    // we need to do this in a loop because it's possible that by adding a SAT
    // table entry we increased the number of bytes we need such that we need
    // yet another SAT table entry.
    do
    {
        num_blocks = num_blocks2;
        total_bytes = fixed_bytes + ((num_blocks + 1) * sizeof(unsigned short));
        num_blocks2 = total_bytes / (block_size - SAT_TAG_SIZE);

        if(total_bytes % (block_size - SAT_TAG_SIZE))
        {
            num_blocks2++;
        }

    }while(num_blocks != num_blocks2);

    *num_save_blocks = num_blocks;

    return SLINGA_SUCCESS;
}

/**
 * @brief Converts save address to block index
 *
 * @param[in] address Block index
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] block_index Address represented as a block index
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR convert_address_to_block_index(const unsigned char* address,
                                                   const unsigned char* partition_buf,
                                                   unsigned int partition_size,
                                                   unsigned int block_size,
                                                   unsigned int skip_bytes,
                                                   unsigned int* block_index)
{
    unsigned int index = 0;

    if(!address || !partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(address < partition_buf)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(address >= partition_buf + partition_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // index is just the number of blocks from the start of the partition
    index = (address - partition_buf)/block_size;

    *block_index = index;
    return SLINGA_SUCCESS;
}

//
// Parsing Save and Metadata
//

/**
 * @brief Reads metadata from a save start block
 *
 * @param[out] address Read metadata on success
 * @param[in] save Pointer to the start block
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR copy_metadata(PSAVE_METADATA metadata, const PSAT_START_BLOCK_HEADER save, unsigned int skip_bytes)
{
    if(!metadata || !save)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    //
    // save name, filename, and comment
    //
    memcpy(metadata->savename, save->savename, MAX_SAVENAME);
    metadata->savename[MAX_SAVENAME] = '\0'; //savename is MAX_SAVENAME + 1 bytes long

    snprintf(metadata->filename, MAX_FILENAME, "%s.BUP", metadata->savename); // .BUP extension will always fit
    metadata->filename[MAX_FILENAME] = '\0'; //filename is MAX_FILENAME + 1 bytes long

    memcpy(metadata->comment, save->comment, MAX_COMMENT);
    metadata->comment[MAX_COMMENT] = '\0'; //comment is MAX_COMMMENT + 2 bytes long

    //
    // language, timestamp, data size, and block size
    //
    metadata->language = save->language;
    metadata->timestamp = save->timestamp;
    metadata->data_size = save->data_size;
    metadata->block_size = 0; // block size isn't needed (and isn't stored in the metadata)

    return SLINGA_SUCCESS;
}

/**
 * @brief Finds save on the SAT partition

 * @param[in] filename Save to query
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] save_start Pointer to start of save on success
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR find_save(const char* filename,
                              const unsigned char* partition_buf,
                              unsigned int partition_size,
                              unsigned int block_size,
                              unsigned char skip_bytes,
                              PSAT_START_BLOCK_HEADER* save_start)
{
    PSAT_START_BLOCK_HEADER metadata = NULL;
    SLINGA_ERROR result = 0;

    if(!filename || !partition_buf || !partition_size || !block_size || !save_start)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // loop through all blocks on the partition
    // the first two blocks are not used for saves
    for(unsigned int i = (2 * block_size); i < partition_size; i += block_size)
    {
        metadata = (PSAT_START_BLOCK_HEADER)(partition_buf + i);

        // validate range
        if((unsigned char*)metadata < partition_buf || (unsigned char*)metadata >= partition_buf + partition_size)
        {
            return SLINGA_INVALID_PARAMETER;
        }

        // every save starts with a tag
        if(metadata->tag == SAT_START_BLOCK_TAG)
        {
            // check if this is our save
            result = strncmp(filename, metadata->savename, MAX_SAVENAME);
            if(result != 0)
            {
                // not our save
                continue;
            }

            *save_start = metadata;
            return SLINGA_SUCCESS;
        }
    }

    return SLINGA_NOT_FOUND;
}

/**
 * @brief Read save and metadata on the SAT parittion

 * @param[in] filename Save to query
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] save_start Pointer to start of save on success
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR read_save_and_metadata(const unsigned char* partition_buf,
                                           unsigned int partition_size,
                                           unsigned int block_size,
                                           unsigned char skip_bytes,
                                           PSAVE_METADATA metadata,
                                           const char* filename,
                                           unsigned char* buffer,
                                           unsigned int size,
                                           unsigned int* bytes_read)
{
    PSAT_START_BLOCK_HEADER save_start = NULL;
    SLINGA_ERROR result = 0;

    result = find_save(filename,
                       partition_buf,
                       partition_size,
                       block_size,
                       skip_bytes,
                       &save_start);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }


    if(buffer)
    {
        unsigned int start_block = 0;
        unsigned int start_data_block = 0;
        unsigned int save_size = 0;

        // read the save size
        save_size = save_start->data_size;
        if(save_size < size)
        {
            // buffer isn't big enough to hold the save
            return SLINGA_BUFFER_TOO_SMALL;
        }

        result = read_sat_table(partition_buf,
                                partition_size,
                                block_size,
                                skip_bytes,
                                save_start,
                                g_SAT_bitmap,
                                sizeof(g_SAT_bitmap),
                                &start_block,
                                &start_data_block);
        if(result != 0)
        {
            return result;
        }

        result = read_save_from_sat_table(buffer,
                                              size,
                                              bytes_read,
                                              start_block,
                                              start_data_block,
                                              g_SAT_bitmap,
                                              sizeof(g_SAT_bitmap),
                                              partition_buf,
                                              partition_size,
                                              block_size,
                                              skip_bytes);
        if(result != 0)
        {
            return result;
        }
    }

    if(metadata)
    {
        result = copy_metadata(metadata, save_start, skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }
    }

    return SLINGA_SUCCESS;
}

/**
 * @brief Walk the partition, finding all saves and metadata
 *
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] saves Filled out SAVE_METADATA array on success
 * @param[in] num_sizes size in elements of saves array
 * @param[out] saves_available Number of saves found on device on success
 * @param[out] used_blocks Number of used blocks on the partition on success
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR walk_partition(const unsigned char* partition_buf,
                                   unsigned int partition_size,
                                   unsigned int block_size,
                                   unsigned char skip_bytes,
                                   PSAVE_METADATA saves,
                                   unsigned int num_saves,
                                   unsigned int* saves_available,
                                   unsigned int* used_blocks)
{
    PSAT_START_BLOCK_HEADER metadata = NULL;
    unsigned int saves_found = 0;
    unsigned int blocks_found = 0;
    unsigned int save_blocks = 0;
    SLINGA_ERROR result = 0;

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(block_size > partition_size || (partition_size % block_size) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // loop through all blocks onthe parititon
    // the first two blocks are not used for saves
    for(unsigned int i = (2 * block_size); i < partition_size; i += block_size)
    {
        metadata = (PSAT_START_BLOCK_HEADER)(partition_buf + i);

        // validate range
        if((unsigned char*)metadata < partition_buf || (unsigned char*)metadata >= partition_buf + partition_size)
        {
            return SLINGA_SAT_INVALID_PARTITION;
        }

        // every save starts with a tag
        if(metadata->tag == SAT_START_BLOCK_TAG)
        {
            result = calc_num_blocks(metadata->data_size, block_size, &save_blocks);
            if(result != SLINGA_SUCCESS)
            {
                // TODO: handle this error
                continue;
            }

            blocks_found += save_blocks;

            //jo_core_error("%d %d %d", saves_found, metadata->saveSize, save_blocks);

            if(saves)
            {
                // check if we are finished looking for saves
                if(saves_found >= num_saves)
                {
                    // no more room in our saves array
                    return SLINGA_BUFFER_TOO_SMALL;
                    break;
                }

                // copy off the metadata
                result = copy_metadata(&saves[saves_found], metadata, skip_bytes);
                if(result)
                {
                    return result;
                }

                //jo_core_error("%s %d %d\n", saves[saves_found].savename, saves[saves_found].data_size, saves[saves_found].block_size);
            }

            saves_found++;
        }
    }

    if(saves_available)
    {
        *saves_available = saves_found;
    }

    if(used_blocks)
    {
        *used_blocks = blocks_found;
    }

    return SLINGA_SUCCESS;
}

//
// SAT table
//

/**
 * @brief Read the SAT table from the specified save
 *
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[in] save_start Pointer to the start of a save on the parit
 * @param[out] bitmap On success the bitmap bits will be set to 1 for each block that is part of the save
 * @param[in] bitmap_size Size of bitmap in bytes *
 * @param[out] start_block First block of the save on success
 * @param[out] start_data_block First block containing save data (as opposed to just metadata). Can be same or different than start_block
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR read_sat_table(const unsigned char* partition_buf,
                                   unsigned int partition_size,
                                   unsigned int block_size,
                                   unsigned char skip_bytes,
                                   const PSAT_START_BLOCK_HEADER save_start,
                                   unsigned char* bitmap,
                                   unsigned int bitmap_size,
                                   unsigned int* start_block,
                                   unsigned int* start_data_block)
{
    unsigned int num_sat_blocks = 0;
    unsigned int written_sat_entries = 0;
    unsigned int cur_sat_block = 0;
    int result = 0;

    if(!partition_buf || !partition_size || !block_size || !save_start || !bitmap || !bitmap_size || !start_block || !start_data_block)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // block size must be 64-byte aligned
    if((block_size % MIN_BLOCK_SIZE) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if((partition_size % block_size) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    result = calc_num_blocks(save_start->data_size, block_size, &num_sat_blocks);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // +1 for the first SAT entry which isn't included
    //num_sat_blocks++;

    // sanity check that the
    //jo_core_error("%d %d\n", num_sat_blocks, bitmap_size);
    if(num_sat_blocks > bitmap_size * 8)
    {
        return SLINGA_SAT_SAVE_OUT_OF_RANGE;
    }

    // zero out the bitmap to begin
    memset(bitmap, 0, bitmap_size);

    result = convert_address_to_block_index((unsigned char*)save_start, partition_buf, partition_size, block_size, skip_bytes, start_block);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // add the start block to our bitmap
    result = set_bitmap(*start_block, bitmap, bitmap_size);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    //
    // set bits in the bitmap
    // read the next bit in the bitmap
    // if it's 1
    // - check if it has sat entries
    // - if it does, add them the bitmap
    // - if it doesn't, we are done
    //

    //
    // special cases
    //
    //- 1st entry start_block
    // - skip past metadata
    // first data entry - first_data
    // - skip past block entries and 0x00 00 terminator then start reading data
    // last block
    // - not necessarily full
    written_sat_entries = 1;
    cur_sat_block = *start_block;

    // loop through the blocks while we read the SAT table
    // this is tricky because we are writing to the satBlocks array while parsing it
    do
    {
        // we calculated we have num_sat_blocks
        // sanity check we aren't beyound that
        if(written_sat_entries > num_sat_blocks)
        {
            return SLINGA_SAT_TOO_MANY_BLOCKS;
        }

        // read SAT table entries from this block
        result = read_sat_table_from_block(cur_sat_block, bitmap, bitmap_size, partition_buf, partition_size, block_size, 0, *start_block, start_data_block, &written_sat_entries);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        result = get_next_block_bitmap(cur_sat_block, bitmap, bitmap_size, &cur_sat_block);
        if(result == SLINGA_NOT_FOUND)
        {
            // no more bits set
            break;
        }
        else if(result != SLINGA_SUCCESS)
        {
            // error
            return result;
        }

    }while(result != 0);

    // success
    return SLINGA_SUCCESS;
}

/**
 * @brief Read the save from the specified sat_table
 *
 * @param[out] buffer Save data read into buffer on success
 * @param[in] size Size of buffer in bytes
 * @param[out] bytes_read Number of bytes read on success
 * @param[in] start_block First block of the save on success
 * @param[in] start_data_block First block containing save data (as opposed to just metadata). Can be same or different than start_block
 * @param[in] bitmap On success the bitmap bits will be set to 1 for each block that is part of the save
 * @param[in] bitmap_size Size of bitmap in bytes *
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR read_save_from_sat_table(unsigned char* buffer,
                                             unsigned int size,
                                             unsigned int* bytes_read,
                                             unsigned int start_block,
                                             unsigned int start_data_block,
                                             const unsigned char* bitmap,
                                             unsigned int bitmap_size,
                                             const unsigned char* partition_buf,
                                             unsigned int partition_size,
                                             unsigned int block_size,
                                             unsigned int skip_bytes)
{

    unsigned int cur_sat_block = 0;
    unsigned char* block = NULL;
    unsigned int bytes_written = 0;
    unsigned int block_data_size = block_size - SAT_TAG_SIZE;
    unsigned int bytes_to_copy = 0;
    SLINGA_ERROR result = 0;

    if(!buffer || !size || !start_block || !start_data_block || !bitmap || !bitmap_size || !partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // we want to start reading at the first block that has save data
    cur_sat_block = start_data_block;

    // loop throught the bitmap until we are out of blocks
    while(1)
    {
        block = (unsigned char*)(partition_buf + (cur_sat_block * block_size));

        // validate we are still in range
        if(block < partition_buf || block >= partition_buf + partition_size)
        {
            return SLINGA_SAT_INVALID_PARTITION;
        }

        // no flags means we are just a data block (no metadata, no SAT entries)
        if(cur_sat_block != start_block && cur_sat_block != start_data_block)
        {
            // check if we are the very last block and we aren't full
            if(size - bytes_written < block_data_size)
            {
                // last block isn't full, copy less data
                bytes_to_copy = size - bytes_written;
            }
            else
            {
                bytes_to_copy = block_data_size;
            }

            // copy the save bytes
            memcpy(buffer + bytes_written, block + SAT_TAG_SIZE, bytes_to_copy);
            bytes_written += bytes_to_copy;
        }

        // SAT table end means this block contains the last of the SAT blocks. It is possible there is save data here
        else if(cur_sat_block == start_data_block)
        {
            unsigned int offset = 0;

            // end of the SAT array, data can follow
            bytes_to_copy = block_data_size;

            // is this the start block?
            if(cur_sat_block == start_block)
            {
                // skip over the header data
                offset += sizeof(SAT_START_BLOCK_HEADER) - SAT_TAG_SIZE;
            }

            // This is a SAT table block, parse until the 0x0000 and then start reading save bytes
            // skip over all SAT entries including the terminating 0x0000
            for(unsigned int i = offset; i < block_data_size; i += sizeof(unsigned short))
            {
                unsigned short index = *(unsigned short*)(block + i + SAT_TAG_SIZE);

                if(index == 0)
                {
                    // found the 0s
                    offset = i + sizeof(unsigned short);
                    break;
                }
            }

            bytes_to_copy -= offset;

            // check if we are the last very last block and we aren't full
            if(size - bytes_written < block_data_size)
            {
                // last block isn't full, copy less data
                bytes_to_copy = size - bytes_written;
            }

            // at maximum we can only copy blockDataSize at once
            if(bytes_to_copy > block_data_size)
            {
                return SLINGA_SAT_INVALID_SIZE;
            }

            // another sanity check
            if(bytes_written + bytes_to_copy > size)
            {
                return SLINGA_SAT_INVALID_SIZE;
            }

            memcpy(buffer + bytes_written, block + offset + SAT_TAG_SIZE, bytes_to_copy);
            bytes_written += bytes_to_copy;
        }

        // TODO: add additional safety checks
        result = get_next_block_bitmap(cur_sat_block, bitmap, bitmap_size, &cur_sat_block);
        if(result == SLINGA_NOT_FOUND)
        {
            // there are no more bits in the bitmap to check
            break;
        }
        else if(result != SLINGA_SUCCESS)
        {
            // error
            return result;
        }
    }

    if(bytes_read)
    {
        *bytes_read = bytes_written;
    }

    if(bytes_written == size)
    {
        return SLINGA_SUCCESS;
    }

    return SLINGA_SAT_INVALID_READ_SIZE;
}

/**
 * @brief Read the SAT table from specified block
 *
 * @param[in] block_index Specified block to read the SAT table from
 * @param[out] bitmap On success the bitmap bits will be set to 1 for each block referenced in this block
 * @param[in] bitmap_size Size of bitmap in bytes
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[in] start_block First block of the save
 * @param[out] start_data_block First block containing save data (as opposed to just metadata). Can be same or different than start_block. Set if found
 * @param[out] written_sat_entries Count of entries written into bitmap. Used to sanity check.
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR read_sat_table_from_block(unsigned int block_index,
                                            unsigned char* bitmap,
                                            unsigned int bitmap_size,
                                            const unsigned char* partition_buf,
                                            unsigned int partition_size,
                                            unsigned int block_size,
                                            unsigned int skip_bytes,
                                            unsigned int start_block,
                                            unsigned int* start_data_block,
                                            unsigned int* written_sat_entries)
{
    unsigned int start_byte = 0;
    PSAT_START_BLOCK_HEADER save_start = NULL;

    if(!block_index || !bitmap || !bitmap_size || !partition_buf || !partition_size || !block_size || !start_block || !start_data_block || !written_sat_entries)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // block size must be 64-byte aligned
    if((block_size % MIN_BLOCK_SIZE) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if((partition_size % block_size) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    save_start = (PSAT_START_BLOCK_HEADER)(partition_buf + (block_index * block_size));

    // validate we are still in range
    if((unsigned char*)save_start < partition_buf || (unsigned char*)save_start >= partition_buf + partition_size)
    {
        return SLINGA_SAT_INVALID_PARTITION;
    }

    // first entry
    if(block_index == start_block)
    {
        // first block must have the start tag
        if(save_start->tag != SAT_START_BLOCK_TAG)
        {
            return SLINGA_SAT_INVALID_TAG;
        }

        // where the block's data starts
        // first block has the PSAT_START_BLOCK_HEADER
        start_byte = sizeof(SAT_START_BLOCK_HEADER);
    }
    else
    {

        // other blocks must not have the continuation tag
        if(save_start->tag != SAT_CONTINUE_BLOCK_TAG)
        {
            return SLINGA_SAT_INVALID_TAG;
        }

        // where the block's data starts
        // continuation block's only have a 4-byte tag field
        start_byte = SAT_TAG_SIZE;
    }

    // loop through the block, recording SAT table entries until you find the
    // 0x0000 terminator or reach the end of the block
    for(; start_byte < block_size; start_byte += sizeof(unsigned short))
    {
        unsigned short index = *(unsigned short*)((unsigned char*)save_start + start_byte);
        SLINGA_ERROR result = 0;

        if(index == 0)
        {
            // we are at the end index
            *start_data_block = block_index;
            return SLINGA_SUCCESS;
        }


        if(index <= block_index)
        {
            // I'm relying on the fact that these saves block are in order!
            // otherwise the bitmap technique will not work
            return SLINGA_SAT_BLOCKS_OUT_OF_ORDER;
        }

        // found a table entry, record it
        result = set_bitmap(index, bitmap, bitmap_size);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        *written_sat_entries += 1;
    }

    // didn't find end val, continue checking
    return SLINGA_MORE_DATA_AVAILABLE;
}

//
// SAT Bitmap
//
/**
 * @brief Sets the bit corresponding to block_index in the bitmap
 *
 * @param[in] block_index Block index
 * @param[in] bitmap Bitmap representing SAT blocks
 * @param[in] bitmap_size Size in bytes of the bitmap
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR set_bitmap(unsigned int block_index, unsigned char* bitmap, unsigned int bitmap_size)
{
    int byte_index = 0;
    int bit_index = 0;

    if(!block_index || !bitmap || !bitmap_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(block_index/8 >= bitmap_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    byte_index = block_index / 8;
    bit_index = block_index % 8;

    // set a single bit in the bitmap
    bitmap[byte_index] = bitmap[byte_index] | 1 << bit_index;

    return SLINGA_SUCCESS;
}

/**
 * @brief Get's the next block in the SAT bitmap
 *
 * @param[in] block_index Block index
 * @param[in] bitmap Bitmap representing SAT blocks
 * @param[in] bitmap_size Size in bytes of the bitmap
 * @param[out] next_block_index Next block index of a block on success.
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR get_next_block_bitmap(unsigned int block_index, const unsigned char* bitmap, unsigned int bitmap_size, unsigned int* next_block_index)
{
    int byte_index = 0;
    int bit_index = 0;

    if(!block_index || !bitmap || !bitmap_size || !next_block_index)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    for(unsigned int i = block_index + 1; i < bitmap_size; i++)
    {
        byte_index = i / 8;
        bit_index = i % 8;

        //printf("\tbi %d byi %d bii %d %d\n", i, byte_index, bit_index, bitmap[byte_index]);

        if((bitmap[byte_index] & (1 << bit_index)) != 0)
        {
            // found the next set bit
            *next_block_index = i;
            return SLINGA_SUCCESS;
        }
    }

    // we didn't find another block!
    return SLINGA_NOT_FOUND;
}
