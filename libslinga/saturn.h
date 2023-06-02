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
#define CARTRIDGE_RAM_BANK_1 (unsigned char*)0x22400000      ///< @brief Start of cartridge RAM bank 1. Safe for both 1 MB and 4 MB carts
//#define CARTRIDGE_RAM_BANK_2 0x22400000      ///< @brief Start of cartridge RAM bank 1. Safe for both 1 MB and 4 MB carts
#define CARTRIDGE_TYPE_MAGIC *(volatile unsigned char*)0x24FFFFFF ///< @brief One byte value for the cartridge type

/** @brief Extended RAM cartridge type */
typedef enum
{
    CARTRIDGE_NONE = 0,        ///< @brief No cartridge persent
    CARTRIDGE_RAM_1MB = 0x5a,  ///< @brief 1 MB RAM expansion
    CARTRIDGE_RAM_4MB = 0x5c,  ///< @brief 4 MB RAM expansion
    CARTRIDGE_UNKNOWN,         ///< @brief Unknown cartridge
    
    // TODO: Add backup cart types
    // 0x21 4Mb backup
    // 0x24 32Mb backup
    
} SATURN_CARTRIDGE_TYPE;

SLINGA_ERROR get_cartridge_type(SATURN_CARTRIDGE_TYPE* cart_type);
