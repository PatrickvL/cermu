# AGENTS.md - CPU Core Template Architecture Guide

## Template-Driven CPU Design Philosophy

This CPU emulation core uses a sophisticated template-based architecture that generates **performant, variant-specific code** at compile time. This design ensures that each CPU variant (6502, 6510, 65C02, 65C816) gets optimized machine code with **zero runtime overhead** for features it doesn't support.

## Core Template Configuration System

### CPU Variant Configurations

Located in [`cpu_config.hpp`](cpu_config.hpp), each processor variant is defined as a template configuration:

```cpp
// NMOS 6502 - Original processor with all bugs/quirks
struct config_6502 {
    static constexpr CpuVariant cpu_variant = CpuVariant::NMOS_6502;
    static constexpr bool has_cmos_fixes = false;        // Keep NMOS bugs
    static constexpr bool has_illegal_opcodes = true;    // Support illegal instructions
    static constexpr bool has_decimal_mode = true;       // With NMOS N/Z flag bugs
    static constexpr bool rdy_affects_writes = false;    // NMOS RDY behavior
    // ... additional variant-specific features
};

// NMOS 6510 - Commodore 64 variant with I/O port
struct config_6510 {
    static constexpr CpuVariant cpu_variant = CpuVariant::NMOS_6510;
    static constexpr bool has_cmos_fixes = false;        // Keep NMOS bugs for C64 compatibility
    static constexpr bool has_aec_pin = true;            // AEC/BA DMA support for VIC-II
    static constexpr bool has_io_port = true;            // Built-in I/O port
    // ... C64-specific features
};
```

### Compile-Time Feature Detection

The template system uses `constexpr` conditions to **eliminate code paths** that don't apply to specific processors:

```cpp
// Only NMOS variants get the page boundary bug
if constexpr (!Config::has_cmos_fixes) {
    // NMOS JMP ($xxFF) bug - reads from $xxFF and $xx00 instead of $(xx+1)00
    handle_nmos_page_boundary_bug();
} else {
    // CMOS variants correctly handle page boundaries
    handle_cmos_correct_addressing();
}
```

### Zero-Overhead Abstractions

**Critical Performance Principle**: Features that don't exist on a processor variant generate **ZERO machine code**:

- **6502** binary contains NO 6510 I/O port handling code
- **NMOS** variants contain NO CMOS timing fixes
- **Simple processors** contain NO 65C816 extended interrupt handling

This is achieved through template specialization and `constexpr` conditions that the compiler completely optimizes away.

## Memory Operations with CPU Context

### Template-Driven NMOS Bug Implementation

The memory operations system demonstrates perfect template-driven design:

```cpp
template<typename Config>
inline bus_state_t execute_memory_operation(
    bus_state_t bus_state, 
    CpuRegisterArray& reg, 
    MemOp mem_op, 
    const fam65xx<Config>& cpu) {
    
    // NMOS page boundary bug - compile-time conditional
    if constexpr (!Config::has_cmos_fixes) {
        if (mem_op == MemOp::READ_ABS && opcode == 0x6C) {
            // NMOS-specific bug implementation
            apply_nmos_page_boundary_bug(reg);
        }
    }
    // CMOS variants: this entire code block is eliminated by compiler
}
```

### Cycle-Accurate Template Execution

Each instruction cycle is defined through template-generated cycle tables:

```cpp
template<typename Config>
class CycleTables {
    static constexpr cycle_desc_t get_cycle(uint16_t opcode, uint8_t step) {
        // Compile-time cycle table generation based on Config
        if constexpr (Config::cpu_variant == CpuVariant::NMOS_6502) {
            return nmos_6502_cycles[opcode][step];
        } else if constexpr (Config::cpu_variant == CpuVariant::CMOS_65C02) {
            return cmos_65c02_cycles[opcode][step];
        }
        // Each variant gets its own optimized cycle table
    }
};
```

## Performance-Critical Template Features

### Branch Elimination

The template system eliminates conditional branches for variant-specific features:

```cpp
// SO pin processing - only compiled for variants that have SO pin
if constexpr (Config::has_so_pin) {
    process_so_pin_edge();
}
// Variants without SO pin: ZERO overhead, no function call, no branch
```

### Instruction Set Optimization

Each CPU variant gets **exactly** the instruction set it supports:

```cpp
// Illegal opcodes - only for NMOS variants
if constexpr (Config::has_illegal_opcodes) {
    handle_jam_instructions();  // 6502/6510 only
} else {
    // CMOS variants: illegal opcodes become NOPs
    // This code path is eliminated for NMOS builds
}
```

### Memory Model Specialization

Different memory models are compiled separately:

```cpp
// 6510 AEC/BA DMA timing - only for Commodore 64
if constexpr (Config::has_aec_pin) {
    handle_vic_ii_dma_timing();
}
// Other variants: no DMA code generated
```

## AI Agent Guidelines

### When Working with Templates

1. **Always use `constexpr` conditions** for variant-specific features
2. **Test multiple configurations** to ensure template instantiation works
3. **Verify zero overhead** - check that unused features don't appear in disassembly
4. **Maintain template consistency** across all CPU subsystems

### Template Debugging

When debugging template issues:

