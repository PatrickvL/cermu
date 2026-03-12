# cermu Codebase Audit — March 2026

Comprehensive scan of the cermu source tree for optimization opportunities, memory savings, inconsistencies, code reuse, and incomplete emulation.

---

## Table of Contents

1. [Register Storage Inconsistency & Generic Debug Display](#1-register-storage-inconsistency--generic-debug-display)
2. [Audio Buffer Inconsistency](#2-audio-buffer-inconsistency)
3. [Timer Implementation Duplication](#3-timer-implementation-duplication)
4. [Incomplete Emulation — Ranked by Impact](#4-incomplete-emulation--ranked-by-impact)
5. [CPU Variants — Near-Unlock New Systems](#5-cpu-variants--near-unlock-new-systems)
6. [Code Reuse Opportunities](#6-code-reuse-opportunities)
7. [Minor Optimization Opportunities](#7-minor-optimization-opportunities)
8. [All TODO/FIXME/Stub Markers](#8-all-todofixmestub-markers)
9. [System Maturity Summary](#9-system-maturity-summary)
10. [Top 10 Actionable Items](#10-top-10-actionable-items)

---

## 1. Register Storage Inconsistency & Generic Debug Display

Chips split into two camps for register storage. Only the flat-array camp gets automatic debug registry display via `set_registers()`.

### Pattern A: Flat `uint8_t[]` array — uses `set_registers()`

| Chip | Storage | Size | File |
|------|---------|------|------|
| VIC-II | `uint8_t data[VICII_REGS_SIZE + 2]` | 66 B | `src/chip/video/vic_ii/vicii_common.h` |
| TED 7360 | `uint8_t data[TED_NUM_REGS]` | 32 B | `src/chip/video/ted/ted7360.h` |
| VIC 6560/6561 | `uint8_t registers[16]` | 16 B | `src/chip/video/vic/vic_common.h` |
| NES PPU | `uint8_t regs[REG_COUNT]` | 8 B | `src/chip/video/nes_ppu/nes_ppu.h` |
| MOS6581 (SID) | `uint8_t regs[SID_REGS_SIZE]` | 32 B | `src/chip/sound/mos6581.h` |
| MOS6526 (CIA) | `uint8_t reg[CIA_REGS_SIZE + 14]` | 31 B | `src/chip/io/mos6526.h` |
| MC6845 (CRTC) | `uint8_t regs[MC6845_NUM_REGISTERS]` | 18 B | `src/chip/video/mc6845/mc6845.h` |
| PIA6532 (RIOT) | `uint8_t regs_[4]` + ref aliases | 4 B | `src/chip/io/pia6532.h` | ✅ Migrated |
| PIA6820 | `uint8_t regs_[6]` + ref aliases | 6 B | `src/chip/io/pia6820.h` | ✅ Migrated |
| MOS6522 (VIA) | `uint8_t regs_[16]` + ref aliases + `io_port` views | 16 B | `src/chip/io/mos6522.h` | ✅ Migrated |

### Pattern B: Individual member fields — uses `UIntFn` callbacks

| Chip | Regs | File | Notes |
|------|------|------|-------|
| NES APU | 24 (5 channels) | `src/chip/sound/nes_apu.h` | Channel objects with per-member state |
| TIA | 45 write + 14 read | `src/chip/video/tia/tia.h` | `write_regs[45]`, `read_regs[14]` + scattered state |

### Opportunity

- [x] ~~Refactor VIA, RIOT, PIA6820~~ → All three migrated to flat arrays with reference aliases for zero external churn. VIA uses `regs_[16]` with `io_port` views mapped over the DDR/data bytes in the array (same pattern as CIA). Timer counters remain as `uint16_t` for hot-path performance. Dead fields (`interrupt_flags`, `interrupt_enable`) removed.
- [x] Call `set_registers()` in their constructors so `RegSource` offsets "just work" for the debug registry.
- [x] Debug registration simplified: port data/DDR fields now use `RegSource` offsets instead of `UIntFn` lambdas.
- [x] A single generic "Registers" hex-dump section is now auto-generated for all chips with `set_registers()` — collapsed by default, 16-bytes-per-row hex dump appended to the debug panel.

**Status:** ✅ Completed (RIOT: `c36cc6fc`, PIA6820: `64dbacd0`, VIA: `7f47c596`, hex-dump: `20a586fc`).

---

## 2. Audio Buffer Inconsistency

Three different audio buffering strategies coexist:

| System | Approach | Location |
|--------|----------|----------|
| C64 / VIC-20 | `AudioRingBuffer` (shared utility) | `src/utils/ring_buffer.hpp` |
| NES | `std::vector<float>` (collect-then-drain) | `src/systems/nes/nes_system.h:145` |
| ~~Atari 2600 (TIA)~~ | ~~`float[4096]` static array~~ → `AudioRingBuffer` | `src/chip/video/tia/tia.h` | ✅ Migrated |

### Opportunity

- [x] Migrate TIA to `AudioRingBuffer` — saved 16 KB static allocation, unified interface.
- [ ] NES could also migrate but its vector-append pattern is functional for the "collect then drain" model — lower priority.

**Status:** ✅ Completed.

---

## 3. Timer Implementation Duplication

Four chips independently implement countdown timer logic with ~95% overlap:

| Chip | Width | Latches | Auto-reload | Location |
|------|-------|---------|-------------|----------|
| CIA (MOS6526) | 16-bit | 2× | Yes | `src/chip/io/mos6526.h:65–75` |
| VIA (MOS6522) | 16-bit | 2× | Yes | `src/chip/io/mos6522.h:98–101` |
| RIOT (PIA6532) | 8-bit | 1× | Wrap-only | `src/chip/io/pia6532.h` |
| TED 7360 | 16-bit | 3× | Yes | `src/chip/video/ted/ted7360.h:308–351` |

### Opportunity

- [x] ~~Extract a `CountdownTimer<BITS>` template~~ → **Skipped.** Analysis showed timer implementations differ too much in reload semantics, interrupt behavior, and chip-specific quirks (CIA force-load, VIA one-shot vs free-run, RIOT divider switching, TED raster-sync) to justify a shared abstraction. The overhead of parameterizing all variants would negate the savings.
- [ ] The `TimerMeta` struct in `ChipDebugRegistry` already exists — a generic timer plugs right in.

**Status:** ⏭️ Skipped with justification — timer implementations are more different than alike.

---

## 4. Incomplete Emulation — Ranked by Impact

### Critical (System-Blocking)

| Item | Status | Location | Impact |
|------|--------|----------|--------|
| **C16/Plus4 — TED 7360 not wired** | Skeleton stubs throughout | `src/systems/commodore/c16/c16_config.h:6`, `c16_bus.h:6` | Two complete platforms non-functional |
| **PET — CRTC display rendering** | MC6845 chip exists; PET-side char→framebuffer pipeline incomplete | `src/chip/video/mc6845/mc6845.h` (chip done), PET system glue missing | PET has no visual output |

### High (Game Compatibility)

| Item | Location | Impact |
|------|----------|--------|
| **NES MMC5 expansion audio** | `src/systems/nes/cartridge/mappers/mapper_005_mmc5.h:398` — `$5000–$5015` stubbed | 20+ games (Castlevania III, Just Breed, etc.) |
| **NES MMC5 vertical split** | `mapper_005_mmc5.h:466` — stubbed | Same games as above |
| **NES Sunsoft 5B audio** | Mapper 069 — Yamaha YM2149 not implemented | Gimmick!, etc. |
| **1541 SAVE channel** | `src/devices/storage/drive_1541.cpp:195` — not implemented | Cannot save to disk images |
| **TAP tape format** | `src/systems/commodore/commodore_load_helpers.cpp:253` | No tape image loading |
| **CRT cartridge loading** | `commodore_load_helpers.cpp:261` — incomplete | Some cartridge types fail |

### Medium (Feature Gaps)

| Item | Location |
|------|----------|
| Apple 1 binary file loading | `src/systems/apple1/apple1_system.cpp:331` |
| ROM MD5 verification | `src/core/storage/rom_loader.cpp:132` |
| C64 test framework BASIC auto-run | `src/testing/c64_test_framework.cpp:1032` |
| C64 test framework HTML reports | `c64_test_framework.cpp:1726` |
| C64 test framework JSON result loading | `c64_test_framework.cpp:1731` |
| Memory viewer for generic systems | `src/gui/system_gui.cpp:952` |
| Window title for container-loaded ROMs | `src/gui/system_gui.cpp:1462` |
| Kernal patches hardcoded | `src/main/multi_emu_main.cpp:209` — should be configurable registry |

### Low (Edge Cases / Refinements)

| Item | Location | Notes |
|------|----------|-------|
| CIA 50/60Hz toggle — reset cycle counter? | `src/chip/io/mos6526.cpp:457` | Theoretical edge case |
| CIA BCD nibble overflow verify | `mos6526.cpp:631–632` | Passes known tests |
| VIC-II graphics sequencer during border | `src/chip/video/vic_ii/vicii_common.cpp:653` | Hidden by overscan |
| PLA RP4 for Ultimax cartridge | `src/chip/logic/pla.cpp:69` | Defaults work |
| SID test sound generation (debug feature) | `src/chip/sound/mos6581_gui.cpp:203` | GUI-only |
| Signetics 2513 charrom data placeholder | `src/chip/video/signetics2513/signetics2513.h:227,232` | Works for known Apple 1 software |

---

## 5. CPU Variants — Near-Unlock New Systems

The `fam65xx` traits system defines CPU variants that would enable new platforms. Headers exist at `src/chip/cpu/fam65xx/`.

### Defined CPUs with trait headers

| CPU | Header | Core Status | Key Missing Feature |
|-----|--------|-------------|---------------------|
| MOS6502 | `mos6502.h` | ✅ Fully working | — |
| MOS6504 | `mos6504.h` | ✅ Working (13-bit addr) | — |
| MOS6507 | `mos6507.h` | ✅ Working (13-bit addr, no IRQ) | — |
| MOS6509 | `mos6509.h` | Traits only | 20-bit indirect-Y addressing |
| MOS6510 | `mos6510.h` | ✅ Fully working (I/O port) | — |
| MOS7501 | `mos7501.h` | ✅ Working (6510 variant, no NMI) | — |
| CSG8502 | `csg8502.h` | Traits only | Clock switching |
| Ricoh 2A03 | `ricoh_2a03.h` | ✅ Fully working (no BCD, APU) | — |
| WDC 65C02 | `wdc65c02.h` | ✅ CMOS instructions working | — |
| WDC 65C02S | `wdc_w65c02s.h` | ✅ WAI/STP working | — |
| Rockwell 65C02 | `rockwell65c02.h` | ✅ Bit manipulation working | — |
| Synertek 65C02 | `synertek65c02.h` | ✅ CMOS base working | — |
| 65CE02 | `csg65ce02.h` | Traits defined | Z register, PHZ/PLZ, TAZ/TZA, BASE page NOT IMPL |
| CSG4510 | `csg4510.h` | Traits defined | MAP instruction, DMA NOT IMPL |
| HuC6280 | `hudson_huc6280.h` | Traits defined | TII/TAM/TMA/CSH/CSL NOT IMPL; PSG NOT IMPL |
| WDC 65C816 | `wdc65c816.h` | 16-bit mode IMPL | — |
| Ricoh 5A22 | `ricoh_5a22.h` | Traits defined | DMA/HDMA NOT IMPL |

### Systems Unlockable — Ranked by Effort

#### Tier 1: Lowest Effort (reuse existing chips)

| System | Reuses | New Chips Needed | Est. Effort |
|--------|--------|------------------|-------------|
| **BBC Micro** | MOS6502 + MC6845 CRTC | SN76489 (sound) + system glue | ~800 lines |
| **Commodore 128 (C64 mode)** | MOS6510 + VIC-II + SID + CIA×2 | MMU 8722 (banking), optionally VDC 8563 (80-col) | ~500–1500 lines |

#### Tier 2: Medium Effort

| System | Reuses | New Chips Needed | Est. Effort |
|--------|--------|------------------|-------------|
| **Atari 8-bit (400/800/XL)** | MOS6502 | ANTIC + GTIA (video), POKEY (sound/IO) | ~2000+ lines |
| **PC Engine / TG-16** | HuC6280 (needs impl) | HuC6270 VDC + PSG channels | ~3000+ lines |

#### Tier 3: High Effort

| System | Reuses | New Chips Needed | Est. Effort |
|--------|--------|------------------|-------------|
| **SNES** | 65C816 / 5A22 | Full PPU (Mode 7, OAM), SPC700/DSP, DMA | ~5000+ lines |

---

## 6. Code Reuse Opportunities

### Already Well-Shared ✅

| Component | Mechanism | Notes |
|-----------|-----------|-------|
| CPU family | `fam65xx_t<CPUTraits>` NTTP | Exemplary design |
| Keyboard matrix | `keyboard_matrix_config_t` | All 4 Commodore systems use same framework |
| Commodore file formats | `commodore_load_helpers.cpp` | Shared across C64, VIC-20, C16 |
| Port / peripheral framework | `Port`, `PeripheralDevice` | Generic across all systems |
| MOS6522 (VIA) | `src/chip/io/mos6522.h` | Shared by VIC-20, PET, Apple 1 |
| MC6845 (CRTC) | `src/chip/video/mc6845/mc6845.h` | Used by PET; ready for BBC Micro |

### Could Be Shared More

| Component | Current State | Opportunity |
|-----------|---------------|-------------|
| Timer logic | 4 independent implementations | Template in `src/utils/` |
| Audio ring buffer | 3 different approaches | Standardize on `AudioRingBuffer` |
| NES expansion audio | Per-mapper inline stubs | Shared expansion audio chip classes |

### Not Worth Factoring

| Component | Reason |
|-----------|--------|
| System tick loops | Unique chip tuples per system; pattern is identical but not abstractable without overhead |
| Bus dispatch strategies | Context-appropriate per system (C64 PLA modes, NES block dispatch, flat addressing) |

---

## 7. Minor Optimization Opportunities

| Area | Details | Saving |
|------|---------|--------|
| ~~**TIA static audio buffer**~~ | ~~16 KB `float[4096]`~~ → `AudioRingBuffer` | ✅ Done — 16 KB saved |
| **NES PPU sprite eval state** | ~250 bytes of evaluation temporaries could be bit-packed | 30–50 bytes |
| ~~**CPU decode tables**~~ | ~~Each `fam65xx_t<>` instantiation carries decode tables~~ → `SharedOpcodeTable<Key>` keyed on instruction-set-relevant flags | ✅ Done — all 5 NMOS variants share one table (`7bddaa2a`) |

**Note:** Hot paths (bus dispatch, CPU execution) are already well-optimized with page-pointer tables, block dispatch, branchless arithmetic, and `LIKELY()`/`UNLIKELY()` hints. No major inefficiencies found on critical paths.

---

## 8. All TODO/FIXME/Stub Markers

### C16/Plus4 System (Stubs)

| File | Line | Comment |
|------|------|---------|
| `c16_config.h` | 6 | `// Stub C16 configuration structure — TODO: Implement proper C16 config when MOS7501 CPU and TED 7360 chips are available` |
| `c16_bus.h` | 6 | `// Stub C16 bus structure — TODO: Implement proper C16 bus` |
| `c16_system.cpp` | 1139 | `// Stub: all input lines HIGH (no external devices connected yet)` |
| `c16_system.cpp` | 1147–1148 | `// Stub: ignore output for now — TODO: Handle cassette motor, serial bus` |
| `c16_system.cpp` | 1058, 1117 | ACIA, PIO1, ROM banking not implemented |

### NES Mappers

| File | Line | Comment |
|------|------|---------|
| `mapper_005_mmc5.h` | 398 | `// Sound registers $5000-$5015 (stub — not implemented)` |
| `mapper_005_mmc5.h` | 466 | `// Vertical split (stub)` |
| `nes_mapper_factory.h` | ~133–139 | Mappers 42–63 missing; Mapper 68 CHR-ROM nametable replacement; Mapper 69 Yamaha 5B audio |

### Storage / File I/O

| File | Line | Comment |
|------|------|---------|
| `drive_1541.cpp` | 195 | SAVE channel (Channel 1) not implemented |
| `drive_1541.cpp` | 971 | LISTEN data byte path missing |
| `commodore_load_helpers.cpp` | 253 | TAP tape format not supported |
| `commodore_load_helpers.cpp` | 261 | CRT cartridge loading incomplete |
| `apple1_system.cpp` | 331 | `// TODO: Implement binary file loading` |
| `rom_loader.cpp` | 132 | MD5 verification not implemented |

### Test Infrastructure

| File | Line | Comment |
|------|------|---------|
| `c64_test_framework.cpp` | 1032 | `// TODO: Implement minimal BASIC interpreter or parse SYS command` |
| `c64_test_framework.cpp` | 1037 | `// BASIC loader protocol not yet fully implemented` |
| `c64_test_framework.cpp` | 1726 | HTML report generation missing |
| `c64_test_framework.cpp` | 1731 | Loading previous results from JSON missing |

### GUI / Infrastructure

| File | Line | Comment |
|------|------|---------|
| `system_gui.cpp` | 952 | Memory viewer not yet implemented for generic systems |
| `system_gui.cpp` | 1462 | Window title not updated for container-loaded ROMs |
| `multi_emu_main.cpp` | 209 | C64 kernal patches hardcoded; should be in configurable registry |
| `c64_kernal_patches.h` | 10 | Skip-memtest flag could be set automatically |

### Chip-Level Refinements

| File | Line | Comment |
|------|------|---------|
| `mos6526.cpp` | 457 | Should toggling 50/60Hz reset cycle counter? |
| `mos6526.cpp` | 631–632 | BCD nibble overflow — verify `== 0x0A?` and `& 0xF0?` |
| `vicii_common.cpp` | 653 | Graphics sequencer behavior during left/right border |
| `pla.cpp` | 69 | Let cartridge/EXROM set RP4 (A12–A15 during Ultimax) |
| `mos6581_gui.cpp` | 203 | Generate test sound (debug feature) |
| `signetics2513.h` | 227, 232 | Charrom data placeholder/unverified |

### CPU Features (Traits-Guarded)

| Feature | File | Line | Status |
|---------|------|------|--------|
| 65CE02 instructions | `fam65xx_processor_traits.hpp` | 38 | NOT IMPLEMENTED |
| HuC6280 instructions | `fam65xx_processor_traits.hpp` | 42 | NOT IMPLEMENTED |
| Variable clock (8502, HuC6280) | `fam65xx_processor_traits.hpp` | 79 | NOT IMPLEMENTED |
| MOS6509 20-bit addressing | `fam65xx_processor_traits.hpp` | 90 | NOT IMPLEMENTED |
| HuC6280 PSG | `fam65xx_processor_traits.hpp` | 103 | NOT IMPLEMENTED |
| CSG4510 DMA | `fam65xx_processor_traits.hpp` | 109 | NOT IMPLEMENTED |
| Ricoh 5A22 DMA/HDMA | `fam65xx_processor_traits.hpp` | 110 | NOT IMPLEMENTED |

---

## 9. System Maturity Summary

| System | Status | Completeness | Key Gaps |
|--------|--------|--------------|----------|
| **C64** | ✅ Production | ~95% | 1541 SAVE, TAP loading, CRT types |
| **NES** | ✅ Production | ~80% | 51/256 mappers; expansion audio (MMC5, 5B, VRC6/7) |
| **CHIP-8** | ✅ Production | ~90% | CHIP8/SCHIP/XO-CHIP all working |
| **BBC Micro** | ✅ New | ~60% | Model B emulation; 6502 + MC6845 + SN76489 + system glue | ✅ Added |
| **VIC-20** | ⚠️ Functional | ~70% | Core working; some ROM config TODOs |
| **Atari 2600** | ⚠️ Functional | ~70% | 11/256 mappers; TIA functional |
| **Apple 1** | ⚠️ Functional | ~65% | Binary loading missing; clean architecture |
| **PET** | ⚠️ Functional | ~55% | CRTC rendering incomplete; audio minimal |
| **C16/Plus4** | 🔴 Early Dev | ~35% | TED 7360 and bus entirely stubbed |

---

## 10. Top 10 Actionable Items

| # | Action | Category | Effort | Status |
|---|--------|----------|--------|--------|
| 1 | Unify register storage → `uint8_t regs_[]` + named accessors in RIOT, PIA6820, VIA | Consistency | Medium | ✅ Done (RIOT: `c36cc6fc`, PIA6820: `64dbacd0`, VIA: `7f47c596`). VIA uses `io_port` views over `regs_[]` bytes — same pattern as CIA. |
| 2 | Standardize TIA audio on `AudioRingBuffer` | Memory / consistency | Low | ✅ Done — saved 16 KB |
| 3 | Extract `CountdownTimer<>` template to `src/utils/` | Code reuse | Medium | ⏭️ Skipped — timer impls too different to abstract |
| 4 | Wire PET CRTC→framebuffer pipeline | Incomplete emulation | Medium | ⬚ Not started |
| 5 | Implement NES MMC5 expansion audio ($5000–$5015) | Incomplete emulation | Medium | ⬚ Not started |
| 6 | Implement 1541 SAVE channel | Incomplete emulation | Medium | ⬚ Not started |
| 7 | Add TAP tape format loading | Incomplete emulation | Low–Medium | ⬚ Not started |
| 8 | C16/Plus4 TED 7360 wire-up | New system | High | ⬚ Not started |
| 9 | BBC Micro system (6502 + MC6845 + SN76489) | New system | Medium | ✅ Done — full Model B emulation |
| 10 | Commodore 128 system (reuse VIC-II/SID/CIA + MMU) | New system | Medium–High | ⬚ Not started |

### Additional Completed Work (outside Top 10)

| Action | Category | Commit |
|--------|----------|--------|
| Deduplicate fam65xx opcode tables via `SharedOpcodeTable<Key>` | Optimization | `7bddaa2a` |
| VIA: migrate to `regs_[16]` with `io_port` views over register array | Consistency | `7f47c596` |
| Auto-generate Registers hex-dump for all chips with `set_registers()` | Debug / consistency | `20a586fc` |

---

*Generated: 2026-03-08. Last updated: 2026-03-09. Confidence: 0.85 — based on thorough automated code inspection. Line numbers verified at time of writing; may drift with future commits.*
*VIA migration + register hex-dump: 2026-03-09.*
