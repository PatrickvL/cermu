# Z9001 / KC87 ROM Files

The Z9001 and KC87 were home computers produced by VEB Robotron in the
DDR (East Germany). The KC87 is a Z9001 with additional BASIC ROM and
color RAM. The original manufacturer no longer exists. ROMs are freely
distributed by preservation communities.

## Required ROMs

| Filename | Size | Variants | Description |
|----------|------|----------|-------------|
| `z9001_os.rom` | 4 KB | All | Operating system ROM at $F000–$FFFF |

### KC87 only

| Filename | Size | Description |
|----------|------|-------------|
| `z9001_basic.rom` | 10 KB | BASIC ROM (split across $C000–$E7FF) |

## Optional ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `z9001_char.rom` | 2 KB | Character generator ROM (not bus-mapped) |

## Alternative Filenames Accepted

- **OS ROM:** `z9001_os.rom`, `os.rom`, `OS.ROM`
- **Character ROM:** `z9001_char.rom`, `charrom.bin`, `CHAR.ROM`
- **BASIC:** `z9001_basic.rom`, `BASIC.ROM`

## Where to Obtain

The Z9001/KC87 ROMs originate from VEB Robotron, a state-owned enterprise
that ceased to exist after German reunification. They are freely distributed
by the community:

- **Robotron-net** — DDR computer preservation
  - https://www.robotrontechnik.de/
- **KC Club** — KC community and preservation
  - https://kc85.info/
- **MAME ROM set** — `z9001.zip` / `kc87.zip`
