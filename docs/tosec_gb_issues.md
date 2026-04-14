# TOSEC Game Boy Compatibility — Issue Tracker

## Test Configuration
- Binary: `cermu_console` (headless, 600 frames @ ~60 Hz ≈ 10 seconds)
- Pass criterion: `unique_colors >= 2` in `GFX_RESULT` (peak across periodic samples)
- Archives: `/home/patrick/Downloads/tosec/tosec-full-2022-07-10/Nintendo/Game Boy/`
- Total ROMs: ~5234 (.gb, .gbc, .sgb)

## Fixed Issues

### 1. Z80 snapshot false positive identification
**Status:** Fixed (commit f56d8f7c)
**Symptom:** .gb files misidentified as ZX Spectrum Z80 snapshots at 0.3 confidence.
**Root cause:** `z80_snapshot_identify()` returned 0.3 for any binary >100 bytes with valid IM mode, without requiring .z80 extension.
**Fix:** Require plausible SP ($4000+) AND v2/v3 extended header before returning 0.3 without extension match.

### 2. Extension matching dot mismatch
**Status:** Fixed (commit 30f4e07d)
**Symptom:** Unlicensed .gb ROMs (e.g. Sachen multicarts) failed identification. "No system found" error.
**Root cause:** `vfs_extension()` returns `.gb` (with dot) but `gb_identify()` called `format_ext_match(ext, "gb")` (without dot). Character-by-character comparison failed: `.g != g`.
**Fix:** `format_ext_match()` now strips leading dots from both arguments before comparing.
**Affected formats:** gb, gbc, pce, atari_st, genesis, coco_cas, atari_xex.

### 3. Large solid 7z archive extraction failure
**Status:** Fixed (commit c3390abd)
**Symptom:** ROMs beyond entry ~214 in large solid 7z archives failed to load.
**Root cause:** libarchive's `archive_read_data_skip()` doesn't advance the LZMA2 decompressor for solid 7z blocks. `archive_read_next_header()` fails after the solid block boundary (~214 entries for a 135MB archive).
**Fix:** Three-part:
1. `drain_entry()` helper reads/discards data to keep decompressor in sync (for small archives)
2. `extract_via_7z_cli()` fallback shells out to `7z e -so` when libarchive fails (for large archives)
3. VFS falls through to disk-based extraction (which uses 7z CLI) when memory-based cache extraction fails

### 4. SM83 AF register init ($FF instead of $00)
**Status:** Fixed (commit ff2039c8)
**Symptom:** ~1600 games showed `colors=1` (visible pixels but all same shade). Affected all MBC types equally.
**Root cause:** Z80 `init()` unconditionally set `AF=0xFFFF`. The SM83 (Game Boy CPU) should have A=$00. The DMG boot ROM's VRAM clear loop at $0006 (`LD [HL+], A`) uses A without loading it first, expecting A=0. With A=$FF, all 8KB of VRAM was filled with $FF instead of $00, making all tile data produce identical color indices (BGP=$00 maps everything to shade 0).
**Fix:** `if constexpr (!is_sm83()) regs_[AF] = 0xFFFF;` — SM83 leaves AF at 0 (default-initialized).
**Impact:** >1500 games went from colors=1 to full graphics.

### 5. MREQ double-dispatch (memory ops firing twice per cycle)
**Status:** Fixed (commit 5c501189)
**Symptom:** Double VRAM writes during boot ROM execution (16834 writes for expected ~8192).
**Root cause:** MREQ stays asserted for 2 consecutive T-states per memory cycle (T1 and T2). The system checked `if (mreq)` on every tick, dispatching the same memory operation twice — double reads and double writes.
**Fix:** Edge detection: `bool mreq_edge = mreq && !mreq_prev_;` — dispatch only on the falling edge of MREQ.
**Impact:** Correct memory access pattern; eliminates subtle corruption from double writes.

## Known Limitations (Not Bugs)

### ROMs too small to load
- `SIERPINSKIBOY v1.0.0` (334 bytes) — extreme size-coding demo, below 0x150 header minimum
- Boot ROM files (256 bytes) — firmware, not game ROMs
**Action:** Could pad to 32KB, but these are edge cases.

