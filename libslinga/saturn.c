/** @file saturn.c
 *
 *  @author Slinga
 *  @brief Sega Saturn specific functions
 *  @bug No known bugs.
 */
#include "saturn.h"

/**
 * @brief Convert backup date to backup timestamp (seconds since 1980)
 *
 * @param[in] date - filled out backup_date structure containing date to convert
 * @param[out] timestamp - seconds since 1980 on success. This is the value used by the BUP header and saves
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
            // we know about this cartridge types
            return SLINGA_SUCCESS;
        

        default:
            // none-zero but we don't know what it is
            *cart_type = CARTRIDGE_UNKNOWN;
            return SLINGA_UNKNOWN_CARTRIDGE;
    }

    return SLINGA_SUCCESS;
}
