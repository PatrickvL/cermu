# MOS6510 Cycle-Accurate Refactoring Plan

## Current Progress

### ✅ Completed Steps

**Phase 1: Memory System Preparation (COMPLETED ✅)**
- **Step 1.1**: Create C64 System Configuration Structure ✅
- **Step 1.2**: Implement Dynamic Unified Memory Buffer Allocation with Configuration ✅
- **Step 1.3**: Add IO_MEM_ACCESS_PENDING Bus Flag ✅
- **Step 1.4**: Create memory_tick() Function with Register Optimization ✅
- **Step 1.5**: Add I/O Port Tracking to MOS6510 ✅
- **Phase 1 Cleanup**: Remove old c64_bus_cpu_read/write functions ✅

**Phase 1 Key Achievements:**
- ✅ **Register-optimized memory access**: Implemented `c64_memory_tick()` with `REGISTER_CALL` convention
- ✅ **Dynamic memory allocation**: Up to 16KB saved when cartridge ROMs not present
- ✅ **Configuration-driven architecture**: Using `c64_config_t` to eliminate parameter proliferation
- ✅ **Bus flag infrastructure**: Added `IO_MEM_ACCESS_PENDING` for I/O coordination
- ✅ **Function consolidation**: Merged read/write logic into single optimized function
- ✅ **Edge detection support**: Bus state comparison capability for future optimizations

**Phase 2: I/O System Integration (COMPLETED ✅)**
- **Step 2.1**: Implement I/O Bus Flag Detection in Core Chips ✅
- **Step 2.2**: Consolidate Chip Functions into Tick Functions ✅
- **Step 2.3**: Update memory_tick() to Use I/O Bus Flag ✅
- **Step 2.4**: Switch to New I/O System ✅

**Phase 2 Key Achievements:**
- ✅ **I/O coordination system**: Implemented `IO_MEM_ACCESS_PENDING` flag detection in all core chips
- ✅ **Consolidated tick functions**: Created unified entry points (`vicii_tick()`, `mos6581_tick()`, `mos6526_tick()`)
- ✅ **Memory-I/O coordination**: Enhanced `c64_memory_tick()` to set I/O flags when accessing I/O regions
- ✅ **System integration**: Updated main cycle function to use new consolidated tick functions
- ✅ **Performance optimization**: Maintained register-based calling convention throughout I/O system

### ✅ Recently Completed
- **Phase 3**: Zero Bank Elimination (COMPLETED ✅)
- **Step 3.1**: Implement I/O Port Handling in MOS6510 (COMPLETED ✅)
- **Step 3.2**: Remove CHIP_ZEROBANK Infrastructure (COMPLETED ✅)
- **Additional**: Simplified VIC-II Bank Change Implementation (COMPLETED ✅)

### 📋 Next Steps
- Phase 4: Fast Path Implementation
- Phase 5: CPU Architecture Preparation

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

### Phase 2: I/O System Integration (Medium Risk) ✅

**Status**: ✅ COMPLETED

**Goal**: Move I/O handling to individual chips using the bus flag approach and consolidate chip functions.

#### Step 2.1: Implement I/O Bus Flag Detection in Core Chips ✅
**Status**: ✅ COMPLETED

Added I/O bus flag detection logic to all three core chips that will interact with the new I/O coordination system.

**Files Modified**:
- `code/c/src/chip/video/vic_ii/vicii_common.c` - Added flag detection in `vicii_advance_cycle()`
- `code/c/src/chip/sound/mos6581.c` - Added flag detection in `mos6581_advance_cycle()`
- `code/c/src/chip/io/mos6526.c` - Added flag detection in `mos6526_advance_cycle()`

**Implementation Details**:
- Each chip checks for `IO_MEM_ACCESS_PENDING` flag using `bus_is_io_pending()`
- When detected, chips clear the flag using `bus_clear_io_pending()`
- Added proper null pointer validation for bus state parameter
- Used `unlikely()` hints for performance optimization

