# Configurable System Architecture

**Date:** 2026-03-13 (revised)
**Purpose:** Design data-driven system configuration to enable users to create custom
systems from hardware components without writing C++ code.
**Builds On:** `CERMU_ARCHITECURE_COMPLETE_REFERENCE.md`, `MANIFEST_EXPANSION_ROADMAP.md`,
`HARDWARE_VISUALIZATION_ARCHITECTURE.md`

---

## Executive Summary

This document describes how cermu's existing C++ infrastructure — `ChipRegistry`,
`Board<Spec>`, `ChipManifest`, `MemoryBus`, `PortRegistry`, and `Session` — can be
bridged into a data-driven layer where complete systems are declared in TOML files.
A `GenericSystem` class reads these declarations at runtime and constructs a working
emulated system using the same chip, port, and bus primitives that hand-coded systems
use.

### Key Benefits

- **User Extensibility** — create new systems without writing C++ code
- **Rapid Prototyping** — test hardware configurations quickly via text files
- **Educational Tool** — learn computer architecture by assembling systems
- **Historical Accuracy** — recreate obscure system variants and prototypes
- **Coexistence** — hand-coded C++ systems and file-driven systems run side by side

### Current Status

| Piece | State |
|-------|-------|
| TOML data files (`data/systems/apple1.toml`, `c64.toml`) | ✅ Written |
| TOML parser library (`external/tomlplusplus/`) | ✅ Available |
| `ChipRegistry` — string → factory | ✅ Implemented |
| `PortRegistry` — `PortType` → `PortDefinition` | ✅ Implemented |
| `SystemRegistry` — file probe + system factory | ✅ Implemented |
| `Board<Spec>` / `BusMap<Spec>` / `ChipManifest` | ✅ Implemented |
| `GenericSystem` runtime builder class | ⏳ Not started |
| TOML loader → `ChipManifest` bridge | ⏳ Not started |
| Generic tick scheduler | ⏳ Not started |
| Callback wiring DSL | ⏳ Not started |

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│            System Declaration (TOML)                    │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │  Hardware   │  │    Memory    │  │  Connections  │  │
│  │ Components  │  │     Map      │  │   & Ports     │  │
│  └─────────────┘  └──────────────┘  └───────────────┘  │
└─────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────┐
│              Existing Registries                        │
│  ChipRegistry    PortRegistry    SystemRegistry         │
│  (REGISTER_CHIP) (REGISTER_PORT) (system probes)        │
└─────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────┐
│       GenericSystem : public System                     │
│  • Parses TOML via tomlplusplus                         │
│  • Builds ChipManifest at runtime                       │
│  • Instantiates Board<GenericBusSpec>                    │
│  • Creates chips via ChipRegistry factories             │
│  • Wires page tables / MMIO via BusMap                  │
│  • Configures ports via PortRegistry                    │
│  • Drives tick loop per timing section                  │
└─────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────┐
│           Running System (same as C++ systems)          │
│  Board<Spec> + MemoryBus + owned chips + ports          │
└─────────────────────────────────────────────────────────┘
```

---

## 1. Existing Infrastructure That GenericSystem Builds On

### ChipBase + ChipRegistry

All hardware chips derive from `ChipBase` (`src/core/chip.hpp`).  Chips that sit on
the memory bus override `has_mmio()`, `on_bus_read()`, `on_bus_write()`.  Non-bus
chips (CPUs, sound generators) participate as zero-size manifest slots.

`ChipRegistry` (`src/core/chip_registry.hpp`) maps string names to factory functions:

```cpp
// Chips self-register in their .cpp files:
REGISTER_CHIP_TYPE("MOS6502", MOS6502);
REGISTER_CHIP_TYPE("PIA6820", pia6820_t);
REGISTER_CHIP_TYPE("MOS6522", mos6522_t);
REGISTER_CHIP_TYPE("Z80",     Z80);
// ... 30+ registered chips

// Runtime lookup:
ChipBase* chip = ChipRegistry::instance().create("MOS6502", slot, system_bus, buffer);
```

### ChipManifest + Board\<Spec\> + BusMap\<Spec\>

`ChipManifest<N>` is a compile-time array of `ChipSlot` entries.  `Board<Spec>`
owns the unified memory buffer and chip lifetimes.  `BusMap<Spec>` handles address
decode — page-pointer tables for RAM/ROM, MMIO trampoline handlers for I/O chips,
masked sub-tables for sub-page decode.

For file-driven systems, the manifest is built at **runtime** from parsed TOML rather
than at compile time.  `Board<GenericBusSpec>` uses a fixed spec large enough to cover
all simple systems (see GenericBusSpec below).

### PortRegistry

`PortRegistry` (`src/core/port_registry.hpp`) maps `PortType` → `PortDefinition`.
Standard port types (DB-9, IEC serial, NES controller, composite video, etc.)
self-register from `port.cpp`.  The TOML `[[ports]]` section references port types
by name; `GenericSystem` looks them up via the registry.

### ManifestBusSpec

`ManifestBusSpec` auto-derives `BusSpec` fields from a `ChipManifest` at compile
time.  For runtime manifests, `GenericBusSpec` provides fixed conservative bounds.

---

## 2. GenericBusSpec

A fixed `BusSpec` that covers all file-driven systems without requiring compile-time
manifest analysis:

```cpp
struct GenericBusSpec {
    using AddrType = uint16_t;

