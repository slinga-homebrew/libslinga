/** @file sat.c
 *
 *  @author Slinga
 *  @brief Saturn Allocation Table (SAT) parsing. Shared by internal, cartridge, and Action Replay
 *  @bug No known bugs.
 */
#include "sat.h"

#include <jo/jo.h>

// given a save size in bytes, calculate how many save blocks are required
// block size is the size of the block including the 4 byte tag field
SLINGA_ERROR calc_num_blocks(unsigned int save_size, unsigned int block_size, unsigned int* num_save_blocks)
{
    unsigned int total_bytes = 0;
    unsigned int fixed_bytes = 0;
    unsigned int num_blocks = 0;
    unsigned int num_blocks2 = 0;

    if(!save_size || !num_save_blocks || !block_size)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    // block size must be 64-byte aligned
    if((block_size % 0x40) != 0)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    //
    // The stored save consists of:
    // - the save metadata header
    // - the variable length SAT table
    // - the save data itself
    //
    // What makes this tricky to compute is that the SAT table itself is stored
    // on the blocks
    //

    fixed_bytes = sizeof(SAT_START_BLOCK_HEADER) - SAT_TAG_SIZE + save_size;

    // calculate the number of SAT table entries required
    num_blocks = fixed_bytes / (block_size - SAT_TAG_SIZE);

    // +1 for 0x0000 SAT table terminator
    total_bytes = fixed_bytes + ((num_blocks + 1) * sizeof(unsigned short));

    // we need to do this in a loop because it's possible that by adding a SAT
    // table entry we increased the number of bytes we need such that we need
    // yet another SAT table entry.
    do
    {
        num_blocks = num_blocks2;
        total_bytes = fixed_bytes + ((num_blocks + 1) * sizeof(unsigned short));
        num_blocks2 = total_bytes / (block_size - SAT_TAG_SIZE);

        if(total_bytes % (block_size - SAT_TAG_SIZE))
        {
            num_blocks2++;
        }

    }while(num_blocks != num_blocks2);

    *num_save_blocks = num_blocks;

    return SLINGA_SUCCESS;
}

SLINGA_ERROR sat_get_used_blocks(const unsigned char* partition_buf,
                                 unsigned int partition_size,
                                 unsigned int block_size,
                                 unsigned char skip_bytes,
                                 unsigned int* used_blocks)
{
    return  sat_walk_partition(partition_buf,
                               partition_size,
                               block_size,
                               skip_bytes,
                               NULL,
                               0,
                               NULL,
                               used_blocks);
}

SLINGA_ERROR sat_list_saves(const unsigned char* partition_buf,
                            unsigned int partition_size,
                            unsigned int block_size,
                            unsigned char skip_bytes,
                            PSAVE_METADATA saves,
                            unsigned int num_saves,
                            unsigned int* saves_available)
{
    // TODO: implement
    return  sat_walk_partition(partition_buf,
                               partition_size,
                               block_size,
                               skip_bytes,
                               NULL,
                               0,
                               NULL,
                               NULL);
}

// find all saves in the partition
// todo: allow to query just num saves, used_bytes, etc
SLINGA_ERROR sat_walk_partition(const unsigned char* partition_buf,
                                unsigned int partition_size,
                                unsigned int block_size,
                                unsigned char skip_bytes,
                                PSAVE_METADATA saves,
                                unsigned int num_saves,
                                unsigned int* saves_available, 
                                unsigned int* used_blocks)
{
    PSAT_START_BLOCK_HEADER metadata = NULL;
    unsigned int saves_found = 0;
    unsigned int blocks_found = 0;
    unsigned int save_blocks = 0;
    SLINGA_ERROR result = 0;

    if(!partition_buf || !partition_size || !block_size)
    {
        jo_core_error("Buffer can't be NULL");
        return -1;
    }

    if(block_size > partition_size || (partition_size % block_size) != 0)
    {
        jo_core_error("Invalid partition size\n");
        return -2;
    }

    for(unsigned int i = (2 * block_size); i < partition_size; i += block_size)
    {
        metadata = (PSAT_START_BLOCK_HEADER)(partition_buf + i);

        // validate range
        if((unsigned char*)metadata < partition_buf || (unsigned char*)metadata >= partition_buf + partition_size)
        {
            jo_core_error("%x %x %x\n", partition_buf, metadata, partition_size);
            return -3;
        }

        // every save starts with a tag
        if(metadata->tag == SAT_START_BLOCK_TAG)
        {
            result = calc_num_blocks(metadata->saveSize, block_size, &save_blocks);
            if(result != SLINGA_SUCCESS)
            {
                // TODO: handle this error
                continue;
            }

            blocks_found += save_blocks;

            //jo_core_error("%d %d %d", saves_found, metadata->saveSize, save_blocks);

            if(saves)
            {
                // check if we are finished looking for saves
                if(saves_found >= num_saves)
                {
                    // no more room in our saves array
                    return SLINGA_BUFFER_TOO_SMALL;
                    break;
                }


                /*
                memcpy(saves[savesFound].name, metadata->saveName, MAX_SAVE_FILENAME - 1);
                saves[savesFound].name[MAX_SAVE_FILENAME -1] = '\0';

                snprintf(saves[savesFound].filename, MAX_FILENAME - 1, "%s.BUP", saves[savesFound].name);
                saves[savesFound].filename[MAX_FILENAME -1] = '\0';

                // langugae
                saves[savesFound].language = metadata->language;
                memcpy(saves[savesFound].comment, metadata->comment, MAX_SAVE_COMMENT - 1);
                saves[savesFound].date = metadata->date;
                saves[savesFound].datasize = metadata->saveSize;

                // blocksize isn't needed
                saves[savesFound].blocksize = 0;
                */
            }

            saves_found++;

            //sgc_core_error("%s %d %s %x %d", savename, language, comment, date, saveSize);


        }
    }

    if(saves_available)
    {
        *saves_available = saves_found;
    }

    if(used_blocks)
    {
        *used_blocks = blocks_found;
    }

    return SLINGA_SUCCESS;
}
