# Emulation Gaps & Incomplete Features

Audit date: 2026-04-07. Covers all systems under `src/systems/` and shared chips under `src/chip/`.

---

## Legend

- **[DONE]** — Fixed or verified complete
- **[EASY]** — Small isolated change, hours not days
- **[MEDIUM]** — Needs meaningful work, not a one-liner
- **[LARGE]** — Multi-day or multi-week effort
- **[BLOCKED]** — Depends on another item being done first

---

## Recently Fixed

- [x] **C128: stale TODO comment** (line 872) — I/O dispatch was already implemented. Comment updated.
- [x] **C128: SID audio output** — `get_audio_samples()` now calls `board_.sid.generate_samples()`, sample rate forwarded.
- [x] **SMS: ROM bank switching** — `select_bank_at()` wired for Sega mapper.
- [x] **VIC-20: color RAM** — Replaced RAMChip with MOS2114 for proper 4-bit masking.
- [x] **C16: bus struct** — Deleted dead placeholder `c16_bus.hpp`; system uses shared `bus_state_t`.
- [x] **C128: apply_configuration** — Region timing (PAL/NTSC) and SID revision now applied.
- [x] **NES Mapper 068** — CHR-ROM nametable replacement wired via `nt_ptr[4]` in `MapperChrConfig`.
- [x] **NES Mapper 069** — IRQ changed from A12 approximation to proper CPU-cycle counter.
- [x] **CIA (MOS 6526)** — BCD increment verified correct (`> 9` + `+= 6`); 50/60Hz toggle verified (no counter reset needed).
- [x] **PLA** — Ultimax RP4 verified: 0x0F default is correct, no known cartridge overrides.
- [x] **SID GUI** — Test sound button implemented (A-440 sawtooth on voice 1).

---

## Commodore Systems

### C64 — Near-Complete
- [ ] **[EASY]** Auto-skip memtest optimization (`c64_kernal_patches.hpp:10`)
- [ ] **[MEDIUM]** TAP (cassette) tape emulation (`commodore_load_helpers.cpp:254`)
- [ ] **[MEDIUM]** CRT (cartridge image) loading (`commodore_load_helpers.cpp:262`)

### VIC-20 — Near-Complete
- [x] **[DONE]** True 4-bit color RAM via MOS2114 masking
- [ ] **[MEDIUM]** TAP tape emulation (shared with C64)
- [ ] **[MEDIUM]** Expansion RAM testing against real programs

### C16/Plus4 — Partial
- [x] **[DONE]** C16 bus struct placeholder deleted — system uses shared `bus_state_t`
- [ ] **[MEDIUM]** Cassette port I/O signals: motor, sense, serial bus bits (`c16_system.cpp:1027-1030`)
- [ ] **[MEDIUM]** TAP tape emulation (shared)
- [ ] **[MEDIUM]** CRT cartridge loading (shared)

### C128 — Substantially Complete
- [x] **[DONE]** `apply_configuration()` now applies SID revision and region timing
- [x] **[DONE]** Duplicate "Reset C128" menu item — verified as intentional design (two reset vectors)
- [ ] **[MEDIUM]** Z80/CP/M compatibility testing — untested

### PET — Complete
- No known gaps.

---

## NES / Famicom

### Core — Complete
- PPU: fully implemented
- APU (2A03 base 5 channels): fully implemented
- PAL timing: present
- Save states: working
- NSF player: functional

### Mappers — ~40 Implemented
- [x] **[DONE]** Mapper 068 (Sunsoft): CHR-ROM nametable replacement wired via `nt_ptr`
- [x] **[DONE]** Mapper 069 (FME-7): IRQ converted to CPU-cycle counter with `notify_cpu_cycle()`
- [ ] **[MEDIUM]** Mapper 005 (MMC5): expansion audio $5000-$5015 stub, vertical split mode stub (`mapper_005_mmc5.hpp:520, 593`)
- [ ] **[LARGE]** Mapper 024/026 (VRC6a/b): Konami expansion audio — 2 pulse + sawtooth channels
- [ ] **[LARGE]** Mapper 085 (VRC7): FM synthesis expansion audio
- [ ] **[LARGE]** Mapper 019 (Namco 163): wavetable expansion audio, complex banking
- [ ] **[LARGE]** Mapper 020 (FDS): Famicom Disk System — disk emulation, wavetable sound, entirely new hardware
- [ ] **[LARGE]** Mapper 069 (Sunsoft 5B): Yamaha expansion audio (on top of existing FME-7)

