Let me compile this systematically — first the full covered system list, then the unlock map for new chips.

---

## Full System Coverage With All Present + Discussed Chips

### Commodore / MOS Family
| System | Key chips |
|---|---|
| Apple-1 | 6502 ✅, PIA 6820 ✅ |
| Apple II / IIe / IIc | 6502 ✅, VIA 6522 ✅ (soft-switch video, no dedicated chip) |
| Commodore PET | 6502 ✅, MC6845 ✅, PIA 6820 ✅, VIA 6522 ✅ |
| VIC-20 | 6502 ✅, VIC-I ✅, VIA ×2 ✅ |
| C64 / C64C | 6510 ✅, VIC-II ✅, SID ✅, CIA ×2 ✅ |
| C16 / C116 / Plus4 | 7501 ✅, TED ✅ |
| C128 | 8502 ✅, Z80 ✅, VIC-IIe ✅, SID ✅, CIA ×2 ✅, 8722 MMU (trivial) |

### Atari 6502-Based
| System | Key chips |
|---|---|
| Atari 2600 | 6507 ✅, TIA ✅, RIOT 6532 ✅ |

### Nintendo
| System | Key chips |
|---|---|
| NES / Famicom | 2A03 ✅, NES PPU ✅ |
| Famicom Disk System | 2A03 ✅, NES PPU ✅, + RAM adapter (trivial) |

### Acorn
| System | Key chips |
|---|---|
| Acorn Atom | 6502 ✅, VIA 6522 ✅, i8255 ✅ |
| BBC Micro B | 6502 ✅, MC6845 ✅, SN76489 ✅, VIA ×2 ✅ |
| BBC Micro B+ / Master | 65C02 ✅, MC6845 ✅, SN76489 ✅, VIA ×2 ✅ |

### Sinclair / Amstrad Z80
| System | Key chips |
|---|---|
| ZX Spectrum 16K/48K | Z80 ✅, Ferranti ULA ✅ |
| ZX Spectrum 128K / +2 / +3 | Z80 ✅, Ferranti ULA ✅, AY-3-8910 ✅ |
| Amstrad CPC 464 / 664 / 6128 | Z80 ✅, MC6845 ✅, AY-3-8910 ✅, i8255 ✅ |

### Motorola 6809-Based (new)
| System | Key chips |
|---|---|
| Dragon 32 / Dragon 64 | MC6809E ✅, MC6847 ✅, PIA 6820 ✅ |
| TRS-80 CoCo 1 / CoCo 2 | MC6809E ✅, MC6847 ✅, PIA 6820 ✅ |

### VTech / Tandy Z80
| System | Key chips |
|---|---|
| VTech VZ200 / VZ300 / Laser 200 | Z80 ✅, MC6847 ✅, AY-3-8910 ✅, i8255 ✅ |

### MSX / Spectravideo (new)
| System | Key chips |
|---|---|
| MSX1 (all brands) | Z80 ✅, TMS9918A ✅, AY-3-8910 ✅, i8255 ✅ |
| MSX2 | Z80 ✅, V9938 ✅, YM2149 ✅, i8255 ✅ |
| MSX2+ | Z80 ✅, V9958 ✅, YM2149 ✅, i8255 ✅ |
| Spectravideo SVI-318 / 328 | Z80 ✅, TMS9918A ✅, AY-3-8910 ✅, i8255 ✅ |

### Sega (new)
| System | Key chips |
|---|---|
| Sega SG-1000 | Z80 ✅, TMS9918A ✅, SN76489 ✅ |
| Sega SC-3000 | Z80 ✅, TMS9918A ✅, SN76489 ✅, i8255 ✅ |
| Sega Master System (NTSC) | Z80 ✅, 315-5124 ✅, SN76489 (integrated) ✅ |
| Sega Master System (PAL) / Game Gear | Z80 ✅, 315-5246 ✅, SN76489 (integrated) ✅ |

### ColecoVision / Memotech / Tatung (new)
| System | Key chips |
|---|---|
| ColecoVision | Z80 ✅, TMS9918A ✅, SN76489 ✅ |
| Memotech MTX 500 / 512 | Z80 ✅, TMS9918A ✅, AY-3-8910 ✅, Z80 CTC ✅ |
| Tatung Einstein | Z80 ✅, TMS9929A ✅, AY-3-8910 ✅, Z80 CTC ✅, Z80 PIO ✅ |

### Oric (one trivial chip — Oric ULA)
| System | Key chips |
|---|---|
| Oric-1 / Oric Atmos | 6502 ✅, AY-3-8910 ✅, + Oric ULA (simple) |

### DDR / Eastern Bloc Z80
| System | Key chips |
|---|---|
| LC-80 | Z80 ✅, Z80 PIO ✅, Z80 CTC ✅ |
| Z1013 | Z80 ✅, Z80 PIO ✅ |
| Z9001 / KC85 | Z80 ✅, Z80 PIO ✅, Z80 CTC ✅ |

### Arcade (partial)
| System | Key chips |
|---|---|
| Bombjack | Z80 ✅, AY-3-8910 ✅ |
| Namco Galaga / Pac-Man family | Z80 ✅, custom tile hardware |

### CHIP-8 / Virtual
| System | Notes |
|---|---|
| CHIP-8 / SCHIP / XO-CHIP | Interpreter — no physical chips |

---

