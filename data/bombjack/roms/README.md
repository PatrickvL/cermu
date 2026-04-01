# Bomb Jack Arcade ROM Files

> **Note:** The Bomb Jack arcade emulation is not yet functional. ROM loading
> is not implemented. This README will be updated when the system is complete.

Bomb Jack is a 1984 arcade game by Tehkan (now Tecmo). The ROM set consists
of multiple program, graphics, and sound ROMs.

## Required ROMs

Bomb Jack uses composite ROM sets loaded from MAME-compatible archives.
The manifest ROM loader does not yet handle arcade-style multi-file ROM sets.

| Archive | Contents | Description |
|---------|----------|-------------|
| `bombjack.zip` | Multiple ROM files | MAME-compatible Bomb Jack ROM set |

### Main CPU ROMs ($0000–$7FFF, 32 KB total)

| MAME filename | Size | Address |
|---------------|------|---------|
| `09_j01b.bin` | 8 KB | $0000–$1FFF |
| `10_l01b.bin` | 8 KB | $2000–$3FFF |
| `11_m01b.bin` | 8 KB | $4000–$5FFF |
| `12_n01b.bin` | 8 KB | $6000–$7FFF |
| `13.1r` | 4 KB | $C000–$CFFF (extra) |

### Sound CPU ROMs ($0000–$1FFF, 8 KB)

| MAME filename | Size | Address |
|---------------|------|---------|
| `01_h03t.bin` | 8 KB | $0000–$1FFF |

## Missing Files

All ROM files are **missing** — obtain from a legal MAME ROM set.

## Legal Status

Bomb Jack ROMs are copyrighted by Tecmo. They are **not** included in this
repository. Obtain them from a legal MAME ROM set (`bombjack.zip`).
