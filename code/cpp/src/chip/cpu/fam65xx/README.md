# MOS 65xx Family C++ CPU Emulator

A complete, template-based C++ implementation of the MOS 65xx family of processors with conditional feature mixins and C API wrappers.

## Architecture Overview

### Template-Based Design
- **Conditional Mixins**: Features are included/excluded at compile-time based on processor type
- **Zero Overhead**: Unused features have no runtime cost through empty base optimization
- **Type Safety**: Template specialization ensures processor-specific behaviors are correctly applied
- **Pin-Accurate**: Hardware-level bus interface with cycle-accurate timing

### Supported Processors

| Processor | I/O Port | BCD | CMOS | Wide Regs | Notes |
|-----------|----------|-----|------|-----------|-------|
| MOS6502 | ✗ | ✓ | ✗ | ✗ | Original NMOS with bugs |
| MOS6510 | ✓ | ✓ | ✗ | ✗ | C64/C128 with I/O port |
| NES6502 | ✗ | ✗ | ✗ | ✗ | Nintendo variant (no BCD) |
| WDC65C02 | ✗ | ✓ | ✓ | ✗ | CMOS with bug fixes |
| ROCKWELL65C02 | ✗ | ✓ | ✓ | ✗ | Rockwell CMOS variant |
| WDC65C816 | ✗ | ✓ | ✓ | ✓ | 16-bit enhanced |

## File Structure

```
fam65xx_cpp/
├── fam65xx.hpp              # MAIN HEADER - CPU template class (488 lines)
├── fam65xx_types.h          # C interface types and compatibility (172 lines)
├── fam65xx_processor_traits.hpp # Processor features and detection (234 lines)
├── fam65xx_mixins.hpp       # Conditional feature mixins (194 lines)
├── mos6502.h/.cpp           # C API wrapper for MOS 6502
├── mos6510.h/.cpp           # C API wrapper for MOS 6510 (C64/C128)
├── nes6502.h/.cpp           # C API wrapper for NES 6502
├── wdc65c02.h/.cpp          # C API wrapper for WDC 65C02
├── rockwell65c02.h/.cpp     # C API wrapper for Rockwell 65C02
├── wdc65c816.h/.cpp         # C API wrapper for WDC 65C816
├── example_usage.cpp        # Usage examples
├── test_consolidation.cpp   # Test file for consolidated header
├── README.md                # This file
└── operations/              # Modular operation implementations
    ├── addressing_modes.inc.hpp
    ├── arithmetic.inc.hpp
    ├── memory.inc.hpp
    ├── control.inc.hpp
    ├── branches.inc.hpp
    ├── stack.inc.hpp
    ├── transfers.inc.hpp
    ├── flags.inc.hpp
    ├── rmw.inc.hpp
    ├── illegal.inc.hpp
    ├── cmos.inc.hpp
    ├── wide.inc.hpp
    └── opcode_tables.inc.hpp
```

## Usage Examples

### C++ Template Interface (Consolidated Header)

```cpp
#include "fam65xx.hpp"
using namespace fam65xx_cpp;

// Create a MOS 6502 CPU
fam65xx_t<MOS6502Tag> cpu;

// Initialize and reset
chip_descriptor_t desc = {};
bus_state_t pins = cpu.init(&desc);
pins = cpu.reset(pins);

// Direct register access using macros
CPU_A(&cpu) = 0x42;
CPU_PC(&cpu) = 0x8000;

// Compile-time feature detection
if constexpr (has_io_port<MOS6502Tag>()) {
    // This code is eliminated at compile-time for MOS6502
    cpu.write_io_ddr(0xFF);
}
```

### MOS 6510 with I/O Port

```cpp
// Create a MOS 6510 CPU (has I/O port)
fam65xx_t<MOS6510Tag> cpu;
cpu.init(&desc);
cpu.reset(pins);

// Access I/O port (available on 6510)
cpu.init_io_port();
cpu.write_io_ddr(0xFF);     // Data direction register
cpu.write_io_data(0x07);    // Data register
uint8_t port = cpu.read_io_port();  // Read combined port state
```

### C API Interface

```c
#include "mos6502.h"
#include "mos6510.h"

// Create MOS 6502 CPU instance
mos6502_t* cpu = mos6502_create();

// Initialize and reset (uses standard chip_descriptor_t)
chip_descriptor_t desc = {};
mos6502_init(cpu, &desc);
mos6502_reset(cpu, pins);

// For MOS 6510 with I/O port capabilities:
mos6510_t* cpu6510 = mos6510_create();
mos6510_desc_t desc6510 = {};
// desc6510.base = {};  // Base chip descriptor
// desc6510.m6510_in_cb = my_io_read;   // Optional I/O callbacks
// desc6510.m6510_out_cb = my_io_write;
mos6510_init(cpu6510, &desc6510);

// Register access through C API
mos6502_set_a(cpu, 0x42);
uint8_t a_value = mos6502_get_a(cpu);

// Clean up
mos6502_destroy(cpu);
```

