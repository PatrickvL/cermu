# CERMU Expansion Plan

> **Last updated: 2026-03-18**
>
> Legend: 🟢 = chip exists with real implementation, 🟡 = chip exists but minimal/stub, ⬚ = not yet implemented
> System status: **LIVE** = system class exists and runs, **READY** = all chips exist (system class needed), **BLOCKED** = missing chip(s)

---

## Full System Coverage — Current + Planned

### Commodore / MOS Family
| System | Status | Key chips |
|---|---|---|
| Apple-1 | **LIVE** | 6502 🟢, PIA 6820 🟢 |
| Apple II / IIe / IIc | **LIVE** | 6502 🟢, VIA 6522 🟢 (soft-switch video, no dedicated chip) |
| Commodore PET | **LIVE** | 6502 🟢, MC6845 🟢, PIA 6820 🟢, VIA 6522 🟢 |
| VIC-20 | **LIVE** | 6502 🟢, VIC-I 🟢, VIA ×2 🟢 |
| C64 / C64C | **LIVE** | 6510 🟢, VIC-II 🟢, SID 🟢, CIA ×2 🟢 |
| C16 / C116 / Plus4 | **LIVE** | 7501 🟢, TED 🟢 |
| C128 | **LIVE** | 8502 🟢, Z80 🟢, VIC-IIe 🟢, SID 🟢, CIA ×2 🟢, 8722 MMU (trivial) |

### Atari 6502-Based
| System | Status | Key chips |
|---|---|---|
| Atari 2600 | **LIVE** | 6507 🟢, TIA 🟢, RIOT 6532 🟢 |

### Nintendo
| System | Status | Key chips |
|---|---|---|
| NES / Famicom | **LIVE** | 2A03 🟢, NES PPU 🟢 |
| Famicom Disk System | **READY** | 2A03 🟢, NES PPU 🟢, + RAM adapter (trivial) |

### Acorn
| System | Status | Key chips |
|---|---|---|
| Acorn Atom | **LIVE** | 6502 🟢, VIA 6522 🟢, i8255 🟢 |
| BBC Micro B | **LIVE** | 6502 🟢, MC6845 🟢, SN76489 🟢, VIA ×2 🟢 |
| BBC Micro B+ / Master | **LIVE** | 65C02 🟢, MC6845 🟢, SN76489 🟢, VIA ×2 🟢 |

### Sinclair / Amstrad Z80
| System | Status | Key chips |
|---|---|---|
| ZX Spectrum 16K/48K | **LIVE** | Z80 🟢, Ferranti ULA 🟢 |
| ZX Spectrum 128K / +2 / +3 | **LIVE** | Z80 🟢, Ferranti ULA 🟢, AY-3-8910 🟢 |
| Amstrad CPC 464 / 664 / 6128 | **LIVE** | Z80 🟢, MC6845 🟢, AY-3-8910 🟢, i8255 🟢 |

### Motorola 6809-Based
| System | Status | Key chips |
|---|---|---|
| Dragon 32 / Dragon 64 | **BLOCKED** | MC6809E ⬚, MC6847 🟡, PIA 6820 🟢 |
| TRS-80 CoCo 1 / CoCo 2 | **BLOCKED** | MC6809E ⬚, MC6847 🟡, PIA 6820 🟢 |

### VTech / Tandy Z80
| System | Status | Key chips |
|---|---|---|
| VTech VZ200 / VZ300 / Laser 200 | **LIVE** | Z80 🟢, MC6847 🟡, AY-3-8910 🟢, i8255 🟢 |

### MSX / Spectravideo
| System | Status | Key chips |
|---|---|---|
| MSX1 (all brands) | **READY** | Z80 🟢, TMS9918A 🟢, AY-3-8910 🟢, i8255 🟢 |
| MSX2 | **READY** | Z80 🟢, V9938 🟢, YM2149 🟢, i8255 🟢 |
| MSX2+ | **READY** | Z80 🟢, V9958 🟢, YM2149 🟢, i8255 🟢 |
| Spectravideo SVI-318 / 328 | **READY** | Z80 🟢, TMS9918A 🟢, AY-3-8910 🟢, i8255 🟢 |

