# 6502 CPU Cycle Model: PHI1/PHI2 Implementation Guide

## Overview

This document describes the precise PHI1/PHI2 cycle model for accurate 6502/6510 emulation, including proper bus semantics, RDY behavior, and VIC-II DMA interaction.

---

## Core Concept: Even/Odd Cycle Mapping

The 6502 operates on a two-phase clock:
- **PHI2 (Even cycles)**: CPU drives the bus (address, R/W̅, data out)
- **PHI1 (Odd cycles)**: CPU performs internal operations (ALU, register updates)

### Our Model

```
Even cycle (PHI2-equivalent):
  ├─ CPU drives bus.address
  ├─ CPU drives bus.rw
  ├─ If write: CPU drives bus.data
  ├─ Memory observes these signals
  └─ cycle_index++

Memory tick (between even → odd):
  ├─ Uses bus.address, bus.rw, bus.data
  ├─ For reads: loads data into bus.data for next PHI1
  ├─ VIC-II can steal cycles here
  └─ Color RAM nibble merging, open bus behavior

Odd cycle (PHI1-equivalent):
  ├─ If last cycle was read: CPU samples bus.data
  ├─ CPU executes micro-operation:
  │  ├─ ALU actions
  │  ├─ Register writes
  │  ├─ Internal flags
  │  └─ PC changes
  ├─ cycle_index++
  ├─ If last addressing-mode cycle → switch to operation callback
  └─ If last operation cycle → switch to next opcode fetch
```

---

## Cycle Breakdown by Phase

### PHI2 (Even Cycle) - Bus Setup Phase

**Purpose**: Set up bus signals for upcoming memory access

**Actions**:
1. Execute `callback_for_cycle_even()`
2. Write to `bus.address`
3. Write to `bus.rw` (0=write, 1=read)
4. If write: write to `bus.data`
5. **No CPU internal state changes**
6. Increment `cycle_index`

**Result**: Memory/VIC sees these signals after this call

---

### Memory Tick (Between Even → Odd)

**Purpose**: Execute memory operation and handle DMA

**Actions**:
1. Read `bus.address`, `bus.rw`, `bus.data` (for writes)
2. For reads: load data into `bus.data` for next PHI1
3. **VIC-II can steal cycles here**
4. Color RAM nibble merging
5. Open bus behavior
6. Graphics DMA

**Hardware Behavior**:
```
┌─────────────────┬──────────────────┬─────────────────────────────┐
│   Component     │    RDY High      │         RDY Low             │
├─────────────────┼──────────────────┼─────────────────────────────┤
│ CPU             │ Normal execution │ STALLED (repeats cycle)     │
│ VIC-II          │ Gets bus access  │ CONTINUES (needs bus)       │
│ Address Bus     │ CPU's address    │ VIC-II's address (previous) │
│ Memory Access   │ CPU operation    │ VIC-II operation            │
│ Bus Pins Update │ Required         │ STILL REQUIRED              │
└─────────────────┴──────────────────┴─────────────────────────────┘
```

---

### PHI1 (Odd Cycle) - Internal Operation Phase

**Purpose**: Load bus data and perform micro-operation

**Actions**:
1. If last cycle was read → CPU samples `bus.data` into CPU register
2. Execute `callback_for_cycle_odd()` for micro-ops:
   - ALU actions
   - Register writes
   - Internal flags
   - PC changes
3. Increment `cycle_index`
4. **State transitions happen here**:
   - If last addressing-mode cycle → switch to operation callback
   - If last operation cycle → switch to next opcode fetch

---

## RDY (Ready) Signal Behavior

The RDY pin allows external hardware to stretch CPU cycles for DMA or slow memory.

### Hardware-Accurate RDY Implementation

**Key Principle**: RDY only affects odd (PHI1) cycles

```c++
if (RDY == 0) {
    // Skip the odd-cycle micro-op
    // Do NOT increment cycle_index
    // Do NOT change registers
    // Do NOT fetch next microcycle callback
}
```

