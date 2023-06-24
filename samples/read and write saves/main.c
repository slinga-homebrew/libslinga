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
    unsigned char major = 0;
    unsigned char minor = 0;
    unsigned char patch = 0;
    DEVICE_TYPE device_type = DEVICE_ACTION_REPLAY;
    unsigned int saves_found = 0;
    char* device_name = NULL;
    PSAVE_METADATA saves = NULL;
    SLINGA_ERROR result = 0;

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

    result = Slinga_GetDeviceName(device_type, &device_name);
    if(result != SLINGA_SUCCESS)
    {
        jo_core_error("Failed to get device name (%d)!!", result);
        return;
    }

    jo_printf(2, 2, "libslinga v%d.%d.%d", major, minor, patch);
    jo_printf(2, 3, "Read and Write Demo");


    // call the first time to get the size
    result = Slinga_List(device_type, 0, NULL, 0, &saves_found);
    if(result != SLINGA_SUCCESS)
    {
        jo_core_error("Failed to count saves on device (%d)!!", result);
        return;
    }

    jo_printf(2, 5, "Device: %s", device_name);
    jo_printf(2, 6, "Number of saves: %d", saves_found);

    if(saves_found == 0)
    {
        jo_printf(2, 8, "No saves found, exiting");
        return;
    }

    // allocate a buffer to hold the saves
    saves = (PSAVE_METADATA)jo_malloc(sizeof(SAVE_METADATA) * saves_found);
    if(!saves)
    {
        jo_core_error("Failed to allocate saves array!!");
        return;
    }

    // call again with the appropriate size buffer
    result = Slinga_List(device_type, 0, saves, saves_found, &saves_found);
    if(result != SLINGA_SUCCESS)
    {
        jo_core_error("Failed to list saves on device (%d)!!", result);
        return;
    }

    // display the save metadata
    jo_printf(2, 8, "Save\tComment\tSize");
    for(unsigned int i = 0; i < saves_found; i++)
    {
        jo_printf(2, 8 + i, "%d) %s %s %d", i+1, saves[i].savename, saves[i].comment, saves[i].data_size);
    }

    // free saves array when no longer needed
    jo_free(saves);

    // call Slinga_Fini() if/when you are unloading and don't need libslinga anymore
    // usually better to not unload unless you know what you are doing
    // Slinga_Fini();

    jo_core_run();
}

/*
** END OF FILE
*/
