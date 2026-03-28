# Declarative, Flexible, and High-Performance Emulation Architecture — Implementation Plan

## Vision

cermu aims to support a wide range of retro systems using a declarative, data-driven approach. The ultimate goal is to describe all emulated systems in a user-friendly text format (TOML/YAML), enabling:
- Cycle-exact, hardware-faithful emulation where needed
- High-performance, JIT-accelerated emulation for less timing-critical systems
- Flexible, composable system design via registries of chips, buses, and peripherals
- User-extensible, configurable, and testable systems

---

## Key Concepts & Insights

### 1. Declarative System Description
- All systems, chips, buses, ports, and timing domains are described in data (TOML/YAML), not code.
- Schema must support:
  - Chip types, parameters, pinout, clocks
  - Bus definitions, region mapping, arbitration
  - Port/peripheral types and host bindings
  - Interconnects (signal wiring)
  - Timing domains and system-level options
- Inheritance/overrides for system variants

### 2. Component Registry & Instantiation
- All chips/peripherals register themselves with a global registry.
- Instantiation is data-driven: type + params → instance.
- Connection logic is generic: pins/signals are wired per declarative spec.
- Plugin/extension API for user-supplied chips/devices.

### 3. Memory & Bus Architecture
- Memory regions are declarative: address, size, type (RAM/ROM/MMIO), access policy.
- Bus model supports:
  - Cycle-exact dispatch (for accuracy)
  - Fast region lookup (for JIT/relaxed)
  - Dynamic reconfiguration (bank switching, overlays)
- MMIO is not a special case: all memory-mapped devices are chips with select logic.
- Bus resolves address to chip/region efficiently, with caching for locality.
- Chips check if they are selected via a fast index/pointer compare, preserving the hardware "chip select" abstraction.

### 3b. Bus Signal Propagation (No Bus Master)
- **Key Principle:** Hardware-accurate emulation does not require a "bus master" abstraction.
- Instead, all relevant bus signals (ADDR, DATA, CS, R/W, clock edges, IRQ, NMI, etc.) are propagated to all chips each cycle.
- The system tick loop sequences chips in the correct order for each clock phase:
  1. Address drivers (CPU, VIC-II, DMA) tick first to drive address lines.
  2. All chips can then see the address and chip-select logic resolves.
  3. Selected chips tick to process MMIO or respond with data.
  4. Data bus lines are updated and floated as needed.
  5. Subsequent clock phases allow additional ticks.
- Each chip's tick() function inspects the bus signals and acts accordingly; arbitration emerges from correct sequencing and signal observation, not explicit "ownership."
- This approach is extensible, composable, and matches real hardware behavior.

### 4. Timing & Synchronization

#### Clock Domains & Frequencies
- Chips may run at different clock rates (e.g., CPU 1 MHz, video 8 MHz, sound 2 MHz).
- System declares all clock sources and their relationships:
  - Frequency, phase shift, derived clocks (e.g., clock/2, clock/8).
- "Cycle" is a relative concept: a CPU cycle may be multiple video cycles and vice versa.
- All timing is synchronized to a single system time base; relative frequencies determine tick schedules.

#### Tick Loop Sequencing
- The system tick loop is sequenced by the highest-frequency clock or a declared master clock.
- Low-frequency chips are ticked only at the correct intervals via dividers or explicit scheduling.
- Example: If CPU runs at 1 MHz and video at 8 MHz, the loop ticks video 8x per CPU cycle.
- Strict, single-threaded sequencing ensures correct bus and signal propagation.

#### Clock-Accurate Ticking (Explicit Phases)
- Chips declare which clock edges they respond to (e.g., PHI0 rising, PHI1 falling).
- The tick loop only invokes chips for the clock phases they care about (efficiency optimization).
- Internally, each chip's tick() receives the current clock signal state and bus values.
- Best for: tightly-coupled, bus-shared systems (C64, NES, SNES).

#### Time-Delta Model (Optional)
- Alternative approach: chips accept a time parameter indicating elapsed time since last update.
- Each chip internally checks if its clocks have advanced and decides what to do.
- Only efficient if chips can skip work for idle periods; otherwise, per-cycle stepping is required.
- Adds overhead: each chip must track its own clock phase and compare against the time delta.
- Use only where batching provides genuine CPU savings (e.g., high-frequency clocks with idle periods).

#### Hybrid Approach
- Use explicit clock ticking for tightly-coupled chips (CPU, video, bus).
- Use time-delta batching for loosely-coupled chips (UARTs, timers, sound) where it's safe.
- Synchronize at MMIO, DMA, and event boundaries to maintain correctness.

#### JIT Mode
- CPU runs ahead, executing host-generated code in batches.
- Must synchronize with bus/memory at MMIO, DMA, and interrupt boundaries.
- Other subsystems (video, audio, peripherals) may need clock-accurate ticking or event-driven updates depending on their coupling.

