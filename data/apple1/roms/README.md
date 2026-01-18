# Apple 1 ROM Files

This directory contains ROM files needed for the Apple 1 emulator.

## Required ROMs

### 1. Woz Monitor ROM (256 bytes)
The Woz Monitor is the basic operating system/debugger for the Apple 1.

**Download commands:**
```bash
# Option 1: From archive.org (Apple 1 collection)
cd data/apple1/roms
wget https://archive.org/download/AppleWozMonitor/apple1.rom -O monitor.rom

# Option 2: Extract from MAME ROMs (if you have them)
# File: apple1.zip contains "apple1.ic2" (the monitor ROM)

# Option 3: Manual hex entry (if needed)
# The Woz Monitor ROM starts at $FF00 in memory
```

### 2. Signetics 2513 Character Generator ROM (512 bytes)
Provides the authentic Apple 1 character set (uppercase only, distinctive @ symbol, etc.)

**Download commands:**
```bash
cd data/apple1/roms

# Option 1: From MAME ROM set
# Extract "342-0036.c1" from apple1.zip
wget https://github.com/mamedev/mame/raw/master/roms/apple1/342-0036-00.c1 -O 2513.rom

# Option 2: From archive.org
wget https://archive.org/download/AppleCharROM/signetics2513.bin -O 2513.rom

# Option 3: If you have MAME apple1.zip locally:
unzip -p apple1.zip 342-0036-00.c1 > 2513.rom
```

### 3. Apple 1 BASIC (Optional, 4KB)
Steve Wozniak's Integer BASIC for the Apple 1.

**Download commands:**
```bash
cd data/apple1/roms

# From various Apple 1 emulator projects
wget https://raw.githubusercontent.com/brouhaha/a1asm/master/basic.bin -O basic.rom
```

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