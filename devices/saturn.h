/** @file saturn.h
 *
 *  @author Slinga
 *  @brief Sega Saturn Internal and Cartridge
 *  @bug No known bugs.
 */

#pragma once

#include "../libslinga/libslinga_conf.h"

#if defined(INCLUDE_INTERNAL) || defined(INCLUDE_CARTRIDGE)

#include "../libslinga.h"
#include "../libslinga/saturn.h"

//
// Sega Saturn Internal and Cartridge
//

#define INTERNAL_MEMORY 0x180000             ///< @brief Start of internal backup memory
#define INTERNAL_MEMORY_SIZE 0x10000         ///< @brief Size of internal backup memory. Only every other byte is valid
#define INTERNAL_MEMORY_BLOCK_SIZE 0x80      ///< @brief Block size of the internal memory partition. Only every other byte is valid
#define INTERNAL_MEMORY_SKIP_BYTES 1         ///< @brief Handles every other byte being valid on internal memory

#define CARTRIDGE_MEMORY_BACKUP 0x04000000   ///< @brief Start backup cartridge address
#define CARTRIDGE_NUM_BLOCKS_0x400 0x400
#define CARTRIDGE_NUM_BLOCKS_0x800 0x800
#define CARTRIDGE_NUM_BLOCKS_0x1000 0x1000

// TODO: don't multiply by2 for the skip bytes here
// you will screw up
#define CARTRIDGE_BLOCK_SIZE_0x200 (0x200 * 2) // skip bytes
#define CARTRIDGE_BLOCK_SIZE_0x400 (0x400 * 2)
#define CARTRIDGE_SKIP_BYTES 1

SLINGA_ERROR Saturn_RegisterHandler(DEVICE_TYPE type, PDEVICE_HANDLER* device_handler);
SLINGA_ERROR Saturn_Init(DEVICE_TYPE device_type);
SLINGA_ERROR Saturn_Fini(DEVICE_TYPE device_type);

SLINGA_ERROR Saturn_GetDeviceName(DEVICE_TYPE type, char** device_name);
SLINGA_ERROR Saturn_IsPresent(DEVICE_TYPE type);
SLINGA_ERROR Saturn_IsReadable(DEVICE_TYPE type);
SLINGA_ERROR Saturn_IsWriteable(DEVICE_TYPE type);

SLINGA_ERROR Saturn_Stat(DEVICE_TYPE device_type, PBACKUP_STAT stat);
SLINGA_ERROR Saturn_QueryFile(DEVICE_TYPE device_type, FLAGS flags, const char* filename, PSAVE_METADATA save);
SLINGA_ERROR Saturn_List(DEVICE_TYPE device_type, FLAGS flags, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found);

SLINGA_ERROR Saturn_Read(DEVICE_TYPE device_type, FLAGS flags, const char* filename, unsigned char* buffer, unsigned int size, unsigned int* bytes_read);
SLINGA_ERROR Saturn_Write(DEVICE_TYPE device_type, FLAGS flags, const char* filename, const PSAVE_METADATA save_metadata, const unsigned char* buffer, unsigned int size);
SLINGA_ERROR Saturn_Delete(DEVICE_TYPE device_type, FLAGS flags, const char* filename);
SLINGA_ERROR Saturn_Format(DEVICE_TYPE device_type);

#endif
