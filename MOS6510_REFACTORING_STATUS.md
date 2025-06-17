# MOS6510 CPU Refactoring Status

## Project Overview

This document tracks the comprehensive refactoring of the MOS6510 CPU implementation in the aiemu project. The goal is to move the `ram_access` member out of the base `mos6502_family_t` structure and into the MOS6510-specific extension, ensuring architectural cleanliness and eliminating unnecessary code duplication.

## Refactoring Goals

### Primary Objectives
- [x] Move `ram_access` from base `mos6502_family_t` to MOS6510-specific structure
- [x] Ensure only CPUs requiring direct RAM access (MOS6510 for zero-bank I/O port bypass) have it
- [x] Eliminate unnecessary wrapper functions in MOS6510
- [x] Move all common 6502 operations to the family base
- [ ] Resolve all architectural and build issues
- [ ] Clean up illegal opcode handling
- [ ] Remove all duplicate code between family and MOS6510
- [ ] Ensure design is clean, type-safe, and efficient

### Secondary Objectives
- [ ] Achieve clean compilation with no errors or warnings
- [ ] Validate all tests pass with new architecture
- [ ] Ensure correct CPU and I/O port behavior
- [ ] Document architectural decisions and patterns

## Completed Work

### Structural Changes ✅
- **Removed `ram_access` from base family struct** (`mos6502_family_core.h`)
  - Eliminated from `mos6502_family_t` structure
  - Added to MOS6510-specific `mos6510_t` structure
  - Updated all MOS6510 code to use `cpu->ram_access` instead of `cpu->family.ram_access`

- **Removed RAM attach functions from other CPUs**
  - Removed `mos6502_attach_ram()` from MOS6502 (`mos6502.h/.c`)
  - Removed `nes6502_attach_ram()` from NES6502 (`nes6502.c`)
  - Only MOS6510 retains RAM attachment capability

### Code Deduplication ✅
- **Replaced all `op_*` function calls with `mos6502_family_op_*` calls**
  - Updated throughout `mos6510.c` and `mos6510_illegal.c`
  - Used PowerShell bulk replacement: `(Get-Content file) -replace '\bop_([a-zA-Z_]+)\b', 'mos6502_family_op_$1'`

- **Removed duplicate operation helpers from MOS6510**
  - Eliminated redundant helper functions from `mos6510.h` and `mos6510.c`
  - All common operations now delegated to family functions

- **Replaced MOS6510-specific helpers with family versions**
  - `mos6510_set_zn` → `mos6502_family_set_nz_flags`
  - `cpu_get_flag` → `mos6502_family_get_flag`
  - Used grep to identify and replace systematically

### Build Fixes ✅
- **Fixed struct redefinition errors** in `mos6502_family_core.h`
- **Added missing macros** (`MOS6502_FAMILY_INTRA_CYCLE`, `MOS6510_INTRA_CYCLE`)
- **Fixed interrupt handler and opcode dispatch macros** in family core
- **Updated function signatures** throughout for correct pointer casting
- **Removed unused functions** from GUI files (`get_cpu_capabilities`)

### Terminology Updates ✅
- **Updated "zero page" to "zero bank"** for banking system references
- Reflects accurate MOS6510 memory banking architecture

## Current Build Status

### Last Build Results
- **Status**: Build errors present
- **CMake Generation**: ✅ Successful
- **Compilation**: ❌ Errors in multiple files

### Known Issues Remaining

#### 1. Type Incompatibility Issues
```c
// In mos6510_illegal.c - callback signature mismatches
void mos6510_sax_illegal(mos6502_family_t* cpu, uint16_t addr, uint8_t value) {
    // Error: Expected callback signature doesn't match
}
```

#### 2. Duplicate Function Definitions
- Some operation helpers still duplicated between family and MOS6510
- Need systematic identification and removal

#### 3. Missing Generic Stub Functions
- Need `ram_access` stub functions to eliminate null pointer checks
- Should provide no-op implementations for CPUs without RAM access

#### 4. Callback-based Illegal Instruction Helpers
- Some illegal instruction helpers still use callback patterns
- Should be converted to direct inline implementations

## Files Modified

### Core Architecture Files
- `src/chip/cpu/mos6502_family/mos6502_family_core.h` - Removed ram_access, fixed macros
- `src/chip/cpu/mos6502_family/mos6502_family_core.c` - Fixed interrupt handling
- `src/chip/cpu/mos6502_family/mos6502_family_opcodes.c` - Operation helpers
- `src/chip/cpu/mos6502_family/mos6502_family_control.c` - Macro usage fixes

