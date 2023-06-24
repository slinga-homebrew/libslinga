/*
** Jo Sega Saturn Engine
** Copyright (c) 2012-2017, Johannes Fetz (johannesfetz@gmail.com)
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of the Johannes Fetz nor the
**       names of its contributors may be used to endorse or promote products
**       derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL Johannes Fetz BE LIABLE FOR ANY
** DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
** ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
*/
#include <jo/jo.h>
#include "libslinga.h"

void			jo_main(void)
{
    BACKUP_STAT stat = {0};
    unsigned char major = 0;
    unsigned char minor = 0;
    unsigned char patch = 0;
    SLINGA_ERROR result = 0;
    char* device_name = NULL;
    DEVICE_TYPE device_id = DEVICE_ACTION_REPLAY; 

	jo_core_init(JO_COLOR_Black);

    // init library
    result = Slinga_Init();
    if(result != SLINGA_SUCCESS)
    {
        jo_core_error("Failed to initialize lib!!");
        return;
    }

    result = Slinga_GetVersion(&major, &minor, &patch);
    if(result != SLINGA_SUCCESS)
    {
        jo_core_error("Failed to get lib version!!");
        return;
    }

    jo_printf(2, 2, "libslinga v%d.%d.%d", major, minor, patch);
    jo_printf(2, 3, "Stat Device Demo");

    result = Slinga_GetDeviceName(device_id, &device_name);
    if(result != SLINGA_SUCCESS)
    {
        // device name not found
        // usually due to being compiled out
        jo_core_error("Failed to get device name for (%d)!!", device_id);
        return;
    }

    result = Slinga_Stat(device_id, &stat);
    if(result != SLINGA_SUCCESS)
    {
        jo_core_error("Failed to stat device!! Is it present?");
        return;
    }

    jo_printf(2, 5, "Device Name: %s", device_name);
    jo_printf(2, 6, "Device ID: %d", device_id);
    jo_printf(2, 7, "Total Bytes: %d", stat.total_bytes);
    jo_printf(2, 8, "Total Blocks: %d", stat.total_blocks);
    jo_printf(2, 9, "Block Size: %d", stat.block_size);
    jo_printf(2, 10, "Free Bytes Available: %d", stat.free_bytes);
    jo_printf(2, 11, "Free Bytes Available: %d", stat.free_blocks);
    jo_printf(2, 12, "Max Saves Possible: %d", stat.max_saves_possible);

    // call Slinga_Fini() if/when you are unloading and don't need libslinga anymore
    // usually better to not unload unless you know what you are doing
    // Slinga_Fini();

	jo_core_run();
}

/*
** END OF FILE
*/
