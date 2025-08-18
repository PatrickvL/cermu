# MOS6510 Cycle-Accurate Refactoring Plan

## Overview

This document outlines a step-by-step migration plan to refactor the MOS6510 CPU emulation from the current callback-based memory system to a cycle-accurate PLA-based implementation. The goal is to achieve hardware-accurate timing while improving performance through simplified memory access paths.

## Key Objectives

1. **Remove CHIP_ZEROBANK** - Move I/O port handling (addresses 0-1) into MOS6510 itself
2. **Introduce fast-path memory_tick()** - Handle RAM/ROM access without callbacks
3. **Simplify I/O handling** - Use bus flag instead of individual CHIP enums for I/O pages
4. **Implement cycle-accurate CPU** - Replace current instruction-based approach with cycle-based PLA emulation
5. **Maintain system stability** - Each step should keep the emulator functional

## Migration Phases

### Phase 1: Memory System Preparation (Low Risk)

**Goal**: Prepare the memory subsystem for the new architecture without breaking current functionality.

#### Step 1.1: Create C64 System Configuration Structure
- Design and implement `c64_config_t` structure to centralize system configuration
- Include cartridge ROM presence flags (`roml_present`, `romh_present`) instead of passing as parameters
- Add system-wide settings like memory optimization flags, debug modes, etc.
- This configuration structure will be used throughout the system to avoid parameter proliferation
- **Why this first**: Configuration structure provides foundation for all subsequent optimizations
- **Risk**: Low - pure design improvement, no behavioral changes

#### Step 1.2: Implement Dynamic Unified Memory Buffer Allocation with Configuration
- Replace fixed `unified_memory_buffer[100 * 1024]` array with dynamic allocation
- Use `c64_config_t` structure to determine cartridge ROM presence instead of function parameters
- When ROML/ROMH not present, allocate only 84KB instead of 100KB (saves 16KB)
- Use pointer arithmetic trick: subtract 16KB from allocated buffer base address
- This makes ROML (offset 0x0000) and ROMH (offset 0x2000) point before allocated memory
- KERNAL ROM (offset 0x4000) becomes the actual start of allocated memory (offset 0x0000)
- **Why this early**: Memory optimization is independent and reduces resource usage immediately
- **Risk**: Low - optimization with no runtime performance impact, no behavioral changes

#### Step 1.3: Add IO_MEM_ACCESS_PENDING Bus Flag
- Add `BUS_MASK_IO_MEM_ACCESS_PENDING` to bus line definitions
- Update `bus_state_t` structure if needed
- Add helper functions to set/clear/check this flag
- **Why**: Infrastructure needed for later I/O coordination
- **Risk**: Minimal - just adding infrastructure

#### Step 1.4: Create memory_tick() Function
- Create new `c64_memory_tick()` function alongside existing memory functions
- Use `c64_config_t` to access system settings for debug logging and behavior control
- Initially, make it call existing `c64_bus_cpu_read/write` functions
- Add debug logging to track when it's called
- **Why**: Parallel implementation allows gradual migration without breaking existing code
- **Risk**: Low - parallel implementation, no behavior change

#### Step 1.5: Add I/O Port Tracking to MOS6510
- Add internal I/O port state variables to MOS6510 structure
- Add functions `mos6510_handle_io_read()` and `mos6510_handle_io_write()`
- Initially, these should call the existing ZEROBANK callbacks
- **Why**: Prepares for moving I/O port handling into CPU where it belongs hardware-wise
- **Risk**: Low - additive changes only

**Validation**: All existing tests pass, emulator runs normally, memory usage reduced

### Phase 2: I/O System Migration (Medium Risk)

**Goal**: Move I/O handling to individual chips using the bus flag approach and consolidate chip functions.

#### Step 2.1: Implement I/O Bus Flag Detection in Core Chips
- Modify VIC-II, SID, CIA1, CIA2 to check `IO_MEM_ACCESS_PENDING` flag
- Add address range checking within each chip (e.g., VIC-II checks for $D000-$D3FF)
- When match found, perform the I/O operation and clear the flag
- Initially run parallel to existing callback system (both paths active)
- **Risk**: Medium - new I/O path, but old path still works

