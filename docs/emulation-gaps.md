# Emulation Gaps & Incomplete Features

Audit date: 2026-04-08. Covers all systems under `src/systems/` and shared chips under `src/chip/`.

---

## Legend

- **[DONE]** — Fixed or verified complete
- **[EASY]** — Small isolated change, hours not days
- **[MEDIUM]** — Needs meaningful work, not a one-liner
- **[LARGE]** — Multi-day or multi-week effort
- **[BLOCKED]** — Depends on another item being done first

---

## Recently Fixed

- [x] **MSX: primary slot banking** — 256 precalculated ModeSnapshots (C64 PLA pattern), cart ROM loading, PPI Port A wiring.
- [x] **NES: VRC6 expansion audio** — Already implemented; stale gap entry removed.
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
- [x] **VIC-II** — Graphics sequencer now runs during L/R border for MxD collision accuracy.
- [x] **Spectrum ULA** — 48K bus contention pattern implemented (6-5-4-3-2-1-0-0 per 8 T-states).
- [x] **YM FM sine table** — Replaced `std::sin()` with hardware-accurate log-sin + exp ROM pipeline.
- [x] **YM2612 ladder effect** — DAC zero-crossing distortion modeled via `ladder_effect` trait flag.
- [x] **MOS 6504** — Instantiated as distinct `fam65xx_t` type alias.
- [x] **M68000 A-line/F-line** — Traps wired to vectors 10/11 via `exception()`.
- [x] **MC6847 AG=1** — CG1-CG6 and RG1-RG6 graphics modes rendered.
- [x] **TMS9918 fine-scroll** — Sega Mode 4 sub-tile X offset and per-column tile fetch.
- [x] **TMS9918 sprite priority** — BG-over-sprite priority bit implemented.
- [x] **BBC VIDPROC SAA5050** — Verified Mullard ROM data, bit expansion, character rounding, mosaic graphics.
- [x] **TIA paddle dump** — Capacitor dump (INPT0-3) via VBLANK bit 7.
- [x] **NES MMC5 audio** — Pulse channels and PCM DAC expansion audio.
- [x] **NES MMC5 split regs** — Vertical split registers ($5200-$5202) stored.
- [x] **YM FM DT1/RS/LFO/SSG-EG/Ch3** — Detune LUT, rate-scaling, LFO AM/PM, SSG-EG shapes, Ch3 special mode.
- [x] **OPL waveform select** — OPL2 waveform lookup infrastructure.
- [x] **MOS 6509 banking** — Exec/ind bank registers with self-clearing ($zp),Y indirection in bus_setup().
- [x] **TMS9918 224/240-line** — Runtime line-count switching for SMS2 315-5246 VDP.
- [x] **ColecoVision file loading** — Cartridge ROM loading with mirroring into 32KB cart window.
- [x] **SG-1000 file loading** — Cartridge ROM loading with mirroring into 32KB cart window.
- [x] **Memotech MTX file loading** — Binary file loading into RAM; `.run` header support.
- [x] **SpectaVideo SVI file loading** — Cartridge ROM loading replacing BASIC ROM.
- [x] **Tatung Einstein file loading** — COM/binary loading into RAM; ROM banking implemented.
- [x] **Namco Video rotation** — GPU-side `DisplayRotation` enum + CRT shader UV rotation; Namco outputs native 288×224.
- [x] **NES MMC5 vertical split** — PPU bus intercept hook; ExRAM nametable, per-tile attributes, split CHR bank.
- [x] **Atari 2600 mappers** — 11 new bank-switching schemes: F8SC/F6SC/F4SC (Superchip), E7 (M-Network), EF/EFSC (64KB homebrew), F0 (Megaboy), UA (UA Ltd), CV (Commavid), 3E (Tigervision+RAM), plus SuperchipRAM composable helper.

---

## Commodore Systems

### C64 — Near-Complete
- [x] **[DONE]** Auto-skip memtest — applied automatically via `on_file_parsed()` for all loaded software
- [x] **[DONE]** TAP cassette signal wiring — per-cycle datasette tick, CASS_READ→CIA1 FLAG, motor/sense
- [x] **[DONE]** CRT cartridge loading (type 0 — normal cartridge) — CHIP→ROML/ROMH, EXROM/GAME, reset

### VIC-20 — Near-Complete
- [x] **[DONE]** True 4-bit color RAM via MOS2114 masking
- [x] **[DONE]** TAP cassette signal wiring — per-cycle datasette tick, CASS_READ→VIA1 CA1, motor via CA2, sense→PA6
- [ ] **[TESTING]** Expansion RAM testing against real programs (code implemented, needs QA)