### Sega
| System | Status | Key chips |
|---|---|---|
| Sega SG-1000 | **READY** | Z80 🟢, TMS9918A 🟢, SN76489 🟢 |
| Sega SC-3000 | **READY** | Z80 🟢, TMS9918A 🟢, SN76489 🟢, i8255 🟢 |
| Sega Master System (NTSC) | **READY** | Z80 🟢, 315-5124 🟢, SN76489 (integrated) 🟢 |
| Sega Master System (PAL) / Game Gear | **READY** | Z80 🟢, 315-5246 🟢, SN76489 (integrated) 🟢 |

### ColecoVision / Memotech / Tatung
| System | Status | Key chips |
|---|---|---|
| ColecoVision | **READY** | Z80 🟢, TMS9918A 🟢, SN76489 🟢 |
| Memotech MTX 500 / 512 | **READY** | Z80 🟢, TMS9918A 🟢, AY-3-8910 🟢, Z80 CTC 🟢 |
| Tatung Einstein | **READY** | Z80 🟢, TMS9929A 🟢, AY-3-8910 🟢, Z80 CTC 🟢, Z80 PIO 🟢 |

### Oric
| System | Status | Key chips |
|---|---|---|
| Oric-1 / Oric Atmos | **LIVE** | 6502 🟢, AY-3-8910 🟢, + Oric ULA (simple) |

### DDR / Eastern Bloc Z80
| System | Status | Key chips |
|---|---|---|
| LC-80 | **LIVE** | Z80 🟢, Z80 PIO 🟢, Z80 CTC 🟢 |
| Z1013 | **LIVE** | Z80 🟢, Z80 PIO 🟢 |
| Z9001 / KC85 | **LIVE** | Z80 🟢, Z80 PIO 🟢, Z80 CTC 🟢 |

### Arcade (partial)
| System | Status | Key chips |
|---|---|---|
| Bombjack | **LIVE** | Z80 🟢, AY-3-8910 🟢 |
| Namco Galaga / Pac-Man family | **LIVE** | Z80 🟢, Namco WSG 🟢, custom tile hardware |

### CHIP-8 / Virtual
| System | Status | Notes |
|---|---|---|
| CHIP-8 / SCHIP / XO-CHIP | **LIVE** | Interpreter — no physical chips |

---

### Summary

| Category | Count |
|---|---|
| **LIVE** systems (running today) | 25 |
| **READY** systems (all chips present, need system class) | 10 |
| **BLOCKED** systems (missing chips) | 2 |
| **Total reachable** | ~55 distinct systems / variants |

**Newly READY since last update:** MSX1/2/2+, Spectravideo, Sega SG-1000/SC-3000/Master System/Game Gear, ColecoVision, Memotech MTX, Tatung Einstein — all unblocked by the TMS9918 VDP family addition.

---

## Implemented Chip Inventory

### CPUs
| Family | Template | Variants |
|---|---|---|
| fam65xx | `fam65xx_t<CPUTraits>` | MOS 6502, 6504, 6507, 6509, 6510, CSG 8502, CSG 4510, CSG 65CE02, Ricoh 2A03, Ricoh 5A22, WDC 65C02 (Synertek/WDC/WDC-S), WDC 65C816, Rockwell 65C02, Hudson HuC6280 |
| Z80 | `z80_t<Z80Traits>` | Zilog Z80, Z80A, Z80B, DDR U880 |
| m680x0 | `m680x0_t<M68KTraits>` | MC68000, MC68010, MC68020 |

