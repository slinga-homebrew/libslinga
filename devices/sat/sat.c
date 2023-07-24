/** @file sat.c
 *
 *  @author Slinga
 *  @brief Saturn Allocation Table (SAT) parsing. Shared by internal, cartridge, and Action Replay
 *  @bug No known bugs.
 */
#include "sat.h"

#include <stdio.h>

//
// Saturn saves are stored in 128-byte blocks (every other byte is valid, see skip_bytes)
// - the first 4 bytes of each block is a tag
// -- 0x80000000 = start of new save
// -- 0x00000000 = continuation block?
// - the next 30 bytes are metadata (SAT_START_BLOCK_HEADER) for the save (save name, language, comment, date, and size)
// -- the size is the size of the save data not counting the metadata
// - next is a variable array of 2-byte block ids. This array ends when 00 00 is encountered
// -- the block id for the 1st block (the one containing the metadata) is not present. In this case only 00 00 will be present
// -- the variable length array can be 0-512 elements (assuming max save is on the order of ~32k)
// -- to complicate matters, the block ids themselves can extend into multiple blocks. This makes computing the block count tricky
// -- we basically have to parse the save while we are also simultaneously figuring how many blocks we need to parse
// - following the block ids is the save data itself
// - this format is basically the same for cartridges (different sizes, addresses, block sizes, etc) and Action Replay (saves are compressed with RLE)
//

//
// skip_bytes - I use this to handle the fact that for internal memory only every other byte of memory is valid. If skip_bytes = 0, every byte is used
// if skip_bytes = 1, every other byte is read.
//

//
// g_SAT_bitmap[] is a large bitmap used to store free\busy blocks
// each bitmap represents a single block
//
// Internal memory
// - 0x8000 parition size
// - 0x40 block size
// - bytes needed = 0x8000 / 0x40 / 8 (bits per byte) = 0x40 (64) bytes
//
// 32 Mb Cartridge
// - 0x400000 partition size
// - 0x400 block size
// - bytes needed - 0x400000 / 0x400 / 8 (bits per byte_ = 0x200 (512) bytes
//
// Action Replay
// - 0x80000 partition size
// - 0x40 block size
// - bytes needed = 0x80000 / 0x40 / 8 (bits per byte) = 0x400 (1024) bytes
//
// This means we need a 1024 byte buffer to support the Action Replay
//

/** @brief Bitmap representing blocks in a partition. Each bit represents one block */
unsigned char g_SAT_bitmap[SAT_MAX_BITMAP] = {0};

// block helper functions
static SLINGA_ERROR calc_num_blocks(unsigned int save_size, unsigned int block_size, unsigned int skip_bytes, unsigned int* num_save_blocks);
static SLINGA_ERROR convert_address_to_block_index(const unsigned char* address, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned int skip_bytes, unsigned int* block_index);
static SLINGA_ERROR convert_block_index_to_address(unsigned int block_index, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned int skip_bytes, unsigned char** address);

// parsing saves and metadata
static SLINGA_ERROR copy_metadata(PSAVE_METADATA metadata, const unsigned char* save, unsigned int skip_bytes);
static SLINGA_ERROR find_save(const char* filename, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, unsigned char** save_start);
static SLINGA_ERROR read_save_and_metadata(const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, PSAVE_METADATA metadata, const char* filename, unsigned char* buffer, unsigned int size, unsigned int* bytes_read);
static SLINGA_ERROR walk_partition(const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found, unsigned int* used_blocks);
static SLINGA_ERROR walk_partition_bitmap(unsigned char* bitmap, unsigned int bitmap_size, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes);
static SLINGA_ERROR metadata_to_header(const PSAVE_METADATA metadata, PSAT_START_BLOCK_HEADER header);

// Read saves
static SLINGA_ERROR read_sat_table(const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, const unsigned char* save_start, unsigned char* bitmap, unsigned int bitmap_size, unsigned int* start_block, unsigned int* start_data_block);
static SLINGA_ERROR read_save_from_sat_table(unsigned char* buffer, unsigned int size, unsigned int* bytes_read, unsigned int start_block, unsigned int start_data_block, const unsigned char* bitmap, unsigned int bitmap_size, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned int skip_bytes);
static SLINGA_ERROR read_sat_table_from_block(unsigned int block_index, unsigned char* bitmap, unsigned int bitmap_size, const unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned int skip_bytes, unsigned int start_block, unsigned int* start_data_block, unsigned int* written_sat_entries);

// Write saves
static SLINGA_ERROR write_header(unsigned int save_start_block, const char* filename, unsigned int size, const PSAVE_METADATA metadata, unsigned char* partition_buf, unsigned int partition_size,  unsigned int block_size, unsigned char skip_bytes);
static SLINGA_ERROR write_block_indexes(unsigned int save_start_block, unsigned int num_blocks, const unsigned char* bitmap, unsigned int bitmap_size, unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes, unsigned int* save_data_start_block, unsigned int* save_data_start_offset);
static SLINGA_ERROR write_data(unsigned int save_data_start_block, unsigned int save_data_start_offset, const unsigned char* data, unsigned int size, const unsigned char* bitmap, unsigned int bitmap_size, unsigned char* partition_buf, unsigned int partition_size, unsigned int block_size, unsigned char skip_bytes);

