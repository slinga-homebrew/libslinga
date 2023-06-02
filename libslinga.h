/** @file libslinga.h
 *
 *  @author Slinga
 *  @brief libslinga API prototypes and constants
 *  @bug No known bugs.
 */
#pragma once

#include <stddef.h>
#include <string.h>
#include "libslinga/libslinga_conf.h"
#include "devices/devices.h"

#define VERSION_MAJOR   (0) ///< @brief Library major version
#define VERSION_MINOR   (0) ///< @brief Library minor version
#define VERSION_PATCH   (1) ///< @brief Library patch version

// TODO: make this customizable
#define MAX_SAVE_SIZE   (256 * 1024) ///< @brief Maximum save size
#define MAX_SAVENAME    (12) ///< @brief Maximum save name
#define MAX_COMMENT     (11) ///< @brief Maximum save comment
#define MAX_FILENAME    (32) ///< @brief Maximum filename

// TODO: why have a limit?
/** @brief Maximum number of saves */
#define MAX_SAVES               255

// all devices should standardize on this directory
// for storing saves
#define SAVES_DIRECTORY "SATSAVES"

/** @brief Error return codes */
typedef enum
{
    SLINGA_SUCCESS = 0,             ///< @brief Success

    // errors
    // TODO: number each one
    SLINGA_NOT_INITIALIZED,             ///< @brief libslinga not initialized
    SLINGA_INVALID_DEVICE_TYPE,         ///< @brief Invalid backup device specified
    SLINGA_BUFFER_TOO_SMALL,            ///< @brief Insufficient buffer provided
    SLINGA_DEVICE_NOT_PRESENT = 4,      ///< @brief Backup device is not present
    SLINGA_NOT_FORMATTED = 5,           ///< @brief Backup device is not formatted
    SLINGA_DEVICE_TYPE_NOT_COMPILED_IN, ///< @brief Specified backup medium type was not compiled in. Check #ifdefs
    SLINGA_NOT_SUPPORTED,               ///< @brief Specified operation is not supported
    SLINGA_INVALID_PARAMETER,           ///< @brief Invalid argument provided
    SLINGA_FILE_EXISTS,                 ///< @brief File already exists
    SLINGA_UNKNOWN_CARTRIDGE,           ///< @brief Unknown cartridge type found


    SLINGA_ACTION_REPLAY_UNSUPPORTED_COMPRESSION,     ///< @brief Action Replay: Unknown compression algorithm
    SLINGA_ACTION_REPLAY_CORRUPT_COMPRESSION_HEADER,  ///< @brief Action Replay: Compression header is corrupt
    SLINGA_ACTION_REPLAY_FAILED_DECOMPRESS_1,         ///< @brief Action Replay: Failed to decompress 1
    SLINGA_ACTION_REPLAY_FAILED_DECOMPRESS_2,         ///< @brief Action Replay: Failed to decompress 2
    SLINGA_ACTION_REPLAY_PARTITION_TOO_LARGE,         ///< @brief Action Replay: The uncompressed partition is too big
    SLINGA_ACTION_REPLAY_EXTENDED_RAM_MISSING,        ///< @brief Action Replay: Extended RAM missing. We need this for decompression

} SLINGA_ERROR;

/**  @brief Languages supported by the Saturn BIOS */
typedef enum SLINGA_LANGUAGE
{
    LANGUAGE_JAPANESE = 0,
    LANGUAGE_ENGLISH = 1,
    LANGUAGE_FRENCH = 2,
    LANGUAGE_GERMAN = 3,
    LANGUAGE_SPANISH = 4,
    LANGUAGE_ITALIAN = 5,
    MAX_LANGUAGE,
}SLINGA_LANGUAGE;

// TODO: verify this structure offsets and size are 4 byte aligned
///< @brief meta data related to save
typedef struct  _SAVE_METADATA {
    char filename[MAX_FILENAME + 1];    ///< @brief Filename on the medium. Will have .BUP extension on CD FS and ODEs.
    char savename[MAX_SAVENAME + 1];    ///< @brief Save name as seen in the BIOS
    char comment[MAX_COMMENT + 2];      ///< @brief Save comment as seen in the BIOS
    unsigned char language;             ///< @brief Language of the save (LANGUAGE_JAPANESE (0) to LANGUAGE_ITALIAN (5))
    unsigned int timestamp;             ///< @brief Save modified time in number of minutes since Jan 1, 1980
    unsigned int data_size;             ///< @brief Size of the save data in bytes
    unsigned short block_size;          ///< @brief Number of blocks used by the save
} SAVE_METADATA, *PSAVE_METADATA;

///< @brief total and available size of the backup medium
typedef struct _BACKUP_STAT
{
    unsigned int total_bytes;      ///< @brief total size in bytes
    unsigned int total_blocks;          ///< @brief total size in number of blocks
    unsigned int block_size;            ///< @brief size of blocks on the medium. If the medium doesn't use blocks, block_size should be 64
    unsigned int free_bytes;       ///< @brief free size in bytes
    unsigned int free_blocks;           ///< @brief free size in number of blocks
    unsigned int max_saves_possible;    ///< @brief maximum number of saves possible
} BACKUP_STAT, *PBACKUP_STAT;

/** @brief libslinga function flags */
typedef enum
{
    DIRECT_WRITE = 1,               ///< @brief Skip the BUP header and write filename and contents directly
    OVERWRITE_EXISTING_SAVE = 2,    ///< @brief Allow existing saves to be overwritten without erroring

} FLAGS;

/** @brief State of library */
typedef struct _LIBSLINGA_CONTEXT
{
    unsigned char isInit;                       ///< @brief 0 if Slinga_Init() has not been called yet
    unsigned char isPresent[MAX_DEVICE_TYPE];   ///< @brief 0 if the device is not present   

} LIBSLINGA_CONTEXT, *PLIBSLINGA_CONTEXT;

/** @brief Global context used for state */
extern LIBSLINGA_CONTEXT g_Context;

// libslinga API
SLINGA_ERROR Slinga_Init(void);
SLINGA_ERROR Slinga_Fini(void);

SLINGA_ERROR Slinga_GetVersion(unsigned char* major, unsigned char* minor, unsigned char* patch);
SLINGA_ERROR Slinga_GetDeviceName(DEVICE_TYPE device_type, char** device_name);

SLINGA_ERROR Slinga_IsPresent(DEVICE_TYPE device_type);
SLINGA_ERROR Slinga_IsReadable(DEVICE_TYPE device_type);
SLINGA_ERROR Slinga_IsWriteable(DEVICE_TYPE device_type);

SLINGA_ERROR Slinga_Stat(DEVICE_TYPE device_type, PBACKUP_STAT stat);
SLINGA_ERROR Slinga_List(DEVICE_TYPE device_type, FLAGS flags, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found);
SLINGA_ERROR Slinga_Read(DEVICE_TYPE device_type, FLAGS flags, const char* filename, const unsigned char* buffer, unsigned int size, unsigned int* bytes_read);

SLINGA_ERROR Slinga_Write(DEVICE_TYPE device_type, FLAGS flags, const char* filename, unsigned char* buffer, unsigned int size);
SLINGA_ERROR Slinga_Delete(DEVICE_TYPE device_type, const char* filename);
SLINGA_ERROR Slinga_Format(DEVICE_TYPE device_type);

SLINGA_ERROR Slinga_SetSaveMetadata(PSAVE_METADATA save_metadata,
                                    const char* filename,
                                    const char* name,
                                    const char* comment,
                                    SLINGA_LANGUAGE language,
                                    unsigned int timestamp,
                                    unsigned int data_size);

// TODO install shim to shim.c