#### Step 2.2: Consolidate Chip Functions into Tick Functions ✅
**Status**: ✅ COMPLETED

Created unified tick functions for all core chips to provide consistent entry points that include I/O coordination.

**Files Modified**:
- `code/c/src/chip/video/vic_ii/vicii_common.h` - Added `vicii_tick()` declaration
- `code/c/src/chip/video/vic_ii/vicii_common.c` - Implemented `vicii_tick()` function
- `code/c/src/chip/sound/mos6581.h` - Added `mos6581_tick()` declaration
- `code/c/src/chip/sound/mos6581.c` - Implemented `mos6581_tick()` function
- `code/c/src/chip/io/mos6526.h` - Added `mos6526_tick()` declaration
- `code/c/src/chip/io/mos6526.c` - Implemented `mos6526_tick()` function

**Implementation Details**:
- Each tick function is a simple wrapper around existing `*_advance_cycle()` functions
- Maintains register-based calling convention for performance
- Provides unified API for the main system cycle function
- All functions use standard `bus_state_t` parameter and return patterns

#### Step 2.3: Update memory_tick() to Use I/O Bus Flag ✅
**Status**: ✅ COMPLETED

Modified the `c64_memory_tick()` function to set the `IO_MEM_ACCESS_PENDING` flag when I/O region access is detected, completing the I/O coordination mechanism.

**Files Modified**:
- `code/c/src/systems/c64/c64_bus.c` - Updated `c64_memory_tick()` function

**Implementation Details**:
- Added `bus_set_io_pending(&bus_state)` calls in both read and write I/O callback paths (lines 117-118 and 147-148)
- Flag is set when `is_io` condition is true, indicating I/O region access ($D000-$DFFF)
- Maintains register optimization and performance characteristics
- Coordinates with individual chip tick functions for proper I/O handling

#### Step 2.4: Switch to New I/O System ✅
**Status**: ✅ COMPLETED

Updated the main system cycle function to use the new consolidated tick functions, completing the I/O coordination system integration.

**Files Modified**:
- `code/c/src/systems/c64/c64.c` - Updated `c64_non_cpu_cycle()` function

**Implementation Details**:
- Replaced direct `*_advance_cycle()` calls with new consolidated tick functions:
  - `vicii_advance_cycle()` → `vicii_tick()`
  - `mos6526_advance_cycle()` → `mos6526_tick()` (for both CIA chips)
  - `mos6581_advance_cycle()` → `mos6581_tick()`
- All chips now use unified entry points that include I/O coordination
- Maintains existing bus state propagation pattern
- Preserves performance characteristics and cycle timing

**Critical Address Range Validation Fix**: ✅ COMPLETED
**Problem**: Initial implementation had chips clearing `IO_MEM_ACCESS_PENDING` flag without validating if the address was actually within their range, causing incorrect I/O coordination where any chip processing first would clear the flag regardless of the intended target.

**Solution**: Added proper address range checking to all chip tick functions:
- **VIC-II**: Only clears flag when `io_offset >= 0x000 && io_offset <= 0x3FF` ($D000-$D3FF)
- **SID**: Only clears flag when `io_offset >= 0x400 && io_offset <= 0x7FF` ($D400-$D7FF)
- **CIA**: Only clears flag when `io_offset >= 0xC00 && io_offset <= 0xDFF` ($DC00-$DDFF)
- **Color RAM**: Only clears flag when `io_offset >= 0x800 && io_offset <= 0xBFF` ($D800-$DBFF)

**Validation**: ✅ I/O coordination system successfully integrated with proper address validation, ensuring chips only handle I/O accesses within their designated memory ranges

**CIA Address Range Disambiguation Fix**: ✅ COMPLETED
**Problem**: Both CIA1 and CIA2 chips were responding to each other's address ranges because they used the same address validation logic `(bus_state.addr & 0x0E00) == 0x0C00`, which covers the entire CIA range ($DC00-$DDFF). This violated hardware-accurate emulation where CIA1 should only respond to $DC00-$DCFF and CIA2 only to $DD00-$DDFF.

