# C++ Chip/System Class Hierarchy Refactoring Plan

> **Goal:** Transform the chip and system type hierarchy into idiomatic C++ — direct class
> inheritance instead of C-style wrappers, templates instead of code duplication, `unique_ptr`
> instead of raw `void*`, and zero runtime overhead where the current code already achieves it.
>
> **Constraint:** No performance regression. The fam65xx NTTP template pattern (compile-time
> `if constexpr` dispatch, empty base optimization) is the gold standard — extend it, don't
> weaken it.

---

## Current Architecture (as of 2025-07)

### Type Inventory

| Type | Location | Role | Status |
|------|----------|------|--------|
| `ChipBase` | `chip.h` | Abstract GUI interface (7 virtuals) | **Active** — all chip GUI goes through this |
| `CChipAdapter` | `chip.h` | Wraps `void*` C-struct + 3× `std::function<void()>` as ChipBase | **Active** — only concrete ChipBase subclass |
| `ChipIdentity` | `chip.h` | POD `{part_number, manufacturer}` | **Active** |
| `ChipDescriptor` | `chip.h` | C-style vtable: `create`, `destroy`, `bus_attach`, `bank_change`, render ptrs | **Legacy** — render ptrs all NULL'd; create/destroy still used |
| `ChipEntry` | `chip.h` | `{void* chip, ChipDescriptor*, size, base_address, chip_id}` slot | **Legacy** — System8Bit only |
| `chip_callback_t` | `chip.h` | `std::function<bus_state_t(void*, bus_state_t)>` | **Dead** — declared, never used |
| `System8Bit` | `system.h/cpp` | Fixed `array<ChipEntry, 16>` chip container | **Legacy** — C64 test framework only |
| `system_8bit_t` | `system.h/cpp` | C wrapper around `System8Bit*` | **Legacy** — C64 test framework only |
| `SystemChip` | `emulated_system.h` | `{unique_ptr<ChipBase>, display_name, short_name, category, base_address, show_detached}` | **Active** |
| `EmulatedSystem` | `emulated_system.h/cpp` | Base class: `vector<SystemChip>` registry + 13 pure virtuals | **Active** |
| `CommodoreSystem` | `commodore_system.h` | Intermediate base for Commodore systems (keyboard, mapper, video timing) | **Active** |
| `emulation_context_t` | `emulation_context.h/cpp` | Vestigial chip context manager | **Dead** — 0 callers, 0 includes outside itself |
| `fam65xx_chip_descriptor_t` | `fam65xx_types.h` | Extended `ChipDescriptor` with `mem_read`/`mem_write` C-function pointers | **Active** — CPU read/write callback wiring |

### Registration Patterns by System

| System | Base Class | Chip Lifecycle | Uses `ChipDescriptor`? | Uses `System8Bit`? |
|--------|-----------|----------------|------------------------|--------------------|
| C64 test framework (`c64.cpp`) | `system_8bit_t` (C) | `desc->create`/`destroy` via `System8Bit` | **Yes** (full) | **Yes** |
| C64 emulator (`c64_system.cpp`) | `CommodoreSystem` | `desc->create`/`destroy` per-chip, manual | **Yes** (create/destroy only) | No |
| VIC-20 (`vic20_system.cpp`) | `CommodoreSystem` | Mixed: factories + `desc->create` + raw `free()` | **Yes** (some chips) | No |
| C16/Plus4 (`c16_system.cpp`) | `CommodoreSystem` | C++ ctor/dtor | No | No |
| Apple 1 (`apple1_system.cpp`) | `EmulatedSystem` | C++ ctor/dtor; CPU via `fam65xx_chip_descriptor_t` | **Yes** (CPU only) | No |
| NES (`nes_system.cpp`) | `EmulatedSystem` | `shared_ptr` (PPU, bus) + factory (CPU) | No | No |
| CHIP-8 (`chip8_system.cpp`) | `EmulatedSystem` | C++ ctor/dtor (= default) | No | No |

### Critical Coupling Point: `io_port_mixin_t` → `bank_change`

