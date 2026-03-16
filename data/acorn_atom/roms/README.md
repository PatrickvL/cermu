# Acorn Atom ROM Files

The Acorn Atom requires OS and BASIC ROMs to boot. These ROMs are copyrighted
by Acorn Computers and are **not** included in this repository.

## Required ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `atom_os.rom` | 4 KB | Atom OS ROM at $F000–$FFFF |
| `atom_basic.rom` | 4 KB | Acorn Atom BASIC at $C000–$CFFF |

## Optional ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `atom_fp.rom` | 2 KB | Floating-point extension ROM at $D000–$D7FF |

## Alternative Filenames Accepted

- **OS ROM:** `atom_os.rom`, `ABASIC.ROM`, `os.rom`
- **BASIC ROM:** `atom_basic.rom`, `BASIC.ROM`, `basic.rom`
- **FP ROM:** `atom_fp.rom`, `FP.ROM`, `fp.rom`

## Where to Obtain

These ROMs are copyrighted by Acorn Computers (now ARM). To obtain them legally:

- **Dump from original hardware** — if you own an Acorn Atom
- **MAME ROM set** — `atom.zip` contains the required ROMs
- **Acorn Atom community** — preservation sites may have information
  - http://www.acornatom.nl/
