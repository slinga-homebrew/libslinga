/** @file ram.c
 *
 *  @author Slinga
 *  @brief Saturn RAM, not the internal built-in memory
 *  @bug No known bugs.
 */
#include "ram.h"

#ifdef INCLUDE_RAM

DEVICE_HANDLER g_RAM_Handler = {0};

SLINGA_ERROR RAM_RegisterHandler(DEVICE_TYPE device_type, PDEVICE_HANDLER* device_handler)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!device_handler)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    g_RAM_Handler.init = RAM_Init;
    g_RAM_Handler.fini = RAM_Fini;
    g_RAM_Handler.get_device_name = RAM_GetDeviceName;
    g_RAM_Handler.is_present = RAM_IsPresent;
    g_RAM_Handler.is_readable = RAM_IsReadable;
    g_RAM_Handler.is_writeable = RAM_IsWriteable;
    g_RAM_Handler.stat = RAM_Stat;
    g_RAM_Handler.list = RAM_List;
    g_RAM_Handler.read = RAM_Read;
    g_RAM_Handler.write = RAM_Write;
    g_RAM_Handler.delete = RAM_Delete;
    g_RAM_Handler.format = RAM_Format;

    *device_handler = &g_RAM_Handler;

    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_Init(DEVICE_TYPE device_type)
{
    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_Fini(DEVICE_TYPE device_type)
{
    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_GetDeviceName(DEVICE_TYPE device_type, char** device_name)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(!device_name)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    *device_name = "RAM";

    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_IsPresent(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    if(g_Context.isPresent[device_type])
    {
        // we already know device is present
        return SLINGA_SUCCESS;
    }

    // RAM is always present
    g_Context.isPresent[device_type] = 1;
    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_IsReadable(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_IsWriteable(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_Stat(DEVICE_TYPE device_type, PBACKUP_STAT stat)
{
    SLINGA_ERROR result = 0;
    unsigned char* partition_buf = NULL;
    unsigned int partition_size = 0;
    unsigned int used_blocks = 0;

    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    //
    // Stating RAM doesn't make sense
    //

    return SLINGA_NOT_SUPPORTED;
}

SLINGA_ERROR RAM_List(DEVICE_TYPE device_type, FLAGS flags, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    //
    // Listing RAM doesn't make sense
    //

    return SLINGA_NOT_SUPPORTED;
}

SLINGA_ERROR RAM_Read(DEVICE_TYPE device_type, FLAGS flags, const char* filename, unsigned char* buffer, unsigned int size, unsigned int* bytes_read)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_Write(DEVICE_TYPE device_type, FLAGS flags, const char* filename, const unsigned char* buffer, unsigned int size)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    return SLINGA_SUCCESS;
}

SLINGA_ERROR RAM_Delete(DEVICE_TYPE device_type, FLAGS flags, const char* filename)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    //
    // deleting from RAM doesn't make sense
    //

    return SLINGA_NOT_SUPPORTED;
}

SLINGA_ERROR RAM_Format(DEVICE_TYPE device_type)
{
    if(device_type != DEVICE_RAM)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    //
    // Foramtting RAM doesn't make sense
    //

    return SLINGA_NOT_SUPPORTED;
}

#endif