**Solution**: Implemented optimal CIA address disambiguation using hardware interrupt lines:
- **Leveraged existing `interrupt_line` field** in `mos6526_t` structure (CIA1=IRQ, CIA2=NMI)
- **Implemented optimal bit-hack address calculation**: `expected_page = 0xDC00 + ((cia->interrupt_line & BUS_MASK_NMI) << 7)`
- **Updated `mos6526_tick()`** to use interrupt-line-based address validation with single comparison
- **Simplified C64 system initialization** to only set CIA2 interrupt line to NMI (CIA1 defaults to IRQ)

**Technical Details**:
- CIA1 (IRQ=0x01): `0xDC00 + ((0x01 & 0x02) << 7) = 0xDC00 + 0x00 = 0xDC00` ✓
- CIA2 (NMI=0x02): `0xDC00 + ((0x02 & 0x02) << 7) = 0xDC00 + 0x0100 = 0xDD00` ✓
- Single address comparison: `(bus_state.addr & 0xFF00) == expected_page`
- Optimal performance: No multiple comparisons, no branches, hardware-accurate distinction

**Files Modified**:
- `code/c/src/chip/io/mos6526.h` - Removed unnecessary `base_address` field
- `code/c/src/chip/io/mos6526.c` - Implemented interrupt-line-based address discrimination with optimal bit-hack
- `code/c/src/systems/c64/c64.c` - Simplified to only set CIA2 interrupt line to NMI

**Validation**: ✅ Each CIA chip now correctly validates only its own 256-byte address range using hardware-accurate interrupt line characteristics, ensuring optimal performance and hardware-accurate I/O behavior

### Phase 3: Zero Bank Elimination (Medium Risk)

**Status**: ✅ COMPLETED

**Goal**: Move I/O port handling entirely into MOS6510, eliminate CHIP_ZEROBANK.

#### Step 3.1: Implement I/O Port Handling in MOS6510

**Status**: ✅ COMPLETED

Successfully implemented generic I/O port device architecture and moved I/O port handling into MOS6510 CPU tick function, achieving hardware-accurate separation of concerns.

**Key Achievements**:
- **Generic I/O Port System**: Created [`io_port_t`](code/c/src/core/io_port.h:1) device with floating bus support for unused pins
- **Hardware-Accurate CPU Integration**: Moved I/O port handling to [`mos6510_tick()`](code/c/src/chip/cpu/mos6510/mos6510.c:1) where it belongs architecturally
- **Optimized Implementation**: Support for 1-32 bit I/O ports with performance optimization for common 8-bit case
- **Floating Bus Support**: Proper handling of unused pins reflecting last data bus state

**Files Modified**:
- `code/c/src/core/io_port.h` - Generic I/O port device structure and functions
- `code/c/src/core/io_port.c` - I/O port implementation with floating bus support
- `code/c/src/chip/cpu/mos6510/mos6510.h` - Updated MOS6510 structure with single io_port
- `code/c/src/chip/cpu/mos6510/mos6510.c` - I/O port handling in mos6510_tick function
- `code/c/src/systems/c64/c64.c` - Removed I/O port handling from c64_memory_tick

**Hardware Compatibility Validated**:
- Port reads reflect actual pin states with floating bus behavior
- MOS6510 6-bit port configuration (bits 0-5 are I/O, bits 6-7 float)
- I/O port access integrated early in CPU tick before other processing

#### Step 3.2: Remove CHIP_ZEROBANK Infrastructure

**Status**: ✅ COMPLETED

Successfully eliminated all CHIP_ZEROBANK infrastructure and moved to bus flag-based I/O coordination.

**Key Achievements**:
- **PLA Mapping Update**: Replaced all CHIP_ZEROBANK entries with CHIP_RAM in mapping tables
- **Enum Cleanup**: Removed CHIP_ZEROBANK from chip enumeration without breaking subsequent values
- **Callback Elimination**: Removed zerobank callback functions and references
- **Complete Infrastructure Removal**: All CHIP_ZEROBANK references eliminated from codebase

