# NES / Famicom Data Directory

No system ROMs are required — the NES uses cartridge-based software only.

## Directory Structure

- `roms/` — Game cartridge ROMs (`.nes` iNES format, `.zip` archives)
- `nsf/` — NES Sound Format music files (`.nsf`)
- `demos/` — Demo ROMs and homebrew
- `test_roms/` — Test ROMs for emulator validation

## Supported Formats

| Format | Extension | Description |
|--------|-----------|-------------|
| iNES | `.nes` | Standard cartridge ROM format (header + PRG + CHR) |
| NSF | `.nsf` | NES Sound Format (music playback) |

## Cartridge ROMs

Game ROMs are copyrighted and must be obtained legally. Homebrew and test ROMs are freely available.

### Homebrew & Free ROMs

- [NESdev Homebrew](https://www.nesdev.org/wiki/Homebrew) — community-made games and demos
- [NES Homebrew on itch.io](https://itch.io/games/tag-nes) — free and commercial homebrew

### Test ROMs (included)

The `test_roms/` directory contains freely distributable test ROMs for emulator validation:

- `nestest.nes` — CPU instruction test (Kevin Horton)
- `blargg_cpu_instr.nes` — Blargg's CPU instruction tests
- `blargg_cpu_timing.nes` — Blargg's CPU timing tests
- `blargg_ppu_vbl_nmi.nes` — Blargg's PPU VBL/NMI tests
- `blargg_sprite_hit.nes` — Blargg's sprite hit tests

Additional test suites: [Blargg's NES tests](https://www.nesdev.org/wiki/Emulator_tests)
