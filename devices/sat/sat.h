/** @file sat.h
 *
 *  @author Slinga
 *  @brief Saturn Allocation Table (SAT) parsing. Shared by internal, cartridge, and Action Replay
 *  @bug No known bugs.
 */
#pragma once

#include "../../libslinga/libslinga_conf.h"

#include "../../libslinga.h"
#include "../../libslinga/saturn.h"

//
// SAT structures
//

#define SAT_MAX_SAVE_NAME       11
#define SAT_MAX_SAVE_COMMENT    10

#define SAT_START_BLOCK_TAG     0x80000000 // beginning of a save must start with this
#define SAT_CONTINUE_BLOCK_TAG  0x0        // all other blocks have a 0 tag

#define SAT_TAG_SIZE sizeof(((SAT_START_BLOCK_HEADER *)0)->tag)

// struct at the beginning of a save block
#pragma pack(1)
typedef struct _SAT_START_BLOCK_HEADER
{
    unsigned int tag;
    char savename[SAT_MAX_SAVE_NAME];   // not necessarily NULL terminated
    unsigned char language;             // language of the save (LANGUAGE_JAPANESE (0) to LANGUAGE_ITALIAN (5))
    char comment[SAT_MAX_SAVE_COMMENT]; // not necessarily NULL terminated
    unsigned int timestamp;             // seconds since 1/1/1980
    unsigned int data_size;              // in bytes
}SAT_START_BLOCK_HEADER, *PSAT_START_BLOCK_HEADER;
#pragma pack()

#define MIN_BLOCK_SIZE (64)
#define INTERNAL_MAX_BLOCKS (512)
#define CARTRIDGE_MAX_BLOCKS (4096) // 32 Mb Cartridge
#define ACTION_REPLAY_MAX_BLOCKS (8192)
#define SAT_MAX_BITMAP (ACTION_REPLAY_MAX_BLOCKS / 8) // Each bit represents a block

#define BACKUP_RAM_FORMAT_STR "BackUpRam Format"
#define BACKUP_RAM_FORMAT_STR_LEN 16

SLINGA_ERROR sat_get_used_blocks(const PPARTITION_INFO partition_info, unsigned int* used_blocks);

SLINGA_ERROR sat_list_saves(const PPARTITION_INFO partition_info,
                            PSAVE_METADATA saves,
                            unsigned int num_saves,
                            unsigned int* saves_available);

SLINGA_ERROR sat_query_file(const char* filename,
                            const PPARTITION_INFO partition_info,
                            PSAVE_METADATA metadata);

SLINGA_ERROR sat_read(const char* filename,
                      unsigned char* buffer,
                      unsigned int size,
                      unsigned int* bytes_read,
                      const PPARTITION_INFO partition_info);

SLINGA_ERROR sat_write(FLAGS flags,
                       const char* filename,
                       const PSAVE_METADATA save_metadata,
                       const unsigned char* buffer,
                       unsigned int size,
                       const PPARTITION_INFO partition_info);

SLINGA_ERROR sat_delete(const char* filename,
                        FLAGS flags,
                        const PPARTITION_INFO partition_info);

SLINGA_ERROR sat_check_formatted(const PPARTITION_INFO partition_info);

SLINGA_ERROR sat_format(const PPARTITION_INFO partition_info);