```cpp
// Use template parameter inspection
static_assert(Config::has_cmos_fixes || Config::has_illegal_opcodes, 
              "Configuration must support either CMOS fixes or illegal opcodes");

// Template instantiation verification
template void fam65xx<config_6502>::cycle_tick();  // Force instantiation test
```

### Performance Verification

Always verify that templates generate optimal code:

```bash
# Check that variants generate different code
objdump -d cpu_6502.o | grep -A 10 "jmp_indirect"
objdump -d cpu_65c02.o | grep -A 10 "jmp_indirect"
# Should show different instruction sequences for NMOS vs CMOS
```

## Template Architecture Benefits

1. **Zero Runtime Overhead** - Only relevant code is compiled per variant
2. **Type Safety** - Compile-time verification of configuration consistency  
3. **Code Reuse** - Single codebase supports all processor variants
4. **Maintainability** - Changes propagate correctly across all variants
5. **Performance** - Each variant is optimized for its specific feature set

## Critical Implementation Patterns

### Template Function Signatures

Always pass CPU template parameter for context-aware operations:

```cpp
template<typename Config>
void handle_instruction_variant(const fam65xx<Config>& cpu, uint8_t opcode) {
    // Access to full CPU configuration and state
}
```

### Configuration Consistency

Ensure template configurations are self-consistent:

```cpp
struct config_65c816 {
    static_assert(has_abort_pin == true, "65C816 must have ABORT pin");
    static_assert(has_cmos_fixes == true, "65C816 is CMOS variant");
    // Compile-time validation of configuration logic
};
```

This template-driven architecture ensures that each CPU variant gets **exactly the performance and features it needs**, with **zero waste** and **maximum accuracy**.

## Current Development Status

### Active Issues (Priority Order)

1. **JMP Indirect (0x6C) Fundamental Execution** - **CRITICAL**
   - **Problem**: High byte of target address not being set correctly
   - **Symptom**: Expected PC=0x8216, Got PC=0x16 (missing high byte)
   - **Root Cause**: Address increment logic between cycles 3 and 4
   - **Status**: Under investigation with cycle-by-cycle debugging

2. **NMOS Page Boundary Bug Implementation**
   - **Feature**: JMP ($xxFF) should read high byte from $xx00 instead of $(xx+1)00
   - **Status**: Template implementation ready, needs integration testing

3. **Increment/Decrement Operations** - **MEDIUM PRIORITY**
   - Various INC/DEC/INX/etc. operations need validation

## Core CPU Implementation Architecture

### CPU Implementation Structure
- **Location**: `src/chip/cpu/fam65xx_cpp/`
- **Main class**: `fam65xx<Config>` template
- **Cycle execution**: `execute_cycle()` method
- **Memory operations**: `memory_operations.hpp` with CPU context
- **ALU operations**: `alu_operations.hpp` with template specialization

### Template Configuration System
- **CPU variants**: Defined in `cpu_config.hpp`
- **Feature detection**: `constexpr` flags for capabilities
- **Compile-time optimization**: Unused features eliminated
- **Type safety**: Template constraints and static assertions

## Critical Implementation Patterns

### CPU Context Passing
Always pass CPU template context for variant-aware operations:
```cpp
template<typename Config>
bus_state_t execute_memory_operation(
    bus_state_t bus_state,
    CpuRegisterArray& reg,
    MemOp mem_op,
    const fam65xx<Config>& cpu);
```

### Template Conditional Compilation
Use constexpr for CPU variant features:
```cpp
if constexpr (!Config::has_cmos_fixes) {
    // NMOS-specific bug implementation
} else {
    // CMOS correct behavior
}
```

### Zero-Overhead Abstractions
Ensure unused features generate no code:
```cpp
// SO pin processing - only compiled for variants that have SO pin
if constexpr (Config::has_so_pin) {
    process_so_pin_edge();
}
// Variants without SO pin: ZERO overhead
```

## CPU-Specific Development Guidelines

### Code Quality Standards

#### Template-First Development
- **Always** use `constexpr` for CPU variant-specific features
- **Never** use runtime conditionals for compile-time known CPU features
- **Template functions** must support all relevant CPU configurations
- **Verify** that unused features generate zero machine code

#### Performance-Critical Code
- **Inline** all hot-path functions
- **Constexpr** all compile-time computations
- **Template specialization** for variant-specific optimizations
- **Branch elimination** using compile-time conditions

### CPU Debugging Protocol
1. **Create targeted debug programs** (like `debug_jmp_indirect_simple.cpp`)
2. **Trace cycle-by-cycle execution** for complex issues
3. **Compare simple vs ProcessorTests** execution environments
4. **Use constexpr verification** for template logic

### CPU Development Workflow

#### Issue Investigation
1. **Create targeted debug program** for specific issue
2. **Trace execution** cycle-by-cycle to understand behavior
3. **Compare** with expected hardware behavior
4. **Implement fix** using template-driven approach
5. **Validate** across all CPU variants and test suites

#### Code Integration
1. **Remove dead code** first (comments, unused variables, etc.)
2. **Implement** using modern C++ templates
3. **Test** thoroughly with multiple CPU configurations
4. **Verify performance** - no unnecessary runtime overhead
5. **Document** template usage and variant behavior