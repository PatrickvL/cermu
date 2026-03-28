# cermu Timing Architecture — Clock Domains, Tick Models, and Composability

**Date:** 2026-03-28  
**Status:** Design specification  
**Supersedes:** Timing sections of previous architecture documents

---

## Overview

cermu's emulation timing is driven by explicit clock signals, not a fixed "cycle" abstraction. Different systems use different sets of clocks at different frequencies; cermu must support them all accurately while remaining composable and efficient.

This document specifies:
1. How clock domains are defined and related
2. How chip ticking is sequenced across multiple clocks
3. Different timing models (clock-accurate, time-delta, hybrid) and when to use each
4. How to declare timing in TOML and validate it at load time

---

## Part 1: Clock Domains & Frequencies

### Fundamental Concepts

- **System Time Base:** A single, master clock frequency defines the smallest meaningful time unit. Typically the highest chip frequency in the system (or LCM of all frequencies).
- **Clock Domain:** A named clock signal (e.g., `PHI0`, `PHI1`, `PIXEL_CLK`) at a specific frequency and phase.
- **Relative Frequency:** How many cycles of one clock occur per cycle of another (e.g., 8 video cycles per CPU cycle).
- **Phase Relationship:** Two clocks may be offset (e.g., `PHI1` = `PHI0` + 180°).

### Example: Commodore 64

```toml
[clocks]
# Master clock: ~1 MHz (985248 Hz for NTSC)
phi0 = { frequency = 985248, phase = 0 }

# PHI1 is PHI0 inverted (180° shift)
phi1 = { frequency = 985248, phase = 180 }

# VIC-II runs at 8x the CPU clock
video_clk = { frequency = 7_881_984, derived_from = "phi0", multiply_by = 8 }

# Sound is slower, but still needs to be precise
sid_clk = { frequency = 985248, same_as = "phi0" }
```

- CPU (MOS6510) ticks on `phi0` rising and `phi1` rising (2x per "cycle")
- VIC-II ticks on `video_clk` rising (8x per CPU cycle, 16x per CPU "cycle")
- All clocks are synchronized to the 985248 Hz base

### Clock Specification in TOML

```toml
[clocks]
# Primary clock (often the CPU)
master = { frequency = 1_000_000, phase = 0 }

# Derived clock: divide by N
slow_io = { base = "master", divide_by = 4 }

# Derived clock: multiply by N
video = { base = "master", multiply_by = 8 }

# Clock with phase shift
quad1 = { base = "master", phase_shift = 0 }
quad2 = { base = "master", phase_shift = 90 }
quad3 = { base = "master", phase_shift = 180 }
quad4 = { base = "master", phase_shift = 270 }

# Independent clock (not derived)
external_clk = { frequency = 2_097_152, independent = true }
```

---

## Part 2: Chip Clock Declaration & Ticking

### Chip Clock Inputs

Each chip declares which clock edges it responds to:

```toml
[chip.cpu]
name = "MOS6510"
clocks = ["phi0_rising", "phi1_rising"]  # Two ticks per CPU cycle

[chip.vicii]
name = "MOS6569"
clocks = ["video_clk_rising"]  # One tick per video clock

[chip.sid]
name = "MOS6581"
clocks = ["phi0_rising"]  # One tick per CPU cycle (though internally clocked by phi0)

[chip.cartridge]
name = "Cartridge"
clocks = []  # No clock ticks; only responds to bus accesses
```

### Chip Tick API

Each chip's tick function receives:

```cpp
// Clock-accurate style (preferred for cycle-exact)
void tick_clock(const std::string& clock_name, 
                signal_edge_t edge,
                const bus_state_t& current_bus);

// Time-delta style (optional, for loosely-coupled systems)
void tick_delta(uint64_t elapsed_ns,
                const bus_state_t& current_bus);
```

The chip's tick implementation:
- Observes clock signal and edge (rising or falling)
- Reads current bus state (ADDR, DATA, CS, R/W, IRQ, NMI, etc.)
- Updates internal state
- May write back to bus (e.g., drive data lines)

### Clock Edge Terminology

```
signal:   ────┐         ┌────────┐
             └─────────┘         └
         falling edge  rising edge

"phi0_rising"   = CLK transitions from low to high
"phi0_falling"  = CLK transitions from high to low
"phi0"          = Either edge (both rising and falling)
```

Chips declare only the edges they care about:
- MOS6510: `["phi0_rising", "phi1_rising", "phi1_falling"]` (three edges per cycle)
- MOS6569: `["video_clk_rising"]` (one edge per clock)

---

## Part 3: System Tick Loop Sequencing

### Clock-Accurate Sequencing (Explicit Phases)

The system tick loop is driven by the highest-frequency clock. At each tick:

1. **Determine the next active clock edge** (e.g., which clock is rising)
2. **Advance system time** by the time delta to that edge
3. **Sequence chips** registered for that edge, in dependency order:
   - Address drivers (CPU, DMA) first → set address lines
   - Address decoding → resolve chip-select
   - Selected memory/IO chips → respond to read/write
   - Other dependent chips → react to results
4. **Repeat** until the emulation advances the desired amount (e.g., one frame)

