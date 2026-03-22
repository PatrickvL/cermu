# Tatung Einstein ROM Files

The Tatung Einstein requires an OS ROM to boot. This ROM is copyrighted
by Tatung and is **not** included in this repository.

## Required ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `einstein.rom` | 8 KB | Einstein OS ROM (MOS at $0000–$1FFF) |

## Alternative Filenames Accepted

- `einstein.rom`, `EINSTEIN.ROM`, `tcei.rom`

## Notes

The OS ROM is mapped at $0000 at boot and can be banked out by writing
to port $23, giving the CPU full access to 64 KB of RAM.

## Where to Obtain

- **MAME ROM set** — `einstein.zip` contains the OS ROM
- **Dump from original hardware** — if you own a Tatung Einstein
- **Tatung Einstein community** — preservation resources
  - http://www.einstein.talktalk.net/