#### Step 2.2: Consolidate Chip Functions into Tick Functions
- Merge existing `read()` and `write()` functions into each chip's `tick()` function
- Each chip now has only one function that handles both emulation and MMIO register access
- Chips check for `IO_MEM_ACCESS_PENDING` flag and handle memory access within their tick
- This applies to VIC-II, SID, CIA1, CIA2, Color RAM, and cartridge I/O chips
- **Why**: Eliminates the artificial separation between chip emulation and register access
- **Why**: More accurately reflects hardware - chips don't have separate "read" and "write" functions
- **Why**: Simplifies chip interface and reduces callback overhead
- **Risk**: Medium - fundamental change to chip interface

#### Step 2.3: Update memory_tick() to Use I/O Bus Flag
- Modify `memory_tick()` to detect I/O region access ($D000-$DFFF)
- Set `IO_MEM_ACCESS_PENDING` flag instead of using callbacks
- Add debug mode validation that flag is always cleared after chip ticks
- **Risk**: Medium - changes I/O access pattern

#### Step 2.4: Switch to New I/O System
- Update main emulation loop to call `memory_tick()` instead of `c64_bus_cpu_read/write`
- Remove old I/O callback infrastructure (CHIP_D0_VIC through CHIP_DF_IO2)
- **Risk**: High - major behavioral change

**Validation**: I/O operations (keyboard, disk, screen) work correctly

### Phase 3: Zero Bank Elimination (Medium Risk)

**Goal**: Move I/O port handling entirely into MOS6510, eliminate CHIP_ZEROBANK.

#### Step 3.1: Implement I/O Port Handling in MOS6510
- Complete `mos6510_handle_io_read/write()` to handle addresses 0-1 directly
- Add memory banking change notifications (LORAM, HIRAM, CHAREN bits)
- Update `memory_tick()` to call MOS6510 I/O functions for addresses 0-1
- **Risk**: Medium - banking changes affect entire memory map

#### Step 3.2: Remove CHIP_ZEROBANK Infrastructure
- Update PLA mapping to use CHIP_RAM for bank 0 instead of CHIP_ZEROBANK
- Remove `c64_bus_chip_read/write_zerobank()` functions
- Remove CHIP_ZEROBANK from all enums and arrays
- **Risk**: Medium - memory mapping change

**Validation**: Memory banking works correctly, I/O ports function properly

### Phase 4: Fast Path Implementation (Medium Risk)

**Goal**: Optimize RAM/ROM access to bypass callback system.

#### Step 4.1: Implement Fast Path in memory_tick()
- Add direct unified buffer access for CHIP_RAM, CHIP_BASIC, CHIP_KERNAL, etc.
- Bypass callback system for ROM/RAM access
- Keep slow path for debugging/validation initially
- **Why**: Most memory accesses are to RAM/ROM, eliminating callbacks improves performance
- **Why**: Direct buffer access is closer to how real hardware works (no indirection)
- **Risk**: Medium - performance optimization, behavior should be identical

#### Step 4.2: Simplify Chip Enumeration
- Remove individual I/O page CHIPs (CHIP_D0_VIC, etc.)
- Keep only: CHIP_RAM, CHIP_BASIC, CHIP_KERNAL, CHIP_CHARROM, CHIP_ROML, CHIP_ROMH, CHIP_IO, CHIP_UNMAPPED
- Update PLA mapping accordingly
- **Why**: Individual I/O page enums are unnecessary with bus flag approach
- **Why**: Reduces complexity and memory usage of lookup tables
- **Risk**: Medium - significant enum simplification

#### Step 4.3: Remove Chip Callback Arrays
- Remove `chip_read_callbacks` and `chip_write_callbacks` arrays from `c64_bus_t`
- Remove `c64_bus_init_chip_callbacks()` function
- All memory access now goes through either fast path or chip tick functions
- **Why**: Callbacks are no longer needed with consolidated tick functions and fast path
- **Why**: Reduces memory usage and eliminates function pointer overhead
- **Risk**: Medium - eliminates callback infrastructure

**Validation**: Performance improves, memory access remains accurate

### Phase 5: CPU Architecture Preparation (High Risk)

**Goal**: Prepare for cycle-accurate CPU implementation without changing timing yet.

#### Step 5.1: Create CPU Family Configuration System
- Design and implement `cpu_family_config_t` structure for CPU variant configuration
- Include settings for decimal mode behavior (6502 vs 6510 vs NES6502 differences)
- Add CPU type enumeration: `CPU_TYPE_6502`, `CPU_TYPE_6510`, `CPU_TYPE_NES6502`
- Include configuration for illegal instruction handling, cycle timing variations
- This allows single codebase to handle multiple 65xx family variants through configuration
- **Why**: Eliminates need for separate CPU implementations for minor behavioral differences
- **Why**: Makes adding new CPU variants easier (65C02, 65816, etc.)
- **Risk**: Low - configuration structure design, no behavioral changes yet