**Files Modified**:
- `code/c/src/systems/c64/c64_bus.h` - Removed CHIP_ZEROBANK from enum, eliminated callback arrays
- `code/c/src/systems/c64/c64_bus.c` - Updated PLA mapping, removed callback functions, optimized c64_memory_tick
- `code/c/src/chip/*/chip_descriptor.c` - Cleaned up all chip descriptors removing dead read/write assignments

**Architecture Improvements**:
- **Bus Flag Coordination**: Moved to `bus_is_io_pending()` flag system for I/O coordination
- **Direct Chip Access**: All I/O register access moved to individual chip tick functions
- **Performance Optimization**: Eliminated callback dispatch overhead in memory access path

#### Additional Achievement: Simplified VIC-II Bank Change Implementation

**Status**: ✅ COMPLETED

Implemented elegant solution for VIC-II bank change functionality using direct bus monitoring approach suggested by user.

**Problem Solved**: VIC-II bank change functionality was lost during callback elimination refactoring.

**User's Insight**: "have the vic tick function watch out for writes to 0xDD00 as well, and capture the bank-selection that way"

**Solution Implemented**:
- **Direct Bus Monitoring**: VIC-II [`vicii_tick()`](code/c/src/chip/video/vic_ii/vicii_common.c:1247) now monitors CIA2 writes at $DD00
- **Hardware-Accurate Mapping**: Uses correct C64 hardware encoding: `vic_bank = 3 - (data & 0x03)`
- **Clean Architecture**: No global state, callbacks, or dependency on io_pending flags
- **Tick Order Independence**: Works regardless of CIA2/VIC-II tick execution order

**Benefits Over Previous Complex Solution**:
- Eliminated global forwarder functions and state
- Removed hard coupling between CIA and VIC-II chips
- Simplified system initialization
- More maintainable and easier to understand

**Validation**: ✅ VIC-II bank changes work correctly, no architectural coupling, maintains full hardware accuracy

### Phase 4: Fast Path Implementation (Medium Risk)

**Status**: 🟡 IN PROGRESS (Step 4.1 completed)

**Goal**: Optimize RAM/ROM access to bypass callback system for improved performance.

#### Step 4.1: Implement Fast Path in memory_tick()

**Status**: ✅ COMPLETED

Successfully enhanced the `c64_memory_tick()` function with comprehensive fast path implementation for all unified buffer chips.

**Key Achievements**:
- **Enhanced Fast Path**: Extended fast path coverage to all unified buffer chips (`CHIP_ROML`, `CHIP_ROMH`, `CHIP_KERNAL`, `CHIP_BASIC`, `CHIP_CHARROM`, `CHIP_RAM`)
- **Performance Measurement Infrastructure**: Added conditional compilation support for performance statistics with `C64_BUS_PERFORMANCE_DEBUG`
- **Validation Framework**: Implemented validation functions for fast path correctness with `C64_BUS_VALIDATION_DEBUG`
- **Detailed Performance Reporting**: Added automatic performance reporting every 1M cycles showing fast/slow path hit ratios

**Implementation Details**:
- **Fast Path Coverage**: All memory chips (`chip <= CHIP_RAM`) now use direct unified buffer access
- **Performance Monitoring**: Static counters track fast_path_hits, slow_path_hits, and total_cycles
- **Validation Support**: Added `c64_bus_validate_fast_path_read()` and `c64_bus_validate_fast_path_write()` functions
- **Public API**: Added `c64_bus_report_fast_path_performance()` function for external performance reporting

**Files Modified**:
- `code/c/src/systems/c64/c64_bus.c` - Enhanced `c64_memory_tick()` with comprehensive fast path
- `code/c/src/systems/c64/c64_bus.h` - Added fast path performance reporting function declaration

**Performance Benefits**:
- Direct unified buffer access eliminates callback overhead for ROM/RAM access
- Branchless address calculation using existing `C64_BUS_UNIFIED_ADDRESS_CALC` macro
- Hardware-accurate behavior with floating bus support for unmapped regions

