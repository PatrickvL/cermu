# Manifest Expansion Roadmap

**Date:** 2026-03-13 (revised)
**Purpose:** Incremental plan to evolve chip manifests into full board declarations,
bridging the gap between current C++ systems and data-driven TOML system definitions.
**Builds On:** Current `ChipManifest` / `Board` / `BusMap` infrastructure,
`CERMU_ARCHITECURE_COMPLETE_REFERENCE.md`, `CONFIGURABLE_SYSTEM_ARCHITECTURE.md`

---

## Infrastructure Completed Since Original Roadmap

The following architectural work has landed since this document was first written and
changes the baseline assumptions for every phase below:

| Work item | Status | Impact |
|-----------|--------|--------|
| `ComponentBase` base class | ✅ Done | `ChipBase` and `Port` derive from it; enables generic component iteration |
| `BoardBase` (non-templated) | ✅ Done | Port ownership, component registry, lifecycle hooks (`reset()`, `power_on()`, `tick()`) |
| `BusMap<Spec>` extraction | ✅ Done | Address-decode logic separated from chip lifetime in `Board<Spec>` |
| `ManifestBusSpec` auto-derivation | ✅ Done | Compile-time `BusSpec` derived from `ChipManifest` — no manual spec writing |
| `System::main_board()` (value member) | ✅ Done | Port ops always go through `main_board()`; no pointer chase, no conditional |
| `System::register_board()` | ✅ Done | Systems with `Board<Spec>` register via `register_board(&board_)` for generic iteration |
| `System::get_boards()` | ✅ Done | Returns all boards (primary first, then registered additional boards) |
| `PortRegistry` + `REGISTER_PORT` | ✅ Done | Singleton `PortType` → `PortDefinition`; standard ports self-register |
| `VideoOutput` / `AudioOutput` descriptors | ✅ Done | Optional output signal metadata on `Port` instances |
| `Session` + `DirectConnection` | ✅ Done | Multi-system composition with bidirectional inter-port wiring |
| `ChipRegistry` + `REGISTER_CHIP` | ✅ Done | Runtime string → factory; two-path resolution (compile-time + registry) |
| `DeviceRegistry` + `REGISTER_DEVICE` | ✅ Done | Runtime peripheral device factories; `get_compatible_devices(PortType)` |
| `SystemRegistry` | ✅ Done | Two-phase file identification, alias-boost, confidence scoring |
| Rename `BusMemory` → `Board` | ✅ Done | Terminology alignment; `Board<Spec>` is the templated owner |
| Rename `EmulatedSystem` → `System` | ✅ Done | Clean naming |
| Rename `GenericEmulatorGUI` → `EmulatorHost`; `SystemGUI` → `SessionGUI` | ✅ Done | GUI host substrate separation |

---

## Current State

### What Manifests Declare Today

Manifests describe **memory-mapped chips** in address space: RAM, ROM, and (in newer
systems) MMIO-only I/O chips.  `Board<Spec>` owns the flat mem, `BusMap<Spec>`
auto-wires page tables / MMIO handlers, and `Board<Spec>` manages chip lifetimes via
factory creation or pre-binding.

| System | Manifest contains | Non-manifest chips (manual) |
|---|---|---|
| VIC-20 | RAM, ROM ×3, CPU, VIC (PAL/NTSC conditional), VIA ×2 — **gold standard** | — |
| Atari 2600 | TIA (MMIO), RIOT (MMIO), CartChip (MMIO), CPU | — |
| Apple 1 | RAM, ROM ×2, PIA (MMIO), CPU | terminal, char ROM (rendering-only) |
| Acorn Atom | RAM ×2, ROM ×3, PPI (MMIO), VIA (MMIO), CPU, VDG | — |
| BBC Micro | RAM, ROM ×2, CPU, CRTC, PSG, VIA ×2 | — |
| Spectrum | RAM, ROM (variant-gated 48K/128K), CPU, ULA (pre-bound), AY (pre-bound) | — |
| C16/Plus4 | RAM, ROM ×2, CPU, TED (pre-bound) | — |
| PET | RAM ×2, ROM ×5, CPU, CRTC, PIA ×2, VIA | — |
| Amstrad CPC | RAM, ROM ×2 (variant-gated 464/6128), CPU, CRTC, PPI, AY, Gate Array (pre-bound) | — |
| DDR KC85 | RAM, IRM, ROM ×1–2 (variant-gated /2, /3, /4), CPU, PIO ×2, CTC, Module System | — |
| DDR Z9001/KC87 | RAM, ROM ×1–4, Video RAM, Color RAM (variant-gated), CPU, PIO ×2, CTC | — |
| DDR LC80 | ROM, RAM, CPU, PIO ×2, CTC | — |
| DDR Z1013 | RAM, ROM, Video RAM (variant-gated 16K/64K), CPU, PIO | char ROM (vector) |
| Bomb Jack | Main: ROM, RAM ×4, CPU.  Sound: ROM, RAM, CPU — **dual `Board<Spec>`** | AY ×3 |
| Namco Arcade | ROM, RAM ×3 (variant-gated Pac-Man/Pengo), CPU | WSG sound |
| C64 | **none** — all chips manual, legacy `c64_bus_t` dispatch | VIC-II, SID, CIA ×2, Color RAM, RAM, ROM ×4, keyboard |
| NES | **none** — all chips manual, separate PPU bus | CPU, PPU, APU, RAM, CIRAM, CD4021 ×2, mapper/cartridge |
| CHIP-8 | **N/A** — pure interpreter, no bus model | ChipPlaceholders for GUI only |

