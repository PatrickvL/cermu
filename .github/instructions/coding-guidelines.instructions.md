---
applyTo: '**'
---
# cermu — Project-Wide Coding & Design Guidelines

## Project Identity

cermu is a multi-system retro hardware emulator (C64, VIC-20, C16/Plus4, NES/Famicom, Apple 1, CHIP-8, …). It models real chips at cycle-level accuracy with bus-level signal fidelity. Performance and hardware correctness are first-order priorities.

---

## Architecture

### Type Hierarchy

```
EmulatedSystem                              (src/core/emulated_system.h)
  ├─ [optional intermediate base]           e.g. CommodoreSystem (keyboard, video timing)
  │     └─ ConcreteSystem                   e.g. C64System, VIC20System, C16System
  └─ ConcreteSystem                         e.g. NintendoSystem<V>, Apple1System, Chip8System

ChipBase                                    (src/core/chip.h)
  └─ ConcreteChip                           e.g. MOS6581, VIC-II, PPU, CD4021

Port                               (src/core/port.hpp)
  → attaches PeripheralDevice               e.g. JoystickDevice, NesStandardController

PeripheralDevice                            (src/core/port.hpp)
  └─ [optional intermediate]                e.g. ControlPortInputDevice (DB-9 base)
       └─ ConcreteDevice                    e.g. JoystickDevice, Commodore1351Mouse
```

### Bus Model

- `bus_state_t` (`uint64_t`) is the universal bus word — address, data, and control signals packed into bit fields.
- All memory and I/O access flows through `bus_state_t`. Functions receive it, service it, return it.
- Access via macros only: `BUS_GET_DATA()`, `BUS_SET_DATA()`, `BUS_GET_ADDR()`, `BUS_SET_ADDR()`, `BUS_GET_BIT()`, `BUS_SET_BIT()`, `BUS_CLR_BIT()`.
- Systems may define additional bus words (e.g. `ppu_bus_state_t` for NES PPU bus) with shared bit positions for bridging signals, enabling zero-cost bitmixing.
- Flat memory + page-pointer bank map for fast dispatch (no cascading if-else). See C64's `c64_bus_t` as gold standard.
- Edge detection: compare current bus against `bus_snapshot_` (stored at end of previous tick). No callbacks.

### Cross-System Sharing Rule

**Any entity used by more than one product lives in a shared folder, never under a system directory.**

| Entity | Shared location | Examples |
|--------|----------------|----------|
| Chips | `src/chip/<category>/` | CD4021, future shared sound ICs |
| Port definitions | `src/ports/` | NES controller port (NES, Famicom, VS. System) |
| Peripheral devices | `src/devices/<category>/` | NES gamepad, Zapper, joystick, mice |
| CPU cores | `src/chip/cpu/<family>/` | fam65xx (6502/6510/2A03/…) |

Categories: `cpu/`, `io/`, `input/`, `video/`, `sound/`, `logic/`, `memory/`, `mmu/`, `storage/`.

System-specific code (system glue, tick loop, system-unique chips) stays under `src/systems/<name>/`.

### GUI Separation

- All ImGui rendering (chip layouts, debug windows, settings) lives in files with a `_gui` suffix (e.g. `nes_ppu_gui.cpp`, `cd4021_gui.cpp`).
- Everything GUI-related is compiled only under `#ifdef CERMU_HAS_GUI`.
- Chip implementations own their rendering through `ChipBase` virtual hooks (`has_debug_content()`, `render_debug_content()`, etc.).
- Non-GUI builds (test runners, headless) must compile cleanly without ImGui.

---

## C++ Style

### Language Standard