**Validation**: ✅ Code compiles successfully, fast path implementation is syntactically correct and ready for testing

#### Step 4.2: Simplify Chip Enumeration

**⚠️ CRITICAL DEPENDENCY WARNING**: `chip_type_t` values are used as indexes and in calculations (e.g., unified memory buffer offsets). Any changes to enum values require careful analysis of all dependent code.

**Implementation Requirements**:
- **Phase A**: Audit all uses of `chip_type_t` values as indexes/calculations
  - Search for array indexing using chip types: `buffer[chip_type]`, `table[chip]`
  - Identify memory offset calculations using chip values
  - Document all dependencies before making changes
- **Phase B**: Plan enum value preservation or systematic replacement
  - Option 1: Keep existing values, only remove unused entries at end
  - Option 2: Create mapping layer to preserve index calculations
  - Option 3: Update all dependent calculations systematically
- **Phase C**: Remove individual I/O page CHIPs with careful validation
  - Remove: `CHIP_D0_VIC`, `CHIP_D4_SID`, `CHIP_DC_CIA1`, `CHIP_DD_CIA2`, `CHIP_D8_COLORRAM`
  - Keep: `CHIP_RAM`, `CHIP_BASIC`, `CHIP_KERNAL`, `CHIP_CHARROM`, `CHIP_ROML`, `CHIP_ROMH`, `CHIP_IO`, `CHIP_UNMAPPED`
  - Update PLA mapping tables to use `CHIP_IO` for entire I/O range
  - Verify all array bounds and offset calculations remain valid

**Critical Files to Analyze**:
- Unified memory buffer allocation and offset calculations
- PLA mapping tables and decode functions
- Any arrays dimensioned by chip count or using chip values as indexes

**Benefits**:
- Individual I/O page enums are unnecessary with bus flag approach
- Reduces complexity and memory usage of lookup tables
- Simplifies PLA mapping logic

**Files to Modify** (after dependency analysis):
- `code/c/src/systems/c64/c64_bus.h` - Simplify chip enumeration
- `code/c/src/systems/c64/c64_bus.c` - Update PLA mapping tables and calculations
- Any files referencing old I/O chip enums or using chip values in calculations

**Risk**: HIGH - enum changes can break index-based calculations and memory layouts

#### Step 4.3: Remove Chip Callback Arrays

**Implementation Requirements**:
- Remove `chip_read_callbacks` and `chip_write_callbacks` arrays from `c64_bus_t` structure
- Remove `c64_bus_init_chip_callbacks()` function and all callback initialization
- Update memory access to use either fast path or chip tick functions exclusively
- Clean up all callback-related infrastructure

**Benefits**:
- Callbacks no longer needed with consolidated tick functions and fast path
- Reduces memory usage and eliminates function pointer overhead
- Simplifies bus structure and initialization

**Files to Modify**:
- `code/c/src/systems/c64/c64_bus.h` - Remove callback arrays from structure
- `code/c/src/systems/c64/c64_bus.c` - Remove callback functions and initialization
- `code/c/src/systems/c64/c64.c` - Update system initialization

**Risk**: Medium - eliminates callback infrastructure completely

**Validation**: Performance improves, memory access remains accurate, no callback dependencies remain

### Phase 5: CPU Architecture Preparation (High Risk)

**Status**: 🕒 PENDING (after Phase 4)

**Goal**: Prepare for cycle-accurate CPU implementation without changing timing yet.

#### Step 5.1: Create CPU Family Configuration System

**Implementation Requirements**:
- Design `cpu_family_config_t` structure for CPU variant configuration
- Add CPU type enumeration: `CPU_TYPE_6502`, `CPU_TYPE_6510`, `CPU_TYPE_NES6502`
- Include settings for decimal mode behavior differences between variants
- Add illegal instruction handling configuration
- Support cycle timing variations between CPU types