    static constexpr size_t AddressBits       = 16;
    static constexpr size_t PageBits           = 8;
    static constexpr size_t NumViewers         = 1;
    static constexpr size_t MaxChipId          = 32;
    static constexpr size_t MaxWriteChipId     = 32;
    static constexpr bool   EnableMmio         = true;
    static constexpr size_t MaxMmioHandlers    = 8;
    static constexpr size_t MaxMaskedSubTables = 2;
    static constexpr size_t MaxMaskedRegions   = 8;
};
```

**Design rationale:**
- 16-bit address / 256-byte pages — fits all 8-bit systems (6502, Z80, 6809)
- 32 chip IDs — comfortably covers the largest simple systems (~12 chips)
- 8 MMIO handlers — enough for typical I/O chip counts
- Single viewer — complex multi-viewer systems (C64 PLA, NES dual-bus) stay as C++
- Conservative bounds keep `MemoryBus` instantiation lightweight

Systems that exceed these bounds (banked memory, multi-viewer, PLA logic) are **not
candidates for file-driven definition** and remain hand-coded C++ `System` subclasses.

---

## 3. TOML Configuration Format

### Existing Data Files

Two system declarations already exist and define the schema by example:

- `data/systems/apple1.toml` — Apple 1 (simple linear address space)
- `data/systems/c64.toml` — Commodore 64 (banked memory, for reference/documentation)

### Schema Overview

```toml
[system]
name = "Apple 1"
short_name = "APPLE1"
description = "Apple Computer 1 (1976)"

[hardware_traits.display]
native_width = 320
native_height = 192
format = "RGBA8888"
palette = [[0,0,0,255], [51,255,51,255]]

[hardware_traits.audio]
format = "NONE"
sample_rate = 0
channels = 0

[hardware_traits.timing]
cpu_frequency = 1000000
target_fps = 60
cycles_per_frame = 16667
region = "NTSC"

# ── Components ─────────────────────────────────────────
# Each component maps to a ChipRegistry entry.
# The "model" field is the ChipRegistry key.

[[components]]
id = "cpu"
type = "CPU"
model = "MOS6502"
description = "MOS 6502 CPU @ 1 MHz"

    [components.config]
    frequency = 1000000

[[components]]
id = "ram"
type = "RAM"
model = "GENERIC"

    [components.config]
    address = "$0000"
    size = 8192

[[components]]
id = "monitor_rom"
type = "ROM"
model = "GENERIC"

    [components.config]
    address = "$FF00"
    size = 256
    file = "data/apple1/roms/apple1.rom"

[[components]]
id = "pia"
type = "PIA"
model = "PIA6820"

    [components.config]
    address = "$D010"
    size = 4

# ── Memory Map ─────────────────────────────────────────
# Translates directly into ChipManifest slots.
# GenericSystem builds the manifest from these regions.

[memory_map]
type = "SIMPLE"

[[memory_map.regions]]
name = "Main RAM"
address = "$0000"
size = 8192
component = "ram"
writable = true

[[memory_map.regions]]
name = "PIA Registers"
address = "$D010"
size = 4
component = "pia"
writable = true

[[memory_map.regions]]
name = "Woz Monitor ROM"
address = "$FF00"
size = 256
component = "monitor_rom"
writable = false

# ── Connections (signal routing) ───────────────────────

[[connections]]
from = "pia"
to = "terminal"
type = "port"
signals = ["PORT_B"]

    [connections.mapping]
    "pia.port_b" = "terminal.data_in"

# ── Ports ──────────────────────────────────────────────
# Optional: physical connector declarations.
# Port types are looked up in PortRegistry.

# ── Board Layout (optional, for visualization) ─────────

[board_layout]
name = "Apple Computer 1 (Original)"
revision = "Rev 0"
width_mm = 210
height_mm = 270

[[board_layout.sockets]]
socket_id = "A1"
component = "cpu"
x = 50
y = 150
orientation = "NORTH"