C++17. Use `if constexpr`, structured bindings, `std::string_view`, fold expressions, `inline constexpr` where appropriate.

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Classes / scoped enums | PascalCase | `ChipBase`, `PortType` |
| Methods, functions | snake_case | `get_chip_info()`, `mem_tick()` |
| Private/protected members | trailing `_` | `cpu_`, `pins_`, `bus_snapshot_` |
| Public struct fields | no trailing `_` | `cutoff_frequency`, `sample_rate` |
| Constants | `constexpr` in namespace | `nes_constants::CPU_FREQ_NTSC` |
| Macros | `UPPER_SNAKE` with prefix | `BUS_GET_DATA`, `VICII_C1_YSCROLL` |
| Files | snake_case | `nes_system.h`, `cd4021_gui.cpp` |
| Namespaces | snake_case preferred | `nes_constants`, `vicii_regs` |
| Template params | `Traits` (NTTP), `V` (enum) | `template<const CPUTraits& Traits>` |

### Formatting

- **4-space indentation**, no tabs.
- **K&R brace style** (opening brace on same line).
- **`#pragma once`** — no include guards.
- Angle brackets for stdlib/SDL (`<cstdint>`), quotes for project headers (`"chip.h"`).
- Align related declarations into visual columns where it aids readability.
- Keep lines under ~120 characters.

### Types

- `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t` exclusively — never `unsigned char`/`int`.
- `using` over `typedef` for new code. `typedef` tolerated in C-interface layers.
- `auto` sparingly — prefer explicit types except in lambdas and range-for.
- `const char*` for string literals in descriptors; `std::string_view` for constexpr contexts.
- Scoped `enum class` for new enums. Unscoped C-style enums tolerated in legacy chip code.

### Constants & Compile-Time

- `constexpr` / `inline constexpr` for all new constants. No new `#define` constants.
- `static constexpr` for class-level compile-time values.
- Legacy `#define` aliases may coexist for backward compatibility but new code uses the constexpr original.
- `static_assert` for template invariants and compile-time checks.

### Templates

- **NTTP** (non-type template parameters) is the core CPU pattern: `fam65xx_t<CPUTraits>` with `if constexpr` for feature gating.
- `std::conditional_t` for compile-time mixin selection.
- Explicit template specialization for variant traits (e.g. `NintendoVariantTraits<V>`).
- No CRTP — classical virtual + template instantiation.
- **Instantiation minimization** — never put non-trivial logic directly in a full-trait NTTP template body. Extract an `_impl` templated only on the scalar fields that actually vary. The outer NTTP wrapper is a one-liner dispatcher.

```cpp
// WRONG — N trait objects → N instantiations of identical code
template <const CPUTraits& T> void adc() { if constexpr (T.has_bcd) ... }

// RIGHT — N trait objects → still only 2 instantiations
template <bool BCD> void adc_impl() { if constexpr (BCD) ... }
template <const CPUTraits& T> void adc() { adc_impl<T.has_bcd>(); }
```

### Memory & Ownership

- `std::unique_ptr` for sole ownership.
- `std::shared_ptr` for shared ownership across subsystems.
- Raw pointers for **non-owning references** and C-callback contexts — never for ownership.
- Raw `uint8_t*` acceptable for hot-path flat memory buffers with manual lifecycle.
- `alignas()` for performance-critical data structures.

### Error Handling

- `bool` return codes for fallible operations (`initialize()`, `load_file()`).
- `static_assert` for compile-time invariants.
- No exceptions on hot paths. `try`/`catch` acceptable for file I/O and serialization only.
- Silent degradation preferred over crashing.

---

## Performance Rules

- **Header-only** where side-effect-free (palette LUTs, bus signal definitions, mapper bank logic, shift registers). Use `inline` / `__attribute__((always_inline))` for hot-path helpers.
- **`.cpp` only for** ImGui rendering, file I/O, system tick loop, and anything with significant translation-unit cost.
- **Page-pointer tables** for memory dispatch — branch-free for common RAM/ROM reads.
- **`LIKELY()` / `UNLIKELY()`** hints on hot branches (defined in `cermu.h`).
- **Zero-cost bus bridging** — shared bit positions between bus words enable mask-OR instead of conditional set/clear.
- Never add abstraction that increases per-tick overhead without measurable benefit.

---

## Design Principles