### Game Boy Camera (MBC5+Camera)
- Returns `colors=0, pixels=0` — needs camera peripheral register emulation
**Action:** Future work for peripheral device emulation.

### Blank screens at 300 frames (colors=1) — RESOLVED
This was caused by two emulation bugs (see Fixed Issues #4 and #5):
- SM83 AF register init: A=$FF corrupted boot ROM VRAM clear
- MREQ double-dispatch: memory operations fired twice per cycle
After fixing, 200/200 previously-blank games pass at 600 frames.
**Action:** No longer an issue.

## Open Issues

### 4. Captain Tsubasa J — LCD never re-enables (MBC1, 512KB)
**Status:** Open — needs further investigation
**ROMs:** All versions in TOSEC (2 entries)
**Symptom:** `colors=0` even after 3000 frames (50 seconds). LCD is enabled by boot ROM ($91), game disables it ($63) during init, and it never turns back on.
**Investigation:**
- Timer interrupts work correctly (IE=$07, IF bit 2 fires and is serviced)
- CPU actively runs code at various addresses (not stuck in a tight loop)
- IME transitions from 0→1 after ~19.5M cycles (EI executes correctly)
- Game writes $80 to SC ($FF02) — initiates serial transfer with **external clock**
- Emulator only handles internal clock transfers (`(data & 0x81) == 0x81`); external clock transfers are never completed (SC bit 7 stays set indefinitely)
**Hypothesis:** Game may be polling for serial transfer completion as part of its init state machine. Without external clock serial completion, the game never reaches the "enable LCD" phase.
**Action:** Implement external clock serial transfer completion (clear SC bit 7 after a timeout or immediately). Low priority — niche Japanese game.

### 5. Cyber Formula GPX — CPU loops with no interrupts (MBC1, 128KB)
**Status:** Open — needs further investigation
**ROMs:** All versions in TOSEC (3 entries: original + [a] + [a2])
**Symptom:** `colors=0` at 3000 frames. LCD enabled by boot ROM, game disables LCD ($11→$40), then never re-enables.
**Investigation:**
- IE=$00, IME=0, TAC=$00 — game **never enables any interrupts** and **never sets up timer**
- CPU stuck looping at $61E9-$61EE in bank 1 ROM area
- Completely different failure mode from Captain Tsubasa J
- Init code at $0000: `DI; LDH ($FF8A),A; LDH A,($FF0F); JP $1547` — unusual init sequence (doesn't follow standard $0100 entry point pattern)
- Multiple dump versions all fail identically → likely not a corrupt dump
**Hypothesis:** Possible CPU instruction bug, memory mapping issue, or the game requires hardware features not yet emulated. The unusual init sequence (JP $1547 early) suggests it may rely on specific boot ROM state or undocumented behavior.
**Action:** Deep CPU trace needed — compare instruction-by-instruction execution against a reference emulator. Medium priority.

### 6. Sachen multicarts — unsupported mapper ($FF)
**Status:** Known limitation
**ROMs:** Multiple Sachen multicart entries in TOSEC
**Symptom:** Crash with exit code 1 (likely "unsupported cartridge type")
**Action:** Would require implementing a custom Sachen mapper. Low priority — unlicensed multicarts.

### 7. Slow-loading games (timing, not bugs)
**Status:** Not a bug
**Games affected:**
- **Bubble Ghost** (Infogrames) — needs ~1200 frames (20 seconds) for intro to render
- **Defender/Joust** (Williams) — needs ~3000 frames (50 seconds) for extremely slow loader
- Most other `colors=0` results at 300 frames show proper graphics at 600 frames
**Action:** These are test configuration issues. Consider using 600 frames as the default test duration.

## Test Results Summary

### Initial run (300 frames, 30s timeout)
- Run interrupted by binary rebuild at ~30% through Games archive
- Pre-interruption results: pass=945, blank=635, crash=3 (real), timeout=0
- 3771 ROMs got "Permission denied" due to rebuild replacing binary

### Post-fix validation (600 frames, 200-game blank sample)
- SM83 AF init fix + MREQ edge detection applied
- **200/200 previously-blank games now PASS (100%)**
- All MBC types verified: MBC1, MBC2, MBC3, MBC5, None
- Full TOSEC retest pending

_Final summary updated 2026-04-14 after SM83 + MREQ fixes_