// SAT bitmap helpers
static SLINGA_ERROR get_bitmap_size(unsigned int partition_size, unsigned int block_size, unsigned int max_bitmap_size, unsigned int* bitmap_size);
static SLINGA_ERROR set_bitmap(unsigned int block_index, unsigned char* bitmap, unsigned int bitmap_size);
static SLINGA_ERROR get_next_block_bitmap(unsigned int block_index, const unsigned char* bitmap, unsigned int bitmap_size, unsigned int* next_block_index);
static SLINGA_ERROR count_bitmap(const unsigned char* bitmap, unsigned int bitmap_size, unsigned int* total);
static SLINGA_ERROR invert_bitmap(unsigned char* bitmap, unsigned int bitmap_size);

// skip bytes
static SLINGA_ERROR read_from_partition(unsigned char* dst, const unsigned char* src, unsigned int src_offset, unsigned int size, unsigned int skip_bytes);
static SLINGA_ERROR write_to_partition(unsigned char* dst, unsigned int dst_offset, const unsigned char* src, unsigned int size, unsigned int skip_bytes);
static SLINGA_ERROR memset_partition(unsigned char* dst, unsigned int dst_offset, unsigned char val, unsigned int size, unsigned int skip_bytes);

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
SLINGA_ERROR sat_read(const char* filename,
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

/**
 * @brief Writes save to the partition. Errors if save already exists unless
 * OVERWRITE_EXISTING_SAVE flag is set
 *
 * @param[in] flags flags 0, OVERWRITE_EXISTING_SAVE
 * @param[in] filename Save to delete
 * @param[in] save_metadata Metadata (comment, date, etc) to write with the save
 * @param[in] buffer Save data
 * @param[in] size size of the save data in bytes
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR sat_write(FLAGS flags,
                       const char* filename,
                       const PSAVE_METADATA save_metadata,
                       const unsigned char* buffer,
                       unsigned int size,
                       unsigned char* partition_buf,
                       unsigned int partition_size,
                       unsigned int block_size,
                       unsigned char skip_bytes)
{
    UNUSED(flags); // TODO: add zero entire save option

    unsigned int bitmap_size = 0;
    unsigned char* save_start = NULL;
    unsigned int blocks_needed = 0;
    unsigned int free_blocks = 0;
    unsigned int save_start_block = 0;
    unsigned int save_data_start_block = 0;
    unsigned int save_data_start_offset = 0;
    SLINGA_ERROR result = 0;

    if(!filename || !save_metadata || !buffer || !size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // block size must be 64-byte aligned
    if((block_size % MIN_BLOCK_SIZE) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    //
    // Writing is kind of complicated:
    // - check if the save exists and whether or not the OVERWRITE_EXISTING_SAVE is set
    // -- if the overwrite flag is we can delete\reuse the existing save data
    // -- otherwise error out
    // - compute how many blocks the save needs
    // - parse the entire partition to compute how many free blocks there are
    // -- I assume saves can't use blocks out of orders. But in case they do I have to parse all of the saves on the partition
    // - Writing the save
    // -- header -> easy
    // -- block indexes array pointing to all of the blocks we will use -> hard
    // -- writing the save data -> medium
    //
    // Hope I get this right...
    //

    // locate the save
    result = find_save(filename,
                       partition_buf,
                       partition_size,
                       block_size,
                       skip_bytes,
                       &save_start);
    if(result == SLINGA_SUCCESS)
    {
        if((flags & OVERWRITE_EXISTING_SAVE) == 0)
        {
            // don't overwrite existing save only the flag is set
            return SLINGA_FILE_EXISTS;
        }

        // delete the save by overwriting the tag field to 0
        result = memset_partition(save_start, 0, 0, SAT_TAG_SIZE, skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }
    }

    // calculate how many blocks are needed for the save
    result = calc_num_blocks(size, block_size, skip_bytes, &blocks_needed);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // calculate how how much of the bitmap we actually need
    result = get_bitmap_size(partition_size, block_size, sizeof(g_SAT_bitmap), &bitmap_size);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    memset(g_SAT_bitmap, 0, bitmap_size);

    // record all of the busy blocks on the partition
    result = walk_partition_bitmap(g_SAT_bitmap,
                                   bitmap_size,
                                   partition_buf,
                                   partition_size,
                                   block_size,
                                   skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // flip the bitmap so free blocks are set to 1
    result = invert_bitmap(g_SAT_bitmap, bitmap_size);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // count the free blocks
    result = count_bitmap(g_SAT_bitmap, bitmap_size, &free_blocks);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // make sure we have enough free blocks
    if(free_blocks < blocks_needed)
    {
        return SLINGA_NOT_ENOUGH_SPACE;
    }

    //
    // We have enough space, write the save
    // - header
    // - array of block indexes
    // - save data
    //

    result = get_next_block_bitmap(0, g_SAT_bitmap, bitmap_size, &save_start_block);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // header
    result = write_header(save_start_block,
                          filename,
                          size,
                          save_metadata,
                          partition_buf,
                          partition_size,
                          block_size,
                          skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // variable array of block indexes
    result = write_block_indexes(save_start_block,
                                 blocks_needed,
                                 g_SAT_bitmap,
                                 bitmap_size,
                                 partition_buf,
                                 partition_size,
                                 block_size,
                                 skip_bytes,
                                 &save_data_start_block,
                                 &save_data_start_offset);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // variable array of data bytes
    result = write_data(save_data_start_block,
                        save_data_start_offset,
                        buffer,
                        size,
                        g_SAT_bitmap,
                        bitmap_size,
                        partition_buf,
                        partition_size,
                        block_size,
                        skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

/**
 * @brief Deletes save from the partition.
 *
 * @param[in] filename Save to delete
 * @param[in] flags flags
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR sat_delete(const char* filename,
                        FLAGS flags,
                        unsigned char* partition_buf,
                        unsigned int partition_size,
                        unsigned int block_size,
                        unsigned char skip_bytes)
{
    UNUSED(flags); // TODO: add zero entire save option

    unsigned char* save_start = NULL;
    SLINGA_ERROR result = 0;

    if(!filename)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // block size must be 64-byte aligned
    if((block_size % MIN_BLOCK_SIZE) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // locate the save
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

    // delete the save by overwriting the tag field to 0
    result = memset_partition(save_start, 0, 0, SAT_TAG_SIZE, skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

/**
 * @brief Returns success if the partition is currently formatted
 *
 * The first block must be filled with the string "BackUpRam Format"
 *
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR sat_check_formatted(const unsigned char* partition_buf,
                                 unsigned int partition_size,
                                 unsigned int block_size,
                                 unsigned char skip_bytes)
{
    char temp[BACKUP_RAM_FORMAT_STR_LEN] = {0};
    unsigned int num_lines = 0;
    SLINGA_ERROR result = 0;

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // block size must be 64-byte aligned
    if((block_size % MIN_BLOCK_SIZE) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // we need atleast 1 block
    if(block_size > partition_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // the first block contains block_size/16 copies of the BACKUP_RAM_FORMAT string
    num_lines = block_size / BACKUP_RAM_FORMAT_STR_LEN;

    if(skip_bytes == 1)
    {
        num_lines = num_lines / 2;
    }

    for(unsigned int i = 0; i < num_lines; i++)
    {
        // copy the data locally to avoid having to deal with skip_bytes
        result = read_from_partition((unsigned char*)temp, partition_buf, (i * BACKUP_RAM_FORMAT_STR_LEN), BACKUP_RAM_FORMAT_STR_LEN, skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        result = memcmp(BACKUP_RAM_FORMAT_STR, temp, BACKUP_RAM_FORMAT_STR_LEN);
        if(result != 0)
        {
            return SLINGA_SAT_UNFORMATTED;
        }
    }

    return SLINGA_SUCCESS;
}

/**
 * @brief Formats the partition. All saves will be lost.
 *
 * The first block will be filled with the string "BackUpRam Format"
 *
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR sat_format(unsigned char* partition_buf,
                        unsigned int partition_size,
                        unsigned int block_size,
                        unsigned char skip_bytes)
{
    unsigned int num_lines = 0;
    SLINGA_ERROR result = 0;

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // block size must be 64-byte aligned
    if((block_size % MIN_BLOCK_SIZE) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // the first block contains block_size/16 copies of the BACKUP_RAM_FORMAT string
    num_lines = block_size / BACKUP_RAM_FORMAT_STR_LEN;

    if(skip_bytes == 1)
    {
        num_lines = num_lines / 2;
    }

    result = memset_partition(partition_buf, 0, 0, partition_size/2, skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    for(unsigned int i = 0; i < num_lines; i++)
    {
        // copy the data locally to avoid having to deal with skip_bytes
        result = write_to_partition(partition_buf, (i * BACKUP_RAM_FORMAT_STR_LEN), (const unsigned char*)BACKUP_RAM_FORMAT_STR, BACKUP_RAM_FORMAT_STR_LEN, skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }
    }

    return SLINGA_SUCCESS;
}

//
// Block helper functions
//

/**
 * @brief Given a save size, calculate how many blocks it needs.
 *
 * @param[in] save_size How big the save is in bytes
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] num_save_blocks Number of blocks needed to record save_size bytes on success
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR calc_num_blocks(unsigned int save_size, unsigned int block_size, unsigned int skip_bytes, unsigned int* num_save_blocks)
{
    unsigned int total_bytes = 0;
    unsigned int fixed_bytes = 0;
    unsigned int num_blocks = 0;
    unsigned int num_blocks2 = 0;

    if(!save_size || !num_save_blocks || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes == 1)
    {
        block_size = block_size/2;
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
 * @param[in] address address of save
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

    UNUSED(skip_bytes);

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

/**
 * @brief Converts block index to save address
 *
 * @param[in] Block index index of block to convert to address
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] address Block index represented as address on success
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR convert_block_index_to_address(unsigned int block_index,
                                                   const unsigned char* partition_buf,
                                                   unsigned int partition_size,
                                                   unsigned int block_size,
                                                   unsigned int skip_bytes,
                                                   unsigned char** address)
{
    if(!address || !partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // index is just the number of blocks from the start of the partition
    *address = (unsigned char*)(partition_buf + (block_index * block_size));

    if(*address < partition_buf)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(*address >= partition_buf + partition_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    return SLINGA_SUCCESS;
}

//
// Parsing Save and Metadata
//

/**
 * @brief Reads metadata from a save start block
 *
 * @param[out] metadata Read metadata on success
 * @param[in] save Pointer to the start block
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR copy_metadata(PSAVE_METADATA metadata, const unsigned char* save, unsigned int skip_bytes)
{
    SAT_START_BLOCK_HEADER temp_save = {0};
    SLINGA_ERROR result = 0;

    if(!metadata || !save)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // copy the data locally to avoid having to deal with skip_bytes
    result = read_from_partition((unsigned char*)&temp_save, (const unsigned char*)save, 0, sizeof(SAT_START_BLOCK_HEADER), skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    //
    // save name, filename, and comment
    //
    memcpy(metadata->savename, temp_save.savename, MAX_SAVENAME);
    metadata->savename[MAX_SAVENAME] = '\0'; //savename is MAX_SAVENAME + 1 bytes long

    snprintf(metadata->filename, MAX_FILENAME, "%s.BUP", metadata->savename); // .BUP extension will always fit
    metadata->filename[MAX_FILENAME] = '\0'; //filename is MAX_FILENAME + 1 bytes long

    memcpy(metadata->comment, temp_save.comment, MAX_COMMENT);
    metadata->comment[MAX_COMMENT] = '\0'; //comment is MAX_COMMMENT + 2 bytes long

    //
    // language, timestamp, data size, and block size
    //
    metadata->language = temp_save.language;
    metadata->timestamp = temp_save.timestamp;
    metadata->data_size = temp_save.data_size;
    metadata->block_size = 0; // block size isn't needed (and isn't stored in the metadata)

    return SLINGA_SUCCESS;
}

/**
 * @brief Converts SAVE_METADATA to SAT_START_BLOCKP_HEADER
 *
 * @param[in] metadata SAVE_METADATA
 * @param[out] header On success, filled out SAT_START_BLOCK_HEADER
  *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR metadata_to_header(const PSAVE_METADATA metadata, PSAT_START_BLOCK_HEADER header)
{
    if(!metadata || !header)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    memset(header, 0, sizeof(SAT_START_BLOCK_HEADER));

    header->tag = SAT_START_BLOCK_TAG;
    memcpy(header->savename, metadata->savename, SAT_MAX_SAVE_NAME);
    header->language = metadata->language;
    memcpy(header->comment, metadata->comment, SAT_MAX_SAVE_COMMENT);
    header->timestamp = metadata->timestamp;
    header->data_size = metadata->data_size;

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
                              unsigned char** save_start)
{
    SAT_START_BLOCK_HEADER metadata = {0};
    SLINGA_ERROR result = 0;

    if(!filename || !partition_buf || !partition_size || !block_size || !save_start)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // loop through all blocks on the partition
    // the first two blocks are not used for saves
    for(unsigned int i = (2 * block_size); i < partition_size; i += block_size)
    {
        const unsigned char* current_block = (partition_buf + i);

        // validate range
        if(current_block < partition_buf || current_block >= partition_buf + partition_size)
        {
            return SLINGA_INVALID_PARAMETER;
        }

        result = read_from_partition((unsigned char*)&metadata, current_block, 0, sizeof(SAT_START_BLOCK_HEADER), skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        // every save starts with a tag
        if(metadata.tag == SAT_START_BLOCK_TAG)
        {
            // check if this is our save
            result = strncmp(filename, metadata.savename, SAT_MAX_SAVE_NAME);
            if(result != 0)
            {
                // not our save
                continue;
            }

            *save_start = (unsigned char*)current_block;
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
    unsigned char* save_start = NULL;
    SAT_START_BLOCK_HEADER save_header = {0};
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

    result = read_from_partition((unsigned char*)&save_header, save_start, 0, sizeof(SAT_START_BLOCK_HEADER), skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    if(buffer)
    {
        unsigned int start_block = 0;
        unsigned int start_data_block = 0;
        unsigned int save_size = 0;
        unsigned int bitmap_size = 0;

        // read the save size
        save_size = save_header.data_size;
        if(save_size < size)
        {
            // buffer isn't big enough to hold the save
            return SLINGA_BUFFER_TOO_SMALL;
        }

        // calculate how how much of the bitmap we actually need
        result = get_bitmap_size(partition_size, block_size, sizeof(g_SAT_bitmap), &bitmap_size);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        // zero out the bitmap to begin
        memset(g_SAT_bitmap, 0, bitmap_size);

        result = read_sat_table(partition_buf,
                                partition_size,
                                block_size,
                                skip_bytes,
                                save_start,
                                g_SAT_bitmap,
                                bitmap_size,
                                &start_block,
                                &start_data_block);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        result = read_save_from_sat_table(buffer,
                                          size,
                                          bytes_read,
                                          start_block,
                                          start_data_block,
                                          g_SAT_bitmap,
                                          bitmap_size,
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
    SAT_START_BLOCK_HEADER metadata = {0};
    const unsigned char* current_block = NULL;
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
        current_block = partition_buf + i;

        // validate range
        if(current_block < partition_buf || current_block >= partition_buf + partition_size)
        {
            return SLINGA_SAT_INVALID_PARTITION;
        }

        result = read_from_partition((unsigned char*)&metadata, current_block, 0, sizeof(SAT_START_BLOCK_HEADER), skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        // every save starts with a tag
        if(metadata.tag == SAT_START_BLOCK_TAG)
        {
            result = calc_num_blocks(metadata.data_size, block_size, skip_bytes, &save_blocks);
            if(result != SLINGA_SUCCESS)
            {
                return SLINGA_SAT_INVALID_PARTITION;
            }

            blocks_found += save_blocks;

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
                result = copy_metadata(&saves[saves_found], current_block, skip_bytes);
                if(result)
                {
                    return result;
                }
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

/**
 * @brief Walk the partition, finding all saves and metadata
 *
 * @param[out] bitmap On success the bitmap bits will be set to 1 for each block that is part of the save
 * @param[in] bitmap_size Size of bitmap in bytes *
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR walk_partition_bitmap(unsigned char* bitmap,
                                          unsigned int bitmap_size,
                                          const unsigned char* partition_buf,
                                          unsigned int partition_size,
                                          unsigned int block_size,
                                          unsigned char skip_bytes)
{
    unsigned int tag = 0;
    const unsigned char* current_block = NULL;
    SLINGA_ERROR result = 0;

    if(!bitmap || !bitmap_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(block_size > partition_size || (partition_size % block_size) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // don't write to the first two blocks
    // add the start block to our bitmap
    result = set_bitmap(0, bitmap, bitmap_size);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = set_bitmap(1, bitmap, bitmap_size);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // loop through all blocks onthe parititon
    // the first two blocks are not used for saves
    for(unsigned int i = (2 * block_size); i < partition_size; i += block_size)
    {
        current_block = partition_buf + i;

        // validate range
        if(current_block < partition_buf || current_block >= partition_buf + partition_size)
        {
            return SLINGA_SAT_INVALID_PARTITION;
        }

        result = read_from_partition((unsigned char*)&tag, current_block, 0, SAT_TAG_SIZE, skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        // every save starts with a tag
        if(tag == SAT_START_BLOCK_TAG)
        {
            unsigned int start_block = 0;
            unsigned int start_data_block = 0;

            result = read_sat_table(partition_buf,
                                    partition_size,
                                    block_size,
                                    skip_bytes,
                                    current_block,
                                    bitmap,
                                    bitmap_size,
                                    &start_block,
                                    &start_data_block);
            if(result != SLINGA_SUCCESS)
            {
                return result;
            }
        }
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
                                   const unsigned char* save_start,
                                   unsigned char* bitmap,
                                   unsigned int bitmap_size,
                                   unsigned int* start_block,
                                   unsigned int* start_data_block)
{
    unsigned int num_sat_blocks = 0;
    unsigned int written_sat_entries = 0;
    unsigned int cur_sat_block = 0;
    SAT_START_BLOCK_HEADER save_header = {0};
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

    // copy the data locally to avoid having to deal with skip_bytes
    result = read_from_partition((unsigned char*)&save_header, save_start, 0, sizeof(SAT_START_BLOCK_HEADER), skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = calc_num_blocks(save_header.data_size, block_size, skip_bytes, &num_sat_blocks);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // sanity check
    if(num_sat_blocks > bitmap_size * 8)
    {
        return SLINGA_SAT_SAVE_OUT_OF_RANGE;
    }

    result = convert_address_to_block_index(save_start, partition_buf, partition_size, block_size, skip_bytes, start_block);
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
        result = read_sat_table_from_block(cur_sat_block, bitmap, bitmap_size, partition_buf, partition_size, block_size, skip_bytes, *start_block, start_data_block, &written_sat_entries);
        if(result == SLINGA_SUCCESS && result != SLINGA_MORE_DATA_AVAILABLE)
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

    }while(1);

    unsigned int bitmap_count = 0;

    result = count_bitmap(bitmap, bitmap_size, &bitmap_count);
    if(result != SLINGA_SUCCESS)
    {
        return result;;
    }

    if(bitmap_count != num_sat_blocks)
    {
        return SLINGA_SAT_INVALID_PARTITION;
    }
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
    unsigned int block_data_size = 0;
    unsigned int bytes_to_copy = 0;
    SLINGA_ERROR result = 0;

    if(!buffer || !size || !start_block || !start_data_block || !bitmap || !bitmap_size || !partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes == 0)
    {
        // all bytes are valid, just subtract off the tag size
        block_data_size = block_size - SAT_TAG_SIZE;
    }
    else if(skip_bytes == 1)
    {
        // every other byte is valid
        block_data_size = (block_size/2) - SAT_TAG_SIZE;
    }
    else
    {
        // invalid skip_bytes value
        return SLINGA_INVALID_PARAMETER;
    }

    // we want to start reading at the first block that has save data
    cur_sat_block = start_data_block;

    // loop throught the bitmap until we are out of blocks
    while(1)
    {
        result = convert_block_index_to_address(cur_sat_block, partition_buf, partition_size, block_size, skip_bytes, &block);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

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
            result = read_from_partition(buffer + bytes_written, block, SAT_TAG_SIZE, bytes_to_copy, skip_bytes);
            if(result != SLINGA_SUCCESS)
            {
                return result;
            }
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
                unsigned short index = 0;

                result = read_from_partition((unsigned char*)&index, block, i + SAT_TAG_SIZE, sizeof(unsigned short), skip_bytes);
                if(result != SLINGA_SUCCESS)
                {
                    return result;
                }

                if(index == 0)
                {
                    // found the 0s
                    offset = i + sizeof(unsigned short);
                    break;
                }
            }

            // handle edge case of end 0x0000 being last block
            if(offset == block_data_size)
            {

                 // TODO: add additional safety checks
                result = get_next_block_bitmap(cur_sat_block, bitmap, bitmap_size, &cur_sat_block);
                if(result == SLINGA_NOT_FOUND)
                {
                    // there are no more bits in the bitmap to check
                    break;
                }

                result = convert_block_index_to_address(cur_sat_block, partition_buf, partition_size, block_size, skip_bytes, &block);
                if(result != SLINGA_SUCCESS)
                {
                    return result;
                }

                offset = 0;
            }

            bytes_to_copy -= offset;

            // check if we are the last very last block and we aren't full
            if(size - bytes_written < block_data_size - offset)
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

            result = read_from_partition(buffer + bytes_written, block, offset + SAT_TAG_SIZE, bytes_to_copy, skip_bytes);
            if(result != SLINGA_SUCCESS)
            {
                return result;
            }
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
    SAT_START_BLOCK_HEADER save_start = {0};
    unsigned char* save_start_block = NULL;
    SLINGA_ERROR result = 0;

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

    result = convert_block_index_to_address(block_index, partition_buf, partition_size, block_size, skip_bytes, &save_start_block);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // validate we are still in range
    if(save_start_block < partition_buf || save_start_block >= partition_buf + partition_size)
    {
        return SLINGA_SAT_INVALID_PARTITION;
    }

    // copy the data locally to avoid having to deal with skip_bytes
    result = read_from_partition((unsigned char*)&save_start, save_start_block, 0, sizeof(SAT_START_BLOCK_HEADER), skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // first entry
    if(block_index == start_block)
    {
        // first block must have the start tag
        if(save_start.tag != SAT_START_BLOCK_TAG)
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
        if(save_start.tag != SAT_CONTINUE_BLOCK_TAG)
        {
            return SLINGA_SAT_INVALID_TAG;
        }

        // where the block's data starts
        // continuation block's only have a 4-byte tag field
        start_byte = SAT_TAG_SIZE;
    }

    // loop through the block, recording SAT table entries until you find the
    // 0x0000 terminator or reach the end of the block

    if(skip_bytes == 1)
    {
        // every other byte is valid
        block_size = block_size/2;
    }

    // look throught the block recording all the block indexes
    // we terminate if:
    // - we reach the end of the block
    // - we encounter a 0x0000
    for(; start_byte < block_size; start_byte += sizeof(unsigned short))
    {
        unsigned short index = 0;

        // copy the data locally to avoid having to deal with skip_bytes
        result = read_from_partition((unsigned char*)&index, save_start_block, start_byte, sizeof(unsigned short), skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

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
// Write saves
//

/**
 * @brief Write the SAT_START_BLOCK_HEADER for the specified save
 *
 * @param[in] save_start_block Index corresponding to the first block of the save
 * @param[in] filename Save name
 * @param[in] size Size in bytes of the save data
 * @param[in] metadata Save metadata
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR write_header(unsigned int save_start_block,
                                 const char* filename,
                                 unsigned int size,
                                 const PSAVE_METADATA metadata,
                                 unsigned char* partition_buf,
                                 unsigned int partition_size,
                                 unsigned int block_size,
                                 unsigned char skip_bytes)
{
    SAT_START_BLOCK_HEADER header = {0};
    unsigned char* save_start = NULL;
    SLINGA_ERROR result = 0;

    if(!filename || !size || !metadata)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // set up the save header
    result = metadata_to_header(metadata, &header);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }
    header.data_size = size;

    // convert the block index to an address
    result = convert_block_index_to_address(save_start_block,
                                            partition_buf,
                                            partition_size,
                                            block_size,
                                            skip_bytes,
                                            &save_start);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // write the header
    result = write_to_partition(save_start, 0, (const unsigned char*)&header, sizeof(header), skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

/**
 * @brief Write the variable array of block indexes after the SAT_START_BLOCK_HEADER for the specified save
 *
 * @param[in] save_start_block Index corresponding to the first block of the save
 * @param[in] num_blocks number of blocks for the save
 * @param[in] bitmap Bitmap of free blocks that will be used by this save
 * @param[in] bitmap_size Size of bitmap in bytes
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 * @param[out] save_data_start_block On success, the index of the 1st block with save data (after header and block index array)
 * @param[out] save_data_start_offset On success, the byte offset within save_data_start_block of the first byte of data
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR write_block_indexes(unsigned int save_start_block,
                                        unsigned int num_blocks,
                                        const unsigned char* bitmap,
                                        unsigned int bitmap_size,
                                        unsigned char* partition_buf,
                                        unsigned int partition_size,
                                        unsigned int block_size,
                                        unsigned char skip_bytes,
                                        unsigned int* save_data_start_block,
                                        unsigned int* save_data_start_offset)
{
    unsigned int cur_block_index = 0;
    unsigned int highest_index_written = 0;
    unsigned int indexes_written = 0;
    unsigned char* cur_block_address = NULL;
    unsigned int offset = 0;
    unsigned int adjusted_block_size = 0;
    SLINGA_ERROR result = 0;

    if(!bitmap || !bitmap_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(!save_data_start_block || !save_data_start_offset)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // minimum save size is one block
    if(!num_blocks)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes == 1)
    {
        // if skip_bytes, only have the block size is valid
        adjusted_block_size = block_size / 2;
    }
    else
    {
        adjusted_block_size = block_size;
    }

    cur_block_index = save_start_block;
    highest_index_written = save_start_block;
    offset = sizeof(SAT_START_BLOCK_HEADER); // in case only one block

    while(indexes_written < num_blocks)
    {
        offset = 0;

        result = convert_block_index_to_address(cur_block_index, partition_buf, partition_size, block_size, skip_bytes, &cur_block_address);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        if(cur_block_index == save_start_block)
        {
            // first block: start after the header
            offset = sizeof(SAT_START_BLOCK_HEADER);
        }
        else
        {
            // not the start block, start after the tag
            offset = SAT_TAG_SIZE;

            // continuation tags are 0x00000000
            result = memset_partition(cur_block_address, 0, 0, SAT_TAG_SIZE, skip_bytes);
            if(result != SLINGA_SUCCESS)
            {
                return result;
            }
        }

        for(; offset < adjusted_block_size; offset += sizeof(unsigned short))
        {
            unsigned short index = 0;
            unsigned int next_sat_block = 0;

            indexes_written++;

            if(indexes_written == num_blocks)
            {
                // this is the last block, write 0x0000 as the terminating index
                index = 0;
            }
            else
            {
                // not the last block
                // get the index of the next block to write
                result = get_next_block_bitmap(highest_index_written, bitmap, bitmap_size, &next_sat_block);
                if(result != SLINGA_SUCCESS)
                {
                    return result;
                }

                index = (unsigned short)(next_sat_block);
                highest_index_written = index;
            }

            // we have the index, write it
            result = write_to_partition(cur_block_address, offset, (unsigned char*)&index, sizeof(unsigned short), skip_bytes);
            if(result != SLINGA_SUCCESS)
            {
                return result;
            }

            // check if we have more blocks to write
            if(indexes_written >= num_blocks)
            {
                // TODO: increment offset here
                offset += sizeof(unsigned short);
                break;
            }
        }

        // check if we have more blocks to write
        if(indexes_written < num_blocks)
        {
            // get the next block to write to
            result = get_next_block_bitmap(cur_block_index, bitmap, bitmap_size, &cur_block_index);
            if(result != SLINGA_SUCCESS)
            {
                return result;
            }
        }
    }

    // edge case of us filling up the current block in the loop
    if(offset == adjusted_block_size)
    {
        // get the next block to write to
        result = get_next_block_bitmap(cur_block_index, bitmap, bitmap_size, &cur_block_index);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }
        offset = SAT_TAG_SIZE;
    }

    *save_data_start_block = cur_block_index;
    *save_data_start_offset = offset;

    return SLINGA_SUCCESS;
}

/**
 * @brief Write the save data for the specified save
 *
 * @param[in] save_data_start_block Index of the 1st block with save data (after header and block index array)
 * @param[in] save_data_start_offset Byte offset within save_data_start_block of the first byte of data
 * @param[in] data save data to write
 * @param[in] size size in bytes of data
 * @param[in] bitmap Bitmap of free blocks that will be used by this save
 * @param[in] bitmap_size Size of bitmap in bytes
 * @param[in] partition_buf Start of the save partition
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.

 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR write_data(unsigned int save_data_start_block,
                               unsigned int save_data_start_offset,
                               const unsigned char* data,
                               unsigned int size,
                               const unsigned char* bitmap,
                               unsigned int bitmap_size,
                               unsigned char* partition_buf,
                               unsigned int partition_size,
                               unsigned int block_size,
                               unsigned char skip_bytes)
{
    unsigned int cur_block_index = 0;
    unsigned int bytes_written = 0;
    unsigned char* cur_block_address = NULL;
    unsigned int offset = 0;
    unsigned int bytes_left = 0;
    unsigned int adjusted_block_size = 0;
    SLINGA_ERROR result = 0;

    // save start block
    // current_index_block - current block that we are writing the array to
    // current_index
    // return current_index_block when finished so we know where to write data to

    if(!bitmap || !bitmap_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(!partition_buf || !partition_size || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(!data || !size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes == 1)
    {
        // if skip_bytes, only have the block size is valid
        adjusted_block_size = block_size / 2;
    }
    else
    {
        adjusted_block_size = block_size;
    }

    cur_block_index = save_data_start_block;

    while(bytes_written < size)
    {
        unsigned int bytes_to_write = 0;
        offset = 0;

        result = convert_block_index_to_address(cur_block_index, partition_buf, partition_size, block_size, skip_bytes, &cur_block_address);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }

        // check if we are the first data block
        if(cur_block_index == save_data_start_block)
        {
            offset = save_data_start_offset;
        }
        else
        {
            // not the save_data_start_block, start after the tag
            offset = SAT_TAG_SIZE;
        }

        bytes_left = size - bytes_written;
        bytes_to_write = LIBSLINGA_MIN(adjusted_block_size - offset, bytes_left);

        result = write_to_partition(cur_block_address, offset, data + bytes_written, bytes_to_write, skip_bytes);
        if(result != SLINGA_SUCCESS)
        {
            return result;
        }
        bytes_written += bytes_to_write;

        // check if we have more blocks to write
        if(bytes_written <= size)
        {
            // get the next block to write to
            result = get_next_block_bitmap(cur_block_index, bitmap, bitmap_size, &cur_block_index);
            if(result != SLINGA_SUCCESS)
            {
                return result;
            }
        }
    }

    return SLINGA_SUCCESS;
}

//
// SAT Bitmap
//

/**
 * @brief Give a partition size and block size, compute how big of a bitmap is required
 *
 * @param[in] partition_size Size in bytes of the save partition
 * @param[in] block_size How big the blocks are on the partition
 * @param[in] max_bitmap_size Maximum size of bitmap. Errors if computed bitmap_size is too big
 * @param[out] bitmap_size On success, size in bytes of the bitmap
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR get_bitmap_size(unsigned int partition_size, unsigned int block_size, unsigned int max_bitmap_size, unsigned int* bitmap_size)
{
    if(!partition_size || !block_size || !max_bitmap_size || !bitmap_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if((block_size % 8) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if((partition_size % block_size) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    *bitmap_size = (partition_size / block_size) / 8;

    return SLINGA_SUCCESS;
}

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

    if(!bitmap || !bitmap_size)
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
 * @brief Get's the next block in the SAT bitmap. Bit must be set to 1
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

    if(!bitmap || !bitmap_size || !next_block_index)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    for(unsigned int i = block_index + 1; i < bitmap_size * 8; i++)
    {
        byte_index = i / 8;
        bit_index = i % 8;

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

/**
 * @brief Counts the number of set bits in the bitmap
 *
 * @param[in] bitmap Bitmap representing SAT blocks
 * @param[in] bitmap_size Size in bytes of the bitmap
 * @param[out] total On success, number of bits set to 1 in the bitmap
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR count_bitmap(const unsigned char* bitmap, unsigned int bitmap_size, unsigned int* total)
{
    unsigned int count = 0;

    const uint8_t NIBBLE_LOOKUP_TABLE[16] =
    {
        0, 1, 1, 2, 1, 2, 2, 3,
        1, 2, 2, 3, 2, 3, 3, 4
    };

    if(!bitmap || !bitmap_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    for(unsigned int i = 0; i < bitmap_size; i++)
    {
        unsigned char byte = bitmap[i];
        count += NIBBLE_LOOKUP_TABLE[byte & 0x0F] + NIBBLE_LOOKUP_TABLE[byte >> 4];
    }

    *total = count;

    return SLINGA_SUCCESS;
}


/**
 * @brief Flips all 0s to 1s and 1s to 0s in the bitmap
 *
 * @param[in] bitmap Bitmap representing SAT blocks
 * @param[in] bitmap_size Size in bytes of the bitmap
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR invert_bitmap(unsigned char* bitmap, unsigned int bitmap_size)
{
    if(!bitmap || !bitmap_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    for(unsigned int i = 0; i < bitmap_size; i++)
    {
        bitmap[i] = ~bitmap[i];
    }

    return SLINGA_SUCCESS;
}

//
// Skip Bytes
//

/**
 * @brief Read bytes from partition. Needed to support skip_bytes
 *
 * @param[in] dst Destination buffer on success
 * @param[in] src Source of bytes to read
 * @param[in] src_offset Offset from src to start reading. Will be adjusted by skip_bytes
 * @param[in] size How many bytes to write from src to dest
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.
 *
 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR read_from_partition(unsigned char* dst, const unsigned char* src, unsigned int src_offset, unsigned int size, unsigned int skip_bytes)
{
    if(!dst || !src || !size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // if skip_bytes is 0, just memcpy
    if(skip_bytes == 0)
    {
        memcpy(dst, src + src_offset, size);
        return SLINGA_SUCCESS;
    }

    src_offset *= 2;

    // skip bytes is 1
    for(unsigned int i = 0; i < size; i++)
    {
        dst[i] = src[(i*2) + skip_bytes + src_offset];
    }

    return SLINGA_SUCCESS;
}

/**
 * @brief Write bytes to partition. Needed to support skip_bytes
 *
 * @param[in] dst Destination buffer to write to
 * @param[in] dst_offset Offset from dst to start writing. Will be adjusted by skip_bytes
 * @param[in] src Source of bytes to write
 * @param[in] size How many bytes to write from src to dest
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.

 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR write_to_partition(unsigned char* dst, unsigned int dst_offset, const unsigned char* src, unsigned int size, unsigned int skip_bytes)
{
    if(!dst || !src || !size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // if skip_bytes is 0, just memcpy
    if(skip_bytes == 0)
    {
        memcpy(dst + dst_offset, src, size);
        return SLINGA_SUCCESS;
    }

    dst_offset *= 2;

    // skip bytes is 1
    for(unsigned int i = 0; i < size; i++)
    {
        dst[(i*2) + skip_bytes + dst_offset] = src[i];
    }

    return SLINGA_SUCCESS;
}

/**
 * @brief Memset bytes in partition. Needed to support skip_bytes
 *
 * @param[in] dst Destination buffer to write to
 * @param[in] dst_offset Offset from dst to start writing. Will be adjusted by skip_bytes
 * @param[in] val byte to write
 * @param[in] size How many bytes to write from src to dest
 * @param[in] skip_bytes How many bytes to skip between valid bytes. This is used by internal\cartridge only.

 * @return SLINGA_SUCCESS on success
 */
static SLINGA_ERROR memset_partition(unsigned char* dst, unsigned int dst_offset, unsigned char val, unsigned int size, unsigned int skip_bytes)
{
    if(!dst || !size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(skip_bytes != 0 && skip_bytes != 1)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // if skip_bytes is 0, just memcpy
    if(skip_bytes == 0)
    {
        memset(dst + dst_offset, val, size);
        return SLINGA_SUCCESS;
    }

    dst_offset *= 2;

    // skip bytes is 1
    for(unsigned int i = 0; i < size; i++)
    {
        dst[(i*2) + skip_bytes + dst_offset] = val;
    }

    return SLINGA_SUCCESS;
}

