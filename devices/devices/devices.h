/** @file devices.h
 *
 *  @author Slinga
 *  @brief List of backup devices supported by libslinga
 *  @bug No known bugs.
 */
#pragma once

/** @brief Backup devices supported by libslinga */
typedef enum
{
    // built-in
    DEVICE_INTERNAL = 0,       ///< @brief Internal Sega Saturn built-in memory
    DEVICE_CARTRIDGE = 1,      ///< @brief Cartridge slot
    DEVICE_SERIAL = 2,         ///< @brief Serial port

    // fake devices to support Save Game Copier
    DEVICE_RAM = 3,            ///< @brief Saturn RAM, not the internal built-in memory
    DEVICE_CD = 4,             ///< @brief CD Files System

    // Special cartridges
    DEVICE_ACTION_REPLAY = 5,  ///< @brief Action Replay cartridge

    // ODEs
    DEVICE_SATIATIOR = 6,      ///< @brief SATIATOR ODE
    DEVICE_MODE = 7,           ///< @brief MODE ODE

    MAX_DEVICE_TYPE,          ///< @brief Max device value
} DEVICE_TYPE;