The **only** runtime use of `ChipDescriptor` during emulation ticking is:
```
io_port_mixin_t::write_io_ddr/write_io_data
  → descriptor->bank_change(chip_instance, banking_bits)
    → c64_cpu_banking_callback(c64_t*, bits)
      → c64_bus_on_banking_change()
        → PLA reconfiguration
```
This is the hottest legacy coupling — it runs every time the CPU I/O port bits 0–2 change
(memory banking). Replacing `chip_descriptor_t*` + `void*` with a typed callback is
**Phase 2's** critical task.

---

## Phased Refactoring Plan

### Phase 0: Delete Dead Code ✅ COMPLETE

**Status:** All dead code removed, builds verified.

| Task | Status |
|------|--------|
| Delete `emulation_context_t` | ✅ Deleted `emulation_context.h` + `.cpp`, removed from CMakeLists.txt |
| Delete `chip_callback_t` typedef | ✅ Removed from `chip.h`, also removed dead `AccessCallback` from `system.h` |
| Remove render function pointer slots from `ChipDescriptor` | ✅ Removed `render_debug_window`/`render_settings_window` from struct |
| Remove render pointers from all descriptors | ✅ Removed from 13 designated-initializer descriptors + 5 CPU imperative descriptors |
| Delete `fam65xx_chip_descriptor_t` | ✅ Removed struct + all 3 supporting functions (`create_descriptor`, `init_enhanced`, `destroy_descriptor`) — discovered all callers were no-ops |

---

### Phase 1: Typed Chip Lifecycle — Eliminate `desc->create` / `desc->destroy` ✅ COMPLETE

**Status:** Implemented — all systems migrated to typed create/destroy

**Summary of changes:**

#### 1a. VIC-20 — Normalize Lifecycle Inconsistencies ✅

| Chip | Change |
|------|--------|
| VIC (mos6560/6561) | `void* create(desc*)` → `mos6560_t*/mos6561_t* create()`, `void destroy(void*)` → typed. Fixed `free(vic_)` bug → proper `mos6560_destroy()`/`mos6561_destroy()` dispatch based on `is_pal`. |
| VIA (mos6522) | `void* create(desc*)` → `mos6522_t* create()`, `void destroy(void*)` → `void destroy(mos6522_t*)`. |
| CPU (mos6502) | Removed dead `fam65xx_chip_descriptor_t` ceremony (create_descriptor → init_enhanced → destroy_descriptor did nothing — `init()` ignores desc, `mem_read`/`mem_write` fields unused). Now calls `mos6502_init(cpu, nullptr)`. |

- Removed `void* desc` field from `vic_base_t` (unused after typed lifecycle).
- All VIC-20 callers in `vic20_system.cpp` updated to typed create/destroy.

#### 1b. C64 System — Decouple from `desc->create` / `desc->destroy` ✅

| Chip | Change |
|------|--------|
| RAM | `void* ram_system_create(desc*)` → `ram_t* ram_create()` + `void ram_destroy(ram_t*)` |
| ROM | `void* rom_system_create(desc*)` → `rom_t* rom_create()`, `rom_create_with_size(size)` + `void rom_destroy(rom_t*)` |
| MOS2114 | `void* mos2114_create(desc*)` → `mos2114_t* mos2114_create()` + `void mos2114_destroy(mos2114_t*)` |
| MOS6526 | `void* mos6526_system_create(desc*)` → `mos6526_t* mos6526_create()` + `void mos6526_destroy(mos6526_t*)` |
| MOS6581 | `void* mos6581_system_create(desc*)` → `mos6581_t* mos6581_create()` + `void mos6581_destroy(mos6581_t*)` |
| VIC-II | `vicii_system_create(desc*, config, bank_change)` → `vicii_create(config, bank_change)`, `mos6569_create()`/`mos6567_create()` + `void vicii_destroy(vicii_t*)` |

- All chip descriptors updated to use forwarding lambdas (e.g. `.create = [](desc*) -> void* { return mos6526_create(); }`)
- `c64_system.cpp`: Removed `create_chip()`/`destroy_chip()` helpers, replaced with direct typed calls
- `c64.cpp` (test framework): Still uses `desc->create()`/`desc->destroy()` through `System8Bit` — works via lambda forwarding. Full migration deferred to Phase 4.

