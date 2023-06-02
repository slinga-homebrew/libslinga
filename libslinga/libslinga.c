/** @file libslinga.c
 *
 *  @author Slinga
 *  @brief libslinga API implementation
 *  @bug No known bugs.
 */
#include "../libslinga.h"
#include "../devices/action_replay.h"

LIBSLINGA_CONTEXT g_Context = {0};

/**
 * @brief Initializes libslinga. Must be called before using library 
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_Init(void)
{
    if(g_Context.isInit)
    {
        // library already initialized
        return SLINGA_SUCCESS;
    }

    memset(&g_Context, 0, sizeof(g_Context));

    g_Context.isInit = 1;

    return SLINGA_SUCCESS;
}

/**
 * @brief Uninitializes libslinga. Must be called when libslinga is no longer needed
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_Fini(void)
{
    if(!g_Context.isInit)
    {
        return SLINGA_NOT_INITIALIZED;
    }

    g_Context.isInit = 0; 

    // TODO: call fini on each device
    return SLINGA_SUCCESS;
}

/**
 * @brief Get the libary's major.minor.patch version
 *
 * @param[out] major major version of lib
 * @param[out] minor major version of lib
 * @param[out] patch major version of lib
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_GetVersion(unsigned char* major, unsigned char* minor, unsigned char* patch)
{
    if(!g_Context.isInit)
    {
        return SLINGA_NOT_INITIALIZED;
    }

    if(!major || !minor || !patch)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    *major = VERSION_MAJOR;
    *minor = VERSION_MINOR;
    *patch = VERSION_PATCH;

    return SLINGA_SUCCESS;
}

/**
 * @brief Initialize SAVE_METADATA struct with user data. Validates user data.
 *
 * @param[out] save_metadata Filled out SAVE_METADATA structure on success
 * @param[in] filename Filename.  Not all backup devices uses this field. <= MAX_FILENAME length
 * @param[in] savename Name of the save as seen by the BIOS. <= MAX_SAVENAME length
 * @param[in] comment Comment of the save as seen by the BIOS. <= MAX_COMMENT length
 * @param[in] langauge Language of the save as seen by the BIOS.  Language must be in the range of LANGUAGE_JAPANESE (0) to LANGUAGE_ITALIAN (5)
 * @param[in] timestamp Number of seconds since 1980. Use Slinga_ConvertDateToTimestamp() to obtain
 * @param[in] data_size Size in bytes of the file
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_SetSaveMetadata(PSAVE_METADATA save_metadata,
                                    const char* filename,
                                    const char* savename,
                                    const char* comment,
                                    SLINGA_LANGUAGE language,
                                    unsigned int timestamp,
                                    unsigned int data_size)
{
    int len = 0;

    if(!save_metadata)
    {
        // metadata buffer must be provided
        return SLINGA_BUFFER_TOO_SMALL;
    }

    memset(save_metadata, 0, sizeof(SAVE_METADATA));

    // TODO: allow either
    if(!filename || !savename)
    {
        // filename and/or savename must be provided
        return SLINGA_INVALID_PARAMETER;
    }

    len = strlen(filename);
    if(len > MAX_FILENAME)
    {
        return SLINGA_INVALID_PARAMETER;
    }
    // save_metadata filename is MAX_FILENAME + 1 long
    // because we memset above this string is guaranteed to be NULL terminated
    memcpy(save_metadata->filename, filename, len);

    len = strlen(savename);
    if(len > MAX_SAVENAME)
    {
        return SLINGA_INVALID_PARAMETER;
    }
    // save_metadata filename is MAX_SAVE_NAME + 1 long
    // because we memset above this string is guaranteed to be NULL terminated
    memcpy(save_metadata->savename, savename, len);

    if(!comment)
    {
        // comment must be provided
        return SLINGA_INVALID_PARAMETER;
    }

    len = strlen(comment);
    if(len > MAX_COMMENT)
    {
        return SLINGA_INVALID_PARAMETER;
    }
    // save_metadata comment is MAX_COMMENT + 2 long
    // because we memset above this string is guaranteed to be NULL terminated
    memcpy(save_metadata->comment, comment, len);

    // Language must be in the range of LANGUAGE_JAPANESE (0) to LANGUAGE_ITALIAN (5)
    if(language < LANGUAGE_JAPANESE || language >= MAX_LANGUAGE)
    {
        return SLINGA_INVALID_PARAMETER;
    }
    save_metadata->language = language;

    // TODO: sanity check timestamp
    save_metadata->timestamp = timestamp;

    // cap save size
    if(data_size > MAX_SAVE_SIZE)
    {
        return SLINGA_INVALID_PARAMETER;
    }
    save_metadata->data_size = data_size;

    return SLINGA_SUCCESS;
}

/**
 * @brief Get device name from device type
 *
 * @param[in] device_type backup device
 * @param[out] device_name name of backup device on success
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_GetDeviceName(DEVICE_TYPE device_type, char** device_name)
{
    if(!g_Context.isInit)
    {
        return SLINGA_NOT_INITIALIZED;
    }

    if(device_type < 0 || device_type >= MAX_DEVICE_TYPE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    switch(device_type)
    {
        /*
        case INTERNAL:
        case CARTRIDGE:
        case SERIAL:
            return SLINGA_DEVICE_TYPE_NOT_COMPILED_IN;
        */