### Video
| Family | Variants |
|---|---|
| VIC | VIC-I (MOS 6560/6561), VIC-II, VIC-IIe |
| TED | 7360/8360 |
| TMS9918 | TMS9918, TMS9918A, TMS9928A, TMS9929, TMS9929A, V9938, V9958, Sega 315-5124, Sega 315-5246 |
| Others | TIA, MC6845, MC6847 (minimal), Spectrum ULA, Amstrad Gate Array, BBC Vidproc (SAA5050), NES PPU, Signetics 2513 |

### Sound
| Family | Variants |
|---|---|
| AY PSG | AY-3-8910, AY-3-8912, AY-3-8913, AY-3-8914, AY8930, YM2149, YM3439 |
| SN76489 | Original, -A, 4966, Sega PSG |
| Others | MOS 6581 (SID), NES APU, Namco WSG |

### I/O
MOS 6526 (CIA), MOS 6522 (VIA), MOS 6529, PIA 6820, PIA 6532 (RIOT), i8255, Z80 CTC, Z80 PIO, KC85 Module System

---

## High-Value Chips That Unlock Further Systems

### 1. MC68000 Family — ✅ DONE
`m680x0_t<M68KTraits>` with MC68000, MC68010, MC68020 variants. Unlocks system-level work for:

| System | Additional chips needed beyond MC68000 |
|---|---|
| Atari ST | MFP 68901 ⬚ (timer/serial), Shifter ⬚ (video colour), GLUE ⬚ (bus logic) |
| Atari STE | Same + DMA sound engine (simple) |
| Sega Genesis / Mega Drive | Z80 🟢, SN76489 🟢, YM2612 ⬚, custom VDP 315-5313 ⬚ |
| NeoGeo MVS / AES | Z80 🟢, YM2610 ⬚ |
| Sharp X68000 | YM2151 ⬚, custom CRTC ⬚ |
| Amiga 500 / 1000 / 2000 | Paula ⬚, Denise ⬚, Agnus ⬚ (all Amiga custom — substantial) |
| Macintosh 128K / 512K / Plus | IWM ⬚ (floppy), custom video ⬚ (simple sync generator) |

---

### 2. YM FM Chip Family — ⬚ NOT STARTED
Trait axes: channel count, operators-per-channel, rhythm mode, SSG (built-in AY), ADPCM, DAC channel. One family covers an enormous range.

| Chip | Variant type | Key systems |
|---|---|---|
| YM2413 (OPLL) | 2-op FM, fixed patches | MSX-Music expansion ← all other chips already present |
| YM2203 (OPN) | 3-ch FM + 3-ch SSG | PC-88 early, arcade |
| YM2612 (OPN2) | 6-ch FM + DAC | Sega Genesis ← just needs VDP 315-5313 |
| YM2151 (OPM) | 8-ch pure FM | Arcade, Sharp X68000 |
| YM2608 (OPNA) | 6-ch FM + ADPCM + SSG | PC-88 VA, PC-98 |
| YM2610 (OPNB) | FM + ADPCM + SSG | NeoGeo ← just needs MC68000 🟢 |
| YM3812 (OPL2) | 9-ch FM | AdLib, early Sound Blaster |

The MSX-Music (YM2413) is the most immediate payoff — zero new chips needed beyond the FM family itself, it slots straight into MSX1/2.

---

### 3. SNES PPU Pair (5C77 + 5C78) + SPC700 — ⬚ NOT STARTED
The CPU is already there — 65C816 is in fam65xx 🟢.

| Chip | Role | Status |
|---|---|---|
| 5C77 (PPU1) | OBJ / sprite rendering | ⬚ |
| 5C78 (PPU2) | BG layer compositing, HDMA | ⬚ |
| SPC700 | Sony 8-bit audio CPU — distinct architecture, not 6502-derived | ⬚ |
| DSP-1 | Optional coprocessor cartridge | ⬚ |

Unlocks the entire SNES library. The SPC700 is the hardest part — it's a novel CPU with its own ISA, not a variant of anything already present.

---

