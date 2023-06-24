/** @file sat.h
 *
 *  @author Slinga
 *  @brief Saturn Allocation Table (SAT) parsing. Shared by internal, cartridge, and Action Replay
 *  @bug No known bugs.
 */
#pragma once


#include "../libslinga/libslinga_conf.h"

#include "../libslinga.h"
#include "../libslinga/saturn.h"


//
// SAT structures
//

//
// Saturn saves are stored in 64-byte blocks
// - the first 4 bytes of each block is a tag
// -- 0x80000000 = start of new save
// -- 0x00000000 = continuation block?
// - the next 30 bytes are metadata for the save (save name, language, comment, date, and size)
// -- the size is the size of the save data not counting the metadata
// - next is a variable array of 2-byte block ids. This array ends when 00 00 is encountered
// -- the block id for the 1st block (the one containing the metadata) is not present. In this case only 00 00 will be present
// -- the variable length array can be 0-512 elements (assuming max save is on the order of ~32k)
// -- to complicate matters, the block ids themselves can extend into multiple blocks. This makes computing the block count tricky
// - following the block ids is the save data itself
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

/*
g_SAT_bitmap[] is a large bitmap used to store free\busy blocks
each bitmap represents a single block

Internal memory
- 0x8000 parition size
- 0x40 block size
- bytes needed = 0x8000 / 0x40 / 8 (bits per byte) = 0x40 (64) bytes

32 Mb Cartridge
- 0x400000 partition size
- 0x400 block size
- bytes needed - 0x400000 / 0x400 / 8 (bits per byte_ = 0x200 (512) bytes

Action Replay
- 0x80000 partition size
- 0x40 block size
- bytes needed = 0x80000 / 0x40 / 8 (bits per byte) = 0x400 (1024) bytes

This means we need a 1024 byte buffer to support the Action Replay
*/
#define MIN_BLOCK_SIZE (64)
#define INTERNAL_MAX_BLOCKS (512)
#define CARTRIDGE_MAX_BLOCKS (4096) // 32 Mb Cartridge
#define ACTION_REPLAY_MAX_BLOCKS (8192)
#define SAT_MAX_BITMAP (ACTION_REPLAY_MAX_BLOCKS / 8) // Each bit represents a block

SLINGA_ERROR sat_get_used_blocks(const unsigned char* partition_buf,
                                 unsigned int partition_size,
                                 unsigned int block_size,
                                 unsigned char skip_bytes,
                                 unsigned int* used_blocks);

SLINGA_ERROR sat_list_saves(const unsigned char* partition_buf,
                            unsigned int partition_size,
                            unsigned int block_size,
                            unsigned char skip_bytes,
                            PSAVE_METADATA saves,
                            unsigned int num_saves,
                            unsigned int* saves_available);

SLINGA_ERROR sat_query_file(const char* filename,
                            const unsigned char* partition_buf,
                            unsigned int partition_size,
                            unsigned int block_size,
                            unsigned char skip_bytes,
                            PSAVE_METADATA metadata);

SLINGA_ERROR sat_read_save(const char* filename,
                           unsigned char* buffer,
                           unsigned int size,
                           unsigned int* bytes_read,
                           const unsigned char* partition_buf,
                           unsigned int partition_size,
                           unsigned int block_size,
                           unsigned char skip_bytes);
