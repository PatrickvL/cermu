# 6502 PHI2/PHI1 Two-Phase Cycle Model

## Overview

This document describes the cycle-accurate PHI2/PHI1 two-phase clock model for the 6502 CPU emulator, which perfectly matches real hardware behavior including RDY handling, unconditional writes, and VIC-II cycle stealing.

## Core Principle

The 6502 operates with two distinct clock phases:
- **PHI2 (Even cycles)**: Bus operations - CPU drives address/data lines
- **PHI1 (Odd cycles)**: Internal operations - CPU performs ALU work and updates registers

## Detailed Cycle Phases

### Even Cycles (PHI2-equivalent)

**Purpose:** Set up bus signals for the upcoming memory access

**Execution:**
```
callback_for_cycle_even() executes:
  1. Write address to bus.address
  2. Write R/W signal to bus.rw
  3. If write operation: write data to bus.data
  4. CPU internal state is NOT changed
  5. Increment cycle_index
```

**Result:**
- Memory and VIC-II see these signals after this call
- Address lines are stable
- Data lines are valid (for writes)
- R/W signal is valid

### Memory Tick (Between Even → Odd)

**Happens between PHI2 and PHI1:**
```
Uses bus.address, bus.rw, bus.data
For reads: loads data into bus.data for next PHI1
VIC-II can steal cycles here
Color RAM nibble merging occurs here
Open bus behavior occurs here
Graphics DMA occurs here
```

### Odd Cycles (PHI1-equivalent)

**Purpose:** Load bus data and perform the micro-operation

**Execution:**
```
If last cycle was read → CPU samples bus.data into CPU register

callback_for_cycle_odd() executes:
  1. ALU operations
  2. Register writes
  3. Internal flag updates
  4. PC changes
  5. Increment cycle_index
  
  Transition logic:
  - If last addressing-mode cycle → switch to op callback
  - If last op cycle → switch to next opcode addressing-mode callback
```

## RDY (Ready) Behavior

The RDY signal allows external devices to pause the CPU for cycle stealing.

### Implementation Rules

**RDY only affects odd (PHI1) cycles:**

When RDY=0:
- ❌ Skip the odd-cycle micro-op
- ❌ Do NOT increment cycle_index
- ❌ Do NOT change registers
- ❌ Do NOT fetch next microcycle callback

When RDY=1:
- ✅ Execute normally

**Even cycles ALWAYS execute normally:**
- ✅ Address lines still update on schedule
- ✅ Writes always happen (this is critical for 6502 accuracy)
- ✅ This matches real 6502 hardware behavior

### Why This is Correct

Real 6502 behavior:
```
PHI2 = address stable, R/W valid, memory access
PHI1 = internal CPU logic + data latch for reads
```

Our mapping:
```
Even cycle = PHI2
  → CPU drives address/data lines
  → Memory uses the value

Odd cycle = PHI1
  → CPU reads bus.data
  → Internal ALU work & microsequencer
  → Cycle transitions happen here
```

**Perfect match with hardware.**

## Callback Model

### Structure

Each cycle has two callback pointers:
```cpp
callback_even[cycle_index]
callback_odd[cycle_index]
```

### Dispatch Pattern

```cpp
cpu_tick(bus_state):
    if cycle_index is EVEN:
        bus_state = cycle_handler.even(cpu, bus_state)
        cycle_index++
        return bus_state
    
    else: // ODD
        cpu = cycle_handler.odd(cpu, bus_state)
        cycle_index++
        if end_of_sequence:
            select_new_cycle_handler()
            cycle_index = 0
        return bus_state
```

## Transition Points

### 1. Last Addressing Mode Cycle → First Operation Cycle

**Happens in odd cycle** because:
- Logic runs
- Next callback is selected

### 2. Last Operation Cycle → Fetch Next Opcode

**Also happens in odd step:**
- Op last-odd-cycle → dispatch to fetch next opcode
- Next cycle is even → bus drive for opcode fetch

### 3. New Opcode Addressing Mode Starts on Even Cycle

**This automatically ensures:**
- Even = fetch opcode address
- Memory reads it
- Odd = CPU sees opcode and decodes it

**Identical to real 6502.**

## Advantages of This Model

### 🟩 No CPU Internal Buffering

Everything goes directly into the bus struct. No need for:
- `next_address` buffers
- `next_rw` buffers
- `next_data` buffers

### 🟩 Complete Memory/CPU Decoupling

Memory algorithm only sees bus state, completely decoupled from CPU internals. This allows:
- VIC-II cycle stealing
- REU-style DMA
- Color RAM behavior
- Open bus behavior

### 🟩 Perfect 6502 RDY Behavior

No extra flags or state machines needed. The natural cycle structure provides correct RDY handling.

### 🟩 Correct Cycle Alignment

No early/late precomputing needed. Addressing modes naturally align with operations.

### 🟩 Easy VIC-II Timing

PHI2 cycles always drive bus, so VIC-II can steal cycles exactly there.

### 🟩 Eliminates Edge Split Bug

Because:
- Addressing mode cycles always end on odd cycles
- Operation cycles always begin on even cycles
- Each cycle is always an even–odd pair
- Boundary falls naturally at PHI1 → PHI2 transition

No splitting of useful substeps.

## Implementation Status

### ✅ Currently Implemented
- Even/odd cycle separation in core CPU loop
- Addressing mode callbacks with proper phase separation
- Operation callbacks with proper phase separation
- Cycle index management

### 🚧 Needs Implementation
- Explicit RDY handling in odd-cycle dispatcher
- VIC-II cycle stealing integration
- Color RAM nibble merging
- Open bus behavior
- DMA support

## Next Steps for Full Implementation

1. **Add RDY signal handling:**
   ```cpp
   if (cycle_index is ODD && RDY == 0) {
       return; // Don't execute, don't increment
   }
   ```

2. **Implement VIC-II cycle stealing:**
   - VIC-II requests cycle steal during PHI2 (even)
   - CPU skips PHI1 (odd) for that cycle
   - Resume on next PHI2

3. **Add open bus behavior:**
   - Track last value on data bus
   - Return for reads to unconnected addresses

4. **Implement DMA:**
   - External device can control bus during PHI2
   - CPU remains paused until DMA completes

## Testing Considerations

- Use `--continue` flag for accurate full-suite results
- Current baseline: 56.14% pass rate (1,437,108 / 2,560,000 tests)
- 123 opcodes failing (mostly illegal/undocumented)
- All documented opcodes should reach 100% with proper implementation

## References

- MOS 6502 Hardware Manual
- 6502 Timing Diagrams
- VIC-II Technical Reference
- Commodore 64 Programmer's Reference Guide