### C16/Plus4 — Partial
- [x] **[DONE]** C16 bus struct placeholder deleted — system uses shared `bus_state_t`
- [x] **[DONE]** Cassette port I/O signals — per-cycle datasette tick, motor/read/sense wired
- [x] **[DONE]** TAP tape emulation — uses shared CommodoreSystem deferred-load path
- ~~CRT cartridge loading~~ — invalid: TED machines (C16/Plus4/C116) do not use CRT format

### C128 — Substantially Complete
- [x] **[DONE]** `apply_configuration()` now applies SID revision and region timing
- [x] **[DONE]** Duplicate "Reset C128" menu item — verified as intentional design (two reset vectors)
- [ ] **[TESTING]** Z80/CP/M compatibility testing — code implemented, needs QA with real CP/M software

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
- [x] **[DONE]** Mapper 005 (MMC5): expansion audio (pulse + PCM DAC) implemented; vertical split rendering via PPU bus intercept
- [x] **[DONE]** Mapper 005 (MMC5): ExRAM mode 1 extended attributes + CHR bank override for BG tiles
- [x] **[DONE]** Mapper 005 (MMC5): scanline detection via consecutive NT read counting (replaces A12 timing)
- [x] **[DONE]** VRC2/4 IRQ: cycle-mode counter clocks directly per M2 (was using scanline prescaler)
- [x] **[DONE]** Mapper 118 (TxSROM): NT mirroring register mapping corrected (R0/R0/R1/R1 for 2KB regs)
- [x] **[DONE]** Mapper 064 (RAMBO-1): cycle-mode IRQ via `notify_cpu_cycle()` override
- [x] **[DONE]** Mapper 210 (Namco 175/340): R0/R1 treated as 2KB CHR banks
- [x] **[DONE]** Mapper 018 (SS88006): IRQ size field D1-D2 corrected; case 3 = 4-bit (0x000F)
- [x] **[DONE]** Mapper 001 (MMC1): SUROM 512KB support — CHR bank 0 bit 4 used as PRG A18
- [x] **[DONE]** Mapper 001 (MMC1): consecutive-write filter via CPU cycle tracking
- [x] **[DONE]** Mapper 068 (Sunsoft-4): NT register writes return true for bank map update
- [x] **[DONE]** Mapper 096 (Oeka Kids): ONESCREEN_LO mirror; latch moved to `ppu_bus_read`
- [x] **[DONE]** Mapper 245 (Waixing): PRG extra bit shift corrected; register masks fixed
- [x] **[DONE]** Mapper 004 (MMC3): refactored to use shared `MMC3IRQ` composable
- [x] **[DONE]** Mapper 120 (FDS hack): ROM-mapped PRG-RAM marked write-protected
- [x] **[DONE]** Mapper 034 (BNROM/NINA-001): NINA-001 path returns false to prevent fallthrough
- [x] **[DONE]** Bus conflict emulation added to 9 discrete-logic mappers (002/003/007/011/034/066/070/093/094)
- [x] **[DONE]** Mapper 230 (22-in-1): power-on mode corrected — starts in Contra
- [x] **[DONE]** Mapper 119 (TQROM): CHR-RAM via `extra_chr_ram_size()` instead of embedded buffer
- [x] **[DONE]** VRC2/4: PRG-RAM at $6000 with WRAM enable bit support
- [x] **[DONE]** MMC2/MMC4 (009/010): unified into shared `MapperMMC24` template
- [x] **[DONE]** CPUCycleIRQ: fixed off-by-one — decrement before underflow check
- [x] **[DONE]** NES 2.0 header: 12-bit mapper ID, submapper, extended PRG/CHR sizes
- [x] **[DONE]** Mapper 069 (FME-7): ROM banking at $6000 when RAM disabled
- [x] **[DONE]** Mapper 003 (CNROM): uses `chr_is_ram_` member instead of hardcoded false
- [x] **[DONE]** Mapper 071 (Camerica): BF9093 submapper ignores $8000-$9FFF mirroring writes
- [x] **[DONE]** Mapper 067 (Sunsoft-3): A12 filter delay on IRQ counter
- [x] **[DONE]** `set_prg_8k_banks` helper: modulo wrapping + `prg_ram_enabled=false` default
- [x] **[DONE]** Mapper 097: reset uses `header_mirror_` instead of hardcoded VERTICAL
- [x] **[DONE]** Removed 52 unused `prg_banks_`/`chr_banks_` members across mappers
- [x] **[DONE]** Mapper 097: PRG layout — last bank fixed at $8000, switchable at $C000
- [x] **[DONE]** Mapper 113 (NINA-06): PRG/CHR bit field decode and D6 as CHR A15
- [x] **[DONE]** Mapper 232 (BF9096): reset outer_block_ to 3 for correct reset vector
- [x] **[DONE]** Cartridge: custom_nt flag — prevent NT page overwrite for mappers 118/019
- [x] **[DONE]** Mapper 028 (Action 53): correct all four PRG banking modes per NESdev
- [x] **[DONE]** Mapper 048 (TC0690): mirroring control at $E000 D6, not $8000 D6
- [x] **[DONE]** Mapper 184 (Sunsoft-1): IC2 offset (|4) for upper CHR pattern table
- [x] **[DONE]** MMC5: $5130 register — upper CHR bank bits for >256KB CHR-ROM
- [x] **[DONE]** MMC1: SOROM/SZROM PRG-RAM banking via CHR bank 0 bit 3
- [x] **[DONE]** Namco 163: allocate extra CHR-RAM for writable bank modes (prevents ROM corruption)
- [x] **[DONE]** Mapper 032 (Irem G-101): submapper 1 one-screen mirroring (Major League)
- [x] **[DONE]** Mapper 030 (UNROM 512): submappers 1/2 fixed H/V mirroring
- [x] **[DONE]** FME-7: $6000 ROM mode — gate chip-enable on bit 7
- [x] **[DONE]** VRC6 sawtooth: 7 rate additions per 14-step cycle (was 6)
- [x] **[DONE]** Mapper 067 (Sunsoft-3): IRQ is CPU-cycle clocked, not A12-based
- [x] **[DONE]** Mapper 079 (NINA): comment fix — PRG select is D3 not D5
- [x] **[DONE]** VRCIRQ: removed dead prescaler member
- [x] **[DONE]** Mapper 033 (TC0190): register mask 0xE003 prevents $C000+ aliasing
- [x] **[DONE]** VRC6: B003.D7 PRG-RAM enable gate
- [x] **[DONE]** MMC3 family (004/118/119/189): compute bank counts from rom sizes, fix UB if 0
- [x] **[DONE]** Mapper 228 (Action 52): CHR bank uses D3-D2, not D1-D0
- [x] **[DONE]** Removed dead `ppu_bus_write()` virtual and unused mirror getters
- [x] **[DONE]** Standardized all mapper constructor unused-param style
- [x] **[DONE]** Mapper 024/026 (VRC6a/b): Konami expansion audio — 2 pulse + sawtooth channels
- [x] **[DONE]** Mapper 085 (VRC7): FM synthesis expansion audio (YM2413 OPLL) — 6-channel 2-op FM with Konami custom patches
- [ ] **[LARGE]** Mapper 019 (Namco 163): wavetable expansion audio
- [x] **[DONE]** Mapper 069 (Sunsoft 5B): Yamaha expansion audio (YM2149 PSG) — 3 square + noise + envelope
- [x] **[DONE]** Mapper 016 (Bandai FCG): 24C01/24C02 I2C EEPROM save support
- [ ] **[LARGE]** Mapper 020 (FDS): Famicom Disk System — disk emulation, wavetable sound, entirely new hardware