### MOS6510 Specific Files
- `src/chip/cpu/mos6510/mos6510.h` - Added ram_access, removed wrappers
- `src/chip/cpu/mos6510/mos6510.c` - Updated RAM access patterns
- `src/chip/cpu/mos6510/mos6510_illegal.c` - Bulk function prefixing

### Other CPU Files
- `src/chip/cpu/mos6502/mos6502.h` - Removed RAM attach functions
- `src/chip/cpu/mos6502/mos6502.c` - Removed RAM attach functions
- `src/chip/cpu/nes6502/nes6502.c` - Removed RAM attach functions

### GUI Files
- Multiple GUI files - Removed unused `get_cpu_capabilities` functions

## Next Steps (Priority Order)

### Immediate (Critical for Build)
1. **Fix Type Incompatibility Issues**
   - Resolve callback signature mismatches in illegal opcode helpers
   - Ensure all function pointers match expected signatures

2. **Remove Remaining Duplicate Functions**
   - Use grep to identify: `grep -r "^[a-zA-Z_][a-zA-Z0-9_]*.*{" src/chip/cpu/mos6510/ | grep -v "mos6502_family_"`
   - Systematically remove duplicates that exist in family

3. **Implement Generic RAM Access Stubs**
   - Create no-op `ram_access` functions for base family
   - Eliminate null pointer checks throughout codebase

### Short Term (Architecture Cleanup)
4. **Replace Callback-based Illegal Helpers**
   - Convert remaining callback patterns to direct implementations
   - Simplify illegal instruction handling

5. **Achieve Clean Build**
   - Run incremental CMake builds
   - Fix compilation errors as they surface
   - Ensure zero warnings

### Medium Term (Validation)
6. **Test Suite Validation**
   - Run all existing tests
   - Verify CPU behavior matches expected
   - Test I/O port functionality specifically

7. **Performance Validation**
   - Ensure refactoring doesn't impact performance
   - Benchmark if necessary

## Useful Commands for Continuation

### Build Commands
```powershell
# From d:\Workspaces\Git\aiemu\code\c
cmake --build . --config Release
```

### Search and Replace Patterns
```powershell
# Find remaining op_ calls that should be mos6502_family_op_
grep -r "\bop_[a-zA-Z_]" src/chip/cpu/mos6510/

# Find potential duplicate functions
grep -r "^[a-zA-Z_][a-zA-Z0-9_]*.*{" src/chip/cpu/mos6510/ | grep -v "mos6502_family_"

# Check for ram_access usage patterns
grep -r "ram_access" src/chip/cpu/
```

### Bulk Replacement Examples
```powershell
# Replace op_ calls with mos6502_family_op_ calls
(Get-Content mos6510.c) -replace '\bop_([a-zA-Z_]+)\b', 'mos6502_family_op_$1' | Set-Content mos6510.c

# Replace MOS6510-specific helpers with family versions
(Get-Content file.c) -replace '\bmos6510_set_zn\b', 'mos6502_family_set_nz_flags' | Set-Content file.c
```

## Architecture Decisions Made

### RAM Access Pattern
- **Decision**: Only MOS6510 has direct RAM access capability
- **Rationale**: MOS6510 needs to bypass I/O ports for zero-bank access; other CPUs don't
- **Implementation**: Moved `ram_access` to MOS6510-specific structure

### Operation Delegation
- **Decision**: All common 6502 operations handled by family functions
- **Rationale**: Eliminates code duplication, ensures consistency
- **Implementation**: MOS6510 calls `mos6502_family_op_*` functions exclusively

### Helper Function Strategy
- **Decision**: Remove all duplicate helpers from MOS6510
- **Rationale**: Single source of truth, easier maintenance
- **Implementation**: Systematic replacement with family equivalents

## Risk Assessment

### Low Risk
- Structural changes are well-contained
- Build system is stable
- Test framework exists for validation

### Medium Risk
- Type compatibility issues may require significant debugging
- Performance impact unknown until testing

### High Risk
- None identified - refactoring is incremental and reversible

## Success Criteria

### Technical
- [x] `ram_access` moved to MOS6510-specific structure
- [ ] Clean compilation with zero errors/warnings
- [ ] All tests pass
- [ ] No duplicate code between family and MOS6510

### Architectural
- [x] Clear separation of concerns
- [ ] Type-safe interfaces
- [ ] Efficient operation delegation
- [ ] Maintainable code structure

---

**Last Updated**: June 17, 2025  
**Status**: In Progress - Build Issues Remain  
**Next Action**: Fix type incompatibility issues in illegal opcode handlers  
**Estimated Completion**: 2-3 more sessions with focused debugging
