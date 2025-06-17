# MOS6510 Refactoring Status and Remaining Tasks

## **Project Goal**
Refactor the MOS6510 CPU implementation to move `ram_access` from the base `mos6502_family_t` structure into the MOS6510-specific extension, and eliminate unnecessary wrapper functions and architectural issues.

## **Completed Work ✅**

### 1. **Core Architectural Changes**
- ✅ **Moved `ram_access` field** from `mos6502_family_t` to `mos6510_t` struct
- ✅ **Updated all MOS6510 code** to use `cpu->ram_access` instead of `cpu->base.ram_access`
- ✅ **Removed RAM attach functions** from MOS6502 and NES6502 (no longer needed)
- ✅ **Updated terminology** from "zero page" to "zero bank" for banking system references

### 2. **Eliminated Unnecessary Wrapper Functions**
- ✅ **Removed wrapper functions**: `addr_imm`, `mos6510_push`, `mos6510_pull`, `mos6510_get_flag`, `mos6510_set_flag`
- ✅ **Removed unused `get_cpu_capabilities`** functions from all GUI files
- ✅ **Eliminated complex callback-based helpers** that caused type incompatibility issues

### 3. **Fixed Compilation Issues**
- ✅ **Resolved struct redefinition** - Removed duplicate `mos6502_family_s` struct definition
- ✅ **Added missing macros** - `MOS6502_FAMILY_INTRA_CYCLE`, `MOS6510_INTRA_CYCLE`
- ✅ **Fixed function name mismatches** - `mos6502_family_addr_indx` vs `mos6502_family_addr_zpx_ind`
- ✅ **Replaced CPU_ prefixed macros** with MOS6510_ prefixed ones

### 4. **Operation Function Cleanup**
- ✅ **Bulk prefixing** - Replaced all `op_*` calls with `mos6502_family_op_*` calls
- ✅ **Removed duplicate operation helpers** from MOS6510 header and source files
- ✅ **Fixed flag function calls** - Replaced `mos6510_set_zn` with `mos6502_family_set_nz_flags`
- ✅ **Fixed flag access** - Replaced `cpu_get_flag` with `mos6502_family_get_flag`

### 5. **Family Integration**
- ✅ **Fixed interrupt handler** - Added missing `MOS6502_FAMILY_OPCODE_FOOTER` call
- ✅ **Improved opcode dispatch** - Removed unnecessary NULL checks (all opcodes have handlers)

## **Remaining Tasks 🔧**

### 1. **Fix Remaining Compilation Errors**
**Current Status**: Still have compilation errors in illegal opcodes and helper functions.

**Tasks**:
- [ ] Remove remaining duplicate function definitions causing redefinition errors
- [ ] Fix type incompatibility issues in illegal instruction helpers
- [ ] Resolve undefined function references
- [ ] Fix any remaining field access issues (`cpu->base.field` vs `cpu->field`)

### 2. **Add Generic Stub Functions for RAM Access**
**Goal**: Eliminate null checks in `ram_access` callbacks for better performance.

**Tasks**:
- [ ] Implement `generic_stub_read(void* context, uint16_t address)` that returns 0xFF
- [ ] Implement `generic_stub_write(void* context, uint16_t address, uint8_t value)` that does nothing
- [ ] Initialize `ram_access` with stub functions by default
- [ ] Remove all null checks for `ram_access.read_func` and `ram_access.write_func`

### 3. **Complete Illegal Instruction Cleanup**
**Current Status**: Illegal instructions still use complex callback systems.

**Tasks**:
- [ ] Replace remaining callback-based illegal instruction helpers with direct inline implementations
- [ ] Eliminate type casting issues between `mos6510_t*` and `mos6502_family_t*`
- [ ] Simplify illegal instruction implementations to use family functions directly

### 4. **Final Architecture Validation**
**Tasks**:
- [ ] Ensure MOS6510 only contains MOS6510-specific code (I/O ports, zero bank handling)
- [ ] Verify all 6502 family operations are properly delegated to family functions
- [ ] Confirm no duplicate code exists between family and MOS6510 implementations
- [ ] Test that MOS6502 and NES6502 don't have unnecessary `ram_access` fields

### 5. **Build and Test Verification**
**Tasks**:
- [ ] Achieve clean compilation with no errors or warnings
- [ ] Run basic functionality tests to ensure CPU still works correctly
- [ ] Verify I/O port functionality still works (zero bank access)
- [ ] Confirm no performance regressions from architectural changes

## **Key Files Modified**
- `src/chip/cpu/mos6502_family/mos6502_family_core.h` - Removed `ram_access`, fixed struct redefinition
- `src/chip/cpu/mos6502_family/mos6502_family_core.c` - Fixed interrupt handler
- `src/chip/cpu/mos6510/mos6510.h` - Added `ram_access`, removed wrappers, fixed field access
- `src/chip/cpu/mos6510/mos6510.c` - Updated RAM access, fixed initialization
- `src/chip/cpu/mos6510/mos6510_illegal.c` - Bulk operation function prefixing
- `src/chip/cpu/mos6502/mos6502.h/.c` - Removed RAM attach functions
- `src/chip/cpu/nes6502/nes6502.c` - Removed RAM attach functions
- All GUI files - Removed unused `get_cpu_capabilities` functions

## **Architecture Insights Gained**
1. **Banking vs Addressing**: "Zero page" (6502 addressing $0000-$00FF) vs "Zero bank" (C64 banking $0000-$0FFF)
2. **Family-based design**: All common 6502 operations should be in the family, not duplicated per CPU
3. **Type safety**: Callback systems with different pointer types cause unnecessary complexity
4. **Performance**: Direct function calls are better than callback indirection for hot paths

## **Next Session Strategy**
1. Start with a build test to see current error count
2. Focus on the first 10-15 compilation errors systematically
3. Prioritize removing duplicate functions over fixing individual call sites
4. Add generic stub functions early to eliminate null checks
5. Test incrementally after each major fix

## **Key Commands for Next Session**
```powershell
# Test current build status
cmake --build . --config Release 2>&1 | Select-Object -First 20

# Find remaining duplicate functions
grep -r "^void mos6502_family_op_" src/chip/cpu/mos6510/

# Check for remaining null checks
grep -r "ram_access.*read_func" src/chip/cpu/mos6510/
```
