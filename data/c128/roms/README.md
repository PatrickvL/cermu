# Commodore 128 ROM Files

ROMs required by the Commodore 128 emulation.

## Required ROMs

| Chip | Part # | Size | Address | Description |
|------|--------|------|---------|-------------|
| BASIC lo  | 318018-04 | 16 KB | $4000–$7FFF | BASIC 7.0 low half |
| BASIC hi  | 318019-04 | 16 KB | $8000–$BFFF | BASIC 7.0 high half |
| Kernal    | 318020-05 |  8 KB | $E000–$FFFF | C128 KERNAL |
| Editor    | 318020-05 |  4 KB | $C000–$CFFF | Screen editor (first 4 KB of kernal chip) |
| Character | 390059-01 |  8 KB | $D000       | Character generator (upper+lower case + VDC) |

The editor and kernal share one physical 16 KB ROM chip (318020-05).
The loader accepts the combined file and slices it via `@offset` syntax.

## Files in This Directory

| Filename | Size | Notes |
|----------|------|-------|
| `basic-4000.318018-04.bin` | 16 KB | BASIC 7.0 low |
| `basic-8000.318019-04.bin` | 16 KB | BASIC 7.0 high |
| `kernal.318020-05.bin`     | 16 KB | Combined editor (4 KB) + kernal (8 KB) |
| `characters.390059-01.bin` |  8 KB | Character ROM |

## Alternative Filenames Accepted

The ROM loader tries each alternative in order:

- **BASIC lo:** `basic-4000.318018-04.bin`, `c128_basic_lo.rom`, `basic_lo.rom`, `basiclo.rom`, `basic.318023-02.bin@0`
- **BASIC hi:** `basic-8000.318019-04.bin`, `c128_basic_hi.rom`, `basic_hi.rom`, `basichi.rom`, `basic.318023-02.bin@16384`
- **Editor:** `c128_editor.rom`, `editor.rom`, `kernal.318020-05.bin@0`
- **Character ROM:** `characters.390059-01.bin`, `c128_chargen.rom`, `chargen.rom`, `characters.rom`
- **Kernal:** `c128_kernal.rom`, `kernal.rom`, `kernal.318020-05.bin@8192`

## Where to Obtain

These ROMs are copyrighted by Commodore and are **not** included in this
repository. To obtain them legally:

- **Cloanto C64 Forever** — officially licensed ROM collection (includes C128)
- **VICE emulator** — ships ROMs under license from Cloanto (`data/C128/`)
- **Dump from original hardware** — if you own a Commodore 128

## Compatibility with C64 ROMs

The C128 uses **entirely different** ROMs from the C64.  C64 ROM files
(BASIC 2.0, KERNAL, 4 KB character ROM) cannot be used here.  The C128
contains its own C64-mode compatibility code within the 318020-05 kernal
chip — no separate C64 ROMs are needed.

## Hardware Notes

The Commodore 128 (1985) was Commodore's successor to the C64:

- **CPU:** CSG 8502 (6502-compatible) @ 1/2 MHz + Zilog Z80 @ 4 MHz
- **Video:** MOS 8564/8566 VIC-IIe (40-col) + MOS 8563 VDC (80-col, 16 KB VRAM)
- **Sound:** MOS 6581 or 8580 SID
- **I/O:** 2× MOS 6526 CIA
- **MMU:** MOS 8722 (bank switching, mode control)
- **RAM:** 128 KB main + 16 KB VDC VRAM
- **ROM:** 64 KB total (BASIC 7.0, KERNAL, character ROM, editor)

Three operating modes:
- **C128 mode:** Native mode with BASIC 7.0 and 128 KB RAM
- **C64 mode:** Full hardware compatibility with Commodore 64
- **CP/M mode:** Z80 CPU running CP/M 3.0

## ROM Layout Notes

The C128 uses a split ROM layout across multiple physical chips:

| Chip | Part # | Capacity | Contents |
|------|--------|----------|----------|
| U33  | 318018-04 | 16 KB | BASIC 7.0 low  ($4000) |
| U34  | 318019-04 | 16 KB | BASIC 7.0 high ($8000) |
| U35  | 318020-05 | 16 KB | Editor ($C000, 4 KB) + KERNAL ($E000, 8 KB) |
| U18  | 390059-01 |  8 KB | Character ROM (two 4 KB banks: upper/gfx + lower) |

VICE distributes these as `basiclo`, `basichi`, `kernal`, `editor`, and
`chargen`.  Some ROM sets combine BASIC lo + hi into a single 32 KB file
(`basic.318023-02.bin`) — the loader accepts this via `@offset` slicing.
