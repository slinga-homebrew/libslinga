/** @file action_replay.h
 *
 *  @author Slinga
 *  @brief Action Replay cartridge prototypes and constants
 *  @bug No known bugs.
 */

#pragma once

#include "../libslinga/libslinga_conf.h"

#ifdef INCLUDE_ACTION_REPLAY

#include "../libslinga.h"
#include "../libslinga/saturn.h"

//
// Action Replay Cartridge
//

#define ACTION_REPLAY_MAGIC_OFFSET                    0x50
#define ACTION_REPLAY_SAVES_OFFSET                    0x20000
#define ACTION_REPLAY_COMPRESSED_PARTITION_MAX_SIZE   0x60000 // TODO: Guessing maximum size of compressed saves partition
#define ACTION_REPLAY_UNCOMPRESSED_MAX_SIZE           0x80000 // 512k maximum uncompressed size
#define ACTION_REPLAY_MAGIC                           "ACTION REPLAY"
#define ACTION_REPLAY_BLOCK_SIZE                      64

#define RLE01_MAGIC                         "RLE01"
#define RLE01_MAX_COUNT                     0x100
#define RLE_MAX_REPEAT                      0xFF

#pragma pack(1)
typedef struct _RLE01_HEADER
{
    char compression_magic[5];   // should be "RLE01"
    unsigned char rle_key;        // key used to compress the datasize
    unsigned int compressed_size; // size of the compressed data
}RLE01_HEADER, *PRLE01_HEADER;
#pragma pack()

SLINGA_ERROR ActionReplay_RegisterHandler(DEVICE_TYPE type, PDEVICE_HANDLER* device_handler);
SLINGA_ERROR ActionReplay_Init(DEVICE_TYPE device_type);
SLINGA_ERROR ActionReplay_Fini(DEVICE_TYPE device_type);

SLINGA_ERROR ActionReplay_GetDeviceName(DEVICE_TYPE type, char** device_name);
SLINGA_ERROR ActionReplay_IsPresent(DEVICE_TYPE type);
SLINGA_ERROR ActionReplay_IsReadable(DEVICE_TYPE type);
SLINGA_ERROR ActionReplay_IsWriteable(DEVICE_TYPE type);

SLINGA_ERROR ActionReplay_Stat(DEVICE_TYPE device_type, PBACKUP_STAT stat);
SLINGA_ERROR ActionReplay_QueryFile(DEVICE_TYPE device_type, FLAGS flags, const char* filename, PSAVE_METADATA save);
SLINGA_ERROR ActionReplay_List(DEVICE_TYPE device_type, FLAGS flags, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found);

SLINGA_ERROR ActionReplay_Read(DEVICE_TYPE device_type, FLAGS flags, const char* filename, unsigned char* buffer, unsigned int size, unsigned int* bytes_read);
SLINGA_ERROR ActionReplay_Write(DEVICE_TYPE device_type, FLAGS flags, const char* filename, const unsigned char* buffer, unsigned int size);
SLINGA_ERROR ActionReplay_Delete(DEVICE_TYPE device_type, FLAGS flags, const char* filename);
SLINGA_ERROR ActionReplay_Format(DEVICE_TYPE device_type);

#endif
