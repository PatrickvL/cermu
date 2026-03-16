# Z1013 ROM Files

The Z1013 was a single-board home computer produced by VEB Robotron in
the DDR (East Germany). The original manufacturer no longer exists. ROMs
are freely distributed by preservation communities.

## Included ROMs

| Filename | Size | Variants | Description |
|----------|------|----------|-------------|
| `z1013_mon.rom` | 2,048 bytes | All | Monitor 2.02 ROM at $F000–$F7FF |
| `z1013_char.rom` | 2,048 bytes | All | Character generator ROM (not bus-mapped) |

## Not Included

| Filename | Size | Variants | Description |
|----------|------|----------|-------------|
| `z1013_basic.rom` | 10,240 bytes | Z1013.64 | BASIC ROM (split across $C000–$E7FF) |

The Z1013 BASIC ROM is not included. It can be sourced from the Z1013
preservation community (see below).

## Alternative Filenames Accepted

- **Monitor:** `z1013_mon.rom`, `monitor.rom`, `MON.ROM`
- **Character ROM:** `z1013_char.rom`, `charrom.bin`, `CHAR.ROM`
- **BASIC:** `z1013_basic.rom`, `BASIC.ROM`

## Source

Included ROMs sourced from the [floooh/chips-test](https://github.com/floooh/chips-test)
project. The Z1013 ROMs originate from VEB Robotron, a state-owned enterprise
that ceased to exist after German reunification in 1990.

For the BASIC ROM:
- **z1013.de** — https://www.z1013.de/
- **MAME ROM set** — `z1013.zip`