# ── File Support ───────────────────────────────────────

[[file_support]]
extension = ".bin"
type = "BINARY"
load_address = "$0280"
```

### Key Design Decisions

**Component `model` = `ChipRegistry` key.**  No new factory or interface needed — the
existing `REGISTER_CHIP_TYPE` / `REGISTER_CHIP` macros provide the runtime lookup.
`GenericSystem` calls `ChipRegistry::instance().create(model, slot, bus, buffer)`.

**Memory map regions → `ChipSlot` entries.**  Each `[[memory_map.regions]]` becomes a
runtime-built `ChipSlot` with `base_addr`, `size_bytes`, and `addr_mask` (derived from
size for I/O chips).  RAM/ROM regions produce buffer-backed slots; I/O regions produce
MMIO-only slots for chips with `has_mmio() == true`.

**`type = "BANKED"` memory maps are documentation only.**  Banked memory (C64, Spectrum
128K) requires PLA logic or custom page-table programming that cannot be expressed
declaratively.  The C64 TOML file serves as **hardware documentation**, not as a
loadable system definition.  `GenericSystem` only supports `type = "SIMPLE"`.

---

## 4. GenericSystem Implementation

```cpp
class GenericSystem : public System {
public:
    explicit GenericSystem(const char* toml_path);

    // System interface
    bool initialize() override;
    void shutdown()   override;
    void reset()      override;
    void tick()       override;
    void run_frame()  override;
    bool load_file(const char* filepath) override;

private:
    // TOML parsing
    bool parse_toml(const char* path);
    bool build_manifest();      // memory_map.regions → ChipSlot vector
    bool create_chips();        // ChipRegistry lookup per component
    bool setup_ports();         // PortRegistry lookup per port declaration
    bool wire_connections();    // Signal routing between components

    // Runtime manifest (built from TOML, not constexpr)
    std::vector<ChipSlot> slots_;

    // Board — uses GenericBusSpec for all file-driven systems
    Board<GenericBusSpec> board_;

    // Parsed config
    struct ParsedConfig {
        std::string name;
        std::string short_name;
        HardwareTraits traits;
        // ... component list, memory regions, connections
    } config_;
};
```

### Initialization Flow

```
1. parse_toml(path)
     ├─ Parse [system], [hardware_traits]
     ├─ Parse [[components]] → component descriptors
     ├─ Parse [memory_map] → region list
     └─ Parse [[connections]], [[ports]], [board_layout]

2. build_manifest()
     ├─ For each memory_map.region:
     │     Convert to ChipSlot {base_addr, size, mask}
     │     Resolve factory via ChipRegistry::instance().lookup(model)
     ├─ For each non-bus component (CPU, sound):
     │     Add zero-size slot {0, 0, 0, label}
     └─ Build runtime manifest, initialize Board<GenericBusSpec>

3. create_chips()
     ├─ board_.create_chips(system_bus)
     │     Factory functions from ChipRegistry create each chip
     └─ Cache typed pointers via board_.chip_as<T>(slot_index)

4. setup_ports()
     ├─ For each [[ports]] entry:
     │     PortRegistry::instance().lookup(port_type)
     │     primary_board_.add_port(definition)
     └─ Attach default peripherals via DeviceRegistry

5. wire_connections()
     ├─ Signal routing (IRQ → CPU, port mappings)
     └─ Set chip descriptors / callbacks