#### 1c. Apple 1 — Decouple CPU Initialization ✅

- Removed dead `fam65xx_chip_descriptor_t` ceremony from Apple 1, VIC-20, and both test harnesses (Klaus, Lorenz).
- Deleted `mos6502_create_descriptor()`, `mos6502_init_enhanced()`, `mos6502_destroy_descriptor()` (all callers removed).
- Deleted `fam65xx_chip_descriptor_t` struct from `fam65xx_types.h` (no remaining consumers).

**Post Phase 1 status:** `ChipDescriptor::create`/`destroy` slots still exist with lambda forwarding.
They are consumed by `c64.cpp` test framework via `System8Bit::destroy_all_chips()`.
Full removal deferred to Phase 4 (eliminate `System8Bit`).

---

### Phase 2: Replace `bank_change` Coupling in `io_port_mixin_t` ✅ COMPLETE

**Status:** Implemented using Option A (function pointer). All systems updated.

**Summary of changes:**

| Component | Change |
|-----------|--------|
| `io_port_mixin_t` | Replaced `chip_descriptor_t* descriptor` + `void* chip_instance` with `bank_change_fn_t bank_change_fn` + `void* bank_change_ctx` |
| `write_io_ddr/data` | Call `bank_change_fn(bank_change_ctx, bits)` instead of `descriptor->bank_change(chip_instance, bits)` |
| `mos6510_init` | No longer sets descriptor/chip_instance on mixin |
| `mos6510_set_bank_change_context` | Replaced with `mos6510_set_bank_change(cpu, fn, ctx)` — sets both callback and context |
| `mos7501_set_bank_change_context` | Replaced with `mos7501_set_bank_change(cpu, fn, ctx)` |
| C64 (c64_system.cpp) | `mos6510_set_bank_change(cpu, cpu_banking_callback, c64_)` — per-instance, no global descriptor mutation |
| C64 (c64.cpp) | Same pattern with `c64_cpu_banking_callback` |
| C16 (c16_system.cpp) | Removed dead `mos7501_set_bank_change_context()` calls (descriptor bank_change was always nullptr) |

**Solution: Typed callback, zero overhead**

Replace the `chip_descriptor_t*` + `void*` pair with a typed callable stored as a template
parameter or a function pointer:

```cpp
// Option A: Function pointer (simplest, zero overhead)
template <const CPUTraits &Traits>
struct io_port_mixin_t {
    using bank_change_fn_t = void(*)(void* context, uint8_t banking_bits);
    bank_change_fn_t bank_change_fn_ = nullptr;
    void*            bank_change_ctx_ = nullptr;
    // ...
    void notify_bank_change(uint8_t bits) {
        if (bank_change_fn_) bank_change_fn_(bank_change_ctx_, bits);
    }
};

// Option B: Template callback type (eliminates void* entirely)
template <const CPUTraits &Traits, typename BankChangeCallback = null_callback>
struct io_port_mixin_t {
    [[no_unique_address]] BankChangeCallback bank_change_;
    // ...
    void notify_bank_change(uint8_t bits) {
        if constexpr (!std::is_same_v<BankChangeCallback, null_callback>)
            bank_change_(bits);
    }
};
```

**Recommendation:** Option A (function pointer). Reasons:
- Same overhead as current `descriptor->bank_change` (1 indirect call).
- Doesn't add template parameters to `fam65xx_t`, which would multiply instantiations.
- `bank_change` is called rarely (only on banking bit transitions, not every cycle).
- Simpler migration path — just replace `descriptor` + `chip_instance` with
  `bank_change_fn_` + `bank_change_ctx_`.

**Actions:**
1. Add `bank_change_fn_t` + `bank_change_ctx_` to `io_port_mixin_t`.
2. Replace `descriptor->bank_change(chip_instance, bits)` calls with
   `bank_change_fn_(bank_change_ctx_, bits)`.
3. In C64 init: wire `bank_change_fn_ = cpu_banking_callback`, `bank_change_ctx_ = c64`.
4. Remove `chip_descriptor_t* descriptor` and `void* chip_instance` from the mixin.
5. Remove `mos6510_set_bank_change_context()` setter — replace with a direct assignment API.

