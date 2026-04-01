# Z9001 / KC87 ROM Files

The Z9001 and KC87 were home computers produced by VEB Robotron in the
DDR (East Germany). The KC87 is a Z9001 with additional BASIC ROM and
color RAM. The original manufacturer no longer exists. ROMs are freely
distributed by preservation communities.

## Included ROMs

| Filename | Size | Variants | Address | Description |
|----------|------|----------|---------|-------------|
| `z9001_os.rom` | 4,096 bytes | All | $F000–$FFFF | OS 1.2 ROM |
| `z9001_char.rom` | 2,048 bytes | All | (not bus-mapped) | Character generator ROM |
| `z9001_basic.rom` | 10,240 bytes | KC87 | $C000–$E7FF | KC-BASIC ROM (combined) |

The combined BASIC ROM file is automatically split using `@offset` syntax:
- `z9001_basic.rom@0` → BASIC lo, 8 KB at $C000
- `z9001_basic.rom@8192` → BASIC hi, 2 KB at $E000

## Alternative Filenames Accepted

- **OS ROM:** `z9001_os.rom`, `os.rom`, `OS.ROM`
- **Character ROM:** `z9001_char.rom`, `charrom.bin`, `CHAR.ROM`
- **BASIC lo:** `z9001_basic.rom@0`, `kc87_basic_lo.rom`, `basic_lo.rom`
- **BASIC hi:** `z9001_basic.rom@8192`, `kc87_basic_hi.rom`, `basic_hi.rom`

## Missing Files

No files are missing. All required ROMs are present.

## Source

ROMs sourced from the [floooh/chips-test](https://github.com/floooh/chips-test)
project. The Z9001/KC87 ROMs originate from VEB Robotron, a state-owned
enterprise that ceased to exist after German reunification in 1990.
