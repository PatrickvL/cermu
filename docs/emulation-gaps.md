# Emulation Gaps, Quality Issues & Expansion Opportunities

Audit date: 2026-04-09. Covers all systems under `src/systems/`, shared chips under `src/chip/`,
core infrastructure, devices, ports, utilities, formats, and rendering pipeline.

---

## Legend

| Tag | Meaning |
|-----|---------|
| **[ACCURACY]** | Emulation fidelity gap vs. real hardware |
| **[STUB]** | Skeleton/placeholder — no functional output |
| **[INCOMPLETE]** | Partially working — key features missing |
| **[QUALITY]** | Code quality, duplication, or convention issue |
| **[PERF]** | Performance concern on hot path |
| **[EXPANSION]** | Opportunity to add new capability |
| **[TESTING]** | Missing or insufficient test coverage |
| Effort: **S** / **M** / **L** / **XL** | Hours / days / week / multi-week |

---

## 1 — Emulation Accuracy Gaps

### 1.1 Spectrum ULA: Memory Contention Not Implemented — **[ACCURACY] M**
The ULA steals bus cycles from the Z80 during active display (6-5-4-3-2-1-0-0 pattern per 8 T-states,
128 T-states per display line). Currently not modeled — programs that rely on contention timing
(tape loaders, border color effects, timing-sensitive games) run faster than real hardware.
48K pattern was previously marked done but code review shows no wait-state injection in the system tick loop.

### 1.2 TMS9918 V9938/V9958 VDP Command Engine — **[STUB] L**
`tms9918_mixins.hpp` declares `vdp_command_mixin_t` with state fields (`cmd_op_`, `cmd_active_`)
but the `start_command()` / `step_command()` functions are commented-out placeholders.
All ~30 blitter commands (HMMC, YMMM, HMMM, HMMV, LMMC, LMCM, LMMM, LMMV, LINE, SRCH, PSET, POINT…)
are documented but unimplemented. Blocks MSX2/MSX2+ software that uses hardware blitting.

### 1.3 C1541 Disk Drive: Protocol-Level Only — **[ACCURACY] L**
The drive handles IEC commands (LISTEN/TALK/OPEN/CLOSE) at protocol level and reads D64 sectors directly.
No cycle-accurate 6502 CPU execution, no VIA bit-banged handshake, no GCR track encoding,
no motor/head seek timing, no track density variation.
- Standard LOAD/SAVE works correctly. Most software loads fine via KERNAL traps.
- Custom fast-loaders that bypass IEC protocol will fail.
- Copy-protected disks (non-standard GCR) will not work.
- A full `DriveSystem` (6502 + 2×VIA + GCR) exists in `drive_1541_system.*` but is not the default mode.

### 1.4 ~~SID Combined Waveforms~~ — **[DONE]** (verified)
Combined waveform tables use the libsidplayfp pulldown algorithm with RLE-encoded XOR deltas
matched against reSID/real hardware OSC3 measurements. Verification pass confirmed:
- 0 OSC3 mismatches across all combined waveform modes (0x30, 0x50, 0x60, 0x70)
- 0 envelope mismatches, 0 accumulator mismatches
- Ring modulation, oscillator sync, test bit: all exact match
- Audio pipeline comparison PASS (8 test scenarios, 0 failures)
Tested against reSID (Dag Lem, 2010) from VICE source tree.

### 1.5 HD6309: Instruction Coverage Incomplete — **[INCOMPLETE] M**
Trait infrastructure exists (`HAS_W_REGISTER`, `HAS_NATIVE_MODE`, `HD6309_DIVZERO`),
W/V/E/F registers are declared, and basic OIM/AIM/EIM/TIM/SEXW/LDQ instructions implemented.
Missing or unverified: TFM (block transfer), MULD/DIVD (multiply/divide),
native-mode extended addressing, all native-mode-only instructions.

