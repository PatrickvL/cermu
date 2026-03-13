# Chip Architecture Refactoring Plan

## Overview

This plan addresses findings from a comprehensive audit of all chip implementations and system integrations in cermu. It covers: dead code removal, system dependency decoupling, bus_state_t signature migration, and C++ class conversions.

**Gold standard references:**
- **C64 memory tick**: `c64_memory_tick(c64_bus_t*, bus_state_t)` — fully bus_state_t, I/O page handlers use `bus_state_t (*handler)(void*, bus_state_t)`
- **TED 7360**: Model for C++ class conversion (callbacks via descriptor, ChipBase inheritance, proper encapsulation)

**Phase dependencies:** Phase 1 first (trivial, no behavioral changes). Phases 2–6 are independent of each other. Phase 7 last (largest effort, benefits from clean boundaries established by earlier phases).

---

## Phase 1: Dead Code & Stale Include Removal ✅ COMPLETE

**All items removed.**

### 1a. VIC-20: Remove dead `cpu_read` / `cpu_write`
- **Files:** `src/systems/vic20/vic20_system.h`, `src/systems/vic20/vic20_system.cpp`
- **What:** `VIC20System::cpu_read()` and `cpu_write()` (lines ~1189–1204 in .cpp) are never called. The live memory path is `mem_tick(bus_state_t)` → `vic20_memory_cpu_tick()`.
- **Action:** Delete declarations from .h and definitions from .cpp.

### 1b. C16: Remove dead static callbacks
- **Files:** `src/systems/c16/c16_system.h`, `src/systems/c16/c16_system.cpp`
- **What:** Static `cpu_read_callback()` and `cpu_write_callback()` (lines ~803–812 in .cpp) use the old callback signature and are never registered anywhere. The live path is `mem_tick(bus_state_t)` → instance `cpu_read()`/`cpu_write()`.
- **Action:** Delete static callback declarations and definitions.

### 1c. MOS 6581: Remove stale include
- **File:** `src/chip/sound/mos6581.cpp`
- **What:** `#include "../../core/cermu.h"` at line 2 — nothing from `cermu.h` is referenced.
- **Action:** Delete the include line.

---

## Phase 2: VIC-II System Dependency Decoupling ✅ COMPLETE

**Status:** VIC-II no longer includes `c64_bus.h`. Memory access uses callback via
`vicii_bus_unit_t`.

### Problem
`src/chip/video/vic_ii/vicii_common.cpp` directly includes `c64_bus.h` and casts `void* bus` to `c64_bus_t*` to call `c64_bus_vic_read()`. This prevents the VIC-II from being reused in other systems (e.g., C128).

### Current coupling points
```cpp
// vicii_common.cpp line 3
#include "../../../systems/c64/c64_bus.h"

// vicii_common.cpp line ~1664
c64_bus_t* c64_bus = (c64_bus_t*)vicii->bus.bus;

// vicii_common.cpp line ~1817
bus_state = c64_bus_vic_read(c64_bus, bus_state, address);
```

### Solution
1. **Add memory-read callback to `vicii_bus_unit_t`** in `vicii_common.h`:
   ```cpp
   using vicii_mem_read_fn_t = bus_state_t (*)(void* ctx, bus_state_t bus, uint16_t addr);
   
   struct vicii_bus_unit_t {
       void* bus;                          // system bus context (opaque)
       vicii_mem_read_fn_t mem_read;       // NEW: memory read callback
       void* mem_read_ctx;                 // NEW: context for callback
       // ... existing bank_change, lp_pin_read callbacks
   };
   ```
2. **Replace direct call** in `vicii_common.cpp`:
   ```cpp
   // Before:
   bus_state = c64_bus_vic_read(c64_bus, bus_state, address);
   // After:
   bus_state = vicii->bus.mem_read(vicii->bus.mem_read_ctx, bus_state, address);
   ```
3. **Remove** `#include "../../../systems/c64/c64_bus.h"` and the `c64_bus_t*` cast.
4. **Register callback** in C64 system initialization (e.g., `c64_system.cpp`):
   ```cpp
   vicii->bus.mem_read = [](void* ctx, bus_state_t bus, uint16_t addr) -> bus_state_t {
       return c64_bus_vic_read(static_cast<c64_bus_t*>(ctx), bus, addr);
   };
   vicii->bus.mem_read_ctx = c64_bus;
   ```

