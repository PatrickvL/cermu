# C64 Test Framework Revival - Progress Tracker

**Started:** 2026-02-13
**Goal:** Revive C64 test framework and run all CIA + VIC-II VICE tests

---

## Status Overview

| Component | Status | Notes |
|-----------|--------|-------|
| VICE-testprogs cloned | ✅ Done | `C:\Workspaces\Mine\VICE-testprogs` (libsidplayfp mirror) |
| c64_test_runner binary | ✅ Builds & runs | `code\cpp\build\bin\c64_test_runner.exe` |
| CMake build system | ✅ Working | VS2022, x64, Debug config |
| Test discovery | ✅ Working | 2332 tests found across 9 categories |
| Test execution | ❌ Broken | All tests TIMEOUT — completion detection fails |
| Official test list parsing | ❌ Not implemented | `testbench/c64-testlist.in` not used |

---

## Test Inventory (from `c64-testlist.in`)

| Category | exitcode | screenshot | interactive | Total |
|----------|----------|------------|-------------|-------|
| CIA | 188 | 0 | 3 | ~191 |
| VICII | 43 | 279 | 0 | ~322 |

- CIA PRG files on disk: **148**
- VICII PRG files on disk: **333**
- VICII reference PNG files: **418**
- CIA tests using `$D7FF` protocol: **50** source files
- VICII tests using `$D7FF` protocol: **49** source files

---

## Problems Found

### P1: Bus State Write Detection is Fundamentally Broken (CRITICAL)
**File:** `code/cpp/src/systems/c64/c64_test_framework.cpp` lines 871-895
**Impact:** ALL exitcode tests timeout — zero pass/fail detection

The framework monitors `$D7FF` writes by checking `c64->bus.state` after each `c64_system_tick()`. However, the tick pipeline is:

1. VIC-II tick → 2. CIA tick → 3. CPU phi2 (write to $D7FF) → 4. Memory service (processes write) → 5. **CPU phi1 (overwrites address bus with NEXT fetch)** → 6. SID tick → 7. `bus->state = s`

By the time the framework reads `bus.state`, the address bus has been overwritten by the CPU phi1 phase preparing the next instruction fetch. The `$D7FF` write address is **never visible** in the final bus state.

**Fix needed:** Either:
- (a) Add a write-interception callback/hook in the bus/memory layer that fires during step 4
- (b) Monitor the bus state between the memory service phase and the phi1 phase
- (c) Read RAM/SID register at `$D7FF` directly instead of monitoring bus lines
- (d) Add a dedicated debug register in the C64 system struct that gets written by memory_tick

### P2: Infinite Loop Detection Too Slow / Wrong Granularity
**File:** `code/cpp/src/systems/c64/c64_test_framework.cpp` lines 904-960
**Impact:** Tests that signal via `jmp *` + border color are not detected

The infinite loop detector samples PC every 1000 cycles and requires 10 consecutive matches (10,000 cycles). However:
- A `jmp *` instruction takes only 3 cycles, so the PC IS stable
- But the sampled PC might catch the CPU at different micro-states
- The 1000-cycle sampling means the check sees the KERNAL IRQ handler interspersed (if IRQs enabled)
- Tests using `SEI; jmp *` should be caught, but tests with IRQs enabled won't be

**Fix needed:** Check for `JMP *` opcode pattern (4C xx xx where target = current PC) or increase sampling resolution

### P3: Default Timeout Too Low for Many Tests
**File:** `code/cpp/src/systems/c64/c64_test_framework.cpp` line 284
**Impact:** Some tests will timeout even with working detection

Default timeout is 10,000,000 cycles. The official `c64-testlist.in` specifies:
- CIA timer tests: **200,000,000** cycles (200M)
- CIA various test (cia15): **160,000,000** cycles
- CIA dd0dtest: **100,000,000** cycles
- CIA-AcountsB tests: **100,000,000** cycles

The framework currently uses a fixed 10M default from `TestDescriptor` constructor, ignoring the official timeouts.

**Fix needed:** Parse `c64-testlist.in` to get official timeouts, or increase default significantly

### P4: KERNAL Boot Phase Takes Too Long / May Be Stuck
**File:** `code/cpp/src/systems/c64/c64_test_framework.cpp` lines 500-520
**Impact:** Performance — 2.2M cycles wasted per test even if boot succeeds earlier

The KERNAL boot runs a fixed 2,200,000 cycles without checking for completion. The PC at $FD70-$FD86 during boot is the IEC serial bus initialization loop (waiting for the serial bus to settle). On a real C64 this takes ~1.5M cycles.

The boot phase doesn't check if it reached the BASIC READY prompt early.

**Fix needed:** Add completion detection during KERNAL boot (check for $A474 BASIC cold start or $E5CD keyboard loop)

### P5: Official Test List (`c64-testlist.in`) Not Parsed
**File:** `code/cpp/src/systems/c64/c64_test_framework.cpp`
**Impact:** Missing test metadata — wrong timeouts, wrong hardware configs, wrong test types

The official VICE test list at `testbench/c64-testlist.in` contains critical per-test information:
- Test type: `exitcode`, `screenshot`, `interactive`, `analyzer`
- Timeout in cycles (per test — varies from 6M to 200M)
- Hardware requirements: `cia-old`, `cia-new`, `vicii-pal`, `vicii-ntsc`
- Expected result: `expect:error` (expected failures)
- Comments and special mount directives

Currently the framework:
- Guesses test type from filename/reference images (often wrong)
- Uses fixed 10M timeout for all tests
- Parses hardware hints from filenames only (misses explicit config)
- Doesn't know about expected failures