### Generic Lifecycle: `reset_chips()`

`Board<Spec>::reset_chips()` iterates all `owned_chips_` and calls `ChipBase::reset()`.
Currently used by: VIC-20, Atari 2600, Apple 1, Acorn Atom, BBC Micro, PET, C16/Plus4,
Spectrum, Amstrad CPC, LC80, Z1013, KC85, Z9001/KC87, Namco Arcade, Bomb Jack.  Safe
for all chip types (RAM/ROM/CPU have no-op `reset()`; Z80 PIO/CTC/VDG delegate to
`init()`; gate array overrides `reset()`).

`CpuChipBase` provides virtual `init()` and `reset(pins = 0)`.  All migrated systems
call `board_.cpu_chip()->init()` and `board_.cpu_chip()->reset()` for CPU lifecycle.

`BoardBase::power_on()` exists as an empty virtual — no system overrides it yet.

---

## Phase 1: Expand Manifests with All Chips

**Goal:** Move ALL manually-created chips into manifests as non-bus or MMIO-only slots.

### Requirements per chip type

| Chip type | Needs for manifest inclusion |
|-----------|------------------------------|
| Memory-mapped I/O | `has_mmio()`, `on_bus_read()`, `on_bus_write()` overrides |
| Non-memory-mapped (CPU, sound) | Non-bus slot `{0, 0, 0, "label"}` — no address decode needed |
| Conditional (PAL/NTSC) | `condition` tag on slot + `ConditionFn` in `create_chips()` |
| Descriptor-initialized | `create_from_slot()` factory, or pre-bind for complex init |

### Per-system migration checklist

For each system, the migration pattern is:

1. Add `Slot<ChipType>` entries to the manifest (MMIO-only for I/O, zero-size for non-mapped)
2. Remove manual `new ChipType()` calls — let `create_chips()` factory handle creation
3. Replace manual `chip_ = new X()` with `chip_ = board_.chip_as<X>(kSlot)`
4. Move `register_chip()` calls → `register_bus_chips()` handles them automatically
5. Replace individual `chip->reset()` calls → `board_.reset_chips()` + post-reset fixups

### Chips needing MMIO interface additions

These chips are memory-mapped in some systems but currently lack `has_mmio()` /
`on_bus_read()` / `on_bus_write()`.  They have been added to manifests as **non-bus**
slots (Z80 port-based I/O, or pre-bound) and work correctly, but would need MMIO
interfaces if they were ever placed at address-decode-capable positions:

- `ted7360_t` — TED video/I/O for C16/Plus4 (pre-bound, non-bus slot)
- `mc6845_t` — CRTC for BBC Micro, PET, Amstrad CPC (non-bus slot)
- `sn76489_t` — PSG for BBC Micro (non-bus slot)
- `ay_3_8910_t` — sound for Spectrum, Amstrad CPC, arcade (non-bus slot)
- `ferranti_ula_t` — ULA for Spectrum (non-bus slot)
- DDR Z80 peripherals: `z80_pio_t`, `z80_ctc_t` (Z80 port-based I/O, non-bus slots)

Chip extraction completed:
- **Amstrad CPC gate array** — ✅ extracted into `amstrad_gate_array_t : ChipBase`
  subclass with `reset()` override.  Pre-bound into manifest as non-bus slot.
  (Gate array I/O is Z80 port-based, so MMIO interfaces are not needed.)

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

### Current lifecycle methods

