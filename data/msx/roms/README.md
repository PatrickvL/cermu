# MSX ROM Files

MSX systems require a BIOS+BASIC ROM to boot. These ROMs are copyrighted
by Microsoft (BASIC) and the respective hardware manufacturers (BIOS),
and are **not** included in this repository.

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

## Missing Files

| Filename | Size | Description |
|----------|------|-------------|
| `msx.rom` | 32 KB | **MISSING** — MSX1 BIOS + BASIC |
| `msx2.rom` | 32 KB | **MISSING** — MSX2 BIOS + BASIC |
| `msx2p.rom` | 32 KB | **MISSING** — MSX2+ BIOS + BASIC |

All ROM files must be obtained from the sources below. C-BIOS is a
free open-source alternative for MSX1.

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
