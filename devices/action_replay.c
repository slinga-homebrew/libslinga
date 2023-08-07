/** @file actionreplay.c
 *
 *  @author Slinga
 *  @brief Action Replay cartridge support
 *  @bug Read-only support.
 */
#include "action_replay.h"
#include "sat/sat.h"

#ifdef INCLUDE_ACTION_REPLAY


DEVICE_HANDLER g_ActionReplay_Handler = {0};

// utility functions

static SLINGA_ERROR decompress_partition(const unsigned char *src, unsigned int src_size, PPARTITION_INFO partition_info);
static SLINGA_ERROR decompress_RLE01(unsigned char rle_key, const unsigned char *src, unsigned int src_size, unsigned char *dest, unsigned int* bytes_needed);

/*
// for writing not implemented yet
int calcRLEKey(unsigned char* src, unsigned int size, unsigned char* key);
int compressRLE01(unsigned char rleKey, unsigned char *src, unsigned int srcSize, unsigned char *dest, unsigned int* bytesNeeded);
*/

SLINGA_ERROR ActionReplay_RegisterHandler(DEVICE_TYPE device_type, PDEVICE_HANDLER* device_handler)
{
    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!device_handler)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    g_ActionReplay_Handler.init = ActionReplay_Init;
    g_ActionReplay_Handler.fini = ActionReplay_Fini;
    g_ActionReplay_Handler.get_device_name = ActionReplay_GetDeviceName;
    g_ActionReplay_Handler.is_present = ActionReplay_IsPresent;
    g_ActionReplay_Handler.is_readable = ActionReplay_IsReadable;
    g_ActionReplay_Handler.is_writeable = ActionReplay_IsWriteable;
    g_ActionReplay_Handler.stat = ActionReplay_Stat;
    g_ActionReplay_Handler.query_file = ActionReplay_QueryFile;
    g_ActionReplay_Handler.list = ActionReplay_List;
    g_ActionReplay_Handler.read = ActionReplay_Read;
    g_ActionReplay_Handler.write = ActionReplay_Write;
    g_ActionReplay_Handler.delete = ActionReplay_Delete;
    g_ActionReplay_Handler.format = ActionReplay_Format;

    *device_handler = &g_ActionReplay_Handler;

    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_Init(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_Fini(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_GetDeviceName(DEVICE_TYPE device_type, char** device_name)
{
    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!device_name)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    *device_name = "Action Replay Plus (Read-Only)";

    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_IsPresent(DEVICE_TYPE device_type)
{
    char* magic = NULL;
    SATURN_CARTRIDGE_TYPE cart_type = 0;
    SLINGA_ERROR result = 0;

    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(g_Context.isPresent[device_type])
    {
        // we already know device is present
        return SLINGA_SUCCESS;
    }

    // check for the action replay signature
    magic = (char*)(CARTRIDGE_MEMORY + ACTION_REPLAY_MAGIC_OFFSET);

    if(strncmp(magic, ACTION_REPLAY_MAGIC, sizeof(ACTION_REPLAY_MAGIC) - 1) != 0)
    {
        // Action Replay magic bytes missing
        return SLINGA_DEVICE_NOT_PRESENT;
    }

    // finally check if the device type has the extra 4MB RAM
    // we need this to have memory to decompress the save partition...
    result = get_cartridge_type(&cart_type);
    if(result != SLINGA_SUCCESS)
    {
        // we failed to detect a cart??
        return SLINGA_DEVICE_NOT_PRESENT;
    }

    if(cart_type != CARTRIDGE_RAM_4MB)
    {
        // The extended ram isn't present
        // unfortunately we use that ram to decompress the partition
        return SLINGA_ACTION_REPLAY_EXTENDED_RAM_MISSING;
    }

    // Found Action Replay
    g_Context.isPresent[device_type] = 1;
    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_IsReadable(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_IsWriteable(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    // flashing AR is nontrivial, a ton of work to support
    // not currently supported
    return SLINGA_NOT_SUPPORTED;
}

SLINGA_ERROR ActionReplay_Stat(DEVICE_TYPE device_type, PBACKUP_STAT stat)
{
    PARTITION_INFO partition_info = {0};
    unsigned int used_blocks = 0;
    SLINGA_ERROR result = 0;

    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!stat)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    memset(stat, 0, sizeof(BACKUP_STAT));

    // decompress the save partition
    result = decompress_partition((const unsigned char*)(CARTRIDGE_MEMORY + ACTION_REPLAY_SAVES_OFFSET), ACTION_REPLAY_COMPRESSED_PARTITION_MAX_SIZE, &partition_info);
    if(result != SLINGA_SUCCESS)
    {
        // failed to decompress
        return result;
    }

    result = sat_get_used_blocks(&partition_info, &used_blocks);
    if(result != SLINGA_SUCCESS)
    {
        // failed to parse partition
        return result;
    }

    //
    // fill out stats
    //

    stat->total_bytes = ACTION_REPLAY_UNCOMPRESSED_MAX_SIZE - (2 * ACTION_REPLAY_BLOCK_SIZE); // two blocks of headers
    stat->total_blocks = stat->total_bytes / ACTION_REPLAY_BLOCK_SIZE;
    stat->block_size = ACTION_REPLAY_BLOCK_SIZE;

    if(used_blocks > stat->total_blocks)
    {
        used_blocks = stat->total_blocks;
    }

    stat->free_blocks = stat->total_blocks - used_blocks;
    stat->free_bytes = stat->free_blocks * ACTION_REPLAY_BLOCK_SIZE;

    stat->max_saves_possible = stat->free_blocks; // one save per free block?

    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_QueryFile(DEVICE_TYPE device_type, FLAGS flags, const char* filename, PSAVE_METADATA save)
{
    PARTITION_INFO partition_info = {0};
    SLINGA_ERROR result = 0;

    UNUSED(flags);

    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!save)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // decompress the save partition
    result = decompress_partition((const unsigned char*)(CARTRIDGE_MEMORY + ACTION_REPLAY_SAVES_OFFSET),
                                  ACTION_REPLAY_COMPRESSED_PARTITION_MAX_SIZE,
                                  &partition_info);
    if(result != SLINGA_SUCCESS)
    {
        // failed to decompress
        return result;
    }

    result = sat_query_file(filename,
                            &partition_info,
                            save);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_List(DEVICE_TYPE device_type, FLAGS flags, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found)
{
    PARTITION_INFO partition_info = {0};
    SLINGA_ERROR result = 0;

    UNUSED(flags);

    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    // decompress the save partition
    result = decompress_partition((const unsigned char*)(CARTRIDGE_MEMORY + ACTION_REPLAY_SAVES_OFFSET),
                                  ACTION_REPLAY_COMPRESSED_PARTITION_MAX_SIZE,
                                  &partition_info);
    if(result != SLINGA_SUCCESS)
    {
        // failed to decompress
        return result;
    }

    result = sat_list_saves(&partition_info,
                            saves,
                            num_saves,
                            saves_found);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}


SLINGA_ERROR ActionReplay_Read(DEVICE_TYPE device_type, FLAGS flags, const char* filename, unsigned char* buffer, unsigned int size, unsigned int* bytes_read)
{
    PARTITION_INFO partition_info = {0};
    SLINGA_ERROR result = 0;

    UNUSED(flags);

    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!filename || !buffer || !size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // decompress the save partition
    result = decompress_partition((const unsigned char*)(CARTRIDGE_MEMORY + ACTION_REPLAY_SAVES_OFFSET),
                                  ACTION_REPLAY_COMPRESSED_PARTITION_MAX_SIZE,
                                  &partition_info);
    if(result != SLINGA_SUCCESS)
    {
        // failed to decompress
        return result;
    }

    result = sat_read(filename,
                      buffer,
                      size,
                      bytes_read,
                      &partition_info);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR ActionReplay_Write(DEVICE_TYPE device_type, FLAGS flags, const char* filename, const PSAVE_METADATA save_metadata, const unsigned char* buffer, unsigned int size)
{
    UNUSED(flags);
    UNUSED(filename);
    UNUSED(save_metadata);
    UNUSED(buffer);
    UNUSED(size);

    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    // writing to AR is nontrivial, a ton of work to support
    // not currently supported
    return SLINGA_NOT_SUPPORTED;
}

SLINGA_ERROR ActionReplay_Delete(DEVICE_TYPE device_type, FLAGS flags, const char* filename)
{
    UNUSED(flags);
    UNUSED(filename);

    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    // writing to AR is nontrivial, a ton of work to support
    // not currently supported
    return SLINGA_NOT_SUPPORTED;
}

SLINGA_ERROR ActionReplay_Format(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    // writing to AR is nontrivial, a ton of work to support
    // not currently supported
    return SLINGA_NOT_SUPPORTED;
}

//
// Action Replay Utility Functions
//

// Takes in a compressed buffer (including header) from an Action Replay cart
// On success dest contains the uncompressed buffer of destSize bytes
// Caller must free dest on success
// returns 0 on success, non-zero on failure
static SLINGA_ERROR decompress_partition(const unsigned char *src, unsigned int src_size, PPARTITION_INFO partition_info)
{
    PRLE01_HEADER header = NULL;
    unsigned char* dest = NULL;
    unsigned int dest_size = 0;
    int result = 0;

    if(!src || !src_size || !dest || !dest_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(src_size < sizeof(RLE01_HEADER))
    {
        // invalid src size
        return SLINGA_INVALID_PARAMETER;
    }

    header = (PRLE01_HEADER)src;

    // must begin with "RLE01"
    // "DEF01" and "DEF02" are not supported
    if(memcmp(header->compression_magic, RLE01_MAGIC, sizeof(header->compression_magic)) != 0)
    {
        // bad compression magic bytes
        return SLINGA_ACTION_REPLAY_UNSUPPORTED_COMPRESSION;
    }

    if(header->compressed_size >= src_size || header->compressed_size < sizeof(RLE01_HEADER))
    {
        // we will read out of bounds
        return SLINGA_ACTION_REPLAY_CORRUPT_COMPRESSION_HEADER;
    }

    //
    // decompress the Action Replay compressed save buffer
    //
    result = decompress_RLE01(header->rle_key, src + sizeof(RLE01_HEADER), header->compressed_size - sizeof(RLE01_HEADER), NULL, &dest_size);
    if(result < 0)
    {
        return SLINGA_ACTION_REPLAY_FAILED_DECOMPRESS_1;
    }

    if(dest_size > ACTION_REPLAY_UNCOMPRESSED_MAX_SIZE)
    {
        return SLINGA_ACTION_REPLAY_PARTITION_TOO_LARGE;
    }

    dest = CARTRIDGE_RAM_BANK_1;
    memset(dest, 0, CARTRIDGE_RAM_BANK_SIZE);

    result = decompress_RLE01(header->rle_key, src  + sizeof(RLE01_HEADER), header->compressed_size - sizeof(RLE01_HEADER), dest, &dest_size);
    if(result < 0)
    {
        return SLINGA_ACTION_REPLAY_FAILED_DECOMPRESS_2;
    }

    partition_info->partition_buf = dest;
    partition_info->partition_size = dest_size;
    partition_info->block_size = ACTION_REPLAY_BLOCK_SIZE;
    partition_info->skip_bytes = 0;

    return SLINGA_SUCCESS;
}

// Decompresses RLE01 compressed buffer into dest
// To calculate number of bytes needed, set dest to NULL
// This function was reversed from function 0x002897dc in ARP_202C.BIN
static SLINGA_ERROR decompress_RLE01(unsigned char rle_key, const unsigned char *src, unsigned int src_size, unsigned char *dest, unsigned int* bytes_needed)
{
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned int k = 0;

    if(src == NULL || bytes_needed == NULL)
    {
        return -1;
    }

    j = 0;
    i = 0;

    do
    {
        unsigned int count = 0;
        unsigned char val = 0;

        // three compressed cases
        // 1) not key
        // - copy the byte directly
        // - src + 1, dest + 1
        // 2) key followed by zero
        // -- copy key
        // -- src + 2, dest + 1
        // 3) key followed by non-zero, followed by val
        // -- copy val count times
        // -- src + 3, dest + count

        if (src[i] == rle_key)
        {
            count = (int)(char)src[i + 1] & 0xff;
            if (count == 0)
            {
                if(dest)
                {
                    dest[j] = rle_key;
                }

                i = i + 2;
                goto continue_loop;
            }

            val = src[i + 2];
            i = i + 3;
            k = 0;

            if (count != 0)
            {
                do
                {
                    if(dest)
                    {
                        dest[j] = val;
                    }

                    j = j + 1;
                    k = k + 1;

                } while (k < count);
            }
        }
        else
        {
            if(dest)
            {
                dest[j] = src[i];
            }

            i = i + 1;
continue_loop:
            j = j + 1;
        }

        // check if we are at the end
        if (src_size <= i)
        {
            if(bytes_needed)
            {
                *bytes_needed = j;
            }

            return 0;
        }
    } while(1);
}

#endif

