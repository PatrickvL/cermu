# Apple 1 ROM Files

This directory contains ROM files needed for the Apple 1 emulator.

## Included ROMs

All required ROMs are included in this directory. The Woz Monitor source code
was published by Steve Wozniak and is freely available.

### 1. Woz Monitor ROM (`monitor.rom`, 256 bytes)
The original Apple 1 operating system / debugger, mapped at $FF00–$FFFF.

### 2. Signetics 2513 Character Generator ROM (`2513.rom`, 512 bytes)
Provides the authentic Apple 1 character set (uppercase only, distinctive @ symbol).

### 3. Apple 1 BASIC (`basic.rom`, 4 KB — optional)
Steve Wozniak's Integer BASIC for the Apple 1.

## File Verification

After downloading, verify file sizes:
```bash
ls -lh data/apple1/roms/

# Expected sizes:
# monitor.rom:  256 bytes (0.25 KB)
# 2513.rom:     512 bytes (0.5 KB)  
# basic.rom:    4096 bytes (4 KB)
```

## Alternative Sources

If the above links don't work, search for:
- "Apple 1 Woz Monitor ROM"
- "Signetics 2513 character ROM"
- "Apple 1 BASIC ROM"
- "MAME apple1.zip ROMs"

The ROMs are also available in most Apple 1 emulator source repositories.

## Legal Note

These ROMs are historical artifacts. The Woz Monitor and BASIC were created by Steve Wozniak.
The Signetics 2513 character ROM is a period-correct character generator chip.
These files are widely distributed for historical preservation and education purposes.