**After Phase 2:** `ChipDescriptor::bank_change` slot is unused. Combined with Phase 1,
the entire `ChipDescriptor` struct is now unused except for `bus_attach`.

---

### Phase 2b: Eliminate `bus_attach` and Remove `ChipDescriptor` Entirely ⚠️ PARTIALLY COMPLETE

**Status:** `bank_change` field removed from `ChipDescriptor` struct and all descriptor
initializers. Full `ChipDescriptor` removal blocked by `c64.cpp` test framework (uses
`desc->create`, `desc->destroy`, `desc->bus_attach` through `System8Bit`). Deferred to
Phase 4.

**Completed:**
- Removed `bank_change` field from `ChipDescriptor` struct in `chip.h`
- Removed `.bank_change = ...` from all 19 descriptor initializers
- Removed `mos6510_descriptor.bank_change = callback` mutations from `c64_system.cpp` and `c64.cpp`
- Removed dead `mos7501_set_bank_change_context()` calls from `c16_system.cpp`

**Remaining (blocked, deferred to Phase 4):**
- `desc->bus_attach()` still called in `c64.cpp` line 756 (System8Bit chip iteration)
- `desc->create()` / `desc->destroy()` still used by `c64.cpp` and `system.cpp`
- `ChipDescriptor` struct cannot be deleted until `System8Bit` is eliminated
- 19 static `chip_descriptor_t` definitions still exist (needed by above)

---

### Phase 3: Convert C-Struct Chips to C++ Classes Inheriting `ChipBase`

**Effort:** Large (1–2 days total, but each chip is independent)
**Risk:** Medium — largest structural change
**Files affected:** ~30–50

**Goal:** Each chip is a proper C++ class that directly inherits `ChipBase` and implements
its GUI rendering as virtual overrides. `CChipAdapter` becomes unnecessary.

**Current state (all chips):**
```
C-struct chip (e.g. mos6522_t)
  ↓ wrapped by
CChipAdapter(void* chip, identity, debug_lambda, settings_lambda, layout_lambda)
  ↓ stored as
unique_ptr<ChipBase> in SystemChip
```

**Target state:**
```
C++ chip class (e.g. MOS6522 : public ChipBase)
  ↓ stored as
unique_ptr<ChipBase> in SystemChip
```

#### Migration Strategy: Chip-by-Chip

Each chip conversion follows the same pattern:

1. **Wrap the C struct in a C++ class:**
   ```cpp
   class MOS6522 : public ChipBase {
       mos6522_state_t state_;  // Embed the existing state struct (renamed from mos6522_t)
   public:
       // Emulation API — same functions, now as methods
       void reset();
       void tick();
       uint8_t read(uint16_t addr);
       void write(uint16_t addr, uint8_t data);

       // ChipBase overrides — move from lambda to virtual
       ChipIdentity chip_identity() const override { return {"MOS6522", "MOS Technology"}; }
       bool has_debug_content() const override { return true; }
       void render_debug_content() override;    // was: mos6522_render_debug_content(chip)
       void render_layout_content() override;   // was: mos6522_render_layout_content(chip)
   };
   ```

2. **System class stores `unique_ptr<MOS6522>` (typed) instead of raw `mos6522_t*`:**
   ```cpp
   class VIC20System {
       std::unique_ptr<MOS6522> via1_;
       std::unique_ptr<MOS6522> via2_;
   };
   ```

3. **Registration passes the typed pointer directly (no CChipAdapter):**
   ```cpp
   register_chip(std::move(via1_clone), "VIA #1", "VIA1", "I/O", 0x9110);
   // register_chip already accepts unique_ptr<ChipBase>
   ```
   Wait — there's a lifetime problem. `register_chip` takes ownership via
   `unique_ptr<ChipBase>`, but the system class also needs to use the chip for emulation.
   See §3.1 below.

#### 3.1 Ownership Model: Shared vs Split

