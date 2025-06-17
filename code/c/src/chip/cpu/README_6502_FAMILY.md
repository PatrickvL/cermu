# MOS 6502 Family CPU Architecture

## Overview
This modular architecture supports the MOS 6502 family of processors with maximum code sharing while allowing for CPU-specific features and optimizations. The `mos` prefix clearly identifies this as MOS Technology's processor family, making room for other manufacturers (Motorola 68xx, Zilog Z80, etc.).

## Folder Structure

```
src/chip/cpu/
├── mos6502_family/           # Shared MOS 6502 family code
│   ├── mos6502_family_core.h # Common CPU state, macros, and interfaces
│   └── mos6502_family_core.c # Shared implementation (memory, interrupts, etc.)
│
├── mos6502/                  # Standard MOS 6502 (with decimal mode)
│   ├── mos6502.h             # MOS 6502 specific interface
│   └── mos6502.c             # MOS 6502 implementation with decimal mode
│
├── mos6510/                  # MOS 6510 (C64 CPU, no decimal mode)
│   ├── mos6510.h             # MOS 6510 specific interface (existing)
│   ├── mos6510.c             # MOS 6510 main implementation (existing)
│   ├── mos6510_arithmetic.c  # Arithmetic operations (existing)
│   ├── mos6510_control.c     # Control flow instructions (existing)
│   └── ... (other existing files)
│
└── nes6502/                  # NES 6502 (Nintendo Entertainment System)
    ├── nes6502.h             # NES 6502 specific interface
    └── nes6502.c             # NES 6502 implementation
```

## Architecture Design

### mos6502_family (Shared Core)
- **Purpose**: Contains all code shared by MOS 6502 family processors
- **Key Features**:
  - Common CPU state structure (`mos6502_family_t`)
  - Shared addressing modes
  - Shared memory access functions  
  - Shared interrupt handling
  - Shared stack operations
  - Performance macros for cycle timing

### mos6502 (Standard MOS 6502)
- **Purpose**: Standard MOS 6502 with full decimal mode support
- **Key Features**:
  - Full BCD arithmetic for ADC/SBC instructions
  - All standard MOS 6502 opcodes including illegal opcodes
  - Compatible with MOS 6502 test suites
  - Used for systems like Apple II, VIC-20, etc.

### mos6510 (C64 CPU)
- **Purpose**: MOS 6510 as used in Commodore 64
- **Key Features**:
  - No decimal mode support (ADC/SBC work in binary only)
  - Built-in I/O ports ($00 and $01)
  - C64-specific memory banking control
  - Optimized for C64 emulation

### nes6502 (Nintendo NES CPU)
- **Purpose**: NES 6502 variant with decimal mode disabled
- **Key Features**:
  - Decimal mode flag exists but is ignored (no BCD arithmetic)
  - All standard opcodes work in binary mode only
  - Compatible with NES test suites
  - Used specifically in Nintendo Entertainment System

## CPU Feature Differentiation

| Feature                | MOS 6502 | MOS 6510 | NES 6502 |
|------------------------|----------|----------|----------|
| Decimal Mode (BCD)     | ✓        | ✗        | ✗        |
| I/O Ports ($00/$01)    | ✗        | ✓        | ✗        |
| Illegal Opcodes        | ✓        | ✓        | ✓        |
| Memory Banking         | ✗        | ✓        | ✗        |

## Usage Examples

### Creating a 6502 CPU (for test suites)
```c
#include "chip/cpu/mos6502/mos6502.h"

mos6502_t cpu;
chip_descriptor_t desc;
mos6502_create(&desc, &cpu);

// This CPU will pass 6502 test suites including decimal mode tests
```

### Creating a 6510 CPU (for C64)
```c
#include "chip/cpu/mos6510/mos6510.h"

mos6510_t cpu;
chip_descriptor_t desc;  
mos6510_create(&desc, &cpu);

// This CPU is optimized for C64 and will not pass decimal mode tests
```

### Creating a NES 6502 CPU (for NES)
```c
#include "chip/cpu/nes6502/nes6502.h"

nes6502_t cpu;
chip_descriptor_t desc;
nes6502_create(&desc, &cpu);

// This CPU is optimized for NES and works in binary mode only
```

## Extension Points

### Adding New MOS 6502 Family Members
1. Create new directory under `cpu/` (e.g., `mos65c02/`)
2. Include `mos6502_family/mos6502_family_core.h`
3. Create CPU-specific structure extending `mos6502_family_t`
4. Implement CPU-specific opcode handlers
5. Configure feature macros as needed

### Example: Adding MOS 65C02
```c
// In mos65c02/mos65c02.h
#include "../mos6502_family/mos6502_family_core.h"

typedef struct {
    mos6502_family_t base;  // Must be first
    // MOS 65C02-specific extensions (new registers, etc.)
} mos65c02_t;
```

## GUI Architecture

The GUI system is modularized to maximize code sharing while allowing CPU-specific extensions:

### Shared GUI Framework (`mos6502_family_gui`)
- **Common Components**: Register display, status flags, control lines, execution controls
- **Configurable**: Each CPU can specify capabilities (decimal mode, I/O ports, etc.)
- **Extensible**: CPU-specific sections can be added via hooks

### CPU-Specific GUI Extensions
- **MOS 6502**: Shows decimal mode support status
- **NES 6502**: Shows decimal mode disabled status  
- **MOS 6510**: Shows I/O port status and bit breakdown

### GUI Configuration Example
```c
mos6502_family_gui_config_t config = {
    .cpu_type_name = "MOS 6502 (Standard)",
    .has_decimal_mode = true,
    .has_io_ports = false,
    .has_extended_opcodes = false,
    .render_cpu_specific = mos6502_render_cpu_specific
};
```

## Testing Strategy

### Test Suite Compatibility
1. **MOS 6502**: Run ALL 6502 test suites (including decimal mode tests)
2. **NES 6502**: Run NES-specific tests OR expect decimal mode failures on 6502 tests
3. **MOS 6510**: Run 6502 tests but expect decimal mode failures

### Available Test Data
- `tests/processor_tests/6502/` - Standard 6502 tests (includes decimal mode)
- `tests/processor_tests/nes6502/` - NES-specific tests (no decimal mode)
- `tests/processor_tests/wdc65c02/` - WDC 65C02 tests (extended instruction set)

### Running Tests
```bash
# Test standard MOS 6502 (should pass all tests)
./processor_tests_runner ../tests/processor_tests/6502/v1/00.json

# Test NES 6502 (should pass non-decimal tests)  
./processor_tests_runner ../tests/processor_tests/nes6502/v1/00.json

# Test MOS 6510 on 6502 data (expect decimal failures)
./processor_tests_runner ../tests/processor_tests/6502/v1/69.json  # ADC
```

## Performance Considerations

1. **Zero-Indirection Access**: All interfaces stored by value in CPU structures
2. **Shared Constants**: Common flags, vectors, and cycles defined once
3. **Modular Compilation**: Each CPU type compiles independently
4. **Cycle Accuracy**: Shared cycle timing ensures consistent behavior
5. **GUI Efficiency**: Shared rendering code reduces binary size

## Future Extensions

This architecture easily supports:
- **VIC-20**: Use MOS 6502 (has decimal mode)
- **C128**: Use MOS 8502 (extend MOS 6510 with additional features)  
- **Apple II**: Use MOS 6502
- **Other MOS 6502 variants**: MOS 65C02, MOS 65816, etc.
- **Other CPU families**: Motorola 68xx, Zilog Z80, Intel 8080, etc.

Each new CPU type gets its own directory and extends the appropriate family code as needed.