| Class | Method | Virtual? | Status |
|-------|--------|----------|--------|
| `ChipBase` | `reset()` | Yes | **Done** — iteratable via `Board::reset_chips()` |
| `ChipBase` | `~ChipBase()` | Yes | **Done** — handled by `unique_ptr` |
| `BoardBase` | `reset()` | Yes | **Done** — pure virtual, overridden per board |
| `BoardBase` | `power_on()` | Yes | **Done** — empty virtual, no overrides yet |
| `BoardBase` | `tick()` | Yes | **Done** — pure virtual, overridden per board |

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

### System generic chip iteration

```cpp
class System {
    // Future: iterate all registered chips (not just bus-owned)
    // void reset_all_chips();        // registered_chips_ iteration
    //
    // Could also walk get_boards() → board->reset_chips() for unified reset
    // across main_board() and registered Board<Spec> instances.
};
```

---

## Phase 3: Board Declarations — Remaining Work

**Goal:** Systems declare boards containing chips, ports, and other components
as a single declarative unit.

### What already exists

`BoardBase` already provides:
- Port ownership (`add_port()`, `get_port()`, `clear_ports()`)
- Component registry (non-owning `ComponentBase*` index over chips + ports)
- Lifecycle hooks (`reset()`, `power_on()`, `tick()`)
- Component lookup (`find_component<T>()`, `find_components<T>()`)

`Board<Spec>` adds:
- Chip ownership (`owned_chips_`, factory creation via `create_chips()`)
- Flat mem ownership
- `BusMap<Spec>` delegation for address-decode
- `register_board_components()` to populate component index

### What's missing

| Missing piece | Description | Blocking? |
|---------------|-------------|-----------|
| Non-chip components | Keyboards, LED displays, speakers, DIP switches — not `ChipBase` subclasses | No — can be added as `ComponentBase` subtypes |
| Multi-board scheduling | Board-level tick scheduling with clock dividers | Blocks multi-board systems |
| Board naming | Human-readable board name (e.g. "C64 Main Board") | Cosmetic |

### Multi-board systems

Some systems have multiple buses on one physical PCB or multiple physical boards:
- **NES**: CPU bus + PPU bus on one PCB (two `Board<Spec>` instances, see NES section)
- **Bomb Jack**: main board + sound board (separate CPUs, already dual `Board<Spec>`)
- **C64 + 1541**: main board + disk drive board (separate system in future `Session`)

Each board has its own `Board<Spec>` instance and tick schedule.

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

### Prerequisites

| Prerequisite | Status | Notes |
|---|---|---|
| Chip registry (string → factory) | ✅ Done | `ChipRegistry` + `REGISTER_CHIP` / `REGISTER_CHIP_TYPE` |
| Port registry (string → `PortDefinition`) | ✅ Done | `PortRegistry` + `REGISTER_PORT`; standard ports self-register |
| Generic tick scheduler | Not started | Clock dividers, multi-CPU coordination |
| Bus wiring DSL | Not started | Address decode rules in TOML |
| Callback wiring | Not started | Chip-to-chip signal routing without C++ glue |

---

## C64 Migration Plan

**Goal:** Replace the legacy `c64_bus_t` manual dispatch with `Board<Spec>` +
`ChipManifest`, absorbing `c64_bus_t` into `C64System`.

### Current architecture

`C64System` embeds a `c64_bus_t` struct that implements a hand-rolled bus model:
- **Flat memory** — single allocation for RAM + all ROMs
- **PLA banking tables** — `cpu_encoded_chip_per_bank_per_mode[32][16]` precomputed
  from the PLA for all 32 modes, copied into active mapping on mode switch
- **I/O page handlers** — function pointer table `io_handlers[16]` covering $D000–$DFFF
  in 256-byte pages (VIC-II, SID, Color RAM, CIA1, CIA2, I/O1, I/O2)
- **VIC-II separate read path** — `vicii_chip_per_bank[16]` table with Character ROM
  visible at $1000–$1FFF (not $D000)
- All 11+ chips owned as raw `new` pointers on `C64System` member fields

The `c64_bus_t` back-references `C64System*` via a raw pointer.

### Migration strategy

The migration replaces `c64_bus_t` with a `Board<C64BusSpec>` while preserving the
C64's critical performance characteristics: branchless page dispatch, precomputed
PLA banking tables, and zero-overhead I/O handler dispatch.

#### Step 1: Define the manifest

