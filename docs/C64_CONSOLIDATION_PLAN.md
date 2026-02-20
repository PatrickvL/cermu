# C64 System Consolidation Plan

**Date:** 2026-02-20
**Goal:** Eliminate the C64SystemWrapper indirection — merge into a single `C64System` class

---

## Current Architecture

```
EmulatedSystem
  └─ CommodoreSystem (base: keyboard_, keyboard_mapper_, cycles_per_frame_)
       └─ C64SystemWrapper            ← C++ adapter layer
            └─ c64_t*                 ← C struct (owns all chips)
                 ├─ system_8bit_t     ← legacy chip registry
                 ├─ c64_bus_t         ← embedded bus + PLA
                 ├─ vicii_t*
                 ├─ mos6581_t*
                 ├─ mos6526_t* ×2
                 ├─ mos6510_t*
                 ├─ ram_t*, rom_t* ×5
                 ├─ mos2114_t*
                 └─ commodore_keyboard_t*
```

Every wrapper method either:
1. Delegates straight through (`tick → c64_system_tick`, `reset → c64_system_reset`)
2. Accesses `c64_->` members to reach chips directly (`c64_->sid`, `c64_->ram->memory`)
3. Passes `c64_` to standalone C functions (`gui_render_c64_system_menu_items`, `c64_apply_sid_load`, `c64_patch_skip_memtest`)

---

## Why the Wrapper Exists (6 Reasons)

| # | Reason | Complexity | Files Affected |
|---|--------|-----------|----------------|
| 1 | **Interface adaptation** — maps EmulatedSystem virtuals to C-style free functions | Low | c64.h/cpp |
| 2 | **Deferred boot loading** — waits for BASIC READY before injecting programs | Self-contained | wrapper only |
| 3 | **Connector port wiring** — creates 7 ports, wires CIA1 joystick callbacks | Self-contained | wrapper only |
| 4 | **GUI state bridge** — owns `gui_state_t*`, forwards to `imgui_interface.cpp` functions | Medium | imgui_interface.cpp |
| 5 | **SID player state machine** — header/payload copies, subtune switching, auto-region | Self-contained | c64_sid_player.cpp |
| 6 | **Configuration bridging** — maps `SystemConfiguration` ↔ `c64_config_t` | Low | c64_config.h/cpp |

Reasons 2, 3, 5 are already wrapper-owned logic — they move directly into `C64System` unchanged.
Reasons 1, 4, 6 are the actual wrapping glue that gets eliminated.

---

## Target Architecture

```
EmulatedSystem
  └─ CommodoreSystem
       └─ C64System                   ← single unified class
            ├─ c64_bus_t bus_         ← direct embedded member
            ├─ vicii_t* vicii_
            ├─ mos6581_t* sid_
            ├─ mos6526_t* cia1_
            ├─ mos6526_t* cia2_
            ├─ void* cpu_             ← mos6510
            ├─ ram_t* ram_
            ├─ rom_t* basic_, kernal_, charrom_
            ├─ rom_t* cart_roml_, cart_romh_
            ├─ mos2114_t* colorram_
            ├─ (deferred load state)
            ├─ (SID player state)
            └─ (connector ports — from base)
```

No `c64_t` struct. No `gui_state_t`. Chip debug windows use the generic system-wide mechanism (see Visualization Proposal).

---

## Migration Steps

### Phase 1: Internalize `c64_t` (mechanical, no behavior change)

**Step 1a — Move `c64_t` members into the wrapper**

Convert `c64_t`'s members to `C64SystemWrapper` private members. The `c64_t` struct becomes an empty shell that aggregates pointers back to the class (or is removed entirely via `this`).

- `c64_->vicii` → `vicii_`
- `c64_->sid` → `sid_`
- `c64_->cia1` → `cia1_`, `c64_->cia2` → `cia2_`
- etc.
- `c64_->bus` → `bus_` (embedded, not pointer — keep `container_of` working or replace with stored back-pointer)
- `c64_->system` → kept temporarily for chip registry iteration

After this step, `c64_` is `this` — the pointer indirection is gone.

**Step 1b — Convert C functions to methods**

