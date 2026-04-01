# LC80 ROM Files

> **Note:** The LC80 emulation is not yet functional. The ROM is included
> for when the system is complete.

The LC80 (Lerncomputer 80) was an educational single-board computer produced
by VEB Mikroelektronik Mühlhausen in the DDR (East Germany).

## Included ROMs

| Filename | Size | Address | Description |
|----------|------|---------|-------------|
| `lc80_mon.rom` | 2,048 bytes | $0000 (mirrored via addr_mask 0x07FF) | Monitor ROM |

## Alternative Filenames Accepted

- **Monitor ROM:** `lc80_mon.rom`, `monitor.rom`, `MON.ROM`

## Missing Files

No files are missing. All required ROMs are present.

## Source

ROM sourced from the [floooh/chips-test](https://github.com/floooh/chips-test)
project. The LC80 ROM originates from VEB Mikroelektronik Mühlhausen, a
state-owned enterprise that ceased to exist after German reunification in 1990.