### Missing Mapper Families (low priority — rare/pirate)
- Mappers 42–63: misc FDS-conversion hacks, multicarts
- Mappers 74, 76, 80–84, 90–92, 96, 98–112: rare pirate/one-game mappers
- 200+ unmapped IDs mostly covering obscure variants

---

## Atari 2600

### Core — Complete
- TIA video/audio: fully implemented
- PIA 6532 RIOT (RAM + I/O + timer): fully implemented
- MOS 6507 CPU: instantiated via `fam65xx_t<Mos6507Traits>`
- Console switches: implemented
- Joystick ports: DB-9 active-low

### Mappers — 21 Implemented
- [x] **[DONE]** 2K — 2KB fixed (no bank switching)
- [x] **[DONE]** 4K — 4KB fixed (no bank switching)
- [x] **[DONE]** F8 — 8KB, 2 banks (hotspot $1FF8/$1FF9)
- [x] **[DONE]** F8SC — F8 + 128B Superchip RAM
- [x] **[DONE]** F6 — 16KB, 4 banks (hotspot $1FF6–$1FF9)
- [x] **[DONE]** F6SC — F6 + 128B Superchip RAM
- [x] **[DONE]** F4 — 32KB, 8 banks (hotspot $1FF4–$1FFB)
- [x] **[DONE]** F4SC — F4 + 128B Superchip RAM
- [x] **[DONE]** E0 — Parker Bros 8KB, 3 switchable + 1 fixed 1KB segments
- [x] **[DONE]** E7 — M-Network 16KB ROM + 2KB RAM, segment/bank select
- [x] **[DONE]** EF — 64KB homebrew, 16 banks (hotspot $1FE0–$1FEF)
- [x] **[DONE]** EFSC — EF + 128B Superchip RAM
- [x] **[DONE]** F0 — Megaboy 64KB, sequential auto-increment at $1FF0
- [x] **[DONE]** 3F — Tigervision (8KB–512KB, bus-snoop STA $xx3F)
- [x] **[DONE]** 3E — Tigervision + RAM banking (bus-snoop $xx3F/$xx3E)
- [x] **[DONE]** FE — Activision 8KB (bus-snoop JSR $xxFE, D5 selects bank)
- [x] **[DONE]** FA — CBS RAM Plus 12KB (3 × 4KB + 256B RAM)
- [x] **[DONE]** UA — UA Limited 8KB (bus-snoop $0220/$0240)
- [x] **[DONE]** CV — Commavid (2KB ROM + 1KB RAM)
- [x] **[DONE]** SuperchipRAM helper — composable 128B split-port RAM struct
- [x] **[DONE]** Auto-detection factory with signature analysis (E0/FE/UA/SC/3F/3E/E7/EF/F0/CV)
- [ ] **[LARGE]** DPC — Pitfall II data fetcher coprocessor (8 channels, music, display-data)
- [ ] **[MEDIUM]** AR — Starpath Supercharger (6KB RAM, multiload from tape)
- [ ] **[MEDIUM]** DPC+ — Enhanced DPC with fractional data fetchers + ARM coprocessor

