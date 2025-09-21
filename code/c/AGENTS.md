# AGENTS.md - 6502/6510 CPU Emulation Project

## Project Goals and Philosophy

This project implements **cycle-accurate, hardware-verified 6502 family CPU emulation** with the following core principles:

### Primary Objectives

1. **100% ProcessorTests Compatibility** - Achieve perfect compatibility with hardware-verified test suites
2. **Cycle-Accurate Execution** - Match real hardware timing down to the individual cycle
3. **Template-Driven Performance** - Generate optimized code for each CPU variant with zero overhead
4. **Hardware Verification** - All behavior verified against multiple comprehensive test suites

### Critical Design Principles

#### No Legacy Code Policy
**ZERO TOLERANCE FOR LEGACY CODE** - This project prioritizes:
- **Immediate removal** of dead code when identified
- **No backwards compatibility** with deprecated implementations
- **Clean, modern C++17/20** template-driven architecture
- **Aggressive refactoring** to eliminate technical debt

#### Performance-First Architecture
- **Template specialization** for CPU variant-specific optimizations
- **Constexpr-driven** compile-time feature detection
- **Zero runtime overhead** for unused CPU features
- **Cycle-accurate execution** without performance penalties

#### Hardware Fidelity
- **Bus-accurate** state modeling
- **Pin-level** hardware simulation
- **Timing-precise** interrupt handling
- **Bug-compatible** NMOS quirks and CMOS fixes

## Test-Driven Development

### Verification Test Suites

The project uses **four comprehensive test suites** for validation:

1. **ProcessorTests** (Primary) - Hardware-verified ground truth
   - 10,000 test cases per opcode
   - Cycle-accurate validation
   - **Current Target**: 100% compatibility across all opcodes

2. **Klaus Test Suite** - Comprehensive functional testing
   - Status: ✅ **100% PASSING**

3. **Wolfgang Lorenz Test Suite** - Illegal instruction and timing verification  
   - Status: ✅ **100% PASSING**

4. **65C02 Extended Test Suite** - Extended instruction set validation
   - Status: ✅ **100% PASSING**

### Success Metrics

Current achievement status:
- **RTI (0x40)**: ✅ 10,000/10,000 (100%)
- **JSR (0x20)**: ✅ 10,000/10,000 (100%)  
- **RTS (0x60)**: ✅ 10,000/10,000 (100%)
- **JMP absolute (0x4C)**: ✅ 10,000/10,000 (100%) - **RECENTLY FIXED**
- **JMP indirect (0x6C)**: ❌ 35/10,000 (0.4%) - **CURRENT FOCUS**

All major instruction groups achieved 100%:
- ✅ Store operations, shift/rotate, transfer instructions, flag operations
- ✅ Stack operations, branch instructions, arithmetic operations
- ✅ Logic operations

## Current Development Status

### Recently Completed
- **JMP absolute regression fix** - Fixed V flag corruption from SO pin processing
- **Template architecture maturation** - Comprehensive constexpr-based CPU variants
- **ProcessorTests integration** - Robust hardware-verified testing framework

## AI Agent Development Guidelines

### Code Quality Standards

#### Immediate Dead Code Removal
When you encounter any of the following, **REMOVE IMMEDIATELY**:
- Commented-out code blocks
- Unused function parameters
- Dead conditional branches
- Deprecated method implementations
- Legacy compatibility layers

### Testing Requirements

#### Before Any Code Changes
1. **Run existing tests** to establish baseline
2. **Identify affected opcodes** for targeted testing
3. **Create debug tools** for specific investigation needs

#### After Implementation
1. **Run ProcessorTests** for affected instructions
2. **Verify template instantiation** for all CPU variants
3. **Check regression** across all test suites
4. **Validate performance** (no unnecessary overhead)

### Project Architecture Overview

#### Test Infrastructure
- **ProcessorTests runner**: `tests/fam65xx_cpp_processor_tests_runner`
- **Comprehensive testing**: `test_mos6510_comprehensive`
- **Debug utilities**: Multiple `debug_*.cpp` programs for specific issues

#### CPU Implementation
- **Location**: `src/chip/cpu/fam65xx_cpp/` (see CPU-specific AGENTS.md for details)
- **Template-driven** architecture with zero-overhead abstractions
- **Hardware-accurate** cycle and pin-level simulation

## Development Workflow

### Issue Investigation
1. **Create targeted debug program** for specific issue
2. **Trace execution** cycle-by-cycle to understand behavior
3. **Compare** with expected hardware behavior
4. **Implement fix** using template-driven approach
5. **Validate** across all CPU variants and test suites

### Code Integration
1. **Remove dead code** first (comments, unused variables, etc.)
2. **Implement** using modern C++ templates
3. **Test** thoroughly with multiple CPU configurations
4. **Verify performance** - no unnecessary runtime overhead
5. **Document** template usage and variant behavior

This project represents **state-of-the-art CPU emulation** with uncompromising accuracy and performance. Every implementation decision prioritizes **hardware fidelity**, **template-driven optimization**, and **comprehensive verification**.