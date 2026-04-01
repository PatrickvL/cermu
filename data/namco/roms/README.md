# Namco Arcade ROM Files

> **Note:** The Namco arcade emulation (Pac-Man, Pengo) is not yet functional.
> ROM loading is not implemented. This README will be updated when the systems
> are complete.

## Required ROMs

Namco arcade games use composite ROM sets loaded from MAME-compatible archives.
The manifest ROM loader does not yet handle arcade-style multi-file ROM sets.

### Pac-Man ($0000–$3FFF, 16 KB total)

| Archive | MAME filenames | Size | Address |
|---------|----------------|------|---------|
| `pacman.zip` | `pacman.6e` | 4 KB | $0000–$0FFF |
| | `pacman.6f` | 4 KB | $1000–$1FFF |
| | `pacman.6h` | 4 KB | $2000–$2FFF |
| | `pacman.6j` | 4 KB | $3000–$3FFF |

### Pengo ($0000–$7FFF, 32 KB total)

| Archive | MAME filenames | Size | Address |
|---------|----------------|------|---------|
| `pengo.zip` | `ep1689c.8` | 4 KB | $0000–$0FFF |
| | `ep1690b.7` | 4 KB | $1000–$1FFF |
| | `ep1691b.15` | 4 KB | $2000–$2FFF |
| | `ep1692b.14` | 4 KB | $3000–$3FFF |
| | `ep1693b.21` | 4 KB | $4000–$4FFF |
| | `ep1694b.20` | 4 KB | $5000–$5FFF |
| | `ep5118b.32` | 4 KB | $6000–$6FFF |
| | `ep5119c.31` | 4 KB | $7000–$7FFF |

## Missing Files

All ROM files are **missing** — obtain from legal MAME ROM sets.

## Legal Status

Pac-Man and Pengo ROMs are copyrighted by Namco (now Bandai Namco). They are
**not** included in this repository. Obtain them from a legal MAME ROM set
(`pacman.zip`, `pengo.zip`).