| Principle | Rule |
|-----------|------|
| One chip = one header | Each hardware chip is a self-contained `ChipBase` subclass. |
| Layouts in `_gui` files | `ChipLayout` definitions + pin rendering under `#ifdef CERMU_HAS_GUI`. |
| Cross-system entities in shared folders | Chips, connectors, and peripherals used by >1 system go in `src/chip/`, `src/ports/`, `src/devices/`. |
| Edge detection via snapshot | Compare current bus against `bus_snapshot_`; no callbacks or state machines. |
| No free functions | Everything is a static member or method. Namespace-level constants are `inline constexpr`. |
| Chip decoupling | Chips must not include system headers. Use opaque callbacks/descriptors to access system resources. |
| Correct first, fast second | Get behavior right, then optimize. But never add gratuitous overhead. |
| Mappers / variants standalone | Each mapper (or system variant) is a standalone type in its own header; a factory creates by ID. |
| Consolidated testing | Use unified test runners, never individual small test programs. |

---

## File Organization

```
src/
├── core/               # Framework: EmulatedSystem, ChipBase, Port, bus_state_t
├── chip/               # Cross-system chip implementations (by category)
│   ├── cpu/            #   CPU cores (fam65xx family, Z80, …)
│   ├── input/          #   Input chips (CD4021 shift register, …)
│   ├── io/             #   I/O controllers (MOS6522 VIA, MOS6526 CIA, …)
│   ├── logic/          #   Logic chips (PLA, …)
│   ├── memory/         #   Memory chips (MOS2114 color RAM, …)
│   ├── mmu/            #   Memory management units (MOS 8722, …)
│   ├── sound/          #   Sound chips (MOS6581 SID, …)
│   └── video/          #   Video chips (VIC-II, TED, VIC 6560/6561, …)
├── ports/         # Cross-system PortDefinition constants
├── devices/            # Cross-system PeripheralDevice implementations
│   ├── input/          #   Controllers, mice, light guns, paddles
│   └── storage/        #   Disk drives, tape drives
├── systems/            # System-specific code
│   ├── c64/            #   Commodore 64
│   ├── vic20/          #   Commodore VIC-20
│   ├── c16/            #   Commodore C16/C116/Plus4
│   ├── nes/            #   Nintendo NES/Famicom
│   ├── apple1/         #   Apple 1
│   └── chip8/          #   CHIP-8
└── utils/              # Shared utilities (shift registers, ring buffers, …)
```

---

## Port / Peripheral Protocol

- `Port` models a physical jack with wired-AND signal propagation (active-low convention).
- `PeripheralDevice` is the abstract base for anything plugged into a port: lifecycle hooks, signal I/O, host input binding, optional `#ifdef CERMU_HAS_GUI` rendering.
- Systems drive connector signals via `Port::write_system_signals()`; devices respond via `on_signal_change()` and update their output signals.
- Devices are registered in `DeviceRegistry` with a `PortType` so they auto-appear in the UI for compatible ports.

---

## Hardware Traits & Configuration

- `HardwareTraits` (fixed) describes immutable hardware characteristics (display resolution, palette size, audio format, CPU frequency).
- `SystemConfiguration` (user-selectable options) allows variant selection (PAL/NTSC, SID revision, memory expansion).
- Systems expose `get_hardware_traits()` and `get_configuration()` / `set_configuration()`.
- TOML-based data-driven configuration enables new systems without C++ code changes.
- Visualization metadata (board layouts, chip packages, PCB positions) lives in TOML files, not in C++ code.

---

## Gold Standards (Reference Implementations)

| Concern | Reference | Why |
|---------|-----------|-----|
| Bus dispatch | C64 `c64_bus_t` / `c64_memory_tick()` | Flat memory, page pointers, I/O handler table |
| CPU template | `fam65xx_t<CPUTraits>` | NTTP + `if constexpr` for zero-overhead variant dispatch |
| C++ chip conversion | TED 7360 | ChipBase inheritance, callbacks via descriptor, proper encapsulation |
| Chip debug/settings | MOS 6581 SID | Left panel (chip viz) + right panel (registers), collapsible sections |
| Port/peripheral | DB-9 `ControlPortInputDevice` + `JoystickDevice` | Signal protocol, host input binding, `#ifdef CERMU_HAS_GUI` |