**Problem:** Currently the system class owns the raw chip pointer (for emulation ticking)
and `CChipAdapter` holds a non-owning `void*` copy (for GUI rendering). If we eliminate
`CChipAdapter`, who owns the chip?

**Option A: System owns, registry borrows (recommended)**
```cpp
class VIC20System {
    MOS6522 via1_;   // Value member (or unique_ptr for heap allocation)

    void register_chips() override {
        // register_chip takes a non-owning ChipBase* instead of unique_ptr
        register_chip(&via1_, "VIA #1", "VIA1", "I/O", 0x9110);
    }
};
```
This requires changing `SystemChip` to store a raw `ChipBase*` (non-owning) instead of
`unique_ptr<ChipBase>`. System classes already manage chip lifetime in their
constructors/destructors, so this is safe.

**Option B: Shared ownership via `shared_ptr`**
```cpp
class VIC20System {
    std::shared_ptr<MOS6522> via1_;
    // registered_chips_ stores shared_ptr<ChipBase>
};
```
Adds reference counting overhead. Not recommended.

**Recommendation: Option A (owner + borrow).** Change `SystemChip::chip` from
`unique_ptr<ChipBase>` to `ChipBase*` (non-owning). The system class guarantees the chip
outlives the registry (which it already does — chips are destroyed in the system
destructor, and `registered_chips_` is cleared first via `EmulatedSystem::~EmulatedSystem`
running after the derived destructor).

Alternatively, keep `unique_ptr<ChipBase>` for chips that ARE the canonical owner (like
the current `CChipAdapter` pattern), and use raw `ChipBase*` for borrowed references.
A `std::variant<unique_ptr<ChipBase>, ChipBase*>` is overengineered — just pick one model.

#### 3.2 Conversion Order

Convert chips in dependency order, simplest first:

| Priority | Chip(s) | Complexity | Notes |
|----------|---------|------------|-------|
| 1 | RAM, ROM | Trivial | Stateless lifecycle |
| 2 | PLA | Low | Simple logic chip |
| 3 | MOS 6522 (VIA) | Low | Clean state struct |
| 4 | MOS 6526 (CIA) | Medium | Timer complexity |
| 5 | MOS 2114 (Color RAM) | Low | Small chip |
| 6 | MOS 6560/6561 (VIC) | Medium | Video chip |
| 7 | MOS 6567/6569 (VIC-II) | High | Complex video, dual PAL/NTSC |
| 8 | TED 7360 | High | Combined video + I/O |
| 9 | MOS 6581 (SID) | High | Complex sound chip |
| 10 | NES PPU, NES APU | Medium | Already partially C++ |
| 11 | fam65xx CPUs | Last | Template architecture, most complex |

#### 3.3 fam65xx CPU — Special Handling

The fam65xx template already IS C++ — it's a `class fam65xx_t<Traits>` with template-based
compile-time dispatch. The conversion here is to make it inherit `ChipBase`:

```cpp
template <const CPUTraits &Traits>
class fam65xx_t : public ChipBase,
                  public io_port_base_t<Traits>,
                  public apu_base_t<Traits> {
public:
    ChipIdentity chip_identity() const override {
        return {Traits.chip_id, Traits.vendor};
    }
    bool has_debug_content() const override { return true; }
    void render_debug_content() override;
    void render_layout_content() override;
    // ...
};
```

The `CPUGUIRenderer` / `CPUGUIRendererImpl<Traits>` template hierarchy becomes unnecessary
— the rendering logic moves into `fam65xx_t<Traits>` itself (or a CRTP mixin).

**Performance note:** Adding `ChipBase` as a base class to `fam65xx_t` adds a vtable
pointer (8 bytes per instance). The virtual dispatch is GUI-only — emulation ticking
never calls virtual methods. The vtable pointer is the only cost. This is acceptable.

**After Phase 3:** `CChipAdapter` is unused. Delete it from `chip.h`.

---

### Phase 4: Eliminate `System8Bit` — Migrate Test Framework

**Effort:** Medium (3–5 hours)
**Risk:** Medium — test framework is a critical verification tool
**Files affected:** ~8