**Benefits**:
- Single codebase handles multiple 65xx family variants through configuration
- Eliminates need for separate CPU implementations for minor behavioral differences
- Makes adding new CPU variants easier (65C02, 65816, etc.)

**Key Configuration Areas**:
- Decimal mode: 6502 (full), 6510 (ADC/SBC only), NES6502 (disabled)
- Illegal instruction behavior variations
- Cycle timing differences
- Register behavior variants

**Files to Create/Modify**:
- `code/c/src/chip/cpu/cpu_family_config.h` - New configuration system
- `code/c/src/chip/cpu/cpu_family_config.c` - Configuration implementation

**Risk**: Low - configuration structure design, no behavioral changes yet

#### Step 5.2: Create New MOS6510 Implementation with Configuration Support

**Implementation Requirements**:
- Create new parallel folder structure: `mos6510_cycle/` alongside existing `mos6510/`
- Integrate `cpu_family_config_t` from the start
- Keep existing implementation intact for comparison and fallback
- Design clean separation between old and new implementations

**Benefits**:
- Allows side-by-side comparison and easy rollback if issues arise
- Preserves working reference implementation during development
- Enables A/B testing between implementations

**Directory Structure**:
```
src/chip/cpu/
├── mos6510/           # Existing instruction-based implementation
└── mos6510_cycle/     # New cycle-accurate implementation
```

**Files to Create**:
- `code/c/src/chip/cpu/mos6510_cycle/` - New implementation directory
- Core files: `mos6510_cycle.h`, `mos6510_cycle.c`, cycle tables, etc.

**Risk**: Low - parallel implementation, no changes to existing code

#### Step 5.3: Design CPU Register Architecture with Configuration

**Implementation Requirements**:
- Define register array/structure with indexes for A, X, Y, SP, PCL, PCH, status
- Use `cpu_family_config_t` to determine register behavior variants
- **Research decision**: 8-bit vs 16-bit register storage approach
  - **8-bit**: Hardware accurate, natural page-cross detection
  - **16-bit**: Host CPU optimized, simpler PC operations
- Design address setup and data operation function signatures
- Create PLA control signal bit patterns with configuration-driven behavior

**Benefits**:
- Fundamental architecture must be solid before implementation begins
- Configuration approach eliminates need for variant-specific code

**Research Areas**:
- Performance profiling of 8-bit vs 16-bit register approaches
- Cache behavior analysis with different struct layouts
- Host CPU architecture optimization (x86-64, ARM)

**Files to Create**:
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle_registers.h` - Register architecture
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle_pla.h` - PLA definitions

**Risk**: Low - design phase, no code changes yet

#### Step 5.4: Create Parallel CPU Cycle Infrastructure with Configuration

**Implementation Requirements**:
- Add CPU state variables for cycle counting, PLA state in new implementation
- Create `mos6510_cycle_address_setup()` and `mos6510_cycle_data_operation()` functions
- Use `cpu_family_config_t` to determine behavior variants
- Initially bridge to existing instruction execution for gradual transition
- Add cycle state tracking and debugging infrastructure

**Benefits**:
- Gradual transition allows testing new architecture with known-good execution
- Provides foundation for full cycle-accurate implementation

**Files to Create**:
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle_infrastructure.c` - Cycle infrastructure
- Bridge functions to existing execution system

**Risk**: Medium - parallel implementation with system integration

#### Step 5.5: Build Configuration-Driven Opcode Cycle Tables

**Implementation Requirements**:
- Create tables mapping opcodes to cycle sequences with CPU variant support
- Include branch prediction, page crossing logic
- Add cycle skipping logic for different conditions based on CPU configuration
- Handle NMI masking during branch operations (hardware-accurate timing)
- Support different decimal mode behaviors per CPU type

**Benefits**:
- Data-driven approach more maintainable than hardcoded switch statements
- Tables allow easy validation against hardware reference manuals
- Configuration approach eliminates need for separate CPU implementations

**Files to Create**:
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle_tables.h` - Cycle tables
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle_tables.c` - Table data

**Risk**: Low - data structure creation

**Validation**: New CPU implementation can be instantiated alongside old one

### Phase 6: CPU Cycle Implementation (High Risk)

**Status**: 🕒 PENDING (after Phase 5)

**Goal**: Replace instruction-based CPU with cycle-accurate implementation.

#### Step 6.1: Implement Address Setup Operations

**Implementation Requirements**:
- Create switch-based address setup handler in new MOS6510 implementation
- Handle immediate, zero page, absolute, indexed addressing modes
- Update bus address at end of CPU tick (matches real hardware timing)
- Add proper cycle counting and state management

**Benefits**:
- Switch statements more efficient than function pointer tables
- Address setup at end of tick matches real hardware timing

**Files to Modify**:
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle.c` - Address setup implementation

