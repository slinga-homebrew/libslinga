# libslinga: Saturn LIbrary for Navigating Gaming Achievements #

Early WIP, probably usable, requires more testing. **BACKUP YOUR SAVES FIRST**

Sega Saturn library for reading\writing save game files. The goals are:
- work on real hardware
- support as many backup devices as possible (internal memory, cartridge, floppy, ODEs, etc)
- provide a shim layer for replacing Sega's BUP library
- MIT license for easy integration with Jo Engine, YAUL, and/or any other Saturn frameworks. MIT licensing will allow commercial products (such as ODE menus) to use the library as well 

## Supported Devices ##

The following devices are planned to be supported. I am open to adding any device as long as the code contributed to libslinga is MIT. 

|Device|Read Support|Write Support|Notes|
|---|:---:|:---:|---|
|Internal Memory|:heavy_check_mark:|:heavy_check_mark:||
|Cartridge Memory|:heavy_check_mark:|:heavy_check_mark:||
|Serial Link||||
|RAM|Partially Implemented|Partially Implemented|Helper device used to read\dump memory for Save Game Copier. Not the same as Internal\Cartridge memory|
|CD||||
|Action Replay Plus Cartridge|:heavy_check_mark:|| Read only support checked in. Requires Action Replay Plus (with 1 and/or 4MB RAM expansion). Write support seems really hard so no plan at the moment...|
|Satiator ODE|||Current code is not MIT|
|MODE ODE|||Current code is not MIT|
|Fenrir ODE|||Need library from developer|
|Phoebe/Rhea ODE|||Phoebe/Rhea do not currently support writing to the SD card AFAIK. Need support from developer|

## Shim Layer ##

For the shim layer I'm envisioning replacing the BUP library pointer at 0x06000354 with the shim. This would require an AR Cheat Code functionality. Once shimmed:
- Cartridge support could be replaced seamlessly with floppy or ODE support
- No limit to cartridge storage space. The support sizes of cartridges are stored in the BUP library. Replacing BUP lib means we can do whatever want. 
- Development of Pseudo Saturn Kai or similar cart that can support both ram expansion and direct save. Honestly not sure if this is already a thing. 

## Documentaiton ##
libslinga makes use of Doxygen. Run "doxygen Doxyfile" to build the documentation. 

## Demos ##
The demos are built with Jo Engine. See [Samples](samples/) dir.

## Credits ## 
- Knight0fDragon - advice
- Antime - advice
- Cafe-Alpha - BUP file format
- Previous research: https://segaxtreme.net/threads/decompilation-of-backup-library.25353/
- Professor Abrassive - Satiator lib
- Jo Engine - used to build the examples