#ifdef INCLUDE_ACTION_REPLAY
        case DEVICE_ACTION_REPLAY:
            return ActionReplay_GetDeviceName(device_type, device_name);
#endif

        default:
            return SLINGA_DEVICE_TYPE_NOT_COMPILED_IN;
    }
}

/**
 * @brief Check if backup device is present
 *
 * @param[in] device_type backup device
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_IsPresent(DEVICE_TYPE device_type)
{
    if(!g_Context.isInit)
    {
        return SLINGA_NOT_INITIALIZED;
    }

    if(device_type < 0 || device_type >= MAX_DEVICE_TYPE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    switch(device_type)
    {
#ifdef INCLUDE_ACTION_REPLAY
        case DEVICE_ACTION_REPLAY:
            return ActionReplay_IsPresent(device_type);
#endif

        default:
            return SLINGA_DEVICE_TYPE_NOT_COMPILED_IN;
    }
}

/**
 * @brief Check if backup device is readable
 *
 * @param[in] device_type backup device
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_IsReadable(DEVICE_TYPE device_type)
{
    if(device_type < 0 || device_type >= MAX_DEVICE_TYPE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    switch(device_type)
    {
#ifdef INCLUDE_ACTION_REPLAY
        case DEVICE_ACTION_REPLAY:
            return ActionReplay_IsReadable(device_type);
#endif

        default:
            return SLINGA_DEVICE_TYPE_NOT_COMPILED_IN;
    }
}

/**
 * @brief Check if backup device is writeable
 *
 * @param[in] device_type backup device
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_IsWriteable(DEVICE_TYPE device_type)
{
    if(!g_Context.isInit)
    {
        return SLINGA_NOT_INITIALIZED;
    }

    if(device_type < 0 || device_type >= MAX_DEVICE_TYPE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    switch(device_type)
    {

#ifdef INCLUDE_ACTION_REPLAY
        case DEVICE_ACTION_REPLAY:
            return ActionReplay_IsWriteable(device_type);
#endif

        default:
            return SLINGA_DEVICE_TYPE_NOT_COMPILED_IN;
    }
}

/**
 * @brief Queries free/used statistics from the device
 *
 * @param[in] device_type backup device
 * @param[out] stat Filled out BACKUP_STAT structure on success
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_Stat(DEVICE_TYPE device_type, PBACKUP_STAT stat)
{
    if(!g_Context.isInit)
    {
        return SLINGA_NOT_INITIALIZED;
    }

    if(device_type < 0 || device_type >= MAX_DEVICE_TYPE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    switch(device_type)
    {

#ifdef INCLUDE_ACTION_REPLAY
        case DEVICE_ACTION_REPLAY:
            return ActionReplay_Stat(device_type, stat);
#endif

        default:
            return SLINGA_DEVICE_TYPE_NOT_COMPILED_IN;
    }
}

/**
 * @brief Deletes save from backup device
 *
 * @param[in] device_type backup device
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_Delete(DEVICE_TYPE device_type, const char* filename)
{
    if(!g_Context.isInit)
    {
        return SLINGA_NOT_INITIALIZED;
    }

    if(device_type < 0 || device_type >= MAX_DEVICE_TYPE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    switch(device_type)
    {

#ifdef INCLUDE_ACTION_REPLAY
        case DEVICE_ACTION_REPLAY:
            return ActionReplay_Delete(device_type, filename);
#endif

        default:
            return SLINGA_DEVICE_TYPE_NOT_COMPILED_IN;
    }
}


/**
 * @brief Format backup device. All saves will be lost 
 *
 * @param[in] device_type backup device
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR Slinga_Format(DEVICE_TYPE device_type)
{
    if(!g_Context.isInit)
    {
        return SLINGA_NOT_INITIALIZED;
    }

    if(device_type < 0 || device_type >= MAX_DEVICE_TYPE)
    {
        return SLINGA_INVALID_DEVICE_TYPE;
    }

    switch(device_type)
    {

#ifdef INCLUDE_ACTION_REPLAY
        case DEVICE_ACTION_REPLAY:
            return ActionReplay_Format(device_type);
#endif

        default:
            return SLINGA_DEVICE_TYPE_NOT_COMPILED_IN;
    }
}

