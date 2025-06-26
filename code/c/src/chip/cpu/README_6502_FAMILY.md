# MOS 6502 Family CPU Architecture

## Overview
This modular architecture supports the MOS 6502 family of processors with maximum code sharing while allowing for CPU-specific features and optimizations. The `fam65xx` prefix clearly identifies this as MOS Technology's processor family, making room for other manufacturers (Motorola 68xx, Zilog Z80, etc.).

## Handler Registration Architecture (2025 Update)

**Unified Static Opcode Handler Table**
- All opcode handler registration (including legal and illegal opcodes) is now performed statically in a single table: `fam65xx_op_default_handlers`.
- There is no dynamic handler override logic or runtime patching; all handlers are assigned at compile time.
- All handler helpers (for both legal and illegal opcodes) are deduplicated and use a unified approach (e.g., `fam65xx_addr_op_helper`).
- Feature flags for illegal opcodes and dynamic override functions (such as `fam65xx_override_opcode`) have been removed.
- All legal and illegal opcode handlers are always included for all family CPUs; there is no conditional compilation for opcode support.
- This approach ensures maximum maintainability, clarity, and performance.

## Folder Structure

```
src/chip/cpu/
├── fam65xx/                    # Shared 6502-family code and all opcode/handler logic
│   ├── fam65xx_core.h          # Common CPU state, macros, and interfaces
│   ├── fam65xx_opcodes.c       # Static opcode handler table and registration
│   ├── fam65xx_illegal.c       # Illegal opcode helpers and implementations
│   ├── fam65xx_arithmetic.c    # Arithmetic and logic helpers
│   ├── fam65xx_shifts.inc      # Shift/rotate helpers
│   └── ... (other shared helpers)
│
├── mos6502/                    # Standard MOS 6502 (with decimal mode)
│   ├── mos6502.h               # MOS 6502 specific interface
│   └── mos6502.c               # MOS 6502 implementation with decimal mode
│
├── mos6510/                    # MOS 6510 (C64 CPU, no decimal mode)
│   ├── mos6510.h               # MOS 6510 specific interface
│   └── mos6510.c               # MOS 6510 implementation
│
├── nes6502/                    # NES 6502 (Nintendo Entertainment System)
│   ├── nes6502.h               # NES 6502 specific interface
│   └── nes6502.c               # NES 6502 implementation
└── ... (other family members)
```

## Architecture Design

### fam65xx (Shared Core)
- **Purpose**: Contains all code shared by MOS 6502 family processors
- **Key Features**:
  - Common CPU state structure (`fam65xx_cpu_t`)
  - Shared addressing modes
  - Shared memory access functions  
  - Shared interrupt handling
  - Shared stack operations
  - Performance macros for cycle timing
  - **Unified static opcode handler table for all opcodes**

### Static Opcode Handler Architecture (2025+)
- **Static Table:** All opcode handlers are assigned in the static `fam65xx_op_default_handlers[256]` array at compile time.
- **No Dynamic Overrides:** There is no runtime handler override or feature flag logic. The static table is the only source of truth.
- **Unified Helpers:** All opcode handler helpers (including decimal mode ADC/SBC and illegal opcodes) are deduplicated and shared across the family where possible.
- **Handler Registration:** To change or extend opcode behavior, update the static table and provide the handler implementation.
- **No Feature Flags:** The `FAM65XX_FEATURE_ILLEGAL_OPCODES` flag and related code have been removed. All opcodes (legal and illegal) are always handled.

#### Example: Static Handler Table (C Pseudocode)
```c
// fam65xx_op_default_handlers[opcode] = handler_function;
fam65xx_op_default_handlers[0x69] = fam65xx_adc_handler; // ADC (with decimal mode if enabled)
fam65xx_op_default_handlers[0x6B] = fam65xx_arr_handler; // Illegal opcode ARR
// ... all 256 opcodes assigned ...
```

#### Adding or Modifying an Opcode Handler
1. Implement or update the handler function (e.g., `fam65xx_new_handler`).
2. Assign it to the correct opcode index in `fam65xx_op_default_handlers`.
3. Ensure the function is declared as `extern` if used across files.
4. Rebuild and test.

#### Removing Obsolete Code
- All dynamic override functions (e.g., `fam65xx_override_opcode`) and feature flags are now removed.
- All handler helpers are deduplicated; only one implementation per unique operation remains.

---

## CPU Feature Differentiation

| Feature                | MOS 6502 | MOS 6510 | NES 6502 |
|------------------------|----------|----------|----------|
| Decimal Mode (BCD)     | ✓        | ✗        | ✗        |
| I/O Ports ($00/$01)    | ✗        | ✓        | ✗        |
| Illegal Opcodes        | ✓        | ✓        | ✓        |
| Memory Banking         | ✗        | ✓        | ✗        |
| **Static Handler Table** | ✓      | ✓        | ✓        |
| **Dynamic Override**     | ✗      | ✗        | ✗        |

## Usage Examples

### Creating a 6502 CPU (for test suites)
```c
#include "chip/cpu/mos6502/mos6502.h"
#include "chip/cpu/fam65xx/fam65xx_core.h"

mos6502_t cpu;
chip_descriptor_t desc;
mos6502_create(&desc, &cpu);
// All opcode handlers are statically assigned via fam65xx_op_default_handlers
```

### Creating a 6510 CPU (for C64)
```c
#include "chip/cpu/mos6510/mos6510.h"
#include "chip/cpu/fam65xx/fam65xx_core.h"

mos6510_t cpu;
chip_descriptor_t desc;  
mos6510_create(&desc, &cpu);
// All opcode handlers are statically assigned via fam65xx_op_default_handlers
```

### Creating a NES 6502 CPU (for NES)
```c
#include "chip/cpu/nes6502/nes6502.h"
#include "chip/cpu/fam65xx/fam65xx_core.h"

nes6502_t cpu;
chip_descriptor_t desc;
nes6502_create(&desc, &cpu);
// All opcode handlers are statically assigned via fam65xx_op_default_handlers
```

## Extension Points

### Adding New MOS 6502 Family Members
1. Create new directory under `cpu/` (e.g., `mos65c02/`)
2. Include `fam65xx/fam65xx_core.h`
3. Create CPU-specific structure extending `fam65xx_cpu_t`
4. Implement CPU-specific opcode handlers as needed
5. Register all opcode handlers statically in the unified handler table (`fam65xx_op_default_handlers`)
6. **No dynamic override or feature flag logic is required.**

### Example: Adding MOS 65C02
```c
// In mos65c02/mos65c02.h
#include "../fam65xx/fam65xx_core.h"

typedef struct {
    fam65xx_cpu_t base;  // Must be first
    // MOS 65C02-specific extensions (new registers, etc.)
} mos65c02_t;
```

## Documentation Notes

- The static handler table approach replaces all previous dynamic handler override logic and feature flags for opcode support.
- All handler helpers are deduplicated and used consistently across the codebase.
- See `fam65xx_op_default_handlers` in the source for the complete opcode-to-handler mapping.
- For more details, refer to the comments in `fam65xx_opcodes.c` and related implementation files.

## Testing Strategy

- **All opcode handlers are now statically assigned and tested via the canonical table.**
- **Test suites should cover all legal and illegal opcodes as appropriate for each CPU variant.**

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

- **Opcode dispatch is now a single static lookup, with no runtime indirection.**
- **All handler helpers are deduplicated for maximum code sharing and efficiency.**

## Future Extensions

- **To support new CPU variants or opcode extensions, follow the static handler registration pattern.**
- **No dynamic handler override logic is needed.**