**Fix needed:** Implement `c64-testlist.in` parser as primary test configuration source

### P6: Interactive Tests Not Properly Skipped
**File:** `code/cpp/src/systems/c64/c64_test_framework.cpp`
**Impact:** 3 CIA interactive tests (ciaports, ghosting, hour-test) run and timeout

The framework classifies tests as `EXITCODE` by default. The `ciaports` and `ghosting` tests require keyboard input and have no `$D7FF` writes — they will always timeout. The official list marks them as `interactive`.

**Fix needed:** Use `c64-testlist.in` to identify interactive tests and skip them

### P7: Screenshot Test Implementation Incomplete
**File:** `code/cpp/src/systems/c64/c64_screenshot.cpp`
**Impact:** 279 VICII screenshot tests can't verify results

Most VICII tests (279 of ~322) are screenshot tests. The framework has screenshot comparison stubs but:
- Reference image format needs to match VICE's PNG output format
- Palette mapping between emulator and reference must be correct
- No tolerance/similarity threshold is tuned

**Fix needed:** Verify screenshot generation and comparison against VICE reference images

### P8: System Not Properly Reset Between Tests
**File:** `code/cpp/src/systems/c64/c64_test_framework.cpp` line 612
**Impact:** Tests may contaminate each other's state

The `load_test_program` function has a comment `// TODO: Implement proper system reset function`. It only resets `total_cycles` but doesn't perform a full system reset (CIA timers, VIC-II state, SID state, CPU registers, interrupt flags, PLA banking).

In `run_tests_with_auto_config`, the system is reused for tests with the same hardware config, but only re-created when hardware config changes. Between tests on the same system, there's no proper reset.

**Fix needed:** Call `c64_system_reset()` between tests, or always create fresh system

### P9: Framebuffer Memory Leak in Auto-Config Mode
**File:** `code/cpp/src/systems/c64/c64_test_framework.cpp` lines 1609-1615
**Impact:** Memory leak — framebuffer allocated per test but tracked via pointer from vicii

In `run_tests_with_auto_config`, `current_framebuffer` is assigned from the VICII's framebuffer pointer, but when the system is destroyed and recreated, the `delete[]` on the old framebuffer may not happen correctly since `create_system_for_test` does `new uint32_t[]` and `c64_system_destroy` may not free it.

**Fix needed:** Ensure framebuffers are properly freed on system destruction, or track ownership explicitly

---

## Fix Priority Order

1. **P1** — Bus state write detection (blocking: ALL tests fail)
2. **P5** — Parse `c64-testlist.in` (unlocks correct timeouts, types, hardware)
3. **P3** — Fix timeouts (required for CIA tests that need >10M cycles)
4. **P6** — Skip interactive tests (prevents wasted time)
5. **P2** — Infinite loop detection (fallback completion method)
6. **P4** — KERNAL boot optimization (performance)
7. **P8** — System reset between tests (correctness)
8. **P9** — Framebuffer memory leak (resource leak)
9. **P7** — Screenshot comparison (VICII tests)

---

## Fix Progress

| Problem | Fix Status | Details |
|---------|-----------|---------|
| P1: Bus write detection | ⬜ Not started | |
| P2: Infinite loop detection | ⬜ Not started | |
| P3: Timeout defaults | ⬜ Not started | |
| P4: KERNAL boot perf | ⬜ Not started | |
| P5: Parse c64-testlist.in | ⬜ Not started | |
| P6: Skip interactive tests | ⬜ Not started | |
| P7: Screenshot tests | ⬜ Not started | |
| P8: System reset | ⬜ Not started | |
| P9: Memory leak | ⬜ Not started | |

---

## Test Run Results

### Run 1: Initial Baseline (Pre-Fix)
- **Date:** 2026-02-13
- **Command:** `c64_test_runner --testprogs C:\Workspaces\Mine\VICE-testprogs --category CIA --filter timerbasics`
- **Result:** 0/3 passed, 3 timeout
- All tests timeout at 10M cycles
- PC at timeout: $E5CF-$E5D1 (BASIC keyboard loop area)
- $D7FF write never detected despite tests writing to it

---

## Architecture Notes

### C64 System Tick Pipeline
```
c64_system_tick():
  1. VIC-II tick        — reads prior cycle data, sets up memory access
  2. CIA1/CIA2 tick     — timers, interrupts, I/O
  3. BA→RDY wiring      — bus available signal to CPU
  4. CPU phi2 tick      — executes instruction, may write to bus
  5. Memory service     — processes read/write from CPU or VIC-II
  6. CPU phi1 tick      — prepares next fetch (OVERWRITES address bus)
  7. SID tick           — sound generation
  → bus.state = final state (has phi1 fetch address, NOT phi2 write address)
```

### VICE Test Protocol
Tests signal completion by writing to `$D7FF`:
- `$00` = PASS
- `$FF` = FAIL
Then enter `JMP *` infinite loop.

Some tests use border color instead:
- GREEN ($05) = PASS
- RED ($02) = FAIL

### Key Paths
- Test runner: `code/cpp/tools/c64_test_runner.cpp`
- Test framework: `code/cpp/src/systems/c64/c64_test_framework.h/.cpp`
- Test loader: `code/cpp/src/systems/c64/c64_test_loader.h/.cpp`
- Screenshot: `code/cpp/src/systems/c64/c64_screenshot.h/.cpp`
- C64 system tick: `code/cpp/src/systems/c64/c64.cpp`
- C64 bus/memory: `code/cpp/src/systems/c64/c64_bus.cpp`
- Official test list: `VICE-testprogs/testbench/c64-testlist.in`
- VICE-testprogs: `C:\Workspaces\Mine\VICE-testprogs`