### Pattern reference
This mirrors the TED's `ted_mem_read_fn` callback and the VIC 6560's existing `vic_mem_read_fn_t`.

---

## Phase 3: PLA GUI System Dependency Decoupling ✅ COMPLETE

**Status:** PLA chip code no longer includes C64 system headers. C64-specific PLA
visualization lives in `src/systems/commodore/c64/c64_pla_chip.hpp`.

### Problem
`src/chip/logic/pla_gui.cpp` includes `c64_bus.h` and `c64.h`, uses `c64_t*`, `c64_chips_get_description()`, and `c64_chips_to_title()` for memory-map debug visualization.

### Solution
Split `pla_gui.cpp` into:
1. **`pla_gui.cpp`** — chip-generic PLA pin visualization (stays in chip layer, no system includes)
2. **`src/systems/c64/c64_pla_debug.cpp`** — C64-specific memory-map debug view (moves to system layer, allowed to include C64 types)

The split point is wherever the `c64_t*` / `c64_chips_*` functions are used. The chip-layer file retains the pin/gate visualization; the system-layer file gets the C64 memory-map overlay.

---

## Phase 4: Apple 1 Memory Servicing Fix ✅ COMPLETE

**Status:** Apple 1 now uses `Board<Spec>` with `bus_.tick()` for memory dispatch.
`cpu_read()`/`cpu_write()` eliminated.

### Problem
- `cpu_read()` and `cpu_write()` contain correct memory-map logic but are **never called**
- `tick_cpu()` calls `mos6502_tick(cpu_, 0)` — passes zero bus state and ignores the return value
- The CPU runs but cannot read or write memory

### Solution
1. Add `mem_tick(bus_state_t)` method to `Apple1System`, following the VIC-20/C64 pattern:
   - Extract address/RW from bus_state_t
   - Service ROM ($FF00–$FFFF), RAM ($0000–$0FFF), PIA ($D010–$D013)
   - Return bus_state_t with data set
2. Fix `tick_cpu()`:
   ```cpp
   // Before:
   mos6502_tick(cpu_, 0);
   // After:
   bus_state_t pins = mos6502_tick(cpu_, pins_);
   pins_ = mem_tick(pins);
   ```
3. Delete the now-redundant `cpu_read()` / `cpu_write()` methods (their logic moves into `mem_tick`).

---

## Phase 5: C16 Memory Path Cleanup ✅ COMPLETE

**Status:** C16 (`Commodore264System<V>`) has `mem_tick(bus_state_t)` as the primary
memory dispatch path. Old-style `cpu_read`/`cpu_write` instance methods eliminated.

### Problem
`mem_tick(bus_state_t)` unpacks bus_state_t, calls old-style `cpu_read(addr)` / `cpu_write(addr, data)` instance methods, then repacks. This extra indirection is unnecessary.

### Solution
Inline the `cpu_read`/`cpu_write` logic directly into `mem_tick(bus_state_t)`:
```cpp
bus_state_t C16System::mem_tick(bus_state_t s) {
    uint16_t addr = BUS_GET_ADDR(s);
    if (s & BUS_MASK_RW) {
        // Read: inline cpu_read logic here
        uint8_t data = /* memory map dispatch */;
        BUS_SET_DATA(s, data);
    } else {
        // Write: inline cpu_write logic here
        uint8_t data = BUS_GET_DATA(s);
        /* memory map dispatch */
    }
    return s;
}
```
Then delete the separate `cpu_read()` and `cpu_write()` instance methods.

---

## Phase 6: NES bus_state_t Migration

**Risk: Medium — touches 3 classes and their callers**

### Problem
NES PPU, Cartridge, and MemoryBus use old-style individual-parameter signatures:
```cpp
// PPU
uint8_t cpu_read(uint16_t addr, bool read_only);
void    cpu_write(uint16_t addr, uint8_t data);

// Cartridge (virtual)
bool cpu_read(uint16_t addr, uint8_t& data);
bool cpu_write(uint16_t addr, uint8_t data);

// MemoryBus
uint8_t cpu_read(uint16_t addr, bool read_only);
void    cpu_write(uint16_t addr, uint8_t data);
```

`NintendoSystem::clock()` manually unpacks/repacks bus_state_t around these calls.

### Solution

#### 6a. MemoryBus — primary interface
Convert to the standard pattern:
```cpp
bus_state_t MemoryBus::mem_tick(bus_state_t bus);
```
Internally dispatches to RAM, PPU registers, APU, cartridge, etc.

