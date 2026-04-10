# Game Boy ROM Files

The Game Boy is a cartridge-based console. Game ROMs are loaded via
File → Open. No system ROMs are strictly required.

## Optional ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `dmg_boot.bin` | 256 bytes | DMG (original Game Boy) boot ROM |
| `cgb_boot.bin` | 2 KB | CGB (Game Boy Color) boot ROM |

The boot ROM plays the Nintendo logo animation and chime at startup.
Without it, the emulator skips directly to the game — this is the
default behavior and works correctly for all games.

## Alternative Filenames Accepted

- **DMG boot:** `dmg_boot.bin`, `dmg_rom.bin`, `gb_bios.bin`
- **CGB boot:** `cgb_boot.bin`, `gbc_bios.bin`

## Where to Obtain

Boot ROMs are copyrighted by Nintendo and are **not** included.

- **Dump from original hardware** — Game Boy Dumper tools
- **SameBoy** — open-source boot ROM replacements available
  - https://sameboy.github.io/
