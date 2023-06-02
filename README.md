# libslinga: Saturn LIbrary for Navigating Gaming Achievements #

Very early WIP, not useable. 

Sega Saturn library for reading\writing save game files. The goals are:
- work on real hardware
- support as many backup devices as possible (internal memory, cartridge, floppy, ODEs, etc)
- provide a shim layer for replacing Sega's BUP library
- MIT license for easy integration with Jo Engine, YAUL, and/or any other Saturn frameworks. ODE menus could also easily use this library

## Supported Devices ##

The following devices are planned to be supported. I am open to adding any device as long as the code is MIT. 

|Device|Read Support|Write Support|Notes|
|---|:---:|---|---|
|Internal Memory|Planned|Planned||
|Cartridge Memory|Planned|Planned||
|Serial Link|Planned|Planned||
|RAM|Planned|Planned||
|CD|Planned|Planned||
|Action Replay Cartridge|*Partially Implemented|Planned||
|Satiator ODE|Planned|Planned||
|MODE ODE|Planned|Planned||

## Shim Layer ##

For the shim layer I'm envisioning replacing the BUP library pointer at 0x06000354 with the shim. This would require an AR Cheat Code functionality. Once shimmed:
- Cartridge support could be replaced seamlessly with floppy or ODE support
- No limit to cartridge storage space. The support sizes of cartridges are stored in the BUP library. Replacing BUP lib means we can do whatever want. 
- Development of Pseudo Saturn Kai or similar cart that can support both ram expansion and direct save. Honestly not sure if this is already a thing. 

## Documentaiton ##
libslinga makes use of Doxygen. Run "doxygen Doxyfile" to build the documentation. 

## Demos ##
The demos are built with Jo Engine. See [Samples dir](samples/).

## Credits## 
- Knight0fDragon - advice
- Antime - advice
- Cafe-Alpha - BUP file format
- Previous research: https://segaxtreme.net/threads/decompilation-of-backup-library.25353/
- Professor Abrassive - Satiator lib
- Jo Engine - used to build the examples

