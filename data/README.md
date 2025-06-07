# Data Directory

This directory contains system-specific data files for the emulator, including ROMs, cartridges, and other binary files.

## Directory Structure

### C64 (Commodore 64)
- `c64/roms/` - System ROMs (BASIC, KERNAL, CHARGEN)
- `c64/cartridges/` - Cartridge ROM images (.crt, .bin)
- `c64/disk-images/` - Disk images (.d64, .d71, .d81)
- `c64/tape-images/` - Tape images (.tap, .t64)

### VIC-20 (Future Support)
- `vic20/roms/` - VIC-20 system ROMs
- `vic20/cartridges/` - VIC-20 cartridge images

### PET (Future Support)
- `pet/roms/` - PET system ROMs
- `pet/cartridges/` - PET cartridge images

### Plus/4 (Future Support)
- `plus4/roms/` - Plus/4 system ROMs
- `plus4/cartridges/` - Plus/4 cartridge images

## ROM Files

### Required C64 ROMs
The emulator requires the following ROM files to function properly:

1. **BASIC ROM** (`basic.rom` or `901226-01.bin`) - 8KB
   - Commodore BASIC V2 interpreter
   - Located at $A000-$BFFF in memory map

2. **KERNAL ROM** (`kernal.rom` or `901227-03.bin`) - 8KB
   - Operating system kernel
   - Located at $E000-$FFFF in memory map

3. **CHARACTER ROM** (`char.rom` or `901225-01.bin`) - 4KB
   - Character set data for VIC-II
   - Located at $D000-$DFFF when CHARGEN is enabled

### Optional Files
- **1541 DOS ROM** (`1541.rom`) - For accurate 1541 floppy drive emulation
- **Cartridge ROMs** - Various cartridge images in .crt or .bin format

## Legal Notice

ROM files are copyrighted material and are not included with this emulator. 
You must obtain these files legally from:
- Original hardware you own
- Licensed ROM collections
- Legal ROM archives

## File Naming Conventions

### System ROMs
- Use descriptive names: `basic.rom`, `kernal.rom`, `char.rom`
- Or use original part numbers: `901226-01.bin`, `901227-03.bin`, `901225-01.bin`

### Cartridges
- Use descriptive names with .crt extension for CRT format
- Use .bin extension for raw binary dumps
- Include version/revision in filename if applicable

### Disk Images
- Use .d64 for standard 1541 disk images
- Use .d71 for 1571 disk images  
- Use .d81 for 1581 disk images

### Tape Images
- Use .tap for standard tape images
- Use .t64 for T64 tape file collections
