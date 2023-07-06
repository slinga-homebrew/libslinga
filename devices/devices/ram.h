/** @file ram.h
 *
 *  @author Slinga
 *  @brief Saturn RAM, not the internal built-in memory
 *  @bug No known bugs.
 */
#pragma once

#include "../libslinga/libslinga_conf.h"

#ifdef INCLUDE_RAM

#include "../libslinga.h"

//
// The RAM device is for reading\writing to the Saturn's RAM. This is not the
// same as the SRAM. This device is used to simplify dumping memory for Save
// Game Copier.
//


SLINGA_ERROR RAM_RegisterHandler(DEVICE_TYPE type, PDEVICE_HANDLER* device_handler);
SLINGA_ERROR RAM_Init(DEVICE_TYPE device_type);
SLINGA_ERROR RAM_Fini(DEVICE_TYPE device_type);

SLINGA_ERROR RAM_GetDeviceName(DEVICE_TYPE, char** device_name);
SLINGA_ERROR RAM_IsPresent(DEVICE_TYPE type);
SLINGA_ERROR RAM_IsReadable(DEVICE_TYPE type);
SLINGA_ERROR RAM_IsWriteable(DEVICE_TYPE type);
SLINGA_ERROR RAM_Stat(DEVICE_TYPE device_type, PBACKUP_STAT stat);
SLINGA_ERROR RAM_List(DEVICE_TYPE device_type, FLAGS flags, PSAVE_METADATA saves, unsigned int num_saves, unsigned int* saves_found);
SLINGA_ERROR RAM_QueryFile(DEVICE_TYPE device_type, FLAGS flags, const char* filename, PSAVE_METADATA save);
SLINGA_ERROR RAM_Read(DEVICE_TYPE device_type, FLAGS flags, const char* filename, unsigned char* buffer, unsigned int size, unsigned int* bytes_read);
SLINGA_ERROR RAM_Write(DEVICE_TYPE device_type, FLAGS flags, const char* filename, const unsigned char* buffer, unsigned int size);
SLINGA_ERROR RAM_Delete(DEVICE_TYPE device_type, FLAGS flags, const char* filename);
SLINGA_ERROR RAM_Format(DEVICE_TYPE device_type);

#endif