**Total: ~55 distinct systems / system variants**

---

## High-Value Chips That Unlock Further Systems

### 1. MC68000 Family — Largest Single Unlock
A clean template family: `68000 → 68008 → 68010 → 68020 → 68030` with trait axes for bus width, address space, cache, and MMU presence.

| System | Additional chips needed beyond 68000 |
|---|---|
| Atari ST | MFP 68901 (timer/serial), Shifter (video colour), GLUE (bus logic) |
| Atari STE | Same + DMA sound engine (simple) |
| Sega Genesis / Mega Drive | Z80 ✅, SN76489 ✅, YM2612, custom VDP 315-5313 |
| NeoGeo MVS / AES | Z80 ✅, YM2610 |
| Sharp X68000 | YM2151, custom CRTC |
| Amiga 500 / 1000 / 2000 | Paula, Denise, Agnus (all Amiga custom — substantial) |
| Macintosh 128K / 512K / Plus | IWM (floppy), custom video (simple sync generator) |

---

### 2. YM FM Chip Family — Perfect Template Candidate
Trait axes: channel count, operators-per-channel, rhythm mode, SSG (built-in AY), ADPCM, DAC channel. One family covers an enormous range.

| Chip | Variant type | Key systems |
|---|---|---|
| YM2413 (OPLL) | 2-op FM, fixed patches | MSX-Music expansion ← all chips already present |
| YM2203 (OPN) | 3-ch FM + 3-ch SSG | PC-88 early, arcade |
| YM2612 (OPN2) | 6-ch FM + DAC | Sega Genesis ← just needs 68000 |
| YM2151 (OPM) | 8-ch pure FM | Arcade, Sharp X68000 |
| YM2608 (OPNA) | 6-ch FM + ADPCM + SSG | PC-88 VA, PC-98 |
| YM2610 (OPNB) | FM + ADPCM + SSG | NeoGeo ← just needs 68000 |
| YM3812 (OPL2) | 9-ch FM | AdLib, early Sound Blaster |

The MSX-Music (YM2413) is the most immediate payoff — zero new chips needed beyond the FM family itself, it slots straight into MSX1/2.

---

### 3. SNES PPU Pair (5C77 + 5C78) + SPC700
The CPU is already there — 65C816 is in fam65xx.

| Chip | Role |
|---|---|
| 5C77 (PPU1) | OBJ / sprite rendering |
| 5C78 (PPU2) | BG layer compositing, HDMA |
| SPC700 | Sony 8-bit audio CPU — distinct architecture, not 6502-derived |
| DSP-1 | Optional coprocessor cartridge |

Unlocks the entire SNES library. The SPC700 is the hardest part — it's a novel CPU with its own ISA, not a variant of anything already present.

---

### 4. Sharp LR35902 — Z80 Trait Extension
The Game Boy CPU is close enough to Z80 to model as a trait variant: `HasIndexRegs=false`, `HasUndocumented=false`, `HasGameBoyExtensions=true` (STOP, SWAP, relative bit ops, etc.).

| System | Additional chips |
|---|---|
| Game Boy (DMG) | LR35902 + custom LCD PPU (simple tile/sprite, 4-shade) |
| Game Boy Pocket | Same silicon |
| Game Boy Color | Same CPU + GBC PPU extensions (colour palettes) |

---

### 5. Atari Custom Trio — ANTIC + GTIA + POKEY
These three together unlock the entire Atari 8-bit line. POKEY also shows up in arcade boards independently.

| System | Chips |
|---|---|
| Atari 400 / 800 | 6502 ✅, ANTIC, CTIA/GTIA, POKEY |
| Atari 800XL / 130XE | 65C02 ✅, ANTIC, GTIA, POKEY |
| Atari 7800 | 6502 ✅, TIA ✅, MARIA (custom VDP), POKEY (optional) |
| Atari arcade (Missile Command, etc.) | 6502 ✅, POKEY alone covers several |

---

### 6. Intel 8080 / 8085 — Opens Early Arcade Era
Small ISA, simpler than Z80, but historically important.

| System | Notes |
|---|---|
| Space Invaders | 8080 + custom shift register hardware |
| Altair 8800 / IMSAI 8080 | 8080, no video — serial terminal only |
| CP/M machines (non-Z80) | Various |
| Taito 8080 arcade boards | Several classic titles |

---

### Unlock Priority Summary

| Chip(s) | Effort | Systems unlocked | Notes |
|---|---|---|---|
| YM2413 alone | Low | MSX-Music on all MSX | Zero other new chips needed |
| Game Boy LR35902 | Low-Medium | GB, GBC, GB Pocket | Z80 trait extension |
| SNES PPU pair + SPC700 | High | Entire SNES library | 65C816 already present |
| MC68000 family | Medium | Atari ST, Genesis*, Amiga*, Mac | *Each needs 1-2 more custom chips |
| YM FM family | Medium | Fills Genesis, NeoGeo, arcade, PC-88 | Pairs with 68000 for most targets |
| ANTIC + GTIA + POKEY | Medium | Entire Atari 8-bit line | Three chips but tightly coupled |
| Intel 8080 | Low | Early arcade, Altair | Limited modern interest |

The **YM2413 → MSX-Music** is the easiest near-term win given everything else is already in place. After that, **68000 + YM FM** form a natural pair that each make the other more valuable — neither fully pays off without the other.