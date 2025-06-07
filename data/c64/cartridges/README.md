# C64 Cartridges Directory

This directory contains Commodore 64 cartridge ROM images.

## Supported Formats

- `.crt` - CRT cartridge format (preferred, includes header with cartridge type info)
- `.bin` - Raw binary ROM dumps
- `.rom` - Raw ROM files

## Common Cartridge Types

### 8KB Cartridges (Normal/HIROM)
- Located at $8000-$9FFF or $A000-$BFFF
- Examples: Pac-Man, Frogger, Jupiter Lander

### 16KB Cartridges  
- Located at $8000-$BFFF
- Examples: Gorf, Wizard of Wor

### Ultimax Cartridges
- Special format used by VIC-1001/Max Machine
- Examples: Avenger, Road Race

### Banking Cartridges
- Support multiple ROM banks via bank switching
- Examples: Ocean, System 3, Funplay

## File Naming

Use descriptive names that include:
- Game/software name
- Publisher (optional)
- Version/revision (if applicable)
- Cartridge type (if known)

Examples:
- `pac-man-atarisoft.crt`
- `gorf-8kb.bin`
- `multimax-ultimax.crt`

## Legal Notice

Cartridge ROM images are copyrighted material and must be obtained legally from cartridges you own.