#### Step 5.2: Create New MOS6510 Implementation with Configuration Support
- Create new folder structure parallel to existing `fam65xx` implementation
- Example: `mos6510_cycle/` or `mos6510_v2/` alongside existing `mos6510/` folder
- Integrate `cpu_family_config_t` into new implementation from the start
- Keep existing implementation intact for comparison and fallback
- **Why**: Allows side-by-side comparison and easy rollback if issues arise
- **Why**: Preserves working reference implementation during development
- **Risk**: Low - parallel implementation, no changes to existing code

#### Step 5.3: Design CPU Register Array System with Configuration
- Define register array with indexes for A, X, Y, SP, PCL, PCH, etc.
- Use `cpu_family_config_t` to determine register behavior variants (e.g., decimal mode differences)
- **Research needed**: 8-bit vs 16-bit register storage decision
  - **8-bit approach**: Hardware accurate, matches real MOS6510 internal structure
  - **16-bit approach**: May be more efficient for PC operations on host CPU
  - **Trade-off**: 16-bit simplifies PC increment but complicates page-cross detection
- Create address setup and data operation function signatures
- Design PLA control signal bit patterns with configuration-driven behavior
- **Why**: Fundamental architecture must be solid before implementation begins
- **Risk**: Low - design phase, no code changes yet

#### Step 5.4: Create Parallel CPU Cycle Infrastructure with Configuration
- Add CPU state variables for cycle counting, PLA state in new implementation
- Create `mos6510_address_setup()` and `mos6510_data_operation()` functions
- Use `cpu_family_config_t` to determine behavior variants (decimal mode, illegal instructions)
- Initially make them call existing instruction execution (bridge to old system)
- **Why**: Gradual transition allows testing new architecture with known-good execution
- **Risk**: Medium - parallel implementation

#### Step 5.5: Build Configuration-Driven Opcode Cycle Tables
- Create tables mapping opcodes to cycle sequences with CPU variant support
- Include branch prediction, page crossing logic
- Add cycle skipping logic for different conditions based on CPU configuration
- Handle NMI masking during branch operations (hardware-accurate behavior)
- Support for different decimal mode behaviors (6502: full, 6510: ADC/SBC only, NES: disabled)
- **Why**: Data-driven approach is more maintainable than hardcoded switch statements
- **Why**: Tables allow easy validation against hardware reference manuals
- **Why**: Configuration approach eliminates need for separate CPU implementations
- **Risk**: Low - data structure creation

**Validation**: New CPU implementation can be instantiated alongside old one

### Phase 6: CPU Cycle Implementation (High Risk)

**Goal**: Replace instruction-based CPU with cycle-accurate implementation.

#### Step 6.1: Implement Address Setup Operations
- Create switch-based address setup handler in new MOS6510 implementation
- Handle immediate, zero page, absolute, indexed addressing modes
- Update bus address at end of CPU tick
- **Why**: Switch statements are more efficient than function pointer tables
- **Why**: Address setup at end of tick matches real hardware timing
- **Risk**: High - fundamental CPU behavior change

#### Step 6.2: Implement Data Operations  
- Create switch-based data operation handler in new MOS6510 implementation
- Handle read, write, read-modify-write operations
- Call `memory_tick()` for actual memory access
- **Why**: Delayed data operations match real 6502 pipeline behavior
- **Why**: Centralizes all memory access through memory_tick()
- **Risk**: High - affects all CPU operations

#### Step 6.3: Replace Main CPU Loop
- Switch system to use new MOS6510 implementation instead of existing fam65xx
- Handle cycle skipping, interrupt timing in new implementation
- Keep old implementation available for comparison/fallback
- **Why**: Allows A/B testing between old and new CPU implementations
- **Why**: Provides safety net if issues are discovered
- **Risk**: Very High - complete CPU replacement

**Validation**: Extensive testing with CPU test suites, real software

### Phase 7: Bus Line Management (Medium Risk)

**Goal**: Implement proper bus line handling (RW, BA, AEC, SYNC, RDY).

#### Step 7.1: Add Bus Line State Management
- Track RW flag state throughout execution
- Handle write mode entry/exit properly
- Manage UNMAPPED access edge cases
- **Risk**: Medium - bus timing critical for some software