```cpp
inline constexpr auto kC64Chips = make_chip_manifest(
    // Memory
    Slot<RAMChip>  {0x0000, 65536, 0, "RAM"},
    Slot<ROMChip>  {0x0000,  8192, 0, "BASIC ROM"},     // $A000, banked
    Slot<ROMChip>  {0x0000,  4096, 0, "Character ROM"},  // $D000 (CPU) / $1000 (VIC-II)
    Slot<ROMChip>  {0x0000,  8192, 0, "Kernal ROM"},     // $E000, banked
    Slot<ROMChip>  {0x0000,  8192, 0, "Cartridge ROML"}, // $8000, conditional
    Slot<ROMChip>  {0x0000,  8192, 0, "Cartridge ROMH"}, // $A000/$E000, conditional
    // I/O (MMIO-only, dispatch via io_handlers)
    Slot<vicii_t>     {0xD000, 0, 0xFC00, "VIC-II"},
    Slot<mos6581_t>   {0xD400, 0, 0xFC00, "SID"},
    Slot<MOS2114>     {0xD800, 0, 0xFC00, "Color RAM"},
    Slot<mos6526_t>   {0xDC00, 0, 0xFF00, "CIA 1"},
    Slot<mos6526_t>   {0xDD00, 0, 0xFF00, "CIA 2"},
    // Non-bus
    Slot<MOS6510>  {0, 0, 0, "MOS 6510"},
);
```

**Note:** ROM base addresses are set to 0 because the PLA banking logic handles all
mapping — `Board<Spec>` owns the buffer but the C64's custom `memory_tick()` drives
page table programming, not the default `BusMap::apply()`.

#### Step 2: Add MMIO interfaces to C64 I/O chips

These chips need `has_mmio()` / `on_bus_read()` / `on_bus_write()`:
- `vicii_t` (VIC-II)
- `mos6581_t` (SID)
- `MOS2114` (Color RAM)
- `mos6526_t` (CIA) — shared with future systems

#### Step 3: Absorb c64_bus_t into C64System

Move the PLA banking tables, I/O handler dispatch, and flat mem access from
`c64_bus_t` into C64System's `Board<C64BusSpec>`:
- PLA table generation → method on C64System or a Board subclass
- `io_handlers[16]` → `BusMap` MMIO handlers or custom MaskedSubTable
- `memory_tick()` → C64System's bus dispatch (may need custom `Board<Spec>` subclass
  to preserve the precomputed table approach)
- Remove `c64_bus_t` struct and its back-pointer to `C64System*`

#### Step 4: Evaluate BusMap fit

The C64's multi-viewer banking (32 PLA modes × CPU/VIC-II) may exceed what
`BusMap::apply()` provides out of the box.  Two options:
- **Option A:** Use `BusMap` for chip ownership + buffer management only, keep
  custom page table programming via `map_chip_read()` / `map_chip_write()` calls
  driven by PLA mode switch.
- **Option B:** Subclass `Board<C64BusSpec>` to override `apply()` with PLA-aware
  logic, using the existing `MemoryBus` multi-viewer infrastructure.

Option A is lower risk and preserves the current branchless hot path.

#### Complexity notes

- The C64 I/O region ($D000–$DFFF) is split into 16 × 256-byte handler pages — this
  maps cleanly to `BusMap` MMIO handlers with masked sub-tables.
- VIC-II's separate memory view (Character ROM at $1000, not $D000) requires a second
  viewer in `MemoryBus` — `ManifestBusSpec` supports `NViewers > 1`.
- Cartridge ROML/ROMH are conditional on EXROM/GAME signals — use manifest `condition`
  tags or manage as dynamic chips.

---

## NES Architecture Notes

The NES is deferred but architecturally significant — its dual-bus design is unique
among our emulated systems and will inform how `Board<Spec>` handles multi-bus PCBs.

### Physical board layout

The NES has a **single main PCB** (the motherboard).  The cartridge plugs into it via
a 72-pin connector (60-pin on the Famicom) and is a second PCB, but the console
itself is one board.

### Dual independent buses

