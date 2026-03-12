# Manifest Expansion Roadmap

**Date:** 2026-07-22
**Purpose:** Incremental plan to evolve chip manifests into full board declarations,
bridging the gap between current C++ systems and data-driven TOML system definitions.
**Builds On:** Current `ChipManifest` / `Board` infrastructure, `CONFIGURABLE_SYSTEM_ARCHITECTURE.md`

---

## Current State

### What Manifests Declare Today

Manifests describe **memory-mapped chips** in address space: RAM, ROM, and (in newer
systems) MMIO-only I/O chips.  `Board` owns the unified buffer, auto-wires page
tables, and manages chip lifetimes via factory creation or pre-binding.

| System      | Manifest contains                              | Non-manifest chips (manual)              |
|-------------|------------------------------------------------|------------------------------------------|
| VIC-20      | RAM, ROM ×3, CPU, VIC, VIA ×2 (gold standard)  | —                                        |
| Atari 2600  | TIA (MMIO), RIOT (MMIO), CartChip (MMIO)       | CPU (manual `new`)                       |
| Apple 1     | RAM, ROM ×2, PIA (MMIO, bound)                 | CPU (manual `new`), terminal             |
| Acorn Atom  | RAM ×2, ROM ×3, PPI (MMIO, bound), VIA (bound) | CPU (manual `new`), VDG                  |
| BBC Micro   | RAM, ROM ×2                                    | CPU, CRTC, PSG, VIA ×2                   |
| Spectrum    | RAM, ROM                                       | CPU, ULA, AY                             |
| C16/Plus4   | RAM, ROM ×2                                    | CPU, TED, keyboard                       |
| PET         | RAM ×2, ROM ×5                                 | CPU, CRTC, PIA ×2, VIA                   |
| Amstrad CPC | RAM, ROM ×2                                    | CPU, CRTC, PPI, AY, gate array           |
| DDR systems | RAM, ROM (memory-only)                         | CPU, PIO, CTC (all embedded)             |
| Arcade      | RAM, ROM (memory-only)                         | CPU(s), sound chips (all embedded)       |

### Generic Lifecycle: `reset_chips()`

`Board::reset_chips()` iterates all bound slots and calls `ChipBase::reset()`.
Currently used by: VIC-20, Atari 2600, Apple 1, Acorn Atom.  Safe for all chip types
(RAM/ROM/CPU have no-op `reset()`).

---

## Phase 1: Expand Manifests with All Chips

**Goal:** Move ALL manually-created chips into manifests as MMIO-only slots.

### Requirements per chip type

| Chip type | Needs for manifest inclusion |
|-----------|------------------------------|
| Memory-mapped I/O | `has_mmio()`, `on_bus_read()`, `on_bus_write()` overrides |
| Non-memory-mapped (CPU, sound) | MMIO-only slot `{0, 0, 0, "label"}` — no address decode needed |
| Conditional (PAL/NTSC) | `condition` tag on slot + `ConditionFn` in `create_chips()` |
| Descriptor-initialized | `create_from_slot()` factory, or pre-bind for complex init |

### Per-system migration checklist

For each system, the migration pattern is:

1. Add `Slot<ChipType>` entries to the manifest (MMIO-only for I/O, zero-size for non-mapped)
2. Remove manual `new ChipType()` calls — let `create_chips()` factory handle creation
3. Replace manual `chip_ = new X()` with `chip_ = bus_mem_.chip_as<X>(kSlot)`
4. Move `register_chip()` calls → `register_bus_chips()` handles them automatically
5. Replace individual `chip->reset()` calls → `bus_mem_.reset_chips()` + post-reset fixups

### Chips needing MMIO interface additions

These chips currently lack `has_mmio()` / `on_bus_read()` / `on_bus_write()` and
need them before they can be placed in manifests with address-decode:

- `ted7360_t` — TED video/I/O for C16/Plus4
- `mc6845_t` — CRTC for BBC Micro, PET, Amstrad CPC
- `sn76489_t` — PSG for BBC Micro
- `ay_3_8910_t` — sound for Spectrum, Amstrad CPC, arcade
- `amstrad_gate_array_t` — gate array for Amstrad CPC
- `ferranti_ula_t` — ULA for Spectrum
- DDR Z80 peripherals: `z80_pio_t`, `z80_ctc_t`
- NES chips: PPU, APU (use separate bus, not memory bus)

Chips that DON'T need MMIO (non-memory-mapped, pure logic):
- CPUs: already have `Slot<MOS6502>{0, 0, 0}` pattern (VIC-20 gold standard)
- Keyboards: `commodore_keyboard_t` — not a bus chip
- Terminal displays: `apple1_terminal_t` — not a bus chip

### Conditional chip patterns

```cpp
// PAL/NTSC video chip variants (VIC-20 gold standard)
Slot<mos6561_t>{0, 0, 0, "VIC (PAL)",  kPAL},
Slot<mos6560_t>{0, 0, 0, "VIC (NTSC)", kNTSC},

// Optional sound chip (Spectrum 128K has AY, 48K doesn't)
Slot<ay_3_8910_t>{0, 0, 0, "AY-3-8910", kHasAYSound},

// Memory expansion (VIC-20 3K/8K/etc.)
Slot<RAMChip>{0x0400, 3072, 0, "3K Expansion", kExpansion3K},
```