**Problem:** The C64 test framework (`c64_test_framework.cpp`, `c64_test_runner.cpp`,
VICII test harnesses) uses the C-style API from `c64.cpp`:
- `c64_system_create()` / `c64_system_destroy()`
- `c64_system_tick()` / `c64_system_reset()`
- Direct access to `c64_t` fields (`c64->mos6510`, `c64->vicii`, etc.)

These functions go through `System8Bit` for chip lifecycle.

**Solution: Port test framework to C++**

Rewrite `c64_test_framework.cpp`, `c64_test_runner.cpp`, and the VICII test harnesses
to use `C64System` directly instead of the C-style `c64_system_create/destroy/tick/reset`
API. This eliminates the translation layer entirely and removes the last consumer of
`System8Bit`.

**Actions:**
1. Create a `C64TestHarness` class wrapping `C64System` with test-friendly accessors
   (typed chip pointers, memory read/write helpers, tick-until-condition, etc.).
2. Port `c64_test_framework.cpp` to use `C64TestHarness` instead of raw `c64_t*` access.
3. Port `c64_test_runner.cpp` — replace `c64_system_create/destroy` with
   `C64TestHarness` construction/destruction.
4. Port VICII test harnesses (`vicii_pixel_tests.h`, `vicii_test_harness.h`) — replace
   `#include "c64.h"` with `C64TestHarness`.
5. Remove `system_8bit_t system` from `c64_t` struct.
6. Delete the C-style API functions from `c64.cpp` (`c64_system_create`, `c64_system_destroy`,
   etc.) once all callers are ported.
7. Delete `System8Bit`, `ChipEntry`, `system_8bit_t` from `system.h`/`system.cpp`.
8. Delete `c64.h` / `c64.cpp` if fully superseded by `c64_system.h` / `c64_system.cpp`
   (or merge any remaining logic into `C64System`).

---

### Phase 5: Template-Based Chip Families

**Effort:** Medium-Large per family (2–4 hours each)
**Risk:** Low — pure refactoring, no behavioral change
**Files affected:** Varies per family

Leverage the fam65xx NTTP pattern for other chip families that have PAL/NTSC or
feature variants:

#### 5a. VIC-II Family (MOS 6567 / MOS 6569)

Currently: Two separate files (`mos6567.cpp`, `mos6569.cpp`) with significant code
duplication, differentiated by timing constants and a few behavioral quirks.

**Target:**
```cpp
struct VICIITraits {
    const char* chip_id;
    uint16_t raster_lines;
    uint16_t cycles_per_line;
    uint16_t visible_lines;
    bool is_pal;
    // ...
};
constexpr VICIITraits MOS6567_NTSC = {"MOS6567", 263, 65, 200, false};
constexpr VICIITraits MOS6569_PAL  = {"MOS6569", 312, 63, 200, true};

template <const VICIITraits &Traits>
class VICII : public ChipBase { /* ... */ };
```

#### 5b. VIC Family (MOS 6560 / MOS 6561)

Same pattern as VIC-II — PAL/NTSC timing variants.

#### 5c. CIA Family (if variants exist)

MOS 6526 / MOS 6526A — if the only difference is timing, a trait struct captures it.

#### 5d. Keyboard / Joystick

`commodore_keyboard_t` and joystick handling could use a trait struct for key matrix
dimensions and layout differences between C64, VIC-20, and C16.

---

### Phase 6: Ownership and Lifetime Cleanup

**Effort:** Small-Medium (2–3 hours)
**Risk:** Low

| Task | Details |
|------|---------|
| Replace raw `void*` with typed pointers everywhere | Audit all remaining `void*` in system classes |
| Replace raw `new`/`delete`/`malloc`/`free` with `unique_ptr` | VIC-20's `free(vic_)` is the worst offender |
| Consolidate memory management patterns | All chips should use the same ownership idiom |
| Remove `raw_chip()` from `CChipAdapter` (after Phase 3 deletes it) | No more type-erased access |

---

## Dependency Graph

