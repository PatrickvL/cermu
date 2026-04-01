# BBC Micro ROM Files

The BBC Micro family requires OS ROMs to boot. These ROMs are copyrighted by
Acorn Computers and are **not** included in this repository.

## Model B — Required ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `os12.rom` | 16 KB | MOS 1.20 — BBC Micro Model B operating system |

## Model B — Optional ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `basic2.rom` | 16 KB | BBC BASIC II — sideways ROM bank 15 |

## BBC B+ — Required ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `bplus_os.rom` | 16 KB | BBC B+ OS 2.0 ($C000–$FFFF) |

## BBC Master 128 — Required ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `master_mos320.rom` | 64 KB | MOS 3.20 — Master 128 OS (4 × 16 KB banks) |

## Included ROMs

| Filename | Size | Model | Description |
|----------|------|-------|-------------|
| `OS12.ROM` | 16 KB | Model B | MOS 1.20 operating system |
| `BASIC2.ROM` | 16 KB | Model B | BBC BASIC II sideways ROM |

## Missing Files

| Filename | Size | Model | Description |
|----------|------|-------|-------------|
| `bplus_os.rom` | 16 KB | B+ | **MISSING** — BBC B+ OS 2.0 |
| `master_mos320.rom` | 64 KB | Master 128 | **MISSING** — MOS 3.20 (4 × 16 KB banks) |

Only the Model B ROMs are present. B+ and Master ROMs must be obtained
separately.

## Alternative Filenames Accepted

### Model B
- **OS ROM:** `os12.rom`, `os.rom`, `OS-1.20.rom`, `MOS120.rom`, `bbc_os.rom`, `os1.2.rom`
- **BASIC ROM:** `basic2.rom`, `BASIC2.rom`, `basic.rom`, `bbc_basic.rom`, `BASIC-2.rom`

### BBC B+
- **OS ROM:** `bplus_os.rom`, `OS20.ROM`, `os20.rom`

### BBC Master 128
- **MOS ROM:** `master_mos320.rom`, `MOS320.ROM`, `mos3.20.rom`

## Where to Obtain

These ROMs are copyrighted by Acorn Computers (now ARM). To obtain them legally:

- **Dump from original hardware** — if you own a BBC Micro, B+, or Master
- **Stardot forums** — BBC Micro community, may have information on legal sources
  - https://stardot.org.uk/forums/
- **BBC Micro Bot** — educational project with ROM information
  - https://www.bbcmicrobot.com/
- **MAME ROM set** — `bbcb.zip`, `bbcbp.zip`, `bbcm.zip` contain the required ROMs

## Hardware Notes

The BBC Micro family (1981–1986) by Acorn Computers:

| Model | Year | CPU | RAM | Key Features |
|-------|------|-----|-----|-------------|
| Model B | 1981 | MOS 6502A @ 2 MHz | 32 KB | MC6845 CRTC, SN76489, 2× VIA |
| Model B+ | 1985 | WDC 65C02 @ 2 MHz | 64 KB | As Model B + extra RAM |
| Master 128 | 1986 | WDC 65C02 @ 2 MHz | 128 KB | MOS 3.20, ADFS, cartridge slots |
