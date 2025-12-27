# AGENTS.md - AI Agent Instructions for C64 Emulator Project

## Rule #1: Binary Location (CRITICAL - READ FIRST)

**ALL binaries output to ONE location ONLY:**
```
code/cpp/build/bin/
```

**Executables:**
- `code/cpp/build/bin/c64emu` - Console emulator
- `code/cpp/build/bin/c64emu_gui` - GUI emulator (if SDL2/OpenGL available)

**Run commands (from code/cpp directory):**
```bash
./build/bin/c64emu      # Console version
./build/bin/c64emu_gui  # GUI version
```

❌ **NEVER use these paths (they don't exist):**
- `./bin/c64emu`
- `code/cpp/bin/c64emu`
- `build/bin/c64emu` (from root)

## Rule #2: Build System

**Primary method: CMake from code/cpp directory**

```bash
# Standard build:
cd code/cpp
cmake -B build
cmake --build build --target c64emu -j$(nproc)

# Clean rebuild:
rm -rf build/
cmake -B build
cmake --build build --target c64emu -j$(nproc)

# Build both targets:
cmake --build build -j$(nproc)
```

**CMakeLists.txt enforces unified output location:**
```cmake
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/bin")
```

## Rule #3: ROM Files Required

**Location:** `data/c64/roms/` (relative to project root)

**Required files:**
- `kernal.901227-03.bin` - KERNAL ROM
- `basic.901226-01.bin` - BASIC ROM
- `characters.901225-01.bin` - Character ROM

**Alternative patterns auto-detected:**
- `C64 - 901226-01 - Commodore (*) Basic.rom`
- `C64 - 901227-03 - Commodore (*) Kernal.rom`
- `C64 - 901225-01 - Commodore (*) Characters.rom`

## Rule #4: Testing & Verification

**Expected behavior:**
```bash
cd code/cpp
./build/bin/c64emu
```
- ROMs load successfully
- System boots through KERNAL initialization
- Runs ~1,000,000 cycles
- KERNAL READY prompt displays (GUI version)
- No BRK loops or fatal errors

**Troubleshooting checklist:**
1. Verify binary exists: `ls -la ./build/bin/c64emu`
2. Check binary timestamp is recent
3. Verify ROMs exist: `ls -la ../data/c64/roms/`
4. Clean rebuild if stale: `rm -rf build/ && cmake -B build && cmake --build build`

## Rule #5: Project Structure

```
aiemu/
├── code/cpp/
│   ├── build/bin/           # ← ALL BINARIES HERE
│   ├── src/                 # Source code
│   │   ├── chip/           # Chip implementations
│   │   ├── systems/        # System implementations (C64, VIC20, etc.)
│   │   ├── core/           # Core emulation framework
│   │   └── main/           # Main entry points
│   ├── tests/              # Test suites
│   ├── CMakeLists.txt      # Build configuration
│   └── AGENTS.md           # This file
├── data/c64/roms/          # C64 ROM files
└── docs/                   # Documentation
```

---

# Development Guidelines

## Core Development Principles

### No Legacy Code Policy
**ZERO TOLERANCE FOR LEGACY CODE** - This project prioritizes:
- **Immediate removal** of dead code when identified
- **No backwards compatibility** with deprecated implementations
- **Clean, modern C++17/20** template-driven architecture
- **Aggressive refactoring** to eliminate technical debt

### Consolidation Philosophy
**When consolidating systems:**
- **Replace multiple implementations** with single unified version
- **Remove ALL duplicate symbols, functions, and files** immediately
- **Eliminate legacy APIs** without backward compatibility
- **Force complete migration** to new unified system
- **Delete obsolete interfaces** - no compromise or gradual transition

### Performance-First Architecture
- **Template specialization** for CPU variant-specific optimizations
- **Constexpr-driven** compile-time feature detection
- **Zero runtime overhead** for unused CPU features
- **Cycle-accurate execution** without performance penalties

### Hardware Fidelity
- **Bus-accurate** state modeling
- **Pin-level** hardware simulation
- **Timing-precise** interrupt handling
- **Bug-compatible** NMOS quirks and CMOS fixes

## AI Agent Behavior Requirements

### In-Place Refactoring (REQUIRED)
**ALWAYS prefer modifying existing code rather than creating new implementations:**
- **Refactor existing files** instead of creating parallel versions
- **Modify in-place** rather than duplicating functionality
- **Update existing implementations** rather than starting from scratch
- **Ask for specific files to modify** when unclear which to target
- **Set boundaries** - confirm if new files should be avoided

### Propose Options First (REQUIRED)
**ALWAYS propose approaches and wait for decisions rather than taking immediate action:**
- **Ask "what are my options?"** before implementing
- **Propose 2-4 specific approaches** with clear trade-offs
- **Request planning phase** - analyze first, then implement
- **Wait for explicit approval** before proceeding with changes
- **Use ask_followup_question** to present choices and get direction

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

## Project Architecture Guidelines

### Test Infrastructure
- **ProcessorTests runner**: Use for hardware-verified validation
- **Comprehensive testing**: Run all test suites after changes
- **Debug utilities**: Create targeted debug programs for specific issues

### CPU Implementation
- **Template-driven** architecture with zero-overhead abstractions
- **Hardware-accurate** cycle and pin-level simulation
- **Single source of truth** for processor variants

### File Organization
- **Unified implementations** over fragmented files
- **Logical functional grouping** instead of arbitrary splits
- **Clear separation of concerns** with minimal interdependencies
- **Template-based feature composition** for processor variants

This document defines **behavioral instructions** for AI agents working on this project. Every implementation decision must prioritize **hardware fidelity**, **template-driven optimization**, and **comprehensive verification**.

---

**Last Updated:** 2025-12-27
**Build System Version:** CMake 3.16+
**Unified Output Since:** 2025-12-27
