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

    jo_printf(2, 2, "libslinga v%d.%d.%d", major, minor, patch);
    jo_printf(2, 3, "List Devices Demo");

    jo_printf(2, 5, "ID Present Name");

    // loop through all devices
    for(int i = 0; i < MAX_DEVICE_TYPE; i++)
    {
        char* device_name = NULL;
        char present = '\0';

        // device id
        // see device.h DEVICE_TYPE enum
        jo_printf(2, 6 + i, "%d", i);

        result = Slinga_GetDeviceName(i, &device_name);
        if(result != SLINGA_SUCCESS)
        {
            // device name not found
            // usually due to being compiled out
            continue;
        }

        // we know about the device (support compiled in), 
        // check if it is present
        result = Slinga_IsPresent(i);
        if(result != SLINGA_SUCCESS)
        {
            present = 'N';
        }
        else
        {
            present = 'Y';
        }

        // device name and if is present
        jo_printf(8, 6 + i, "%c", present);
        jo_printf(13, 6 + i, "%s", device_name);
    }

    // call Slinga_Fini() if/when you are unloading and don't need libslinga anymore
    // usually better to not unload unless you know what you are doing
    // Slinga_Fini();

	jo_core_run();
}

/*
** END OF FILE
*/