6. board_.apply(mem_bus)  — wire page tables + MMIO handlers
7. register_board(&board_)
8. register_bus_chips(board_)
```

### Memory Dispatch

`GenericSystem` uses the **same page-pointer dispatch** as all other cermu systems.
There is no linear-scan region lookup at runtime.  The `BusMap<GenericBusSpec>`
programs `MemoryBus` page tables during `apply()` — RAM/ROM pages get direct buffer
pointers, MMIO chips get trampoline handlers.  The hot path is identical to a
hand-coded system.

---

## 5. Scope Boundaries

### What File-Driven Systems CAN Express

| Feature | Support |
|---------|---------|
| Linear address space (no banking) | ✅ Full |
| RAM, ROM, MMIO chips on one bus | ✅ Full |
| Sub-page MMIO (e.g. PIA at 4 bytes) | ✅ Via `addr_mask` |
| Conditional chips (PAL/NTSC) | ✅ Via `condition` field |
| Physical port declarations | ✅ Via `PortRegistry` |
| Board layout visualization | ✅ Via `[board_layout]` |
| Hardware traits (palette, timing) | ✅ Via `[hardware_traits]` |
| File format associations | ✅ Via `[[file_support]]` |

### What File-Driven Systems CANNOT Express

| Feature | Why | Alternative |
|---------|-----|-------------|
| Banked memory (C64 PLA, Spectrum 128K) | Requires custom page-table programming | C++ `System` subclass |
| Multi-viewer buses (C64 CPU/VIC-II) | `GenericBusSpec` has 1 viewer | C++ with `NViewers > 1` |
| Multi-bus systems (NES CPU + PPU) | Needs two `Board<Spec>` instances | C++ dual-board |
| Custom tick ordering | Phase-accurate interleaving | C++ `tick()` override |
| Chip-to-chip callbacks | IRQ wiring, bank-select callbacks | C++ glue code |
| Indexed sub-tables | Complex address aliasing | C++ `BusMap` direct use |

**Rule of thumb:** if a system needs custom `tick()` ordering or dynamic bank
switching, it stays as a C++ `System` subclass.  File-driven definitions are for
systems with straightforward linear address maps.

---

## 6. Registration with SystemRegistry

`GenericSystem` registers itself once with `SystemRegistry`.  When the user opens a
`.toml` file, the registry's probe identifies it as a system declaration:

```cpp
// In generic_system.cpp (one-time registration)
REGISTER_SYSTEM({
    .name = "Generic (TOML)",
    .probe_file = [](const char* path, const uint8_t*, size_t) -> float {
        // Check for .toml extension + [system] section
        return is_toml_system_file(path) ? 0.9f : 0.0f;
    },
    .supported_formats = { ".toml" },
}, []() {
    return std::make_unique<GenericSystem>();
});
```

This means `.toml` system files are loadable via the standard `File → Open` path —
`SystemRegistry` routes them to `GenericSystem` automatically.

---

## 7. Migration Path

### Phase 1: Parallel Implementation (No Breaking Changes)

1. Implement `GenericSystem` + `GenericBusSpec`
2. Implement TOML parser → runtime `ChipManifest` bridge
3. Load `data/systems/apple1.toml` and verify identical behavior to `Apple1System`
4. Both coexist — `Apple1System` (C++) and the TOML version produce the same output

### Phase 2: Expand Coverage

1. Create TOML definitions for other simple systems (Acorn Atom, LC80, Z1013)
2. Add `[[connections]]` signal wiring for systems that need it
3. Extend `GenericBusSpec` bounds if any system exceeds them
4. All existing C++ systems remain unchanged and available

### Phase 3: Advanced Features (Future)

1. Generic tick scheduler (clock dividers, multi-rate components)
2. Callback wiring DSL for chip-to-chip signal routing
3. Multi-board TOML declarations for composite systems
4. C++ systems can optionally export their config as TOML for documentation

**C++ systems are never removed.**  They serve as reference implementations,
performance baselines, and hosts for features that TOML cannot express.

---

## 8. Use Cases

### Custom Hardware

```toml
[system]
name = "My Homebrew 6502"

[[components]]
id = "cpu"
model = "MOS6502"
    [components.config]
    frequency = 2000000

[[components]]
id = "ram"
model = "GENERIC"
    [components.config]
    address = "$0000"
    size = 32768

[[components]]
id = "via"
model = "MOS6522"
    [components.config]
    address = "$6000"
    size = 16
```

### Historical Variants

- `apple1_4k.toml` — 4 KB RAM variant
- `apple1_basic.toml` — with Woz BASIC ROM loaded
- `pet_8k.toml` — early PET with 8 KB RAM

### Educational Progression

1. `minimal_6502.toml` — just CPU + RAM (bare-metal)
2. `basic_computer.toml` — add ROM monitor + PIA I/O
3. `text_terminal.toml` — add character display
4. `full_system.toml` — complete retro computer with sound

### Multi-System Sessions (Future)

```toml
# session.toml — loaded by Session
[[systems]]
name = "C64"
config = "data/systems/c64_reference.toml"

[[systems]]
name = "1541 Drive"
config = "data/systems/1541.toml"

[[connections]]
from = "C64.serial_port"
to = "1541 Drive.serial_port"
type = "cable"
```

---

## 9. Relationship to Other Documents

| Document | Relationship |
|----------|-------------|
| `MANIFEST_EXPANSION_ROADMAP.md` | Phase 4 of that roadmap is the implementation of this architecture |
| `CERMU_ARCHITECURE_COMPLETE_REFERENCE.md` | Defines `Board`, `BusMap`, `Session`, `GenericBusSpec` |
| `HARDWARE_VISUALIZATION_ARCHITECTURE.md` | `[board_layout]` section feeds into the board visualization system |
| `CHIP_VISUALIZATION_PROPOSAL.md` | Chips created by `GenericSystem` use the same debug UI as C++ chips |
