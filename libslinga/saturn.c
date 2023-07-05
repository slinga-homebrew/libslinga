/** @file saturn.c
 *
 *  @author Slinga
 *  @brief Sega Saturn specific functions
 *  @bug No known bugs.
 */
#include "saturn.h"

/**
 * @brief Check what type of cartridge is connected
 *
 * @param[out] cart_type - a cartridge in the SATURN_CARTRIDGE_TYPE enum on success
 *
 * @return SLINGA_SUCCESS on success
 */
SLINGA_ERROR get_cartridge_type(SATURN_CARTRIDGE_TYPE* cart_type)
{
    if(!cart_type)
    {
        return SLINGA_INVALID_PARAMETER;
    }

    *cart_type = CARTRIDGE_TYPE_MAGIC;
    if(*cart_type == CARTRIDGE_NONE)
    {
        // no cartridge found        
        return SLINGA_DEVICE_NOT_PRESENT;
    }

    // found a cartridge, validate it
    switch(*cart_type)
    {
        case CARTRIDGE_RAM_1MB:
        case CARTRIDGE_RAM_4MB:
            // we know about this cartridge type
            return SLINGA_SUCCESS;

        default:
            break;
    }

    //
    // we can't just use a switch statement as the backup
    // cartridges don't look at all of the bits
    //
    if((*cart_type & CARTRIDGE_BACKUP_MAGIC_MASK) == 0x20)
    {
        unsigned int cart_subtype = 0;

        cart_subtype = *cart_type & 0x7;

        if(cart_subtype == 0 || cart_subtype == 1)
        {
            *cart_type = CARTRIDGE_BACKUP_400_200_512K;
            return SLINGA_SUCCESS;
        }
        else if(cart_subtype == 2)
        {
            *cart_type = CARTRIDGE_BACKUP_800_200_1MB;
            return SLINGA_SUCCESS;
        }
        else if(cart_subtype == 3)
        {
            *cart_type = CARTRIDGE_BACKUP_1000_200_2MB;
             return SLINGA_SUCCESS;
        }
        else if(cart_subtype == 4)
        {
            *cart_type = CARTRIDGE_BACKUP_1000_400_4MB;
             return SLINGA_SUCCESS;
        }
    }

    // none-zero but we don't know what it is
    *cart_type = CARTRIDGE_UNKNOWN;
    return SLINGA_UNKNOWN_CARTRIDGE;
}
