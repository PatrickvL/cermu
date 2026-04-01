# Amstrad CPC ROM Files

> **Note:** The Amstrad CPC emulation is not yet functional. ROM files are
> present for when the system is complete.

## Required ROMs

Each CPC model needs a 32 KB combined ROM containing the firmware (Lower ROM,
16 KB at $0000) and Locomotive BASIC (Upper ROM, 16 KB at $C000).

### CPC 464

| Filename | Size | Address | Description |
|----------|------|---------|-------------|
| `cpc464.rom` | 32 KB | $0000+$C000 | Combined firmware + BASIC 1.0 |

The emulator splits this file automatically:
- Lower ROM (OS): bytes 0–16383
- Upper ROM (BASIC 1.0): bytes 16384–32767

### CPC 6128

| Filename | Size | Address | Description |
|----------|------|---------|-------------|
| `cpc6128.rom` | 32 KB | $0000+$C000 | Combined firmware + BASIC 1.1 |

### CPC 664

| Filename | Size | Address | Description |
|----------|------|---------|-------------|
| `cpc664.rom` | 32 KB | $0000+$C000 | Combined firmware + BASIC 1.0 |

> The CPC 664 variant is not yet emulated but this ROM is preserved for
> future use.

## Alternative Filenames Accepted

- **CPC 464:** `cpc464.rom`, `cpc464_os.rom` (lower only), `cpc464_basic.rom` (upper only)
- **CPC 6128:** `cpc6128.rom`, `cpc6128_os.rom` (lower only), `cpc6128_basic.rom` (upper only)

## Optional ROMs

| Filename | Size | Description |
|----------|------|-------------|
| `amsdos.rom` | 16 KB | AMSDOS disc operating system (sideways ROM) |
| `MF2.rom` | 8 KB | Multiface 2 ROM |
| `system.cpr` | ~128 KB | CPC+ system cartridge |

## Missing Files

All required ROMs are present. No files are missing.

## Legal Status

Amstrad retains copyright on CPC firmware ROMs. Amstrad has given blanket
permission for the redistribution of Sinclair ZX Spectrum ROMs, but this
does **not** extend to CPC firmware.