### 1.6 M68000: Bus Cycle Timing Approximate — **[ACCURACY] M**
All 14 EA modes are implemented with correct semantics. Address error handling and function codes present.
However, S0–S7 bus cycle sequencing is not fully modeled — cycle counts are totaled rather than stepped.
68020 cache (CACR/CAAR) registers are handled but not functionally cached.
Impact: instruction-level accuracy OK; sub-instruction bus-cycle accuracy insufficient for
cycle-exact arcade hardware where bus contention matters.

### 1.7 YM FM ADPCM-A / ADPCM-B Channels — **[STUB] L**
Trait flags `has_adpcm_a()` / `has_adpcm_b()` exist and register decode infrastructure is in place,
but zero audio decode or playback logic. Rhythm tracks on YM2608 (PC-88/PC-98) and streaming
ADPCM on YM2610 (Neo Geo) will produce silence. Core FM synthesis is accurate.

### 1.8 OPM (YM2151): Register Map Not Implemented — **[STUB] L**
OPM uses fundamentally different register addressing from OPN family.
Noise channel, key-fraction register, and the OPM-specific register map are unimplemented.
Blocks accurate Sharp X68000 and arcade board emulation.

### 1.9 Signetics 2513: Placeholder Font ROM — **[ACCURACY] S** (blocked)
The CM4800 Katakana character variant uses an unverified placeholder ROM.
Requires physical chip read for verification. Only affects Japanese Apple I rendering.

---

## 2 — Incomplete System Implementations

### 2.1 Oric (Atmos) — **[STUB] L**
Framework exists (6502, AY-3-8912, MOS 6522 VIA instantiated) but functionally non-runnable:
- ULA text/hi-res rendering: not implemented
- AY-3-8912 audio output routing through VIA: not wired
- Keyboard matrix mapping: stub
- ~~TAP tape format loading: unimplemented~~ — **DONE** (`oric_tap_format`)

Remaining three items must be addressed together to produce any visible/audible output.

### 2.2 VTech VZ (VZ200/VZ300) — **[STUB] L**
Z80A instantiated with basic memory map, but:
- Video rendering: stub (no pixel output)
- Speaker synthesis: not wired
- Keyboard mapping: stub
- ~~VZ tape format: unimplemented~~ — **DONE** (`vz_format`)

### 2.3 Apple II: Video Rendering Incomplete — **[INCOMPLETE] L**
3-variant template exists (II/IIe/IIc) with correct soft-switch memory map, but:
- Hi-res (280×192): rendering logic not producing correct output
- Double hi-res (560×192, IIe/IIc): unimplemented
- Artifact color generation: shader infrastructure exists (`artifact_signal_shader.hpp`) but not wired to Apple II
- Disk controller (Disk II): not implemented — blocks most software
- Speaker toggle: not connected to audio output
- ~~DSK/NIB/2MG format loading~~ — **DONE** (`apple_dsk_format`)

### 2.4 BBC Micro/Master: Remaining Gaps — **[INCOMPLETE] M**
System emulation is functional (MC6845 + Video ULA + SN76489 + 2×VIA all working).
File format loading now works: SSD/DSD, UEF, CPC DSK all supported via format registry.
Remaining:
- ~~SSD/DSD disc image loading~~ — **DONE** (`ssd_format`)
- ~~UEF tape loading~~ — **DONE** (`uef_format`, gzip via `os_decompress`)
- ~~BBC Master `load_file()`~~ — **DONE** (wired via `format_load_and_apply`, commit `59acaa1b`)
- Sideways ROM (.rom) loading: unimplemented
- BBC Master ACCCON shadow screen RAM: not mapped

### 2.5 ~~Sega Master System: ROM Loading~~ — **[DONE]**
ROM loading implemented: raw `.sms` files with optional 512-byte header detection,
Sega mapper banking (16KB pages), power-of-2 mirroring. Committed `8ad44293`.

### 2.6 KC85/4: Module Memory Mapping — **[INCOMPLETE] M** (blocked on module insertion)
Code contains explicit TODO: "map/unmap module memory based on active bit" for port 0x80.
However, `insert_module()` is never called — module slots are always empty. The mapping
code is dead until module insertion is exposed via UI or configuration. Reclassified
from quick-win to medium; depends on a module-insertion mechanism being built first.

