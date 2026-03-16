# PET ROM Files

ROM files required by the Commodore PET emulation (PET 4032 target).

## Expected ROM Files

| File | Part Number | Size | Address | Description |
|------|-------------|------|---------|-------------|
| `characters-2.901447-10.bin` | 901447-10 | 2 KB | Character generator | PET 4032 character ROM |
| `basic-4.901465-23-20-21.bin` | 901465-23/20/21 | 12 KB | $B000-$DFFF | BASIC 4.0 (combined) |
| `edit-4-40-n-50Hz.901498-01.bin` | 901498-01 | 2 KB | $E000-$E7FF | Editor ROM (40-col, PAL) |
| `edit-4-40-n-60Hz.901499-01.bin` | 901499-01 | 2 KB | $E000-$E7FF | Editor ROM (40-col, NTSC) |
| `kernal-4.901465-22.bin` | 901465-22 | 4 KB | $F000-$FFFF | KERNAL ROM (BASIC 4.0) |

## Alternative Filenames

The ROM loader also accepts these alternative names:
- Character ROM: `chargen`, `chargen.rom`, `901447-10.bin`, `characters.901640-01.bin` (4KB 8032 version)
- BASIC ROM: individual chips `901465-23.bin` ($B000), `901465-20.bin` ($C000), `901465-21.bin` ($D000)
- Kernal ROM: `kernal4.rom`, `kernal.rom`, `901465-22.bin`

## Where to Obtain

These ROMs are copyrighted by Commodore and are **not** included in this repository.

- **Cloanto C64 Forever** — officially licensed ROM collections (includes PET)
  - https://www.c64forever.com/
- **VICE emulator** — ships ROMs under license from Cloanto (`data/PET/`)
  - https://vice-emu.sourceforge.io/