| C function | Becomes |
|------------|---------|
| `c64_system_create(config)` | constructor body / `initialize()` |
| `c64_system_destroy(c64)` | destructor body / `shutdown()` |
| `c64_system_tick(c64)` | `void tick()` — inline or private method |
| `c64_system_reset(c64)` | `void reset()` body |
| `c64_set_framebuffer(c64, ...)` | `void set_framebuffer(...)` body |
| `c64_pla_maps_generate(c64)` | `void generate_pla_maps()` private |
| `c64_memory_init(sys, config)` | `void init_memory()` private |
| `c64_reload_roms(c64, config)` | `void reload_roms()` private |
| `c64_patch_skip_memtest(c64)` | `void patch_skip_memtest()` private |
| `c64_apply_sid_load(c64, ...)` | `void apply_sid_load(...)` private |
| `c64_sid_switch_subtune(...)` | `void switch_sid_subtune(...)` private |

**Step 1c — Fix the bus back-pointer**

The `BUS_TO_C64(bus)` macro uses `container_of` to recover `c64_t*` from an embedded `c64_bus_t`. Two options:

- **Option A (minimal):** Keep the macro — `c64_bus_t` is still embedded in the class, `container_of` still works. Change the type from `c64_t*` to `C64System*`.
- **Option B (cleaner):** Store a typed `C64System*` back-pointer in `c64_bus_t` (rename `void* c64` to `C64System* system`). Eliminates `container_of`.

Option B is cleaner; the bus already has `void* c64` back-pointer.

**Step 1d — Merge `c64_config_t` into `SystemConfiguration`**

- `vicii_standard_t` ↔ `region_option_index` — already mapped, keep lookup table
- ROM paths — already in `SystemConfiguration::rom_paths` or platform config
- SID revision — add to `SystemConfiguration::custom_settings` map

Eliminate `c64_config_t` struct entirely.

### Phase 2: Eliminate `gui_state_t` dependency

The `gui_state_t` struct is the legacy C64 GUI's global state. The chip debug arrays (`show_chip_debug[16]`, `show_chip_settings[16]`) need to move to the generic visualization system (see Visualization Proposal). The other fields are either:

- Already in `SimpleSystemGUI` (show_memory_viewer, emulation state, screen_texture_id, etc.)
- C64-specific dialogs (test binary dialog) — move to `C64System::render_system_menu_items()` with local state
- ROM path buffers — eliminate (use platform file dialogs)

**Step 2a:** Move `show_chip_debug[]` / `show_chip_settings[]` to a generic `ChipDebugState` in the base class or visualization subsystem.

**Step 2b:** Inline `gui_render_c64_system_menu_items()` into `C64System::render_system_menu_items()`. It's already a thin function — no need for the forwarding indirection.

**Step 2c:** Inline `gui_render_test_binary_dialog()` into `C64System::render_debug_windows()` or make it a private method.

**Step 2d:** Remove `gui_state_t` typedef and `imgui_interface.h` dependency.

### Phase 3: Eliminate `system_8bit_t` / legacy chip registry

The `system_8bit_t` struct exists only so the GUI can iterate chips generically. Once the visualization system provides its own chip enumeration (see Visualization Proposal), this is unnecessary.

**Step 3a:** Have `C64System` implement the generic chip enumeration interface directly (return chip descriptors from its own members — it knows what chips it has).

**Step 3b:** Remove `system_chip_register()` calls, `create_and_register_chip()` helper, `system_chips_destroy()`.

**Step 3c:** Chip ownership transfers to `C64System` directly. Each chip's `create`/`destroy` is called from the constructor/destructor. Consider `unique_ptr` with custom deleters, or keep raw pointers with explicit cleanup (consistent with current style).

### Phase 4: Clean up remaining C64-specific files