**Even cycles (PHI2) always execute normally**:
- ✅ Writes always happen (NMOS bug/CMOS behavior)
- ✅ Address lines still update on schedule
- ✅ Bus signals remain valid

---

### Processor-Variant RDY Behavior

#### NMOS Processors (6502, 6510, NES 6502)

**Hardware Bug**: Write cycles ignore RDY

```c++
bool should_complete_write_cycle(bus_state_t pins) {
    // NMOS: Always complete write regardless of RDY
    return true;  // Hardware bug
}
```

**Read Cycles**: Respect RDY normally

---

#### CMOS Processors (65C02, 65C816)

**Bug Fixed**: Both read AND write cycles respect RDY

```c++
bool should_complete_write_cycle(bus_state_t pins) {
    // CMOS: Only complete when RDY is high
    return FAM65XX_GET_RDY(pins);  // Proper behavior
}
```

---

## Why This Model is Correct

### Real 6502 Hardware Mapping

| Real Hardware | Our Model | Purpose |
|--------------|-----------|---------|
| PHI2 high | Even cycle | Address stable, R/W̅ valid, memory access |
| PHI1 high | Odd cycle | Internal CPU logic + data latch for reads |

**Perfect match**: Our even/odd mapping directly corresponds to PHI2/PHI1 hardware behavior.

---

### Avoiding Crossover Bugs

**Problem Eliminated**: Addressing mode cycles always end on odd cycles, operation cycles always begin on even cycles.

**Result**: Boundary falls naturally at PHI1 → PHI2 transition, never splitting a useful substep.

**No more edge split problems!**

---

## Callback Model

### Two Callbacks Per Cycle Index

```c++
callback_even[cycle_index]  // PHI2 phase
callback_odd[cycle_index]   // PHI1 phase
```

### Recommended Implementation

```c++
cpu_tick(bus_state):
    if (cycle_index is EVEN):
        bus_state = cycle_handler.even(cpu, bus_state)
        cycle_index++
        return bus_state
    
    else:  // ODD
        cpu = cycle_handler.odd(cpu, bus_state)
        cycle_index++
        
        if (end_of_sequence):
            select_new_cycle_handler()
            cycle_index = 0
        
        return bus_state
```

---

## State Transitions (Three Critical Points)

### 1. Last Addressing Mode Cycle → First Operation Cycle

**When**: During odd cycle step (where logic runs)

**Why**: Odd cycle is where next callback is selected

```c++
// In odd cycle handler
if (last_addressing_mode_cycle) {
    current_handler = operation_handler;
    cycle_index = 0;
}
```

---

### 2. Last Operation Cycle → Fetch Next Opcode

**When**: During odd cycle step

**Why**: Operation completes on odd cycle, dispatch to fetch on next even

```c++
// In odd cycle handler (operation's last cycle)
transition_to_fetch();
// Next cycle will be even → bus drive for opcode fetch
```

---

### 3. New Opcode Addressing Mode Starts on Even Cycle

**Automatic**: Ensures even = fetch opcode address, memory reads it, odd = CPU decodes

**Result**: Identical to real 6502 behavior

---

## Complete Instruction Cycle Example: LDA $1234,X

### Opcode Fetch (2 cycles)

```
Cycle 0 (EVEN - PHI2):
  └─ Drive PC on address bus, assert R/W̅ high
  
Cycle 1 (ODD - PHI1):
  ├─ Sample opcode from bus.data → IR
  ├─ Decode: LDA absolute,X
  └─ Increment PC

Cycle 2 (EVEN - PHI2):
  └─ Drive PC on address bus for operand low byte
  
Cycle 3 (ODD - PHI1):
  ├─ Sample low byte → ABL
  └─ Increment PC
```

### Addressing Mode: Absolute,X (3 more cycles)

```
Cycle 4 (EVEN - PHI2):
  └─ Drive PC on address bus for operand high byte
  
Cycle 5 (ODD - PHI1):
  ├─ Sample high byte → ABH
  ├─ Store intermediate high byte in DL (for illegal opcodes)
  ├─ Add X to ABL only (creates "wrong" address if page cross)
  └─ Check page crossing
  
Cycle 6 (EVEN - PHI2):
  └─ Drive intermediate address on bus (if page cross)
  
Cycle 7 (ODD - PHI1):
  ├─ Dummy read (if page cross)
  ├─ Correct address: restore base, add X with carry
  └─ TRANSITION TO OPERATION
```

