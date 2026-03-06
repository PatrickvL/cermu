# Atari 2600 ROM Files

Place Atari 2600 cartridge ROM files here (.a26 or .bin format).

## Test ROMs

- `test_gradient.a26` — 4KB test ROM that displays a scrolling color gradient with playfield pattern. Useful for verifying video output and basic TIA functionality.

## Supported ROM Sizes

| Size | Banks | Scheme |
|------|-------|--------|
| ≤ 4 KB | 1 | No bankswitching |
| ≤ 8 KB | 2 | F8 bankswitching |
| ≤ 16 KB | 4 | F6 bankswitching |

## Loading

Use `--system A2600` and load a cartridge ROM through the file dialog or command line.
