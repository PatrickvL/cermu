# KC85 ROM Files

The KC85 series (KC85/2, KC85/3, KC85/4) were home computers produced by
VEB Mikroelektronik Mühlhausen in the DDR (East Germany). The original
manufacturer no longer exists. ROMs are widely distributed by preservation
communities.

## Required ROMs

All variants require a CAOS (Cassette Aided Operating System) ROM.
The KC85/3 and KC85/4 additionally require a BASIC ROM.

| Filename | Size | Variants | Description |
|----------|------|----------|-------------|
| `caos.rom` | 8 KB | All | CAOS operating system ROM at $E000–$FFFF |
| `basic.rom` | 8 KB | KC85/3, KC85/4 | BASIC interpreter ROM at $C000–$DFFF |

## Alternative Filenames Accepted

- **CAOS ROM:** `caos.rom`, `CAOS.ROM`
- **BASIC ROM:** `basic.rom`, `BASIC.ROM`

## Where to Obtain

The KC85 ROMs originate from a state-owned enterprise (VEB) that ceased to
exist after German reunification. They are freely distributed by the KC85
preservation community:

- **KC85-Emu** — KC85 emulator project with ROM archive
  - https://www.kc85-emu.de/
- **KC Club** — KC85 community and preservation
  - https://kc85.info/
- **MAME ROM set** — `kc85_3.zip` / `kc85_4.zip`