### 5. Performance & Scalability
- JIT support: pluggable CPU cores, fast memory access, direct loads/stores for RAM/ROM, callbacks for MMIO.
- Threading: offload video/audio where safe, but maintain determinism unless user opts in.
- Bus exposes region/chip info for JIT fast-paths.

### 6. User-Facing Configuration & Extensibility
- Schema-driven UI for system creation/configuration.
- Hot-pluggable devices, live reconfiguration.
- Documentation and validation tools.

### 7. Testing, Validation, and Debuggability
- Automated validation of declarative specs (lint, schema check).
- Regression tests for all system variants.
- Debug overlays (bus, signal, timing) must work for data-driven systems.

---

## Timing Model Selection by System Type

Different system architectures benefit from different timing models. Here's how well-known emulators approach this:

### Tightly-Coupled, Bus-Shared Systems
- **Examples:** C64, NES, SNES, Atari 2600
- **Model:** Clock-accurate, explicit phase sequencing, no batching
- **Rationale:** Bus arbitration and signal timing must be simulated at every cycle.
- **Emulators:** MAME (per-device scheduler), VICE (strict tick loop), bsnes/higan (dot-clock model)

### Loosely-Coupled, Event-Driven Systems
- **Examples:** Game Boy, some arcade systems, microcontrollers
- **Model:** Event queues, time-delta batching where safe, synchronization at MMIO/interrupt
- **Rationale:** Subsystems can run largely independently; synchronization points are rare.
- **Emulators:** Gambatte (CPU ahead, sync at events), QEMU (event-driven architecture)

### Hybrid/Multi-Clock Systems
- **Examples:** PS1, Dreamcast, modern retro systems with multiple independent subsystems
- **Model:** Per-subsystem clocks with synchronization at bus/event boundaries
- **Rationale:** Multiple clock domains must be coordinated without per-cycle coupling.
- **Emulators:** MAME (multi-device), modern parallel emulators

### Summary Table

| System Type         | Timing Model               | Batching? | Key Sync Points          |
|---------------------|---------------------------|-----------|------------------------|
| Bus-shared (C64)    | Clock-accurate phases      | No        | Every clock edge         |
| Event-driven (GB)   | Time-delta, event queue   | Yes       | Interrupts, I/O, DMA     |
| Multi-clock (PS1)   | Per-clock + sync          | Selective | Bus access, events       |

---

## Composable Timing Models for cermu

To support multiple timing models within a single flexible framework:

### 1. Chip Tick API Abstraction
Each chip can implement one or more tick interfaces:
- **Clock-phase tick:** `tick(clock_name, phase, bus_state)` → pure signal-level tick
- **Time-delta tick:** `tick_delta(time_elapsed_ns, bus_state)` → chip decides internal action
- **Event callback:** `on_event(event_type, data)` → react to specific events
- Chips declare which models they support; system validates compatibility at load time.

### 2. Declarative Timing Specification
Systems declare their timing model in TOML/YAML:
```toml
[system]
name = "Commodore 64"
timing_model = "clock-accurate"  # or "time-delta" or "hybrid"

[clocks]
phi0 = { frequency = 985248 }      # ~1 MHz
phi1 = { frequency = 985248, phase_shift = 180 }
video_clk = { frequency = 8 * 985248 }  # 8x CPU

[chip.cpu]
tick_model = "clock-accurate"
clocks = ["phi0", "phi1"]

[chip.vicii]
tick_model = "clock-accurate"
clocks = ["video_clk", "phi1"]

[chip.uart]
tick_model = "time-delta"  # optional override
frequency = 19200  # independent clock
```

### 3. Pluggable System Scheduler
The scheduler is selected and configured based on the system's timing model:
- **Clock-accurate scheduler:** Iterates through clock edges, sequences chips per phase.
- **Time-delta scheduler:** Computes next event, advances all chips, processes events.
- **Hybrid scheduler:** Mixes both, synchronizing clock-accurate and batched chips at defined points.

### 4. Clock Hierarchy and Derivation
Support derived clocks for cleaner chip declarations:
```toml
[clocks]
master = { frequency = 8_000_000 }
cpu_clk = { derived_from = "master", divide_by = 8 }  # 1 MHz
audio_clk = { derived_from = "master", divide_by = 4, phase_shift = 90 }
```

### 5. Explicit Synchronization Points
For hybrid systems, declare where different timing models must agree:
```toml
[synchronization]
sync_on_mmio = true        # Time-delta chips sync at MMIO access
sync_on_dma = true         # Sync before/after DMA
sync_on_interrupt = true   # Sync on interrupt delivery
```

### 6. Unified Event System
- All timing models use the same event queue for correctness.
- Clock-accurate systems emit events at specific cycles.
- Time-delta systems query the queue to determine the next synchronization point.
- The scheduler advances all chips to the next event, then processes it.

### 7. Composability Rules
- **Same-model systems:** Use a single scheduler; no sync overhead.
- **Hybrid systems:** Use a "unified scheduler" that:
  - Ticks clock-accurate chips per phase.
  - Batches time-delta chips to the next clock edge or event.
  - Synchronizes at MMIO, DMA, interrupt boundaries.
