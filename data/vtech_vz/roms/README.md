# VTech VZ200 / VZ300 ROM Files

ROMs required by the VTech VZ200 and VZ300 emulation.

## Required ROMs

### VZ200 (Laser 200 / Salora Fellow)

| Filename | Size | Description |
|----------|------|-------------|
| `vz200.rom` | 16 KB | VZ200 BASIC ROM ($0000–$3FFF) |

### VZ300 (Laser 310 / Salora Fellow II)

| Filename | Size | Description |
|----------|------|-------------|
| `vz300.rom` | 16 KB | VZ300 BASIC ROM ($0000–$3FFF) |

## Alternative Filenames Accepted

- **VZ200:** `vz200.rom`, `BASIC.ROM`, `laser200.rom`
- **VZ300:** `vz300.rom`, `BASIC.ROM`, `laser310.rom`

## Where to Obtain

These ROMs are copyrighted by VTech Holdings and are **not** included in this
repository. To obtain them legally:

- **Dump from original hardware** — if you own a VZ200/300 or Laser 200/310
- **MAME ROM set** — `vz200.zip` / `laser310.zip`
- **VZ community** — preservation sites
  - https://www.vz200.org/
  - http://www.vzalive.com/

## Hardware Notes

The VTech Laser 200 / VZ200 (1983) and Laser 310 / VZ300 (1985) were
budget Z80-based home computers sold under many brand names worldwide:

| Brand Name | Region | Model |
|-----------|--------|-------|
| VTech Laser 200 | International | VZ200 |
| VTech Laser 210 | International | VZ200 (updated) |
| VTech Laser 310 | International | VZ300 |
| Dick Smith VZ200 | Australia | VZ200 |
| Dick Smith VZ300 | Australia | VZ300 |
| Salora Fellow | Finland | VZ200 |
| Salora Fellow II | Finland | VZ300 |
| Texet TX8000 | UK | VZ200 |

Hardware specifications:
- **CPU:** Zilog Z80A @ 3.58 MHz (NTSC color burst crystal)
- **Video:** Motorola MC6847 VDG — text 32×16, graphics up to 256×192
- **Sound:** 1-bit speaker (Z80 port-driven)
- **RAM:** VZ200: 8KB, VZ300: 16KB (expandable)
- **ROM:** 16KB BASIC interpreter

## Supported File Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| VZ | `.vz` | VZ tape image |