### 2.7 MSX2/MSX2+: Secondary Slot Expansion & Memory Mapper — **[INCOMPLETE] M**
Primary slot banking with 256 precalculated ModeSnapshots works correctly.
Missing: MSX2 memory mapper registers ($FC–$FF) for RAM paging, and sub-slot expansion
(secondary slot select register at $FFFF). Blocks most MSX2-specific software.

### 2.8 C128: Z80/CP/M Mode — **[TESTING] M**
Code implemented for Z80 mode switching and CP/M memory layout.
Needs QA testing with real CP/M software (WordStar, Turbo Pascal, etc.).

### 2.9 VIC-20: Expansion RAM — **[TESTING] S**
Memory expansion code implemented. Needs testing against programs that
depend on specific expansion configurations (3K, 8K, 16K, 24K+).

---

## 3 — Arcade Systems

### 3.1 Namco Arcade: ROM Set Handling — **[INCOMPLETE] M**
Board emulation works (Z80A + Namco Video + WSG3), Pac-Man/Pengo traits defined.
Missing: proper ROM set loading from MAME-style zip archives.
Encrypted ROM support for Pengo exists but untested without loadable ROM sets.

### 3.2 Atari Vector Arcade: Input Wiring — **[INCOMPLETE] M**
DVG/AVG vector processors, 6502 CPU, and optional POKEY all instantiated.
Host input (coin, start, joystick/spinner) not wired. Second POKEY for stereo not connected.
Mathbox coprocessor (for Battlezone, Tempest) not implemented.

### 3.3 Bomb Jack: Complete — no gaps identified.
Dual Z80 board with 3×AY-3-8910, background/sprite/foreground layers all implemented.

---

## 4 — CPU Expansion Opportunities

### 4.1 HuC6280 (PC Engine / TurboGrafx-16) — **[EXPANSION] XL**
8-bank MMU, 21-bit address space, unique block-transfer instructions (TII, TDD, TIN, TIA, TAI),
clock speed switching (CSH/CSL), integrated 6-channel PSG, timer, I/O port.
Enables: PC Engine, TurboGrafx-16, SuperGrafx systems.

### 4.2 CSG 4510 (Commodore 65) — **[EXPANSION] XL**
MAP instruction for 20-bit flat addressing, integrated DMA controller, enhanced I/O.
Enables: Commodore 65 prototype, MEGA65 compatibility testing.

### 4.3 65CE02 (CSG 65CE02) — **[EXPANSION] L**
Z register, PHZ/PLZ, TAZ/TZA, BASE page extensions, branch-always (BRA) without offset limit.
Prerequisite for CSG 4510.

### 4.4 WDC 65C816 — **[EXPANSION] XL**
16-bit accumulator/index modes, 24-bit addressing, bank registers, emulation/native mode switching.
Enables: Apple IIGS, SNES (Ricoh 5A22 is 65C816-based).

---

## 5 — Code Quality & Technical Debt

### 5.1 ~~Storage Devices: malloc/free Instead of RAII~~ — **[DONE]**
`datasette_1530.cpp` and `drive_1541.cpp` were already clean. Remaining `free()` in
`drive_1541_system.cpp` and `sega_sms_system.cpp` converted to `VfsData` RAII wrapper.
Committed `ac9156fe`.

### 5.2 Device GUI Rendering Duplication — **[QUALITY] S** (partially done)
Extracted `render_autofire_rate_slider()` into `InputPeripheralDevice` base class,
migrated `JoystickDevice` and `NesStandardController`. Committed `0b24d09f`.
Remaining: keymap preset UI and analog stats still duplicated in some devices
but less severe (~3–4 lines each, not the 9-line autofire block).

### 5.3 ~~Port Definitions Under-Populated~~ — **[CLOSED]**
Audit shows ports are already well-structured: DB-9 joystick shared across 5 systems,
IEC/cassette/user ports shared across Commodore family, NES ports in `src/ports/`.
System-local expansion ports (NES 48-pin, VIC-20 44-pin, Apple 44-pin, MSX cartridge)
are genuinely distinct connectors — consolidating them would be incorrect.
Minor note: Apple 1 cassette (analog `CASS_IN`/`CASS_OUT`) shares `PortType::CASSETTE_PORT`
with Commodore datasette (motor control) — harmless since devices never cross systems.

