/** @file saturn.h
 *
 *  @author Slinga
 *  @brief Sega Saturn specific defines and prototypes
 *  @bug No known bugs.
 */
#pragma once

#include "../libslinga.h"

#define CARTRIDGE_MEMORY 0x02000000          ///< @brief Start of cartridge address
#define CARTRIDGE_RAM_BANK_SIZE 0x80000      ///< @brief Size of cartridge RAM bank size. Safe for both 1 MB and 4 MB carts
#define CARTRIDGE_RAM_BANK_1 (unsigned char*)0x22400000           ///< @brief Start of cartridge RAM bank 1. Safe for both 1 MB and 4 MB carts
#define CARTRIDGE_TYPE_MAGIC *(volatile unsigned char*)0x24FFFFFF ///< @brief One byte value for the cartridge type
#define CARTRIDGE_BACKUP_MAGIC_MASK 0xe0

/** @brief Extended RAM cartridge type */
typedef enum
{
    CARTRIDGE_NONE = 0,               ///< @brief No cartridge persent

    CARTRIDGE_BACKUP_400_200_512K = 1,  ///< @brief Backup cart 0x400 blocks * 0x200 block size  = 512K
    CARTRIDGE_BACKUP_800_200_1MB = 2,   ///< @brief Backup cart 0x800 blocks * 0x200 block size = 1MB
    CARTRIDGE_BACKUP_1000_200_2MB = 3,  ///< @brief Backup cart 0x1000 blocks * 0x200 block size = 2MB
    CARTRIDGE_BACKUP_1000_400_4MB = 4,  ///< @brief Backup cart 0x1000 blocks * 0x400 block size = 4MB

    CARTRIDGE_RAM_1MB = 0x5a,       ///< @brief 1 MB RAM expansion
    CARTRIDGE_RAM_4MB = 0x5c,       ///< @brief 4 MB RAM expansion

    CARTRIDGE_UNKNOWN,              ///< @brief Unknown cartridge
    
} SATURN_CARTRIDGE_TYPE;

SLINGA_ERROR get_cartridge_type(SATURN_CARTRIDGE_TYPE* cart_type);
