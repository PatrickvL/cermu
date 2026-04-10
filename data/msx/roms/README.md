# MSX ROM Files

MSX systems require a BIOS+BASIC ROM to boot.

## Included ROMs (C-BIOS — Free Alternative)

This directory includes **C-BIOS** (v0.29a), a free open-source MSX BIOS
replacement licensed under the 2-clause BSD license. C-BIOS supports most
ROM-based games but does **not** include BASIC or cassette/disk support.

| Filename | Size | Source | Description |
|----------|------|--------|-------------|
| `msx.rom` | 32 KB | C-BIOS 0.29a | MSX1 BIOS (international) |
| `msx2.rom` | 32 KB | C-BIOS 0.29a | MSX2 BIOS (international) |
| `msx2p.rom` | 32 KB | C-BIOS 0.29a | MSX2+ BIOS (international) |

C-BIOS project: http://cbios.sourceforge.net/

## Required ROMs

### MSX1

| Filename | Size | Description |
|----------|------|-------------|
| `msx.rom` | 32 KB | MSX1 BIOS + BASIC ROM |

### MSX2

| Filename | Size | Description |
|----------|------|-------------|
| `msx2.rom` | 32 KB | MSX2 BIOS + BASIC ROM |

### MSX2+

| Filename | Size | Description |
|----------|------|-------------|
| `msx2p.rom` | 32 KB | MSX2+ BIOS + BASIC ROM |

## Alternative Filenames Accepted

- **MSX1:** `msx.rom`, `msx1.rom`, `MSX.ROM`
- **MSX2:** `msx2.rom`, `MSX2.ROM`
- **MSX2+:** `msx2p.rom`, `MSX2P.ROM`, `msx2+.rom`

All variants share this directory. The emulator selects the correct ROM
based on the system variant chosen at startup.

## Original ROMs (Not Included)

For full BASIC and disk/cassette support, replace the C-BIOS files with
original MSX BIOS+BASIC ROMs from your hardware or licensed sources:

## Where to Obtain

- **MAME ROM set** — `msx.zip`, `msx2.zip` etc. contain BIOS images
  for various MSX manufacturers
- **openMSX emulator** — includes open-source C-BIOS as a free alternative
  - https://openmsx.org/
  - C-BIOS: http://cbios.sourceforge.net/
- **Dump from original hardware** — if you own an MSX computer

## Notes

Different MSX manufacturers (Sony, Panasonic, Philips, etc.) shipped
different BIOS ROMs. Any standard MSX1/2/2+ BIOS should work. The
open-source **C-BIOS** project provides free replacement ROMs with
partial BASIC compatibility.