```
Phase 0 (dead code)
   ↓
Phase 1 (typed lifecycle)
   ↓
Phase 2 (bank_change coupling) ──→ Phase 2b (delete ChipDescriptor)
   ↓
Phase 3 (chips inherit ChipBase) ──→ Delete CChipAdapter
   ↓
Phase 4 (eliminate System8Bit)
   ↓
Phase 5 (template chip families)  ←── can start after Phase 3
   ↓
Phase 6 (ownership cleanup)       ←── can start after Phase 3
```

Phases 5 and 6 are independent of Phase 4 and can proceed in parallel after Phase 3.

---

## Performance Analysis

| Change | Impact | Justification |
|--------|--------|---------------|
| Adding vtable pointer to chip classes | +8 bytes per chip instance | GUI-only virtual dispatch; emulation tick never calls virtuals |
| Removing `std::function<void()>` × 3 per chip | −96–192 bytes per chip, eliminates heap allocs | Replaced by direct virtual calls on ChipBase |
| Replacing `descriptor->bank_change()` with function pointer | Identical overhead | One indirect call instead of another |
| Template chip families (VIC-II, VIC, etc.) | Zero overhead | Same as fam65xx: `if constexpr` eliminates dead branches at compile time |
| `unique_ptr` instead of raw pointers | Zero runtime overhead | Destructor call is same as manual `delete` |
| Removing `System8Bit` array scan | Marginal improvement | Eliminated linear scan of 16-slot array in destruction |
| Uniform `bus_state_t` read/write/tick signatures | Zero overhead | Already the chip-level pattern; formalizes existing convention; enables floating bus without bus-level fixup |

**Net effect:** Slight improvement — `std::function` heap allocations eliminated,
code-size reduction from template deduplication, no new overhead on the emulation
hot path.

---

## Files Affected Summary

| Phase | Files Created | Files Modified | Files Deleted |
|-------|---------------|----------------|---------------|
| 0 | 0 | ~5 | 2 (`emulation_context.h/cpp`) |
| 1 | 0 | ~15–20 | 0 |
| 2 | 0 | ~8 | 0 |
| 2b | 0 | ~20 | 0 (descriptors removed from existing files) |
| 3 | 0 | ~30–50 | 0 (CChipAdapter deleted from `chip.h`) |
| 4 | ~1 (`C64TestHarness`) | ~12–15 | ~2 (`c64.h/cpp` if fully superseded, `system.h/cpp` trimmed) |
| 5 | ~2–4 trait headers | ~10–15 | ~4–6 (deduplicated variant files) |
| 6 | 0 | ~10–15 | 0 |

---

## Verification Strategy

Each phase must pass before the next begins:

1. **Build:** `cmake --build build -j16` succeeds with zero warnings (`-Wall -Wextra`).
2. **Unit tests:** `ctest --test-dir build` — all tests pass.
3. **Test framework:** `c64_test_runner` — all CPU/VICII test suites pass.
4. **Smoke test:** Launch each system (C64, VIC-20, C16, NES, Apple 1, CHIP-8),
   verify Hardware menu works, chips render correctly, emulation runs.
5. **Performance:** No measurable regression in frames-per-second or CPU time per frame.
6. **Git commit:** Each completed phase is committed as a self-contained, buildable
   changeset before the next phase begins. This provides rollback points and makes
   bisection possible if regressions surface later.

---

## Cross-Cutting Concern: Chip Read/Write Signatures

### Read Signatures — Decision: Option B (full `bus_state_t`)

All chip MMIO / register **read** methods receive the full `bus_state_t` and return it
(possibly modified). This was chosen over Option A (`uint8_t bus_data`) because:

1. **Signature uniformity with `tick`:** tick, read, and write all become
   `bus_state_t f(bus_state_t)` — one signature to reason about.
2. **Already the established pattern:** The current codebase's chip-level register
   functions (`mos6526_registers_read`, `mos6581_registers_read`, `mos6522_registers_read`,
   all VIC-II register functions) already use `bus_state_t` input and return. This isn't
   a migration — it's formalizing what most chips already do.
3. **VIC-II open bus** needs address-line context that only `bus_state_t` provides.
4. Floating bus behavior (returning whatever was last on the data bus when no chip
   drives it) is a natural part of the code flow — the default case simply returns
   `bus_state` unmodified.