### 5.4 ~~ROM Loader MD5 Verification Stub~~ — **[DONE]**
Dead `rom_loader_verify_md5()` function already removed (no callers existed).

### 5.5 ~~Performance Metrics Potential Duplication~~ — **[DONE]**
No duplication. `PerformanceTracker` is a generic circular-buffer time-series
primitive; `PerformanceMetrics` is the application-level facade composing 5
tracker instances + atomic audio counters. Clean composition hierarchy.

### 5.6 ~~Pin State Boilerplate in _gui.cpp~~ — **[DONE]**
Added `build_pin_states<ChipType>()` template to `chip_layout.hpp` (two overloads:
direct pass-through and lambda-based override). Migrated 31 `_gui.cpp` files from
per-chip static helpers + manual `populate_pin_states_from_bus()` calls. Net −133 lines.
Committed `0efd41db`.

### 5.7 File Watcher Platform Coverage — **[QUALITY] S**
`file_watcher.hpp` only implements Linux (`inotify`). macOS/Windows return no-op.
Config/ROM hot-reload non-functional on those platforms. Document or implement.

### 5.8 Drive Disc Set Detection: Regex on File Open Path — **[PERF] S**
`drive_1541.cpp` uses regex-based multi-disc set detection (~700 lines of scanning logic).
Profile under large directory listing scenarios. Consider simpler pattern matching
if this becomes a bottleneck.

### 5.9 Register Accessor Convention Inconsistency — **[QUALITY] S**
Some chips use `r_(reg_name)` accessor helpers, others use `regs_.data[addr]` direct indexing.
Not a functional issue but hurts readability consistency across chip implementations.

### 5.10 MMIO Dispatch Signature Inconsistency — **[QUALITY] S**
Some chips use `bus_state_t on_bus_read()` member dispatch, others use static `bus_read()` callbacks.
Both patterns work correctly. Standardize new code on one pattern.

---

## 6 — Test Coverage Gaps

### 6.1 Missing System Test Runners — **[TESTING] M**
Strong coverage: C64, NES, C16, Atari 2600, Z80, 6502, 6809, 68000.
No test runners found for:
- VIC-20 (expansion RAM configurations)
- Spectrum (contention timing, tape loading)
- Apple 1 / Apple II
- CHIP-8 / SCHIP / XO-CHIP
- Amstrad CPC
- BBC Micro
- MSX
- Sega SMS / SG-1000
- PET
- Any arcade system
- Any DDR system (KC85, Z9001, Z1013, LC80)

### 6.2 SID Audio Comparison — **[TESTING] S**
`sid_comparison_runner` exists. Verify it covers combined waveform edge cases
and 6581-vs-8580 filter divergence against latest reference recordings.

### 6.3 NES Mapper Edge Cases — **[TESTING] M**
60+ mappers implemented with consistent structure. No automated mapper regression suite
beyond Nestest basic validation. Should run Blargg mapper tests and Holy Mapperel
for systematic coverage of banking, IRQ, and mirroring edge cases.

---

## 7 — File Format Gaps

### 7.1 Supported Formats (26+)
PRG, SID, CRT, D64, T64, TAP (Commodore), iNES, NSF, A26,
SNA, Z80 snapshot, Spectrum TAP, SCL, TRD, BIN, LNX,
SSD/DSD, UEF, CPC DSK, CPR, Apple DSK/NIB/2MG, MSX CAS,
Oric TAP, VZ, KC TAP.

### 7.2 ~~Missing Formats — High Value~~ — **[DONE]**
All 9 high-value formats implemented with shared abstractions:
- `tape_common.hpp`: shared `block_t` + `load_best_entry()` used by 5 tape formats
- `disk_image_common.hpp`: shared `file_entry_t` + `load_best_entry()` used by 3 disk formats
- `format_load_helpers.hpp`: `format_apply_program()` deduplicates per-system load logic
- `CpuChipBase::set_pc()`: virtual PC setter with fam65xx/z80 overrides
- UEF gzip decompression via `os_decompress()` (libarchive raw format)

