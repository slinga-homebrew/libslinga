/** @file saturn.c
 *
 *  @author Slinga
 *  @brief Sega Saturn Internal and Cartridge
 *  @bug Read-only support.
 */
#include "saturn.h"
#include "sat/sat.h"

#if defined(INCLUDE_INTERNAL) || defined(INCLUDE_CARTRIDGE)

DEVICE_HANDLER g_Saturn_Handler = {0};
SATURN_CARTRIDGE_TYPE g_Cartridge_Type = 0;

static SLINGA_ERROR get_partition_info(DEVICE_TYPE device_type, SATURN_CARTRIDGE_TYPE cart_type, unsigned char** partition_start, unsigned int* partition_size, unsigned int* block_size, unsigned int* skip_bytes);
static SLINGA_ERROR is_supported_backup_cartridge(SATURN_CARTRIDGE_TYPE cart_type);

SLINGA_ERROR Saturn_RegisterHandler(DEVICE_TYPE device_type, PDEVICE_HANDLER* device_handler)
{
    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!device_handler)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    g_Saturn_Handler.init = Saturn_Init;
    g_Saturn_Handler.fini = Saturn_Fini;
    g_Saturn_Handler.get_device_name = Saturn_GetDeviceName;
    g_Saturn_Handler.is_present = Saturn_IsPresent;
    g_Saturn_Handler.is_readable = Saturn_IsReadable;
    g_Saturn_Handler.is_writeable = Saturn_IsWriteable;
    g_Saturn_Handler.stat = Saturn_Stat;
    g_Saturn_Handler.query_file = Saturn_QueryFile;
    g_Saturn_Handler.list = Saturn_List;
    g_Saturn_Handler.read = Saturn_Read;
    g_Saturn_Handler.write = Saturn_Write;
    g_Saturn_Handler.delete = Saturn_Delete;
    g_Saturn_Handler.format = Saturn_Format;

    *device_handler = &g_Saturn_Handler;

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_Init(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_Fini(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_GetDeviceName(DEVICE_TYPE device_type, char** device_name)
{
    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!device_name)
    {
        return SLINGA_INVALID_PARAMETER;
    }

   if(device_type == DEVICE_INTERNAL)
   {
        *device_name = "Internal Memory";
   }
   else
   {
        *device_name = "Cartridge Memory";
   }

    return SLINGA_SUCCESS;
}

// TODO: add cartridge support
SLINGA_ERROR Saturn_IsPresent(DEVICE_TYPE device_type)
{
    SATURN_CARTRIDGE_TYPE cart_type = 0;
    SLINGA_ERROR result = 0;

    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(g_Context.isPresent[device_type])
    {
        // we already know device is present
        return SLINGA_SUCCESS;
    }

    if(device_type == DEVICE_INTERNAL)
    {
        // always claim the internal memory is there
        g_Context.isPresent[device_type] = 1;
        return SLINGA_SUCCESS;
    }

    // Check if a support cart is plugged in
    result = get_cartridge_type(&cart_type);
    if(result != SLINGA_SUCCESS)
    {
        // we failed to detect a cart??
        return SLINGA_DEVICE_NOT_PRESENT;
    }

    result = is_supported_backup_cartridge(cart_type);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // Found valid cartridge
    g_Cartridge_Type = cart_type;
    g_Context.isPresent[device_type] = 1;

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_IsReadable(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_IsWriteable(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_Stat(DEVICE_TYPE device_type, PBACKUP_STAT stat)
{
    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int block_size = 0;
    unsigned int skip_bytes = 0;
    unsigned int used_blocks = 0;
    SLINGA_ERROR result = 0;

    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    result = Saturn_IsPresent(device_type);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    if(!stat)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    memset(stat, 0, sizeof(BACKUP_STAT));

    result = get_partition_info(device_type, g_Cartridge_Type, &partition_buf, &partition_size, &block_size, &skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = sat_get_used_blocks(partition_buf, partition_size, block_size, skip_bytes, &used_blocks);
    if(result != SLINGA_SUCCESS)
    {
        // failed to parse partition
        return result;
    }

    //
    // fill out stats
    //
    if(skip_bytes == INTERNAL_MEMORY_SKIP_BYTES)
    {
        partition_size = partition_size/2;
        block_size = block_size/2;
    }

    stat->total_bytes = partition_size - (2 * block_size); // two blocks of headers
    stat->total_blocks = stat->total_bytes / block_size;
    stat->block_size = block_size;

    if(used_blocks > stat->total_blocks)
    {
        used_blocks = stat->total_blocks;
    }

    stat->free_blocks = stat->total_blocks - used_blocks;
    stat->free_bytes = stat->free_blocks * block_size;

    stat->max_saves_possible = stat->free_blocks; // one save per free block?

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_QueryFile(DEVICE_TYPE device_type, FLAGS flags, const char* filename, PSAVE_METADATA save)
{
    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int block_size = 0;
    unsigned int skip_bytes = 0;
    SLINGA_ERROR result = 0;

    UNUSED(flags);

    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!save)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    result = Saturn_IsPresent(device_type);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = get_partition_info(device_type, g_Cartridge_Type, &partition_buf, &partition_size, &block_size, &skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = sat_query_file(filename,
                            partition_buf,
                            partition_size,
                            block_size,
                            skip_bytes,
                            save);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_List(DEVICE_TYPE device_type, FLAGS flags, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found)
{
    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int block_size = 0;
    unsigned int skip_bytes = 0;
    SLINGA_ERROR result = 0;

    UNUSED(flags);

    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    result = Saturn_IsPresent(device_type);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = get_partition_info(device_type, g_Cartridge_Type, &partition_buf, &partition_size, &block_size, &skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = sat_list_saves(partition_buf,
                            partition_size,
                            block_size,
                            skip_bytes,
                            saves,
                            num_saves,
                            saves_found);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_Read(DEVICE_TYPE device_type, FLAGS flags, const char* filename, unsigned char* buffer, unsigned int size, unsigned int* bytes_read)
{
    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int block_size = 0;
    unsigned int skip_bytes = 0;
    SLINGA_ERROR result = 0;

    UNUSED(flags);

    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!filename || !buffer || !size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    result = Saturn_IsPresent(device_type);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = get_partition_info(device_type, g_Cartridge_Type, &partition_buf, &partition_size, &block_size, &skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // make sure the device is formatted
    result = sat_check_formatted(partition_buf, partition_size, block_size, skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = sat_read_save(filename,
                           buffer,
                           size,
                           bytes_read,
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

SLINGA_ERROR Saturn_Write(DEVICE_TYPE device_type, FLAGS flags, const char* filename, const unsigned char* buffer, unsigned int size)
{
    UNUSED(flags);
    UNUSED(filename);
    UNUSED(buffer);
    UNUSED(size);

    SLINGA_ERROR result = 0;

    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    result = Saturn_IsPresent(device_type);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    // not currently supported
    return SLINGA_NOT_SUPPORTED;
}

SLINGA_ERROR Saturn_Delete(DEVICE_TYPE device_type, FLAGS flags, const char* filename)
{
    UNUSED(flags);
    UNUSED(filename);

    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int block_size = 0;
    unsigned int skip_bytes = 0;
    SLINGA_ERROR result = 0;

    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    result = Saturn_IsPresent(device_type);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = get_partition_info(device_type, g_Cartridge_Type, &partition_buf, &partition_size, &block_size, &skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = sat_delete(filename, flags, partition_buf, partition_size, block_size, skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR Saturn_Format(DEVICE_TYPE device_type)
{
    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int block_size = 0;
    unsigned int skip_bytes = 0;
    SLINGA_ERROR result = 0;

    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    result = Saturn_IsPresent(device_type);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = get_partition_info(device_type, g_Cartridge_Type, &partition_buf, &partition_size, &block_size, &skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    result = sat_format(partition_buf, partition_size, block_size, skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

//
// helper functions
//

static SLINGA_ERROR get_partition_info(DEVICE_TYPE device_type, SATURN_CARTRIDGE_TYPE cart_type, unsigned char** partition_start, unsigned int* partition_size, unsigned int* block_size, unsigned int* skip_bytes)
{
    if(device_type != DEVICE_INTERNAL && device_type != DEVICE_CARTRIDGE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!partition_start || !partition_size || !block_size || !skip_bytes)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    if(device_type == DEVICE_INTERNAL)
    {
        *partition_start = (unsigned char*)INTERNAL_MEMORY;
        *partition_size = INTERNAL_MEMORY_SIZE;
        *block_size = INTERNAL_MEMORY_BLOCK_SIZE;
        *skip_bytes = INTERNAL_MEMORY_SKIP_BYTES;

        return SLINGA_SUCCESS;
    }


    switch(cart_type)
    {
        case CARTRIDGE_BACKUP_400_200_512K:
        {
            // 0x400 blocks * 0x200 block size
            *partition_start = (unsigned char*)CARTRIDGE_MEMORY_BACKUP;
            *partition_size = CARTRIDGE_NUM_BLOCKS_0x400 * CARTRIDGE_BLOCK_SIZE_0x200;
            *block_size = CARTRIDGE_BLOCK_SIZE_0x200;
            *skip_bytes = CARTRIDGE_SKIP_BYTES;

            return SLINGA_SUCCESS;
        }
        case CARTRIDGE_BACKUP_800_200_1MB:
        {
            // 0x800 blocks * 0x200 block size
            *partition_start = (unsigned char*)CARTRIDGE_MEMORY_BACKUP;
            *partition_size = CARTRIDGE_NUM_BLOCKS_0x800 * CARTRIDGE_BLOCK_SIZE_0x200;
            *block_size = CARTRIDGE_BLOCK_SIZE_0x200;
            *skip_bytes = CARTRIDGE_SKIP_BYTES;

            return SLINGA_SUCCESS;
        }
        case CARTRIDGE_BACKUP_1000_200_2MB:
        {
            // 0x1000 blocks * 0x200 block size
            *partition_start = (unsigned char*)CARTRIDGE_MEMORY_BACKUP;
            *partition_size = CARTRIDGE_NUM_BLOCKS_0x1000 * CARTRIDGE_BLOCK_SIZE_0x200;
            *block_size = CARTRIDGE_BLOCK_SIZE_0x200;
            *skip_bytes = CARTRIDGE_SKIP_BYTES;

            return SLINGA_SUCCESS;
        }
        case CARTRIDGE_BACKUP_1000_400_4MB:
        {
            // 0x1000 blocks * 0x400 block size
            *partition_start = (unsigned char*)CARTRIDGE_MEMORY_BACKUP;
            *partition_size = CARTRIDGE_NUM_BLOCKS_0x1000 * CARTRIDGE_BLOCK_SIZE_0x400;
            *block_size = CARTRIDGE_BLOCK_SIZE_0x400;
            *skip_bytes = CARTRIDGE_SKIP_BYTES;

            return SLINGA_SUCCESS;
        }
        default:
            break;
   }

    return SLINGA_INVALID_DEVICE_TYPE;
}

static SLINGA_ERROR is_supported_backup_cartridge(SATURN_CARTRIDGE_TYPE cart_type)
{
    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int block_size = 0;
    unsigned int skip_bytes = 0;
    SLINGA_ERROR result = 0;

    // instead of duplicating a list of support cartridges, just see if we have
    // settings for them
    result = get_partition_info(DEVICE_CARTRIDGE, cart_type, &partition_buf, &partition_size, &block_size, &skip_bytes);
    if(result != SLINGA_SUCCESS)
    {
        return result;
    }

    return SLINGA_SUCCESS;
}

#endif
