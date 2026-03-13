# C64 System Consolidation Plan

**Date:** 2026-03-13 (revised)
**Goal:** Eliminate the C64System indirection — merge into a single `C64System` class

---

## Completed Work

The bulk of the consolidation is finished.  This section documents what was done for
historical reference.

### What was eliminated

| Legacy artifact | Status | What replaced it |
|----------------|--------|-----------------|
| `c64_t` struct | ✅ Removed | Chips are direct members on `C64System` |
| `gui_state_t` | ✅ Removed | Base class `chip_debug_state_` + virtual methods |
| `system_8bit_t` (legacy chip registry) | ✅ Removed | `get_chip_info()` virtual method |
| `imgui_interface.h` / `.cpp` | ✅ Removed | GUI uses generic base class rendering |
| C-style free functions (`c64_system_tick`, etc.) | ✅ Removed | Methods on `C64System` class |
| `void* c64` back-pointer in `c64_bus_t` | ✅ Replaced | Typed `C64System* c64` back-pointer |
| `SimpleSystemGUI` | ✅ Renamed | `SessionGUI` (via `EmulatorHost`) |

### Current architecture

```
System
  └─ CommodoreSystem (keyboard_, keyboard_mapper_, cycles_per_frame_)
       └─ C64System                   ← single unified class
            ├─ c64_bus_t bus{}        ← direct embedded member
            ├─ vicii_t* vicii
            ├─ mos6581_t* sid
            ├─ mos6526_t* cia1, cia2
            ├─ MOS6510* mos6510
            ├─ RAMChip* ram
            ├─ ROMChip* basic, kernal, charrom
            ├─ ROMChip* cartridge_roml, cartridge_romh
            ├─ MOS2114* colorram
            ├─ (SID player state)
            └─ (port wiring — via primary_board_)
```

No `c64_t` struct.  No `gui_state_t`.  No `system_8bit_t`.  All chip pointers are
direct members on `C64System`.  The `c64_bus_t` struct is embedded (not heap-allocated)
with a typed `C64System*` back-pointer.

### Files in src/systems/commodore/c64/

| File | Purpose | Status |
|------|---------|--------|
| `c64_system.hpp` / `.cpp` | Main system class — tick loop, init, reset | Active |
| `c64_bus.hpp` / `.cpp` | Bus controller — unified buffer, PLA banking, I/O dispatch | Active |
| `c64_chips.hpp` / `.cpp` | Chip ID enum + descriptors, address routing constants | Active |
| `c64_config.hpp` / `.cpp` | `c64_config_t` — C64-specific config struct | **To be consolidated** |
| `c64_hardware_config.hpp` / `.cpp` | Extended config for VICE test framework | Active |
| `c64_constants.hpp` | Memory map, timing, signal constants | Active |
| `c64_pla_chip.hpp` / `.cpp` | PLA ChipBase wrapper for debug visualization | Active |
| `c64_kernal_patches.hpp` / `.cpp` | Memory test skip patch | Active |
| `c64_keyboard_matrix.hpp` / `.cpp` | Keyboard scanning logic | Active |
| `c64_screen_utils.hpp` / `.cpp` | Text display utilities (SID player) | Active |
| `c64_sid_player.hpp` / `.cpp` | SID file playback integration | Active |

---

## Remaining Work

### Phase A: Merge `c64_config_t` into `SystemConfiguration`

`c64_config_t` (`c64_config.hpp`) still exists as a separate struct.  The plan merges
its fields into `SystemConfiguration` (the base class config system):

- `vicii_standard_t` (PAL/NTSC) ↔ `region_option_index` — already mapped via lookup
- ROM paths — already in `SystemConfiguration::rom_paths` or platform config
- SID revision — add to `SystemConfiguration::custom_settings` map
- RAM/video/color RAM sizing — trivial mapping

**After merge:**
- Delete `c64_config.hpp` / `c64_config.cpp`
- `c64_hardware_config.hpp` may survive if VICE test framework needs extended options

### Phase B: Manifest Migration

See `MANIFEST_EXPANSION_ROADMAP.md` § "C64 Migration Plan" for the full strategy.

Summary: replace the hand-rolled `c64_bus_t` with `Board<C64BusSpec>` + `ChipManifest`,
absorb the PLA banking tables and I/O handler dispatch into the `Board<Spec>` / `BusMap`
infrastructure while preserving branchless hot-path performance.

This is the highest-effort manifest migration and depends on:
1. Adding `has_mmio()` / `on_bus_read()` / `on_bus_write()` to VIC-II, SID, CIA, Color RAM
2. Evaluating `BusMap` fit for the 32-mode PLA banking × CPU/VIC-II dual views
3. Multi-viewer `MemoryBus` for the VIC-II's separate address space

### Phase C: Dead Code Audit

After Phases A and B, audit for leftover artifacts:

- SID player utility functions (`c64_apply_sid_load`, `c64_sid_switch_subtune`) take
  `C64System*` — these are correctly scoped helper functions, not legacy wrappers
- `c64_kernal_patches.cpp` takes chip pointers directly — no changes needed
- `c64_screen_utils.cpp` — pure utility, no system dependency
- Verify no orphaned `#include` paths or forward declarations reference deleted types

---

## Historical Phases (Completed)

These phases are documented for reference.  All are done.

### Phase 1: Internalize `c64_t` ✅

- **1a:** Moved `c64_t` members into `C64System` as direct chip pointer fields
- **1b:** Converted C functions (`c64_system_tick`, `c64_system_reset`, etc.) to methods
- **1c:** Replaced `void* c64` in `c64_bus_t` with typed `C64System*` back-pointer
- **1d:** `c64_config_t` merge started but not yet completed (see Phase A above)

### Phase 2: Eliminate `gui_state_t` ✅

- **2a:** `show_chip_debug[]` / `show_chip_settings[]` moved to base class
- **2b:** `gui_render_c64_system_menu_items()` inlined into `C64System::render_system_menu_items()`
- **2c:** `gui_render_test_binary_dialog()` inlined
- **2d:** `gui_state_t` typedef and `imgui_interface.h` removed

### Phase 3: Eliminate `system_8bit_t` ✅

- **3a:** `C64System` implements `get_chip_info()` virtual directly
- **3b:** `system_chip_register()` calls removed
- **3c:** Chip ownership is direct member pointers on `C64System`

### Phase 5: SimpleSystemGUI → SessionGUI ✅

- Completed as part of the broader rename (`SystemGUI` → `SessionGUI` via `EmulatorHost`)

### Phase 6: Legacy GUI Code Removal ✅

- `imgui_interface.cpp` / `imgui_interface.h` removed
- All C64-specific GUI rendering moved to `C64System` virtual methods