| Format | System | Status |
|--------|--------|--------|
| **DSK** | Amstrad CPC | ✅ `cpc_dsk_format` — standard + extended CPCEMU |
| **SSD/DSD** | BBC Micro | ✅ `ssd_format` — Acorn DFS catalog parsing |
| **UEF** | BBC Micro, Acorn Atom | ✅ `uef_format` — gzip support via libarchive |
| **DSK/NIB/2MG** | Apple II | ✅ `apple_dsk_format` — DOS 3.3 VTOC + T/S lists |
| **CAS** | MSX | ✅ `msx_cas_format` — binary + BASIC blocks |
| **TAP** | Oric | ✅ `oric_tap_format` — multi-program support |
| **CPR** | Amstrad CPC | ✅ `cpr_format` — RIFF/AMS! container |
| **VZ** | VTech VZ | ✅ `vz_format` — VZF0/VZF1 tape files |
| **K7/TAP** | KC85 | ✅ `kc_tap_format` — block-structured, COM/BASIC |

### 7.3 Missing Formats — Lower Priority
| Format | System | Impact |
|--------|--------|--------|
| **FDS** | NES/Famicom | Famicom Disk System images |
| **GBS** | Game Boy | Game Boy Sound format (requires GB CPU) |
| **ADF** | Amiga | Amiga Disk File (requires Amiga system) |
| **.rom** | BBC Micro | Sideways ROM loading |
| **G64** | C64 | Full GCR disk image (needs cycle-accurate drive) |
| **D71/D81** | C128 | 1571/1581 disk images |

---

## 8 — Expansion Opportunities (New Systems)

### 8.1 Systems Achievable with Existing Chips

| System | CPU | Video | Sound | Missing Pieces | Effort |
|--------|-----|-------|-------|----------------|--------|
| **CoCo 1/2** | MC6809 ✅ | MC6847 ✅ | DAC/1-bit | SAM chip, keyboard, ROM | **M** |
| **CoCo 3** | MC6809 ✅ | GIME (new) | — | GIME video chip, 512KB RAM | **L** |
| **Dragon 32/64** | MC6809 ✅ | MC6847 ✅ | 1-bit | Same as CoCo; different ROM/keyboard | **M** |
| **Game Boy** | Z80 variant | PPU (new) | APU (new) | Custom Z80 (no IX/IY), PPU, APU | **L** |
| **Sega Genesis** | M68000 ✅ | VDP (new) | YM2612 ✅ + SN76489 ✅ | Genesis VDP, Z80 sub-CPU, I/O | **XL** |
| **Neo Geo** | M68000 ✅ | LSPC (new) | YM2610 (partial ✅) | LSPC2 video, ADPCM-A/B | **XL** |
| **Sharp X68000** | M68000 ✅ | CRTC (new) | YM2151 (stub) | OPM register map, DMA, custom video | **XL** |
| **Atari 800/5200** | 6502 ✅ | ANTIC+GTIA (new) | POKEY ✅ | ANTIC display list, GTIA modes | **L** |
| **MSX turboR** | Z80 ✅ + R800 (new) | V9958 ✅ | YM2413 ✅ + PCM | R800 CPU, PCM sound | **L** |
| **Atari ST** | M68000 ✅ | Shifter (new) | YM2149 ✅ | Video shifter, GLUE, DMA, FDC | **XL** |
| **PC Engine** | HuC6280 (new) | HuC6270 (new) | HuC6280 PSG | All three chips new | **XL** |
| **SNES** | 65C816 (new) | PPU (new) | SPC700+DSP (new) | Everything new; massive scope | **XXL** |