| File | Action |
|------|--------|
| `c64.h` / `c64.cpp` | **Delete** — all logic absorbed into C64System |
| `c64_config.h` / `c64_config.cpp` | **Delete** — merged into SystemConfiguration |
| `c64_system_wrapper.h` / `c64_system_wrapper.cpp` | **Rename** → `c64_system.h` / `c64_system.cpp`, class → `C64System` |
| `c64_bus.h` / `c64_bus.cpp` | **Keep** — bus is complex enough to be its own translation unit |
| `c64_chips.h` / `c64_chips.cpp` | **Keep** — chip ID enum + description lookup (used by debug UI) |
| `c64_kernal_patches.cpp` | **Keep** — change signature from `c64_t*` to `rom_t*` (it only touches kernal ROM) |
| `c64_sid_player.cpp` | **Keep** — change signatures to take chip pointers directly instead of `c64_t*` |
| `c64_keyboard_matrix.cpp` | **Keep** — pure data, no changes |
| `c64_screen_utils.cpp` | **Keep** — pure utility, no changes |
| `c64_screenshot.cpp` | **Keep** — pure utility, no changes |
| `c64_test_loader.cpp` | **Keep** — takes `ram_t*`, no changes needed |
| `c64_test_framework.cpp` | **Refactor** — change `C64System*` accesses to use the new class interface |
| `c64_banking_verify.cpp` | **Keep** — uses `c64_bus_t` directly, no changes |
| `c64_hardware_config.cpp` | **Keep** — pure data/mapping |

### Phase 5: Rename `SimpleSystemGUI` → `SystemGUI`

Once the C64 wrapper is gone, every system flows through the same GUI. The "Simple" prefix was only meaningful relative to the old C64-specific GUI path — it's now just THE system GUI.

| Change | Details |
|--------|---------|
| File rename | `simple_system_gui.h` → `system_gui.h`, `.cpp` → `.cpp` |
| Class rename | `SimpleSystemGUI` → `SystemGUI` |
| Include guard | `SIMPLE_SYSTEM_GUI_H` → `SYSTEM_GUI_H` |
| References | ~46 occurrences across 7 files |
| CMakeLists.txt | Update source path |
| Comments | Update `generic_gui.h` ("Derived classes (C64GUI, SimpleSystemGUI, etc.)"), `system_selection_dialog.h`, `imgui_interface.cpp` |

This is a pure mechanical rename — no logic changes.

### Phase 6: Remove dead legacy GUI code

After the C64 wrapper is gone and `gui_state_t` is eliminated:

- Audit `imgui_interface.cpp` / `imgui_interface.h` for functions only reachable from the C64 wrapper
- Remove `gui_render_c64_system_menu_items()`, `gui_render_test_binary_dialog()`, `gui_render_debugger()` if they were C64-only
- Remove any `c64_t*`-taking functions
- Consider whether `imgui_interface.cpp` still has a purpose or whether its remaining functions should move elsewhere

---

## Test Framework Considerations

The `c64_test_framework.cpp` accesses `->bus`, `->ram`, `->mos6510`, `->vicii` on `C64System*`. After Phase 1, these become direct member accesses on the new `C64System` class. The test framework needs:

1. A public accessor API on `C64System` for test-visible state (PC, RAM bytes, bus mode, VIC frame count)
2. The `TestFramework` class to use the new API instead of reaching into `c64_t` internals

This is a natural encapsulation boundary — the test framework shouldn't reach into chip structs anyway.

---

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| Bus `container_of` breaks | Replace with typed back-pointer (Phase 1c) |
| Test framework regression | Keep test suite green throughout; refactor after Phase 1 |
| C function callback contexts | Callbacks that receive `c64_t*` as context now receive `C64System*` — all internal |
| GUI breakage | Inline the 3 forwarded GUI functions — they're small |
| Large diff | Phase 1a alone is ~400 line shuffle; do incrementally, commit per phase |

---

## Estimated Effort

| Phase | Scope | Estimate |
|-------|-------|----------|
| 1a-1d | Internalize c64_t, convert functions | 2–3 hours |
| 2 | Eliminate gui_state_t | 1 hour |
| 3 | Remove system_8bit_t legacy registry | 30 min |
| 4 | File cleanup + rename | 30 min |
| 5 | SimpleSystemGUI → SystemGUI rename | 15 min |
| 6 | Dead legacy GUI code removal | 30 min |
| **Total** | | **~5–6 hours** |

---

## Order of Operations

```
Phase 1a → 1b → 1c → 1d → build+test
Phase 2a → 2b → 2c → 2d → build+test
Phase 3a → 3b → 3c → build+test
Phase 4 → build+test
Phase 5 → build+test
Phase 6 → build+test → commit
```

Each phase boundary is a commit point. The system should build and pass smoke tests at every phase boundary.