#### Step 7.2: Implement Interrupt Timing
- Add proper NMI/IRQ cycle handling
- Handle interrupt masking during branch operations
- Implement RESET timing if needed
- **Risk**: High - interrupt timing is complex

**Validation**: Interrupt-dependent software works correctly

### Phase 8: Memory Optimization and Cleanup (Low Risk)

**Goal**: Clean up temporary code and finalize optimizations.

#### Step 8.1: Remove Deprecated Infrastructure
- Remove old callback arrays and functions  
- Clean up temporary debug code
- Remove parallel implementation paths
- Remove unused CHIP enums and related code
- **Why**: Cleanup reduces maintenance burden and code complexity
- **Risk**: Low - cleanup only

#### Step 8.2: Performance Optimization
- Profile and optimize hot paths
- Consider compiler optimization hints  
- Validate performance improvements
- Document memory savings from optimizations
- **Why**: Quantify the benefits achieved by the refactoring
- **Risk**: Low - optimization of working code

**Validation**: Memory usage reduced, performance maintained or improved

## Risk Mitigation Strategies

### Testing Strategy
- Maintain comprehensive test suite throughout migration
- Test after each step before proceeding
- Keep reference implementation available for comparison
- Use real C64 software for validation

### Rollback Plan
- Each phase should be in a separate git branch
- Keep ability to revert to previous phase
- Document known issues and workarounds

### Debug Infrastructure
- Add extensive debug logging for new systems
- Include cycle counting and timing validation
- Add memory access tracing capabilities

## Success Criteria

1. **Functional**: All existing software continues to work
2. **Performance**: Memory access is faster due to reduced callback overhead  
3. **Accuracy**: Cycle timing matches real hardware more closely
4. **Maintainable**: Code is cleaner and easier to understand
5. **Testable**: New architecture supports better testing infrastructure

## Timeline Estimate

- **Phase 1-2**: 2-3 weeks (Memory system preparation and I/O migration)
- **Phase 3-4**: 2-3 weeks (Zero bank removal and fast path)  
- **Phase 5**: 2-3 weeks (CPU architecture preparation)
- **Phase 6**: 4-6 weeks (CPU cycle implementation - most complex)
- **Phase 7**: 2-3 weeks (Bus line management)
- **Phase 8**: 1-2 weeks (Cleanup and optimization)

**Total**: 13-20 weeks for complete migration

## Notes

- Each phase should include comprehensive testing before moving to the next
- Consider implementing feature flags to enable/disable new systems during development
- Document any behavioral changes discovered during implementation
- Plan for potential performance regressions during intermediate phases

## Implementation Details

### Memory Tick Function Design

The new `c64_memory_tick()` function will be the central memory access handler:

```c
void c64_memory_tick(c64_bus_t* bus, const c64_config_t* config) {
    uint16_t address = bus->state.addr;
    bool is_write = !(bus->state.lines & BUS_MASK_RW);
    
    // Handle I/O port addresses in MOS6510 first
    if (address <= 1) {
        mos6510_handle_io_access(bus->c64->mos6510, &bus->state, is_write);
        return;
    }
    
    // Fast path for RAM/ROM access
    uint8_t bank = address >> 12;
    uint8_t chip = is_write ?
        decode_write_chip(bus->cpu_encoded_chip_per_bank[bank]) :
        decode_read_chip(bus->cpu_encoded_chip_per_bank[bank]);
    
    if (chip <= CHIP_RAM) {
        // Direct unified buffer access
        c64_memory_fast_path(bus, address, chip, is_write);
        return;
    }
    
    // I/O region access
    if (chip == CHIP_IO) {
        bus->state.lines |= BUS_MASK_IO_MEM_ACCESS_PENDING;
        return; // Let individual chips handle it in their tick functions
    }
    
    // CHIP_UNMAPPED - do nothing (floating bus)
}
```

### Consolidated Chip Tick Functions

After the refactoring, each chip will have a single tick function that handles both emulation and MMIO:

```c
// Example: VIC-II tick function
void mos6569_tick(mos6569_t* vicii) {
    // 1. Handle any pending I/O memory access first
    if (bus_is_io_pending(vicii->bus_state) && 
        (vicii->bus_state->addr & 0xFC00) == 0xD000) { // VIC-II range $D000-$D3FF
        
        bool is_write = !(vicii->bus_state->lines & BUS_MASK_RW);
        if (is_write) {
            vicii_register_write(vicii, vicii->bus_state->addr, vicii->bus_state->data);
        } else {
            vicii->bus_state->data = vicii_register_read(vicii, vicii->bus_state->addr);
        }
        
        bus_clear_io_pending(vicii->bus_state); // Clear the flag
    }
    
    // 2. Perform normal chip emulation (pixel generation, timing, etc.)
    vicii_emulate_cycle(vicii);
}
```