#### 6b. PPU register access
Convert:
```cpp
bus_state_t PPU::cpu_access(bus_state_t bus);
```
The `read_only` parameter is eliminated — bus_state_t's RW bit determines read vs write.

#### 6c. Cartridge mapped access
Convert:
```cpp
bus_state_t Cartridge::cpu_access(bus_state_t bus);
bus_state_t Cartridge::ppu_access(bus_state_t bus);
```
The bool return (mapped/unmapped) can be encoded as a status bit in bus_state_t, or handled via a separate `bool cpu_maps(uint16_t addr)` check.

#### 6d. Simplify `NintendoSystem::clock()`
```cpp
// Before:
pins = nes6502_tick(cpu_, pins);
uint16_t addr = BUS_GET_ADDR(pins);
if (pins & BUS_MASK_RW) {
    uint8_t data = bus_->cpu_read(addr, false);
    BUS_SET_DATA(pins, data);
} else {
    bus_->cpu_write(addr, BUS_GET_DATA(pins));
}

// After:
pins = nes6502_tick(cpu_, pins);
pins = bus_->mem_tick(pins);
```

#### 6e. Leave Mapper as-is
`cpu_map_read(addr, &mapped_addr)` / `ppu_map_read(addr, &mapped_addr)` are address-translation functions, not bus operations. They stay unchanged.

#### 6f. Test screen utilities
Update `nes_screen_utils.cpp` direct `ppu->cpu_write(0x2000, 0x80)` calls to use the new signature.

---

## Phase 7: C++ Class Conversions ✅ COMPLETE

**Status:** All chips inherit from `ChipBase` via category base classes (`VideoChipBase`,
`SoundChipBase`, `IoChipBase`, `InputChipBase`, `CpuChipBase`, `MemoryChipBase`).
`CChipAdapter` has been deleted.

Each chip follows the TED 7360 template:
1. Convert C-style struct + free functions → C++ class
2. Inherit from `ChipBase`
3. Replace raw pointers with callbacks via descriptor
4. Encapsulate internal state (private members)
5. Keep the same tick-level API semantics

### 7a. Chips requiring full C++ class conversion

| Chip | Files | Complexity | Notes |
|------|-------|------------|-------|
| VIC-II (6567/6569) | `src/chip/video/vic_ii/` | High (~2500 lines) | Do after Phase 2 (decoupling) |
| VIC (6560/6561) | `src/chip/video/vic/` | High | Already callback-based, cleaner starting point |
| MOS 6522 (VIA) | `src/chip/io/mos6522/` | Medium | Timer + shift register + I/O ports |
| MOS 6526 (CIA) | `src/chip/io/mos6526/` | High | Complex shift register, TOD clock |
| MOS 6581 (SID) | `src/chip/sound/` | High (~1000 lines) | Voice sub-structs, filter state |

### 7b. Chips requiring ChipBase addition only

| Chip | Files | Complexity | Notes |
|------|-------|------------|-------|
| PLA 906114 | `src/chip/logic/pla*` | Low | Plain struct, just add inheritance |
| PIA 6820 | `src/chip/io/pia6820/` | Low | Plain struct, just add inheritance |

### 7c. Chips already clean (no changes needed)

- TED 7360 — already refactored (model for others)
- MOS 2114 (SRAM) — already a class
- Signetics 2513 — header-only class
- fam65xx CPU — template class
- RAM/ROM generic wrappers
- Commodore Keyboard — input device, no chip logic

---

## Commit Strategy

Each phase (or sub-phase for larger ones) should be a separate commit:
- `Phase 1: Remove dead code and stale includes`
- `Phase 2: VIC-II — decouple from C64 system via mem_read callback`
- `Phase 3: PLA GUI — split chip-generic vs C64-specific visualization`
- `Phase 4: Apple 1 — add mem_tick to fix memory servicing`
- `Phase 5: C16 — inline cpu_read/write into mem_tick`
- `Phase 6a-f: NES — migrate to bus_state_t signatures`
- `Phase 7a: Convert [chip] to C++ class` (one commit per chip)

Build verification (`make -j16 cermu`) after each commit.

---

*Plan created: 2026-02-22*
*Status updated: 2026-03-13 — Phases 1–5, 7 marked complete; only Phase 6 (NES bus\_state\_t) remains*
*Based on: comprehensive chip architecture audit of all systems and chips*