### Operation: LDA (1 cycle)

```
Cycle 8 (EVEN - PHI2):
  └─ Drive final address on bus, assert R/W̅ high
  
Cycle 9 (ODD - PHI1):
  ├─ Sample data → A register
  ├─ Update N and Z flags
  └─ TRANSITION TO FETCH (next instruction)
```

**Total**: 10 cycles for page-crossing LDA absolute,X

---

## Major Advantages of This Model

### 🟩 No CPU Internal Buffering

Everything goes directly into the `bus` struct. No next-address/next-RW/next-data buffers needed.

### 🟩 Memory Algorithm Completely Decoupled

Memory only sees `bus_state` - allows VIC and REU-style DMA to coexist cleanly.

### 🟩 Perfect 6502 RDY Behavior

No extra flags or state machines needed. RDY implementation is straightforward.

### 🟩 Correct Cycle Alignment

No early/late precomputing needed for addressing modes.

### 🟩 Easy VIC Timing

PHI2 cycles always drive bus, so VIC can steal cycles exactly there.

---

## Implementation Guidelines

### Current Code Structure

The existing [`fam65xx.hpp`](../src/chip/cpu/fam65xx/fam65xx.hpp) implementation already follows many of these principles:

```c++
bus_state_t tick(bus_state_t pins) {
    // Hardware-accurate interrupt detection every cycle
    if (this->process_interrupt_detection(pins)) {
        // Handle interrupt hijacking
    }
    
    // Execute current instruction cycle
    if (this->current_handler != nullptr) {
        pins = this->call_current_handler(pins);
    }
    
    return pins;
}
```

### Memory Access Template

```c++
template<bool IsWrite, bool IsDummy, Addr addr_arg, Bank bank_arg = Bank::DBR>
bus_state_t phi2_access(bus_state_t pins, uint8_t data = 0) {
    // Hardware-accurate RDY check
    if (!FAM65XX_GET_RDY(pins)) {
        // DMA device has bus control
        // Perform DMA memory access
        return pins;
    }
    
    // CPU has bus control
    // Set up bus lines and perform access
    // ...
}
```

### RDY Handling Example

```c++
// NMOS write (always completes)
inline bool should_complete_write_cycle(bus_state_t pins) {
    if constexpr (has_nmos_bugs()) {
        return true;  // NMOS bug
    } else {
        return FAM65XX_GET_RDY(pins);  // CMOS fix
    }
}
```

---

## Next Steps for Full Implementation

To fully implement this model, the following areas need attention:

1. **Separate PHI2/PHI1 callbacks** for each cycle
2. **Explicit cycle phase tracking** (even/odd state)
3. **Bus setup vs internal operation separation** in handlers
4. **State transition timing verification** at boundaries
5. **Test suite** for cycle-accurate behavior with RDY signals

---

## References

- Visual 6502 simulation: http://visual6502.org/
- MOS 6502 datasheet (original NMOS)
- WDC 65C02 datasheet (CMOS improvements)
- "Programming the 65816" by David Eyes & Ron Lichty
- Bruce Clark's 6502 interrupt handling document
- VIC-II timing diagrams from C64 Programmer's Reference Guide

---

## Conclusion

This PHI1/PHI2 cycle model provides:

✅ **Hardware accuracy** - Matches real 6502 bus semantics  
✅ **Clean abstraction** - No CPU buffering, decoupled memory  
✅ **Correct RDY behavior** - Processor-variant support  
✅ **VIC-II compatibility** - Proper DMA cycle stealing  
✅ **Minimal state** - Simple implementation  
✅ **No crossover bugs** - Natural boundary alignment  

The model is ready for implementation and will produce accurate VIC-II behavior, color RAM leakage, and correct cycle counting for all 6502 family processors.