### CPU Cycle Structure

The new CPU implementation will use cycle-based execution:

```c
typedef struct {
    uint8_t address_setup_op;
    uint8_t data_operation;
    uint8_t cycle_flags; // branch, page cross, etc.
} cpu_cycle_step_t;

// Opcode cycle table (256 * 8 entries max)
extern cpu_cycle_step_t cpu_cycle_table[256 * 8];
```

### CPU Register Architecture Decision

The new CPU implementation faces an important architectural choice regarding register storage:

#### 8-bit Register Approach (Hardware Accurate)
```c
typedef struct {
    uint8_t a;      // Accumulator
    uint8_t x;      // X index register  
    uint8_t y;      // Y index register
    uint8_t sp;     // Stack pointer
    uint8_t pcl;    // Program counter low byte
    uint8_t pch;    // Program counter high byte
    uint8_t status; // Processor status register
} mos6510_registers_8bit_t;
```

**Advantages:**
- Matches real MOS6510 internal structure exactly
- Page crossing detection is natural (PCL overflow sets carry)
- 8-bit arithmetic overflow detection is straightforward
- Debugger display matches hardware registers

**Disadvantages:**  
- PC operations require two 8-bit operations
- More complex address calculations

#### 16-bit Register Approach (Host Optimized)
```c
typedef struct {
    uint8_t a;      // Accumulator
    uint8_t x;      // X index register
    uint8_t y;      // Y index register  
    uint8_t sp;     // Stack pointer
    uint16_t pc;    // Program counter (full 16-bit)
    uint8_t status; // Processor status register
} mos6510_registers_16bit_t;
```

**Advantages:**
- PC increment/decrement is single operation on host CPU
- Address calculations may be more efficient
- Natural fit for host processor architecture

**Disadvantages:**
- Page crossing detection requires additional logic
- Less obvious relationship to hardware
- 8-bit overflow detection on PC operations needs masking

#### Research Area
This decision should be made after profiling both approaches, as the performance impact may vary significantly depending on:
- Host CPU architecture (x86-64, ARM, etc.)
- Compiler optimizations  
- Frequency of different operation types
- Cache behavior with different struct layouts

The implementation should prototype both approaches and measure actual performance before committing to one design.

### Bus Flag Management

New bus line flag for I/O access coordination:

```c
#define BUS_MASK_IO_MEM_ACCESS_PENDING 0x80

// Helper functions
static inline void bus_set_io_pending(bus_state_t* state) {
    state->lines |= BUS_MASK_IO_MEM_ACCESS_PENDING;
}

static inline void bus_clear_io_pending(bus_state_t* state) {
    state->lines &= ~BUS_MASK_IO_MEM_ACCESS_PENDING;
}

static inline bool bus_is_io_pending(const bus_state_t* state) {
    return (state->lines & BUS_MASK_IO_MEM_ACCESS_PENDING) != 0;
}
```

### Chip Interface Simplification

After the refactoring, chip descriptors will be simplified:

```c
typedef struct chip_descriptor_s {
    const char* description;
    void* (*create)(struct chip_descriptor_s* desc);
    void (*destroy)(void* chip);
    void (*tick)(void* chip);           // Only function needed - handles emulation + MMIO
    void (*bus_attach)(void* chip, void* bus);
    // Removed: read, write, bank_change functions
} chip_descriptor_t;
```

## Enhanced Migration Plan Summary

This migration plan now includes the important optimizations and architectural improvements:

1. **Configuration-driven architecture** - Use configuration structures instead of multiple function parameters to eliminate parameter proliferation
2. **CPU family configuration system** - Single CPU implementation supports 6502/6510/NES6502 variants through configuration (different decimal mode handling)
3. **Removal of callback arrays** in Phase 4
4. **Consolidation of chip functions** into single tick functions in Phase 2
5. **Dynamic unified memory buffer allocation** with pointer arithmetic optimization based on configuration

The configuration-driven approach eliminates parameter proliferation while the optimizations will save memory (up to 16KB when cartridge ROMs aren't present) and simplify the chip interface while maintaining performance.

This migration plan provides a systematic approach to the complex refactoring while minimizing risk and maintaining system functionality throughout the process.