The NES has two genuinely separate, independent buses — not two views of the same bus
(unlike the C64's CPU/VIC-II viewer model):

- **CPU bus** — 16-bit address, 8-bit data.  Sees the 2A03 CPU, 2 KB WRAM, PRG-ROM
  (via cartridge), APU registers, I/O ports ($4016/$4017), and the PPU's register
  window at $2000–$2007.

- **PPU bus** — 14-bit address, 8-bit data.  Sees CHR-ROM/RAM (via cartridge), 2 KB
  nametable VRAM on the NES board, and the PPU's internal palette RAM.  The CPU has
  **zero direct visibility** into this address space.

The two buses are connected only through the narrow 8-register interface ($2000–$2007
on the CPU side), which acts as a mailbox.  The CPU writes to PPU registers and
accesses VRAM indirectly via PPUADDR/PPUDATA; the PPU drives its own bus entirely
independently during rendering.

### Emulation implications

- Two separate `MemoryBus` instances with independent read/write dispatch tables and
  address decoding logic.
- Separate "open bus" / floating bus behaviour — the PPU bus has its own data latch
  distinct from the CPU bus latch.
- The PPU's register window ($2000–$2007) is an MMIO chip on the CPU bus, bridging
  into the PPU bus — a natural `BusMap` MMIO handler.
- The cartridge connector spans **both** buses (PRG on CPU bus, CHR on PPU bus) — the
  mapper sits at the junction and must be visible to both `Board<Spec>` instances.
- This is distinct from the C64's multi-viewer approach (one bus, two viewers).  The
  NES genuinely needs two `Board<Spec>` instances on one logical PCB — similar to
  Bomb Jack's dual-board pattern but within a single physical board.

### Mapping to cermu architecture

```
NESBoard (logical, wraps two Board<Spec>)
├── Board<CPUBusSpec>     ← 16-bit, CPU + WRAM + PRG-ROM + APU + I/O + PPU regs
├── Board<PPUBusSpec>     ← 14-bit, CHR-ROM/RAM + CIRAM + palette
└── Mapper                ← bridges both buses, handles bank switching
```

The Bomb Jack migration (dual `Board<Spec>`, separate CPUs) is a direct stepping
stone — lessons learned there transfer directly to the NES's dual-bus model.

---

## Migration Priority

Systems ordered by migration complexity (easiest first):

| Priority | System | Effort | Chips to add | Notes |
|----------|--------|--------|---|---|
| ✅ Done | VIC-20 | — | — | Gold standard: all chips in manifest including CPU, conditional VIC |
| ✅ Done | Atari 2600 | — | — | All chips in manifest (TIA, RIOT, Cart, CPU); uses `reset_chips()` |
| ✅ Done | Apple 1 | — | — | RAM, ROM ×2, PIA, CPU all in manifest; char ROM is rendering-only, terminal non-chip |
| ✅ Done | Acorn Atom | — | — | RAM ×2, ROM ×3, PPI, VIA, CPU, VDG all in manifest |
| ✅ Done | DDR LC80 | — | — | ROM, RAM, CPU, PIOs ×2, CTC all in manifest; uses `reset_chips()` |
| ✅ Done | DDR Z1013 | — | — | RAM, ROM, Video RAM, CPU, PIO all in manifest; uses `reset_chips()` |
| ✅ Done | DDR KC85 | — | — | All chips in manifest (RAM, IRM, ROMs, CPU, PIOs ×2, CTC, Module System); uses `reset_chips()` |
| ✅ Done | DDR Z9001/KC87 | — | — | All chips in manifest (RAM, ROMs, CPU, PIOs ×2, CTC); uses `reset_chips()` |
| ✅ Done | Namco Arcade | — | — | All memory + CPU in manifest; WSG sound is tick-driven (non-bus) |
| ✅ Done | Bomb Jack | — | — | Dual `Board<Spec>`, both CPUs in manifest; AY sound tick-driven |
| ✅ Done | BBC Micro | — | — | All chips in manifest (RAM, ROMs, CPU, CRTC, PSG, VIAs ×2); uses `reset_chips()` |
| ✅ Done | PET | — | — | All chips in manifest (RAM, Screen RAM, ROMs ×5, CPU, CRTC, PIAs ×2, VIA); uses `reset_chips()` |
| ✅ Done | C16/Plus4 | — | — | All chips in manifest (RAM, ROMs, CPU, TED pre-bound); uses `reset_chips()` |
| ✅ Done | Spectrum | — | — | All chips in manifest (RAM, ROM, CPU, ULA + AY pre-bound); uses `reset_chips()` |
| ✅ Done | Amstrad CPC | — | — | All chips in manifest (RAM, ROMs, CPU, CRTC, PPI, AY pre-bound, Gate Array extracted to ChipBase + pre-bound); uses `reset_chips()` |
| High | C64 | High | All 12 chips | See C64 Migration Plan above; `c64_bus_t` absorbed into `Board<Spec>` |
| Deferred | NES | High | All 7+ chips | Two independent buses (16-bit CPU + 14-bit PPU) on one PCB; dual `Board<Spec>` with mapper bridging both; see NES Architecture Notes above |
| N/A | CHIP-8 | — | — | Pure interpreter, no bus model; not a candidate for manifest migration |
