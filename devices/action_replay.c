/** @file actionreplay.c
 *
 *  @author Slinga
 *  @brief Action Replay cartridge support
 *  @bug Read-only support.
 */
#include "action_replay.h"
#include "sat.h"

#ifdef INCLUDE_ACTION_REPLAY

// utility functions

SLINGA_ERROR decompress_partition(const unsigned char *src, unsigned int src_size, unsigned char **dest, unsigned int* dest_size);
SLINGA_ERROR decompress_RLE01(unsigned char rle_key, const unsigned char *src, unsigned int src_size, unsigned char *dest, unsigned int* bytes_needed);

/*
// for writing not implemented yet
int calcRLEKey(unsigned char* src, unsigned int size, unsigned char* key);
int compressRLE01(unsigned char rleKey, unsigned char *src, unsigned int srcSize, unsigned char *dest, unsigned int* bytesNeeded);
*/

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

    *device_name = "Action Replay (Read-Only)";

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
    SLINGA_ERROR result = 0;
    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int used_blocks = 0;

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
    result = decompress_partition((const unsigned char*)(CARTRIDGE_MEMORY + ACTION_REPLAY_SAVES_OFFSET), ACTION_REPLAY_COMPRESSED_PARTITION_MAX_SIZE, &partition_buf, &partition_size);
    if(result != SLINGA_SUCCESS)
    {
        // failed to decompress
        return result;
    }

    result = sat_get_used_blocks(partition_buf, partition_size, ACTION_REPLAY_BLOCK_SIZE, 0, &used_blocks);
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


SLINGA_ERROR ActionReplay_Write(DEVICE_TYPE device_type, FLAGS flags, const char* filename, unsigned char* buffer, unsigned int size)
{
    if(device_type != DEVICE_ACTION_REPLAY)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    // writing to AR is nontrivial, a ton of work to support
    // not currently supported
    return SLINGA_NOT_SUPPORTED;
}

SLINGA_ERROR ActionReplay_Delete(DEVICE_TYPE device_type, const char* filename)
{
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
SLINGA_ERROR decompress_partition(const unsigned char *src, unsigned int src_size, unsigned char **dest, unsigned int* dest_size)
{
    PRLE01_HEADER header = NULL;
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
    result = decompress_RLE01(header->rle_key, src + sizeof(RLE01_HEADER), header->compressed_size - sizeof(RLE01_HEADER), NULL, dest_size);
    if(result < 0)
    {
        return SLINGA_ACTION_REPLAY_FAILED_DECOMPRESS_1;
    }

    if(*dest_size > ACTION_REPLAY_UNCOMPRESSED_MAX_SIZE)
    {
        return SLINGA_ACTION_REPLAY_PARTITION_TOO_LARGE;
    }

    *dest = CARTRIDGE_RAM_BANK_1;
    memset(*dest, 0, CARTRIDGE_RAM_BANK_SIZE);

    result = decompress_RLE01(header->rle_key, src  + sizeof(RLE01_HEADER), header->compressed_size - sizeof(RLE01_HEADER), *dest, dest_size);
    if(result < 0)
    {
        return SLINGA_ACTION_REPLAY_FAILED_DECOMPRESS_2;
    }

    return SLINGA_SUCCESS;
}

// Decompresses RLE01 compressed buffer into dest
// To calculate number of bytes needed, set dest to NULL
// This function was reversed from function 0x002897dc in ARP_202C.BIN
SLINGA_ERROR decompress_RLE01(unsigned char rle_key, const unsigned char *src, unsigned int src_size, unsigned char *dest, unsigned int* bytes_needed)
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