### Optimized Sequencing: Early Exit

Only tick chips that care about the current clock edge:

```cpp
// Pseudo-code
for (const auto& clock_event : get_next_clock_events()) {
    const std::string& clock_name = clock_event.clock;
    signal_edge_t edge = clock_event.edge;
    
    // Only tick chips registered for this clock+edge
    for (auto chip : chips_for_clock[clock_name][edge]) {
        chip->tick_clock(clock_name, edge, current_bus);
    }
}
```

This avoids calling `tick()` for every chip on every cycle; only active chips are ticked.

### Handling Lower-Frequency Clocks

A chip running at 1/4 the master clock doesn't need to tick 4x per iteration:

```cpp
// Option 1: Frequency divider in the scheduler
if ((master_cycle_count % 4) == 0) {
    slow_chip->tick_clock("slow_clk", edge, current_bus);
}

// Option 2: Chip registers only for every 4th edge
chips_for_clock["slow_clk_rising_div4"].push_back(slow_chip);
// Scheduler only calls this when master_cycle % 4 == 0
```

---

## Part 4: Timing Models

### Model 1: Clock-Accurate (Explicit Phases)

**Use for:** Tightly-coupled, bus-shared systems (C64, NES, SNES)

**How it works:**
- All chips declared to use clock-accurate ticking
- Scheduler ticks each chip when its clock edge occurs
- Strict sequencing ensures correct signal propagation
- No batching; every clock edge is simulated

**Pros:**
- Most accurate for bus-level timing
- Matches real hardware signal flow
- Good for cycle-exact emulation

**Cons:**
- Must simulate every clock; no way to skip idle periods
- Requires careful sequencing

**TOML:**
```toml
[system]
timing_model = "clock-accurate"

[chip.cpu]
tick_model = "clock-accurate"
clocks = ["phi0_rising", "phi1_rising"]
```

### Model 2: Time-Delta (Batching)

**Use for:** Loosely-coupled, event-driven systems (Game Boy, some microcontrollers)

**How it works:**
- Chips accept a time delta parameter instead of explicit clock edges
- Each chip tracks how much time has passed since last update
- Chip decides internally whether its clocks have advanced enough to do work
- System can "fast-forward" if no events occur (e.g., idle wait loops)

**Pros:**
- Can batch work for idle periods (no bus activity, no state changes)
- Reduces per-cycle overhead

**Cons:**
- Each chip must track its own clock phase and compare against delta
- Only beneficial if chips can skip work
- Harder to reason about for shared-bus systems

**TOML:**
```toml
[system]
timing_model = "time-delta"

[chip.uart]
tick_model = "time-delta"
frequency = 115200  # Chip's independent clock
batching_safe = true  # Can skip work during idle
```

**When time-delta saves nothing:**
If every clock cycle involves bus activity or state changes, batching provides no benefit; per-cycle ticking is just as fast and more straightforward.

### Model 3: Hybrid (Mixed Models)

**Use for:** Complex systems with both tightly-coupled and loosely-coupled subsystems

**How it works:**
- Some chips use clock-accurate ticking (e.g., CPU + bus)
- Others use time-delta batching (e.g., UART, timer)
- Scheduler synchronizes at defined boundaries (MMIO, DMA, interrupts)

**Example: C64 with real-time clock**
```toml
[system]
timing_model = "hybrid"

[chip.cpu]
tick_model = "clock-accurate"
clocks = ["phi0_rising", "phi1_rising"]

[chip.rtc]
tick_model = "time-delta"
frequency = 32768  # Independent crystal

[synchronization]
sync_on_mmio = true        # Sync RTC when CPU hits MMIO
sync_on_interrupt = true   # Sync RTC before IRQ delivery
```

**Synchronization Points:**
The scheduler advances clock-accurate and time-delta chips to the next sync point, then processes any events.

**Pros:**
- Combines accuracy and efficiency
- Flexible for diverse chip types

**Cons:**
- Complex to implement correctly
- Must identify safe sync points
- More difficult to debug

---

## Part 5: Composability & Validation

### Declaring System Timing in TOML

```toml
[system]
name = "Commodore 64"
timing_model = "clock-accurate"  # or "time-delta" or "hybrid"

[clocks]
phi0 = { frequency = 985248 }
phi1 = { frequency = 985248, phase = 180 }
video_clk = { base = "phi0", multiply_by = 8 }

[chip.cpu]
name = "MOS6510"
tick_model = "clock-accurate"
clocks = ["phi0_rising", "phi1_rising"]

[chip.vicii]
name = "MOS6569"
tick_model = "clock-accurate"
clocks = ["video_clk_rising"]

# Synchronization (if hybrid)
[synchronization]
sync_on_mmio = true
sync_on_interrupt = true
```

### Runtime Validation

At system load time, check:

1. **All chips support the system's timing model**
   - If system declares "clock-accurate" but a chip only supports "time-delta", error.
   - Fail-fast with clear diagnostics.

2. **Clock declarations are consistent**
   - Derived clocks have valid base clocks
   - Frequencies are consistent (e.g., video_clk is truly 8x CPU)
   - Phase shifts are valid (0–360°)