**Target read signature:**
```cpp
bus_state_t MOS6526::read(bus_state_t bus) const;
```

### Write Signatures — Decision: Option B (full `bus_state_t`)

**Analysis of `addr + data` vs `bus_state_t` for writes:**

| Criterion | `void write(uint16_t addr, uint8_t data)` | `bus_state_t write(bus_state_t bus)` |
|-----------|-------------------------------------------|-------------------------------------|
| Caller convenience | Slightly easier — no packing needed | Caller must construct `bus_state_t` (but typically already has one) |
| Implementation | Direct use of `addr` and `data` | Two macros at top: `BUS_GET_ADDR`, `BUS_GET_DATA` |
| Signature consistency | Different from tick/read | Same as tick/read — uniform `bus_state_t → bus_state_t` |
| Return value | `void` — no bus state feedback | Returns `bus_state_t` — can modify control lines (e.g., CIA IRQ assertion) |
| Current codebase | Used only at CPU memory callback layer | Already used by all chip register write functions (CIA, SID, VIA, VIC-II) |

**Recommendation: `bus_state_t write(bus_state_t bus)` (Option B).**

The decisive factor is that **this is already the pattern**. Every chip-level register
write function in the codebase (`mos6526_registers_write`, `mos6581_registers_write`,
`mos6522_registers_write`, `mos6567_registers_write`, `mos6569_registers_write`) already
takes and returns `bus_state_t`. The two-macro extraction at the top of each function
(`uint8_t reg = BUS_GET_ADDR(bus) & MASK; uint8_t value = BUS_GET_DATA(bus);`) is
trivial boilerplate — one line per argument.

The `addr + data` pattern exists only at the CPU memory callback layer
(`cpu_write(void*, uint32_t addr, uint8_t data)`), which is a different abstraction level.
At the chip register level, `bus_state_t` is already universal.

Signature uniformity (tick = read = write = `bus_state_t → bus_state_t`) makes the
interface easier to compose and reason about. The return value also has practical value:
write handlers that modify bus control lines (e.g., CIA IRQ assertion via `pending_bus_lines`)
can propagate those changes through the return value instead of requiring side-channel state.

**Target signatures for Phase 3 chip methods:**
```cpp
class MOS6526 : public ChipBase {
    bus_state_t tick(bus_state_t bus);   // already exists
    bus_state_t read(bus_state_t bus);   // already exists as mos6526_registers_read
    bus_state_t write(bus_state_t bus);  // already exists as mos6526_registers_write
};
```

**Note:** The `addr + data` signature at the CPU memory callback layer
(`fam65xx_mem_read_t`, `fam65xx_mem_write_t`) is a separate concern and may remain
as-is — the CPU callback dispatches to the appropriate chip's `bus_state_t`-based
method after constructing the bus state. This is the natural boundary between the
CPU's scalar address/data view and the bus's full-state view.

---

## Open Questions

1. **Ownership model for Phase 3:** `SystemChip` currently owns chips via `unique_ptr<ChipBase>`.
   Should it switch to non-owning `ChipBase*` (system owns chip lifetime) or keep
   `unique_ptr` with the system class yielding ownership? The non-owning model is simpler
   and matches how systems already manage chips, but requires careful lifetime ordering.

2. **VIC-20 memory subsystem:** `vic20_memory_t` is a complex C struct with its own
   create/destroy pattern. Does it become a ChipBase subclass, or remain a system-internal
   implementation detail that doesn't appear in the chip registry?

3. **NES `shared_ptr` usage:** The NES already uses `shared_ptr<PPU>` and `shared_ptr<MemoryBus>`.
   Should these be converted to `unique_ptr` for consistency, or left as-is since they
   may have legitimate shared-ownership semantics?

---

*Plan authored: 2025-07-15*
*Last updated: 2026-02-21 — Phase 0 complete, bus signatures resolved (Option B: full bus_state_t for read+write+tick)*
*Confidence: 0.93*
*Key uncertainties: Ownership model for Phase 3 (system-owns-chip-borrows vs unique_ptr transfer)*
*How to improve: Complete Phase 1a (VIC-20) to validate the lifecycle migration pattern*
