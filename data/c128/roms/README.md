# Commodore 128 ROM Files

ROMs required by the Commodore 128 emulation.

## Required ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `c128_basic_lo.rom` | 16 KB | BASIC 7.0 low half ($4000–$7FFF) |
| `c128_basic_hi.rom` | 16 KB | BASIC 7.0 high half ($8000–$BFFF) |
| `c128_kernal.rom` | 8 KB | C128 KERNAL ($E000–$FFFF) |
| `c128_chargen.rom` | 4 KB | Character generator ROM |
| `c128_editor.rom` | 4 KB | Screen editor ROM ($C000–$CFFF) |

## Alternative Filenames Accepted

- **BASIC lo:** `c128_basic_lo.rom`, `basic_lo.rom`, `basiclo.rom`
- **BASIC hi:** `c128_basic_hi.rom`, `basic_hi.rom`, `basichi.rom`
- **Kernal:** `c128_kernal.rom`, `kernal.rom`
- **Character ROM:** `c128_chargen.rom`, `chargen.rom`, `characters.rom`
- **Editor:** `c128_editor.rom`, `editor.rom`

## Where to Obtain

These ROMs are copyrighted by Commodore and are **not** included in this
repository. To obtain them legally:

- **Cloanto C64 Forever** — officially licensed ROM collection (includes C128)
  - https://www.c64forever.com/
- **VICE emulator** — ships ROMs under license from Cloanto (`data/C128/`)
  - https://vice-emu.sourceforge.io/
- **Dump from original hardware** — if you own a Commodore 128

## Hardware Notes

The Commodore 128 (1985) was Commodore's successor to the C64:

- **CPU:** CSG 8502 (6502-compatible) @ 1/2 MHz + Zilog Z80 @ 4 MHz
- **Video:** MOS 8564/8566 VIC-IIe (40-col) + MOS 8563 VDC (80-col, 16KB VRAM)
- **Sound:** MOS 6581 or 8580 SID
- **I/O:** 2× MOS 6526 CIA
- **MMU:** MOS 8722 (bank switching, mode control)
- **RAM:** 128KB main + 16KB VDC VRAM
- **ROM:** 64KB (BASIC 7.0, KERNAL, character ROM, editor)

Three operating modes:
- **C128 mode:** Native mode with BASIC 7.0 and 128KB RAM
- **C64 mode:** Full hardware compatibility with Commodore 64
- **CP/M mode:** Z80 CPU running CP/M 3.0

## ROM Notes

The C128 uses a split ROM layout. VICE distributes the ROMs as:
- `basiclo` (16 KB) — BASIC 7.0 part 1
- `basichi` (16 KB) — BASIC 7.0 part 2
- `kernal` (8 KB) — C128 KERNAL
- `charg64` / `chargen` (4 KB) — character generator
- `editor` (4 KB) — screen editor

Some ROM distributions combine BASIC lo + hi into a single 32 KB file.
