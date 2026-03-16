# Z9001 / KC87 ROM Files

The Z9001 and KC87 were home computers produced by VEB Robotron in the
DDR (East Germany). The KC87 is a Z9001 with additional BASIC ROM and
color RAM. The original manufacturer no longer exists. ROMs are freely
distributed by preservation communities.

## Included ROMs

| Filename | Size | Variants | Description |
|----------|------|----------|-------------|
| `z9001_os.rom` | 4,096 bytes | All | OS 1.2 ROM at $F000–$FFFF |
| `z9001_char.rom` | 2,048 bytes | All | Character generator ROM (not bus-mapped) |
| `z9001_basic.rom` | 10,240 bytes | KC87 | KC-BASIC ROM (split across $C000–$E7FF) |

## Alternative Filenames Accepted

- **OS ROM:** `z9001_os.rom`, `os.rom`, `OS.ROM`
- **Character ROM:** `z9001_char.rom`, `charrom.bin`, `CHAR.ROM`
- **BASIC:** `z9001_basic.rom`, `BASIC.ROM`

## Source

ROMs sourced from the [floooh/chips-test](https://github.com/floooh/chips-test)
project. The Z9001/KC87 ROMs originate from VEB Robotron, a state-owned
enterprise that ceased to exist after German reunification in 1990.