---

## Shared Chips

### Sound

#### Yamaha FM (YM2612 / OPM / OPL) — 3 Remaining Gaps
- [x] **[DONE]** FM modulation: inter-operator phase feed implemented — operators evaluated in algorithm order
- [x] **[DONE]** Envelope generator: hardware-accurate per-rate LUT with exponential attack, linear-in-dB decay
- [ ] **[LARGE]** ADPCM-A & ADPCM-B: flags exist, no decode/playback logic (`ym_fm.hpp:49`)
- [x] **[DONE]** LFO AM/PM: applied to operator output
- [x] **[DONE]** Ch3 special mode: per-operator frequencies applied
- [x] **[DONE]** SSG-EG control: envelope shape alteration implemented
- [x] **[DONE]** DT1 detune: block-dependent 32-entry LUT implemented
- [x] **[DONE]** Rate-scaling: RS bits factor into envelope rate
- [x] **[DONE]** Sine table: hardware-accurate log-sin + exp ROM pipeline replaces `std::sin()`
- [x] **[DONE]** YM2612 ladder-effect DAC distortion modeled via `ladder_effect` trait flag
- [x] **[DONE]** SSG composition: ay_psg_t<YM2149_Traits> embedded in OPN-family chips, register routing, clocking, audio mix
- [x] **[DONE]** OPL-family: OPLL ROM patches (15 YM2413 + 15 VRC7), register decode, instrument loading
- [x] **[DONE]** OPL-family: waveform select lookup implemented
- [ ] **[LARGE]** OPM-specific: noise channel, key-fraction register, OPM register map (fundamentally different addressing from OPN)

#### AY-3-8910 Variants
- [x] **[DONE]** AY8930 extended mode: per-channel envelopes, duty cycle, Bank B register routing

#### MOS 6581 (SID)
- [x] **[DONE]** GUI: Test sound button implemented (A-440 sawtooth on voice 1)

### Video

#### TMS9918 / Sega VDP
- [x] **[DONE]** Sprite priority (BG-over-sprite) implemented
- [x] **[DONE]** Sega Mode 4 fine-scroll (sub-tile X offset) and per-column tile fetch
- [x] **[DONE]** Sega Mode 4 scroll inhibit: R0.D6 H-scroll lock (top 2 rows), R0.D7 V-scroll lock (right 8 cols), R0.D5 left-column mask
- [ ] **[MEDIUM]** Per-line scroll table not implemented (`tms9918_mixins.hpp:141`)
- [ ] **[LARGE]** VDP Command execution placeholder (`tms9918_mixins.hpp:125`)

