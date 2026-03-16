# KC85 ROM Files

The KC85 series (KC85/2, KC85/3, KC85/4) were home computers produced by
VEB Mikroelektronik Mühlhausen in the DDR (East Germany). The original
manufacturer no longer exists. ROMs are widely distributed by preservation
communities.

## Included ROMs

The following ROMs are included in this distribution. All variants require
a CAOS (Cassette Aided Operating System) ROM. The KC85/3 and KC85/4
additionally require a BASIC ROM.

| Filename | Size | Variants | Description |
|----------|------|----------|-------------|
| `caos.rom` | 8,192 bytes | All | CAOS 3.1 operating system ROM at $E000–$FFFF |
| `basic.rom` | 8,192 bytes | KC85/3, KC85/4 | HC-BASIC interpreter ROM at $C000–$DFFF |

The included `caos.rom` is **CAOS 3.1** for the KC85/3. If you need a
different CAOS version for another variant, replace this file:

| CAOS version | Variant | Original filename | Size |
|--------------|---------|-------------------|------|
| CAOS 2.2 | KC85/2 | `caos22.852` | 8,192 bytes |
| CAOS 3.1 | KC85/3 | `caos31.853` | 8,192 bytes (included) |
| CAOS 4.2 E | KC85/4 | `caos42e.854` | 8,192 bytes |

> **Note:** KC85/4 CAOS 4.2 also requires a 4 KB C-part (`caos42c.854`)
> which is not yet supported by the emulator's ROM loading.

## Alternative Filenames Accepted

- **CAOS ROM:** `caos.rom`, `CAOS.ROM`
- **BASIC ROM:** `basic.rom`, `BASIC.ROM`

## Source

ROMs sourced from the [floooh/chips-test](https://github.com/floooh/chips-test)
project. The KC85 ROMs originate from VEB Mikroelektronik Mühlhausen, a
state-owned enterprise that ceased to exist after German reunification in 1990.