- **Fail-fast:** If a chip doesn't support the system's timing model, error at load time with clear diagnostics.

---

## Implementation Plan

### Phase 1: Schema & Registry Foundation
- [ ] Design and document the system description schema (TOML/YAML)
- [ ] Refactor all chips/peripherals to register with a global registry
- [ ] Implement data-driven instantiation and connection logic

### Phase 2: MemoryBus & Chip Select Generalization
- [ ] Ensure MemoryBus resolves address to chip/region efficiently, with locality caching
- [ ] Refactor chip tick API to take clock/select signals as input
- [ ] Remove special-case MMIO; all memory-mapped devices are chips with select logic

### Phase 3: Timing & Tick Loop Abstraction
- [ ] Design clock domain abstraction: frequencies, phase relationships, derived clocks
- [ ] Implement clock-accurate scheduler: phase sequencing, efficient chip ticking
- [ ] Implement pluggable scheduler interface for different timing models
- [ ] Add chip API for both explicit clock-phase ticks and time-delta ticks
- [ ] Implement hybrid scheduler for multi-model systems
- [ ] Add synchronization point validation for hybrid systems

### Phase 4: Timing Model Composition
- [ ] Implement declarative timing model specification in schema
- [ ] Add chip capability registration for supported timing models
- [ ] Build unified event system for all timing models
- [ ] Implement system validation: check timing model compatibility at load
- [ ] Test hybrid systems and synchronization correctness

### Phase 5: JIT & Parallelism Integration
- [ ] Integrate JIT CPU core(s) with MemoryBus region info
- [ ] Implement synchronization points for MMIO, interrupts, and events
- [ ] Implement time-delta scheduler for event-driven systems
- [ ] Explore safe parallelization of decoupled subsystems

### Phase 6: User-Facing Tools & Validation
- [ ] Build schema-driven UI for system configuration
- [ ] Implement validation and linting tools for declarative specs
- [ ] Add timing model diagnostics and conflict detection
- [ ] Ensure debug overlays and test infrastructure work for data-driven systems

---

## Key Design Decisions

### No Bus Master Abstraction
- Instead of explicit bus arbitration, all chips see all signals every cycle.
- Correct sequencing of chip ticks ensures proper signal propagation.
- Hardware-faithful, reduces architectural complexity.

### Clock-Accurate Ticking as Default
- For most retro systems, explicit clock/phase sequencing is the most accurate and efficient.
- Time-delta is only beneficial when chips can batch work; otherwise, per-cycle is required.
- Hybrid approach allows selective use of time-delta where beneficial.

### Single Timing Model per System
- Mixing very different timing models within a single system is error-prone and difficult to get right.
- Systems should declare a primary timing model (clock-accurate, time-delta, or hybrid).
- Validation at load time catches incompatibilities early.

---

## Open Questions & Risks
- How to validate synchronization correctness in hybrid systems?
- Can time-delta efficiency gains justify the added complexity for any retro system?
- Plugin API for user chips/peripherals: ABI, safety, and documentation?
- UI/UX for timing model selection: how to make it accessible?
- Save states and serialization across different timing models?
- Security for user-supplied configs/plugins?

---

## Summary Table: Cycle-Exact vs. JIT Model

| Concern         | Cycle-Exact Model                | JIT Model                        |
|-----------------|----------------------------------|----------------------------------|
| Memory Access   | Bus dispatch, chip-select logic  | Direct pointer, bus for MMIO     |
| MMIO            | Chip-select + clock phase        | Callback on access, region check |
| Clocks          | Explicit, per-chip, per-phase    | Only for event timing            |
| Tick API        | All chips tick per clock phase   | Only CPU, with event polling     |
| Events/IRQs     | Scheduled per tick               | Checked at block/cycle boundary  |
| Threading       | Single-threaded                  | Parallel subsystems possible     |

---

## References & Inspirations
- **VICE (C64):** Strict tick loop, all chips see all signals, no bus master.
- **MAME:** Per-device scheduler, supports multiple timing models via device callbacks.
- **bsnes/higan:** Dot-clock model, explicit phase sequencing for accuracy.
- **Gambatte (GB):** Event-driven, CPU ahead with sync points.
- **QEMU:** Time-delta/event-driven architecture for flexibility.
- **ClockSignal:** Time-delta model with bus accuracy challenges (cautionary example).

---

## Next Steps
1. Prototype a simple system (e.g., 8-bit micro) using clock-accurate ticking.
2. Design and validate the clock domain abstraction.
3. Build the pluggable scheduler interface.
4. Add chip tick API variants and registration.
5. Implement hybrid system support with validation.
6. Iterate on schema, registry, and bus design.
7. Involve users early for feedback on timing model selection and configuration.

---

_Last updated: 2026-03-28 (revised with bus signal propagation, clock domains, and composable timing models)_