### Missing Mapper Families (low priority — rare/pirate)
- Mappers 42–63: misc FDS-conversion hacks, multicarts
- Mappers 74, 76, 80–84, 90–92, 96, 98–112: rare pirate/one-game mappers
- 200+ unmapped IDs mostly covering obscure variants

---

## Shared Chips

### Sound

#### Yamaha FM (YM2612 / OPM / OPL) — 13 Major Gaps
- [ ] **[LARGE]** FM modulation: operators advance independently, modulator→carrier phase feed not implemented (`ym_fm.hpp:17`)
- [ ] **[LARGE]** Envelope generator: linear approximation, should be per-rate LUT with non-linear attack (`ym_fm.hpp:22`)
- [ ] **[LARGE]** ADPCM-A & ADPCM-B: flags exist, no decode/playback logic (`ym_fm.hpp:49`)
- [ ] **[MEDIUM]** LFO AM/PM: values computed then discarded — not applied (`ym_fm.hpp:71`)
- [ ] **[MEDIUM]** Ch3 special mode: register bits read but per-operator frequencies never applied (`ym_fm.hpp:31`)
- [ ] **[MEDIUM]** SSG-EG control: bits stored, shape alteration not applied (`ym_fm.hpp:67`)
- [ ] **[MEDIUM]** DT1 detune: flat ±0-3 placeholder, should be block-dependent 32-entry LUT; DT2 absent (`ym_fm.hpp:45`)
- [ ] **[MEDIUM]** Rate-scaling: RS bits stored, never factor into envelope rate (`ym_fm.hpp:63`)
- [ ] **[EASY]** Sine table: using `std::sin()`, removing hardware quantization artifacts (`ym_fm.hpp:36`)
- [ ] **[MEDIUM]** SSG composition: AY PSG declared but never instantiated or clocked (`ym_fm.hpp:26`)
- [ ] **[MEDIUM]** OPL-family: waveform select, rhythm mode percussion, OPLL ROM patches missing (`ym_fm.hpp:53`)
- [ ] **[MEDIUM]** OPM-specific: noise channel, key-fraction register, OPM addressing missing (`ym_fm.hpp:57`)
- [ ] **[EASY]** YM3438 ladder-effect difference not modeled (`ym_fm.hpp:75`)

#### AY-3-8910 Variants
- [ ] **[MEDIUM]** AY8930 extended mode: per-channel envelopes, extended noise period, duty cycle (`ay8930.hpp:11`)

#### MOS 6581 (SID)
- [x] **[DONE]** GUI: Test sound button implemented (A-440 sawtooth on voice 1)

### Video

#### TMS9918 / Sega VDP
- [ ] **[MEDIUM]** Sprite priority not implemented (`tms9918_render.inc.hpp:377`)
- [ ] **[MEDIUM]** Sega Mode 4 fine-scroll (sub-tile X offset) not implemented (`tms9918_render.inc.hpp:431`)
- [ ] **[MEDIUM]** Per-line scroll table not implemented (`tms9918_mixins.hpp:141`)
- [ ] **[LARGE]** VDP Command execution placeholder (`tms9918_mixins.hpp:125`)

#### MC6847
- [ ] **[MEDIUM]** Graphics modes (AG=1) not rendered — frame left black (`mc6847.hpp:276`)

#### BBC VIDPROC (SAA5050 Teletext)
- [ ] **[MEDIUM]** Proper SAA5050 12×20 Teletext character generator needed (`bbc_vidproc.hpp:213`)

#### Signetics 2513 (Apple II font ROM)
- [ ] **[EASY]** Placeholder font — needs verified CM4800 ROM dump (`signetics2513.hpp:227`)

#### Bomb Jack Video
- [ ] **[MEDIUM]** Background and sprite layers not implemented — only foreground tilemap (`bombjack_video.hpp:23`)

#### VIC-II
- [ ] **[EASY]** Graphics sequencer continues to run during left/right border — not implemented (`vicii_common.cpp:702`)

#### Atari 2600 TIA
- [ ] **[MEDIUM]** Paddle capacitor dump (INPT0-3) not implemented — VBLANK bit 7 (`tia.cpp:629`)