3. **Synchronization points are reachable**
   - For hybrid systems, check that sync points (MMIO, interrupt) are actually triggered

4. **Chip capability registration**
   - Each chip registers which timing models it supports:
   ```cpp
   class MOS6510 : public ChipBase {
   public:
       std::vector<std::string> supported_tick_models() const override {
           return {"clock-accurate"};  // Only clock-accurate
       }
   };
   
   class Uart : public ChipBase {
   public:
       std::vector<std::string> supported_tick_models() const override {
           return {"clock-accurate", "time-delta"};  // Both
       }
   };
   ```

---

## Part 6: Scheduler Implementation

### Clock-Accurate Scheduler

```cpp
class ClockAccurateScheduler {
    struct ClockEvent {
        uint64_t time_ns;
        std::string clock_name;
        signal_edge_t edge;
        std::vector<ChipBase*> listeners;
    };
    
    std::priority_queue<ClockEvent> events;
    
    void run_until(uint64_t target_time_ns) {
        while (!events.empty() && events.top().time_ns <= target_time_ns) {
            auto event = events.top();
            events.pop();
            
            // Tick all chips listening for this clock edge
            for (auto chip : event.listeners) {
                chip->tick_clock(event.clock_name, event.edge, current_bus);
            }
            
            // Schedule the next edge of this clock
            enqueue_next_edge(event.clock_name, event.edge);
        }
    }
};
```

### Time-Delta Scheduler

```cpp
class TimeDeltaScheduler {
    void run_until(uint64_t target_time_ns) {
        while (current_time_ns < target_time_ns) {
            // Determine next event (MMIO, interrupt, etc.)
            uint64_t next_event_time = find_next_event();
            uint64_t batch_end = std::min(next_event_time, target_time_ns);
            
            // Advance all time-delta chips by the batch
            uint64_t elapsed = batch_end - current_time_ns;
            for (auto chip : all_chips) {
                chip->tick_delta(elapsed, current_bus);
            }
            
            current_time_ns = batch_end;
            
            // Process events at the boundary
            process_events_at(current_time_ns);
        }
    }
};
```

---

## Part 7: Design Decisions & Rationale

### No Bus Master Abstraction

**Decision:** Hardware-faithful emulation does not require explicit "bus master" logic. Instead, all chips see all signals every cycle, and correct sequencing ensures proper behavior.

**Rationale:**
- Real hardware has no "bus master" abstraction; signals propagate freely.
- Correct sequencing of chip ticks is simpler and more composable than explicit arbitration.
- Reduces architectural complexity and potential for bugs.

**Implication:** The system tick loop must order chips correctly (address drivers first, then selected chips).

### Clock-Accurate as Default

**Decision:** For most retro systems, clock-accurate ticking is the safest and most efficient default.

**Rationale:**
- Time-delta only saves work if chips can skip idle periods.
- If every cycle has bus activity, per-cycle ticking is as fast as batching.
- Clock-accurate guarantees correctness for tightly-coupled systems.

**Implication:** Use time-delta only where explicitly safe; validate at load time.

### Single Timing Model per System

**Decision:** Each system declares a single primary timing model (not a mix).

**Rationale:**
- Mixing very different models within a single system is error-prone.
- Validation and debugging become tractable if model is declared upfront.
- Hybrid is allowed as a declared model, but still single choice.

**Implication:** Systems that mix models must declare `timing_model = "hybrid"` and specify sync points.

---

## Part 8: Implementation Phases

### Phase 1: Clock Infrastructure
- [ ] Design clock domain data structure (frequency, phase, derivation)
- [ ] Implement clock edge calculation (given master time, compute all active edges)
- [ ] Build clock-accurate scheduler

### Phase 2: Chip Registration
- [ ] Add `supported_tick_models()` method to `ChipBase`
- [ ] Update all existing chips to declare support (clock-accurate for now)
- [ ] Create chip registration tests

### Phase 3: Declarative Timing in TOML
- [ ] Design schema for `[clocks]`, `[chip.*.clocks]`, `[synchronization]`
- [ ] Implement TOML parser for timing sections
- [ ] Validate clock consistency at load time

### Phase 4: Hybrid & Time-Delta Support
- [ ] Implement time-delta tick API
- [ ] Build hybrid scheduler
- [ ] Add validation for synchronization points

### Phase 5: System Integration
- [ ] Integrate schedulers into `System` / `Board` base classes
- [ ] Update all existing systems to declare clocks and timing model
- [ ] Test mixed systems

### Phase 6: Validation & Debugging
- [ ] Add diagnostic output for clock/timing issues
- [ ] Build timing analyzer (show clock relationships, tick counts, etc.)
- [ ] Create regression tests for all timing models

---

## References & Related Code

- `src/core/chip.hpp` — ChipBase definition
- `src/core/board.hpp` — Board template
- `src/core/memory_bus.hpp` — Bus state propagation
- `docs/declarative-architecture-plan.md` — Higher-level architecture
- `docs/CONFIGURABLE_SYSTEM_ARCHITECTURE.md` — TOML-driven systems

---

_Last updated: 2026-03-28_