### 8.2 NES Mapper Expansion
| Mapper | Name | Games | Effort |
|--------|------|-------|--------|
| **020** | FDS | Famicom Disk System (disk + wavetable sound) | **L** |
| **090** | JY Company | Pirate multicarts | **M** |
| DPC | — | Atari 2600 Pitfall II data fetcher coprocessor | **M** |
| DPC+ | — | Enhanced DPC with ARM coprocessor | **L** |

Note: Mappers 080, 082, 095 were previously listed but are already implemented.

### 8.3 Atari 2600 Mapper Expansion
Pitfall II DPC coprocessor is the most-requested missing mapper.
DPC+ (ARM-based homebrew) is a stretch goal.

---

## 9 — Infrastructure & Architecture

### 9.1 ~~Chip Registry Coverage~~ — **[DONE]**
All 8 chip categories have registry files. MOS6504 and MOS6509 were unregistered
— added to `fam65xx_registry.cpp` + GUI instantiations. Fixed latent `UNLIKELY`
macro bug. Three stub CPU types (CSG65CE02, CSG4510, HuC6280) have traits but
no type alias yet — intentionally excluded until instantiated. Committed `a8b920e6`.

### 9.2 ~~NES Mapper Factory Completeness~~ — **[DONE]**
88 mapper IDs across 80 files. Zero orphans, zero stale entries, zero duplicates.
Mappers 080, 082, 095 were incorrectly listed as missing in §8.2 — corrected.

### 9.3 ~~Shared Mapper Helper Expansion~~ — **[DONE]**
Analyzed boilerplate across 79 mapper files. Added `set_prg_ram()` (eliminates
3-line PRG-RAM boilerplate from ~20 mappers) and `set_chr_2x2k_4x1k()` (2×2KB +
4×1KB mixed CHR layout used by 7+ MMC3-family mappers). Removed dead helpers:
`set_chr_4k_split` (0 callers), `set_chr_1k_pages_wide` (0 callers).
Migrated 7 mappers to `set_prg_8k_banks`, 2 to `mirror_from_2bit`.
Parameterized `CPUCycleIRQ` with `IRQFireCondition` template (ON_UNDERFLOW/ON_ZERO);
migrated mappers 065 and 067. Net −75 lines. Committed `760758e4`.

### 9.4 ~~VFS File Read RAII Wrapper~~ — **[DONE]**
`VfsData` (`std::unique_ptr<uint8_t[], VfsFreeDeleter>`) already exists in `vfs.hpp`.
Storage devices converted to use it. Remaining system `load_file()` callers across
~15 systems still use raw `free()` — low-priority incremental cleanup.

### 9.5 ~~format_load_and_apply + load_raw_rom_mirrored~~ — **[DONE]**
Added `format_load_and_apply()` (full load→format→apply pipeline replacing 15-line
boilerplate) and `load_raw_rom_mirrored()` (power-of-2 ROM mirroring for cartridge
systems). Refactored 5 systems to `format_load_and_apply` (Oric, BBC Micro/Master,
Apple II, VTech VZ) and 3 to `load_raw_rom_mirrored` (ColecoVision, Spectravideo,
SG-1000). Also migrated MSX ROM loading from `fopen` to `vfs_read_file` for archive
support. Net −26 lines across 11 files. Committed `59acaa1b`.

---

## 10 — Display & Audio Pipeline

### 10.1 Apple II Artifact Color Wiring — **[INCOMPLETE] M**
`artifact_signal_shader.hpp` exists with NTSC phase-based artifact color generation,
but Apple II system does not connect to it. Apple II hi-res artifact colors are a
defining visual characteristic of the platform.

### 10.2 CRT Post-Processing — No System-Side Action Needed
CRT shader uniforms (curvature, phosphor tint, mask type, scanline gap, gamma, etc.)
are all *monitor* characteristics, not system properties. The system's contribution
to the display pipeline — signal encoding (`VideoSignalType`), interlace mode,
pixel aspect ratio, display rotation, NTSC artifact phase — is already correctly
modeled via `DisplayTraits`, shader selection, and `PhaseIncrement`. Architecture
is sound; monitor presets are a GUI/user-preference concern.