#### MC6847
- [x] **[DONE]** Graphics modes (AG=1) — CG1-CG6 and RG1-RG6 rendered

#### BBC VIDPROC (SAA5050 Teletext)
- [x] **[DONE]** SAA5050 Teletext — verified ROM data, bit expansion, character rounding, mosaic graphics

#### Signetics 2513 (Apple II font ROM)
- [ ] **[BLOCKED]** Placeholder font — needs verified CM4800 ROM dump (physical chip read required)

#### Bomb Jack Video
- [x] **[DONE]** Background, sprite, and foreground layers fully implemented (render_background, render_sprites, render_foreground)

#### VIC-II
- [x] **[DONE]** Graphics sequencer runs during L/R border for MxD collision accuracy

#### Atari 2600 TIA
- [x] **[DONE]** Paddle capacitor dump (INPT0-3) via VBLANK bit 7

#### Spectrum ULA
- [x] **[DONE]** 48K contention pattern implemented (8-T-state cycle, 128 T-states per display line)

#### Namco Video
- [x] **[DONE]** GPU-side display rotation — `DisplayRotation` in HardwareTraits, CRT shader UV rotation, native 288×224 output

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
- [x] **[DONE]** MOS 6509: exec/ind bank registers, self-clearing ($zp),Y indirection bank in bus_setup()
- [x] **[DONE]** MOS 6504: instantiated as distinct fam65xx_t type alias
- [x] **[DONE]** CPU tracing: memory-read callback for operand readback in instruction trace

#### Motorola M68000
- [x] **[DONE]** A-line & F-line traps: wired to vectors 10/11 via `exception()`

#### Hitachi MC6809
- [x] **[DONE]** HD6309 extensions: OIM/AIM/EIM/TIM bitop instructions + SEXW + LDQ

---

## Systems by Completeness Tier

### Mostly Complete (minor gaps)
| System | Key Missing Items |
|--------|-------------------|
| C64 | Banked CRT types |
| VIC-20 | Expansion RAM testing (QA) |
| C16/Plus4 | — |
| C128 | Z80/CP/M testing (QA) |
| PET | — |
| NES | N163 expansion audio, FDS |
| Atari 2600 | DPC (Pitfall II), Starpath Supercharger |
| Amstrad CPC | — |
| ZX Spectrum | — |

### Partially Working (core runs, significant features missing)
| System | Key Missing Items |
|--------|-------------------|
| Chip-8 | ChipPlaceholders for peripherals |
| Acorn Atom | — |
| BBC Micro | SSD/DSD disc, UEF tape, sideways ROM loading (SAA5050 done) |
| Sega SG-1000 | — |
| Sega SMS | Per-line scroll table |
| MSX | MSX2 memory mapper ($FC-$FF), sub-slot expansion |
| ColecoVision | — |
| Memotech MTX | — |
| SpectaVideo | — |
| Tatung Einstein | — |
| DDR (Z9001/KC85/Z1013/LC80) | KC85 module mapping, serial keyboard PIO |
| Arcade — Bomb Jack | — |
| Arcade — Atari Vector | Mathbox, second POKEY, host input wiring |
| Arcade — Namco | ROM loading (requires romset handling), sprites |

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
3. ~~ColecoVision / SG-1000 / MTX / SVI / Einstein file loading~~ **[DONE]**
4. ~~MC6847 graphics modes (Acorn Atom)~~ **[DONE]**
5. ~~NES Mapper 068/069 fixes~~ **[DONE]**

### Medium Effort, High Value (days)
6. ~~TAP/CRT loading for Commodore systems~~ **[DONE]** (C64 cassette + CRT type 0; C16 cassette)
7. VIC-20 cassette wiring, C16/C128 CRT loading
8. ~~MSX slot banking~~ **[DONE]** (256 precalculated snapshots, cartridge loading)
9. Bomb Jack sprite/background layers
10. TIA paddle support
11. BBC Teletext font (SAA5050)

### Large Projects (weeks)
12. ~~Yamaha FM synthesizer overhaul~~ **[DONE]** (inter-op modulation, EG LUT, OPLL patches)
13. ~~NES expansion audio (VRC7/Sunsoft 5B)~~ **[DONE]** — N163 wavetable remains
14. FDS emulation
15. Apple II full implementation
16. HuC6280 / PC Engine support