## Key Features

### Conditional Compilation
Features are included/excluded at compile-time based on processor capabilities:

```cpp
// This check happens at compile-time, no runtime overhead
if constexpr (has_feature<ProcessorTag::WDC65C816>(ProcessorFeatures::WIDE_REGISTERS)) {
    // 16-bit register operations
    cpu.a_full = 0x1234;
} else {
    // 8-bit register operations
    cpu.a = 0x42;
}
```

### Zero-Overhead Mixins
Unused features have zero memory and performance overhead:

```cpp
// If processor doesn't have I/O port, io_port_mixin_t becomes empty struct
template<ProcessorTag tag>
using io_port_mixin_t = std::conditional_t<
    has_io_port<tag>(),
    io_port_state_t,
    empty_mixin_t
>;
```

### Hardware-Accurate Bus Interface
All operations use pin-accurate bus modeling:

```cpp
// All memory operations return bus state for hardware simulation
bus_state_t pins = phi2_read_internal(address);
uint8_t data = fam65xx_core::FAM65XX_GET_DATA(pins);
```

### Processor-Specific Opcode Tables
Each processor variant has its own opcode table specialization:

```cpp
// Specialized for each processor with correct instruction set
template<>
static constexpr std::array<OpcodeDef, 256> opcode_table<ProcessorTag::MOS6502> = {
    // MOS6502-specific opcodes including illegal instructions
};

template<>
static constexpr std::array<OpcodeDef, 256> opcode_table<ProcessorTag::WDC65C02> = {
    // WDC65C02-specific opcodes with CMOS fixes
};
```

## Performance Characteristics

- **Compile-Time Feature Detection**: No runtime branching for feature checks
- **Template Specialization**: Processor-specific optimizations
- **Empty Base Optimization**: Zero memory overhead for unused features
- **Cycle-Accurate Timing**: Hardware-precise execution timing
- **Pin-Level Accuracy**: True hardware behavior simulation

## Integration

### With Existing C Code
The C wrapper API provides seamless integration with existing C codebases:

```c
// Drop-in replacement for existing CPU implementations
mos6502_t* cpu = mos6502_create();
// ... use existing C API patterns
```

### With C++ Code
The template interface provides modern C++ benefits:

```cpp
// Type-safe, zero-overhead, feature-rich
auto cpu = std::make_unique<fam65xx_base_t<ProcessorTag::MOS6510>>();
// ... use modern C++ patterns
```

## Architecture: Modular Header Design

**DESIGN**: This implementation uses a **modular header design** with logical separation:

### Core Headers:
- **`fam65xx.hpp`** (488 lines) - Main CPU template class and coordination
- **`fam65xx_types.h`** (172 lines) - C interface types and compatibility layer
- **`fam65xx_processor_traits.hpp`** (234 lines) - Processor features and detection
- **`fam65xx_mixins.hpp`** (194 lines) - Conditional feature implementations

### Benefits:
- ✅ **Single include**: Only `#include "fam65xx.hpp"` needed (auto-includes others)
- ✅ **Manageable files**: Largest file is ~500 lines vs previous 700+
- ✅ **Logical separation**: C compatibility, processor traits, mixins, main class
- ✅ **Better maintainability**: Each file has single, clear responsibility
- ✅ **Easy extension**: New processors → traits, new features → mixins

## Building

Include the consolidated header file in your project and compile with C++17 or later:

```bash
g++ -std=c++17 -O3 -I../fam65xx example_usage.cpp mos6502.cpp
```

The implementation is header-only for the template parts. Each processor has its own C API wrapper file that needs to be linked.

## Dependencies

- C++17/C++20 compiler with template support  
- System headers: `../../../core/aiemuc.h`, `../../../core/system_lines.h`, `../../../core/chip.h`
- **Uses `chip_descriptor_t`** - standard chip initialization interface
- **No other dependencies** - fully self-contained

## Architecture Changes

- **Removed `fam65xx_desc_t`** - Now uses standard `chip_descriptor_t` interface
- **Added `mos6510_desc_t`** - MOS 6510-specific descriptor wrapping chip_descriptor_t with I/O callbacks
- **Memory callbacks** - Still supported but integrated through chip descriptor system
- **Register access** - Uses accessor macros (CPU_A, CPU_X, etc.) instead of direct field access
- **Processor-specific descriptors** - Each processor can have its own extended descriptor as needed

## Design Philosophy

1. **Zero Runtime Overhead**: Feature detection at compile-time
2. **Hardware Accuracy**: Pin-level bus simulation
3. **Type Safety**: Template-based processor variants
4. **Code Reuse**: Shared operations with processor-specific specialization
5. **Modern C++**: Template metaprogramming and conditional compilation
6. **C Compatibility**: Clean C API for integration with existing code

This implementation provides the best of both worlds: modern C++ template features with traditional C compatibility, all while maintaining cycle-accurate hardware simulation.