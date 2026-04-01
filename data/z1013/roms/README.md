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
preservation community (see below). The combined file is automatically
split using `@offset` syntax:
- `z1013_basic.rom@0` → BASIC lo, 8 KB at $C000
- `z1013_basic.rom@8192` → BASIC hi, 2 KB at $E000

## Missing Files

| Filename | Size | Description |
|----------|------|-------------|
| `z1013_basic.rom` | 10,240 bytes | **MISSING** — Required for Z1013.64 variant only |

## Alternative Filenames Accepted

- **Monitor:** `z1013_mon.rom`, `monitor.rom`, `MON.ROM`
- **Character ROM:** `z1013_char.rom`, `charrom.bin`, `CHAR.ROM`
- **BASIC lo:** `z1013_basic.rom@0`, `basic_lo.rom`, `BASIC.ROM@0`
- **BASIC hi:** `z1013_basic.rom@8192`, `basic_hi.rom`, `BASIC.ROM@8192`

## Source

Included ROMs sourced from the [floooh/chips-test](https://github.com/floooh/chips-test)
project. The Z1013 ROMs originate from VEB Robotron, a state-owned enterprise
that ceased to exist after German reunification in 1990.

For the BASIC ROM:
- **z1013.de** — https://www.z1013.de/
- **MAME ROM set** — `z1013.zip`
