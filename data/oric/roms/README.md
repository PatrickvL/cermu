# Oric-1 / Oric Atmos ROM Files

ROMs required by the Oric-1 and Oric Atmos emulation.

## Required ROMs

### Oric-1

| Filename | Size | Description |
|----------|------|-------------|
| `oric1.rom` | 16 KB | Oric-1 ROM — BASIC 1.0 + OS ($C000–$FFFF) |

### Oric Atmos

| Filename | Size | Description |
|----------|------|-------------|
| `atmos.rom` | 16 KB | Oric Atmos ROM — BASIC 1.1 + OS ($C000–$FFFF) |

## Alternative Filenames Accepted

- **Oric-1:** `oric1.rom`, `basic10.rom`, `BASIC10.ROM`
- **Atmos:** `atmos.rom`, `basic11.rom`, `BASIC11.ROM`

## Missing Files

| Filename | Size | Description |
|----------|------|-------------|
| `oric1.rom` | 16 KB | **MISSING** — Oric-1 BASIC 1.0 + OS |
| `atmos.rom` | 16 KB | **MISSING** — Oric Atmos BASIC 1.1 + OS |

Both ROM files must be obtained from one of the sources below.

## Where to Obtain

These ROMs are copyrighted by Tangerine Computer Systems / Oric International
and are **not** included in this repository. To obtain them legally:

- **Dump from original hardware** — if you own an Oric-1 or Atmos
- **Defence Force** — Oric community and preservation project
  - https://defence-force.org/
- **MAME ROM set** — `oric1.zip` / `orica.zip` contain the required ROMs
- **Oricutron** — Oric emulator which documents ROM requirements
  - https://github.com/pete-gordon/oricutron

## Hardware Notes

The Oric-1 (1983) and Oric Atmos (1984) are MOS 6502-based home computers
manufactured by Oric International (formerly Tangerine Computer Systems):

- **CPU:** MOS 6502 @ 1 MHz
- **Video:** Custom Oric ULA — text 40×28, hi-res 240×200, 8 colors
- **Sound:** General Instrument AY-3-8912 PSG (3 channels)
- **I/O:** MOS 6522 VIA (timers, cassette, printer)
- **RAM:** 48KB ($0000–$BFFF)
- **ROM:** 16KB ($C000–$FFFF) — BASIC + OS

The Atmos improved the keyboard (full-travel keys) and fixed several
BASIC 1.0 bugs. The ROM is the only hardware difference.