**Risk**: High - fundamental CPU behavior change

#### Step 6.2: Implement Data Operations

**Implementation Requirements**:
- Create switch-based data operation handler in new MOS6510 implementation
- Handle read, write, read-modify-write operations
- Call `c64_memory_tick()` for actual memory access
- Implement proper pipeline behavior matching real 6502

**Benefits**:
- Delayed data operations match real 6502 pipeline behavior
- Centralizes all memory access through memory_tick()

**Files to Modify**:
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle.c` - Data operation implementation

**Risk**: High - affects all CPU operations

#### Step 6.3: Replace Main CPU Loop

**Implementation Requirements**:
- Switch system to use new MOS6510 implementation instead of existing fam65xx
- Handle cycle skipping, interrupt timing in new implementation
- Keep old implementation available for comparison/fallback
- Add comprehensive testing and validation

**Benefits**:
- Allows A/B testing between old and new CPU implementations
- Provides safety net if issues are discovered

**Files to Modify**:
- `code/c/src/systems/c64/c64.c` - Switch to new CPU implementation
- Build system updates for new vs old CPU selection

**Risk**: Very High - complete CPU replacement

**Validation**: Extensive testing with CPU test suites, real software

### Phase 7: Bus Line Management (Medium Risk)

**Status**: 🕒 PENDING (after Phase 6)

**Goal**: Implement proper bus line handling (RW, BA, AEC, SYNC, RDY).

#### Step 7.1: Add Bus Line State Management

**Implementation Requirements**:
- Track RW flag state throughout execution
- Handle write mode entry/exit properly
- Manage UNMAPPED access edge cases
- Add proper bus timing for VIC-II coordination

**Files to Modify**:
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle.c` - Bus line management

**Risk**: Medium - bus timing critical for some software

#### Step 7.2: Implement Interrupt Timing

**Implementation Requirements**:
- Add proper NMI/IRQ cycle handling with accurate timing
- Handle interrupt masking during branch operations
- Implement RESET timing if needed
- Add interrupt priority and timing validation

**Files to Modify**:
- `code/c/src/chip/cpu/mos6510_cycle/mos6510_cycle_interrupts.c` - New interrupt system

**Risk**: High - interrupt timing is complex and critical

**Validation**: Interrupt-dependent software works correctly

### Phase 8: Memory Optimization and Cleanup (Low Risk)

**Status**: 🕒 PENDING (after Phase 7)

**Goal**: Clean up temporary code and finalize optimizations.

#### Step 8.1: Remove Deprecated Infrastructure

**Implementation Requirements**:
- Remove old callback arrays and functions
- Clean up temporary debug code and parallel implementation paths
- Remove unused CHIP enums and related code
- Remove old fam65xx CPU implementation if no longer needed

**Benefits**:
- Cleanup reduces maintenance burden and code complexity

**Risk**: Low - cleanup only

#### Step 8.2: Performance Optimization

**Implementation Requirements**:
- Profile and optimize hot paths in new implementation
- Consider compiler optimization hints for critical sections
- Validate performance improvements and document gains
- Measure memory savings from optimizations

**Benefits**:
- Quantify the benefits achieved by the refactoring

**Risk**: Low - optimization of working code

**Validation**: Memory usage reduced, performance maintained or improved

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