#### Spectrum ULA
- [ ] **[EASY]** Exact contention pattern (48K vs 128K variant) (`ferranti_ula.hpp:244`)

#### Namco Video
- [ ] **[EASY]** Output native 288×224 when GPU-side rotation available (`namco_video.hpp:37`)

### I/O

#### MOS 6526 (CIA)
- [x] **[DONE]** 50/60Hz toggle: verified no counter reset needed — matches real hardware
- [x] **[DONE]** BCD increment: verified `> 9` + `+= 6` is correct

#### PLA
- [x] **[DONE]** Ultimax mode: 0x0F default verified correct, no known cartridge overrides

### CPU

#### Family 65xx
- [ ] **[LARGE]** HuC6280: 8-bank mapper, unique instructions (TII/TAM/TMA/CSH/CSL), integrated 6-ch PSG
- [ ] **[LARGE]** CSG 4510: MAP instruction, 20-bit addressing, integrated DMA
- [ ] **[LARGE]** 65CE02: Z register, PHZ/PLZ, TAZ/TZA, BASE page extensions
- [ ] **[MEDIUM]** MOS 6509: banking for indirect addressing
- [ ] **[EASY]** MOS 6504: not yet instantiated as distinct type
- [ ] **[EASY]** CPU tracing: TODO to move to system level (`fam65xx.hpp:232`)

#### Motorola M68000
- [ ] **[MEDIUM]** A-line & F-line traps: unimplemented, trap to respective vectors (`m680x0_opcodes.hpp:31,36`)

#### Hitachi MC6809
- [ ] **[MEDIUM]** HD6309 extensions: stubs (`mc6809_base_ops.inc.hpp:2082`)

---

## Systems by Completeness Tier

### Mostly Complete (minor gaps)
| System | Key Missing Items |
|--------|-------------------|
| C64 | TAP/CRT loading |
| VIC-20 | TAP loading |
| C128 | Z80/CP/M testing |
| PET | — |
| NES | Expansion audio, FDS |
| Atari 2600 | Paddle capacitor dump |
| Amstrad CPC | — |
| ZX Spectrum | Memory contention |

### Partially Working (core runs, significant features missing)
| System | Key Missing Items |
|--------|-------------------|
| C16/Plus4 | Cassette I/O port |
| Chip-8 | ChipPlaceholders for peripherals |
| Acorn Atom | MC6847 graphics modes |
| BBC Micro | SSD/DSD disc, UEF tape, sideways ROM loading |
| Sega SG-1000 | Cartridge file loading |
| Sega SMS | (banking fixed) — VDP sprite priority, fine-scroll |
| MSX | Slot/page banking (critical), cartridge loading |
| ColecoVision | Cartridge file loading |
| Memotech MTX | File loading |
| SpectaVideo | File loading |
| Tatung Einstein | ROM overlay, file loading |
| DDR (Z9001/KC85/Z1013/LC80) | KC85 module mapping, serial keyboard PIO |
| Arcade — Bomb Jack | Background/sprite layers |
| Arcade — Atari Vector | Mathbox, second POKEY, host input wiring |
| Arcade — Namco | ROM loading (requires romset handling) |

### Skeleton/Stub (framework only, not runnable)
| System | Everything Missing |
|--------|-------------------|
| Apple II | Video, disk controller, keyboard, speaker, floppy formats |
| BBC Master | Audio, ACCCON banking, keyboard, file formats |
| Oric (Atmos) | ULA rendering, AY audio via VIA, keyboard, tape |
| VTech VZ | Video, speaker, keyboard, tape |

---

## Recommended Priority Order

### Quick Payoff (hours)
1. ~~C128 SID audio wiring~~ **[DONE]**
2. ~~SMS bank switching~~ **[DONE]**
3. ColecoVision / SG-1000 / MTX / SVI / Einstein file loading (each ~30 min)
4. MC6847 graphics modes (Acorn Atom)
5. NES Mapper 068/069 fixes

### Medium Effort, High Value (days)
6. TAP/CRT loading for Commodore systems
7. MSX slot banking
8. Bomb Jack sprite/background layers
9. TIA paddle support
10. BBC Teletext font (SAA5050)

### Large Projects (weeks)
11. Yamaha FM synthesizer overhaul
12. NES expansion audio (VRC6 first — simplest: 2 pulse + sawtooth)
13. FDS emulation
14. Apple II full implementation
15. HuC6280 / PC Engine support