### 10.3 ~~Audio Thread Safety~~ — **[DONE]**
Audit confirmed ring buffer uses correct SPSC acquire/release ordering. Found one
data race: `use_speaker_sim_` was a non-atomic `bool` read by SDL audio callback
while toggled by GUI thread. Fixed by changing to `std::atomic<bool>` with relaxed
ordering. `speaker_sim_.reset()` is safe because `SDL_CloseAudioDevice()` guarantees
no in-flight callbacks. All perf counters use atomic relaxed increments.
Committed `41e0ff44`.

---

## 11 — Priority Recommendations

### Quick Wins (hours, high impact)
1. ~~SMS `.sms` ROM loading completion~~ (§2.5) — **DONE**
2. ~~VFS RAII wrapper~~ (§9.4) — **DONE** (already existed)
3. ~~Storage `malloc`/`free` cleanup~~ (§5.1) — **DONE**
4. ~~ROM loader MD5 stub removal~~ (§5.4) — **DONE** (already removed)
5. ~~Pin state GUI boilerplate~~ (§5.6) — **DONE**
6. ~~BBC sideways ROM (.rom) loading~~ (§2.4) — **DONE**
7. ~~Audio thread safety verification~~ (§10.3) — **DONE**
8. ~~SID combined waveform verification~~ (§1.4) — **DONE**

### Medium Effort, High Value (days)
9. ~~BBC disc format loading: SSD/DSD~~ (§7.2) — **DONE**
10. ~~Amstrad CPC DSK format loading~~ (§7.2) — **DONE**
11. Spectrum ULA memory contention (§1.1)
12. MSX2 memory mapper + sub-slot expansion (§2.7)
13. ~~Device GUI deduplication~~ (§5.2) — **DONE** (autofire slider extracted; residual minimal)
14. ~~Port definitions consolidation~~ (§5.3) — **CLOSED** (already well-structured)
15. NES mapper regression tests (§6.3)
16. Missing system test runners (§6.1)

### Large Projects (week+)
17. Apple II video rendering + disk controller (§2.3)
18. ~~BBC UEF tape~~ (§7.2) — **DONE**; sideways ROM still needed (§2.4)
19. TMS9918 VDP command engine for MSX2 (§1.2)
20. YM ADPCM-A/B decode (§1.7)
21. Oric full implementation (§2.1)
22. C1541 cycle-accurate drive mode (§1.3)
23. HD6309 full instruction coverage (§1.5)
24. OPM (YM2151) register map (§1.8)
25. Namco/Atari vector arcade ROM loading + input (§3.1, §3.2)

### Expansion Goals (multi-week)
26. NES FDS (mapper 020) — disk emulation + wavetable sound
27. Atari 2600 DPC (Pitfall II)
28. CoCo / Dragon systems (MC6809 + MC6847 reuse)
29. HuC6280 CPU → PC Engine
30. Sega Genesis (M68000 + YM2612 + SN76489 reuse)
31. Game Boy (Z80 variant + custom PPU/APU)
32. WDC 65C816 → Apple IIGS / SNES foundation

---

## 12 — System Maturity Summary

| Tier | Systems | Notes |
|------|---------|-------|
| **Production** | C64, NES/Famicom, VIC-20, PET | Full chip accuracy, tested, polished |
| **Near-Complete** | C16/Plus4, C128, Atari 2600, Amstrad CPC, Bomb Jack | Minor gaps or testing needed |
| **Functional** | Spectrum 48K/128K, MSX1, Apple 1, Acorn Atom, CHIP-8 variants, KC85, Sega SMS/SG-1000, ColecoVision | Core runs, missing formats or chip features |
| **Partial** | BBC Micro/Master, DDR (Z9001/Z1013/LC80), MSX2, Apple II, Namco arcade, Atari vector | Significant features missing; limited usability |
| **Stub** | Oric, VTech VZ, SpectaVideo, Memotech MTX, Tatung Einstein | Framework only; not runnable |

**Total: 30+ system variants across 20 board families.**

---

*This document supersedes the previous emulation-gaps.md dated 2026-04-09 (pre-format-helper).*