---

## Phase 2: Generic Lifecycle Management

**Goal:** Extend `reset_chips()` pattern to cover all lifecycle events.

### Current ChipBase lifecycle methods

| Method      | Virtual? | Signature             | Generic-callable?             |
|-------------|----------|-----------------------|-------------------------------|
| `reset()`   | Yes      | `void reset()`        | **Yes** — already done        |
| Constructor | No       | Varies                | No — handled by factory       |
| Destructor  | Yes      | `virtual ~ChipBase()` | Yes — handled by `unique_ptr` |

### Future lifecycle additions to ChipBase

These would enable more generic management:

```cpp
class ChipBase {
    // Existing:
    virtual void reset() {}

    // Future candidates:
    virtual void power_on() {}       // One-time init after creation (post-bind)
    virtual void shutdown() {}       // Pre-destruction cleanup
};
```

### Board lifecycle API growth

```cpp
class Board {
    // Existing:
    void reset_chips() noexcept;     // Calls reset() on all bound chips

    // Future:
    // void power_on_chips() noexcept;   // Calls power_on() on all bound chips
    // void shutdown_chips() noexcept;   // Calls shutdown() on all bound chips
};
```

### EmulatedSystem generic chip iteration

```cpp
class EmulatedSystem {
    // Future: iterate all registered chips (not just bus-owned)
    // void reset_all_chips();        // registered_chips_ iteration
};
```

---

## Phase 3: Board Declarations

**Goal:** Systems declare boards containing chips, connectors, and other components.

### Board concept

A board is a named collection of:
- **Chips** — from the manifest (memory + I/O + CPU + support)
- **Connectors** — physical ports (joystick, cartridge, serial, power)
- **Embedded components** — keyboards, LEDs, speakers, DIP switches
- **Bus wiring** — which chips share which bus, address decoding rules

```cpp
struct BoardDeclaration {
    const char* name;                    // "C64 Main Board", "NES CPU Board"
    ChipManifest<N> chips;               // All chips on this board
    std::span<const ConnectorDef> connectors;  // Physical ports
    std::span<const ComponentDef> components;  // Non-chip components
    // Bus wiring is implicit from the manifest's address map
};
```

### Multi-board systems

Some systems have multiple physical boards:
- **NES**: CPU board + cartridge board (mapper)
- **Bomb Jack**: main board + sound board (separate CPUs)
- **C64 + 1541**: main board + disk drive board

Each board has its own `Board` instance and tick schedule.

### Component types beyond chips

| Component | Example | Interface |
|-----------|---------|-----------|
| Keyboard | C64 keyboard matrix | Key scan callback, matrix size |
| LED display | LC80 7-segment | Segment state array |
| Speaker | PET piezo | On/off toggle state |
| DIP switches | Arcade config | Bit field read callback |
| Power supply | All systems | Reset line, voltage rails |

---

## Phase 4: Markup-Language System Definitions

**Goal:** Define complete systems in TOML/LJON files, no C++ needed.

This is the culmination — see `CONFIGURABLE_SYSTEM_ARCHITECTURE.md` for the full
TOML schema design.  Key bridge from Phase 3:

1. `BoardDeclaration` becomes serializable to/from TOML
2. `GenericSystem` class reads TOML, constructs boards, wires buses
3. Tick scheduler driven by TOML-declared clock relationships
4. Existing C++ systems become "reference implementations" — TOML equivalents
   can be generated from them

### Prerequisites (not yet built)

- Generic tick scheduler (clock dividers, multi-CPU coordination)
- Chip registry (string name → factory function mapping)
- Connector registry (string name → ConnectorDefinition)
- Bus wiring DSL (address decode rules in TOML)
- Callback wiring (chip-to-chip signal routing without C++ glue)

---

## Migration Priority

Systems ordered by migration complexity (easiest first):

| Priority | System | Effort | Notes |
|----------|--------|--------|-------|
| ✅ Done | VIC-20 | — | Gold standard, all chips in manifest |
| ✅ Done | Atari 2600 | — | TIA + RIOT + CartChip in manifest |
| Low | Apple 1 | Small | CPU only remaining chip outside manifest |
| Low | Acorn Atom | Small | CPU + VDG outside manifest |
| Medium | BBC Micro | Medium | 5 chips to add (CPU, CRTC, PSG, VIA ×2) |
| Medium | PET | Medium | 5 chips to add (CPU, CRTC, PIA ×2, VIA) |
| Medium | C16/Plus4 | Medium | 3 chips to add (CPU, TED, keyboard) |
| Medium | Spectrum | Medium | 3 chips to add (CPU, ULA, AY) |
| Medium | Amstrad CPC | Medium | 5 chips to add (CPU, CRTC, PPI, AY, GA) |
| High | DDR systems | High | Z80 peripherals need MMIO; 4–6 systems |
| High | Arcade | High | Multi-CPU, custom wiring |
| Deferred | C64 | Large | Complex PLA banking, no bus_mem_ yet |
| Deferred | NES | Large | Separate PPU bus, mapper complexity |
