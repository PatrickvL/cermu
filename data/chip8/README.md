# CHIP-8 Data Directory

No system ROMs are required — CHIP-8 is a virtual machine with a built-in
interpreter and font set. Only user-provided programs are needed.

## Supported Formats

| Extension | Description |
|-----------|-------------|
| `.ch8` | Standard CHIP-8 program |
| `.c8` | Alternative extension |

Programs are loaded at address `$0200` (512 bytes into memory).

## Program Size Limits

| Mode | Max Size | Memory |
|------|----------|--------|
| Standard CHIP-8 | 3,584 bytes | 4 KB total |
| XO-CHIP (auto-detected) | ~64 KB | 64 KB total |

Programs larger than 3,584 bytes automatically enable XO-CHIP extended memory.

## Where to Find Programs

- [Chip-8 Archive](https://johnearnest.github.io/chip8Archive/) — curated collection of public domain programs
- [Revival Studios](https://github.com/JohnEarnest/Octo) — Octo IDE for creating CHIP-8 programs
- [David Winter's CHIP-8 page](https://www.pong-story.com/chip8/) — classic CHIP-8 games
- [Tobias Langhoff's guide](https://tobiasvl.github.io/blog/write-a-chip-8-emulator/) — test ROMs and references