### 4. Sharp LR35902 — 🟢 DONE (Z80 Trait Extension)
The Game Boy CPU is modeled as a Z80 trait variant: SM83 via `if constexpr` gating
in `z80_t<Traits>`, with 15 SM83-unique instruction handlers, SWAP, SM83 DAA, and
SM83 interrupt model. PPU and APU fully functional. 200/200 TOSEC sample pass rate.

| System | Additional chips |
|---|---|
| Game Boy (DMG) | LR35902 + custom LCD PPU (simple tile/sprite, 4-shade) — 🟢 DONE |
| Game Boy Pocket | Same silicon |
| Game Boy Color | Same CPU + GBC PPU extensions (colour palettes) — ⬜ not yet |

---

### 5. Atari Custom Trio — ANTIC + GTIA + POKEY — ⬚ NOT STARTED
These three together unlock the entire Atari 8-bit line. POKEY also shows up in arcade boards independently.

| System | Chips |
|---|---|
| Atari 400 / 800 | 6502 🟢, ANTIC ⬚, CTIA/GTIA ⬚, POKEY ⬚ |
| Atari 800XL / 130XE | 65C02 🟢, ANTIC ⬚, GTIA ⬚, POKEY ⬚ |
| Atari 7800 | 6502 🟢, TIA 🟢, MARIA ⬚ (custom VDP), POKEY ⬚ (optional) |
| Atari arcade (Missile Command, etc.) | 6502 🟢, POKEY alone covers several |

---

### 6. MC6809 — ⬚ NOT STARTED
Required for Dragon 32/64 and TRS-80 CoCo. MC6847 exists but is minimal 🟡.

---

### 7. Intel 8080 / 8085 — ⬚ NOT STARTED
Small ISA, simpler than Z80, but historically important.

| System | Notes |
|---|---|
| Space Invaders | 8080 + custom shift register hardware |
| Altair 8800 / IMSAI 8080 | 8080, no video — serial terminal only |
| CP/M machines (non-Z80) | Various |
| Taito 8080 arcade boards | Several classic titles |

---

### Unlock Priority Summary

| Chip(s) | Effort | Status | Systems unlocked | Notes |
|---|---|---|---|---|
| MC68000 family | Medium | 🟢 **DONE** | Atari ST*, Genesis*, Amiga*, Mac* | *Each needs custom chips |
| TMS9918 family | Medium | 🟢 **DONE** | MSX, Sega SG/SMS, ColecoVision, Memotech, Tatung | 10 systems now READY |
| YM2413 alone | Low | ⬚ | MSX-Music on all MSX | Zero other new chips needed |
| Game Boy LR35902 | Low-Medium | 🟢 **DONE** | GB, GBC, GB Pocket | Z80 trait extension; DMG fully functional |
| SNES PPU pair + SPC700 | High | ⬚ | Entire SNES library | 65C816 already present |
| YM FM family | Medium | ⬚ | Fills Genesis, NeoGeo, arcade, PC-88 | Pairs with MC68000 🟢 for most targets |
| ANTIC + GTIA + POKEY | Medium | ⬚ | Entire Atari 8-bit line | Three chips but tightly coupled |
| MC6809 | Low-Medium | ⬚ | Dragon, CoCo | MC6847 needs work too |
| Intel 8080 | Low | ⬚ | Early arcade, Altair | Limited modern interest |

---

## Near-Term Priorities

1. **MSX system class** — All chips present (Z80, TMS9918A, AY-3-8910, i8255). Highest leverage system to bring LIVE.
2. **Sega SG-1000 / Master System** — All chips present. Simple bus, well-documented.
3. **ColecoVision** — All chips present (Z80, TMS9918A, SN76489).
4. **YM2413 → MSX-Music** — Easiest FM synth win, slots straight into MSX.
5. **MC6847 hardening** — Current impl is minimal 🟡; needed for VTech VZ (already LIVE) and Dragon/CoCo (blocked on MC6809).

After that, **YM FM family** is the natural next chip template — it pairs with MC68000 🟢 to unlock Genesis, NeoGeo, and arcade boards.