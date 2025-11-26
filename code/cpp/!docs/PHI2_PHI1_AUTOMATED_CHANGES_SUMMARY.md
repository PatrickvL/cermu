# PHI2/PHI1 Automated Refactoring - Summary Report

**Date**: 2025-11-26  
**Script**: `refactor_phi2_phi1.py`  
**Total Markers Added**: 259

---

## Overview

This document summarizes the automated marking phase of the PHI2/PHI1 refactoring. The script has added TODO markers to all code locations that require manual refactoring, making them easy to find and track.

---

## Phase 1: Function Renames (✅ Completed Previously)

### Completed in Previous Run
- **`phi2_dummy_read` → `bus_setup_dummy`**: 69 instances renamed
- **`phi2_read_operand` marked as deprecated**: Function to be removed

---

## Phase 2: Code Markers Added (✅ Just Completed)

### 1. RDY Checks - 26 Markers Added

**Search Pattern**: `/* TODO_RDY: Remove check */`

**Purpose**: RDY checks should be removed from individual handlers. RDY is now handled centrally in `tick<Phase::PHI2>()`.

**Files Affected** (4 files):
- `fam65xx.hpp`: 1 marker
- `operations/arithmetic.inc.hpp`: 2 markers
- `operations/transfers.inc.hpp`: 16 markers
- `operations/flags.inc.hpp`: 7 markers

**Action Required**:
1. Search for `/* TODO_RDY */`
2. Remove the entire `if (!FAM65XX_GET_RDY(pins))` check
3. Remove any associated early returns or cycle_index manipulations
4. RDY is now handled by `tick<Phase::PHI2>()` returning early

**Example**:
```cpp
// BEFORE (marked):
/* TODO_RDY: Remove check */ if (!FAM65XX_GET_RDY(pins)) {
    return pins; // Don't advance if RDY held
}

// AFTER:
// (entire block removed)
```

---

### 2. Write Cycle Checks - 47 Markers Added

**Search Pattern**: `/* TODO_WRITE: Remove check */`

**Purpose**: `should_complete_write_cycle()` checks are obsolete. The new model performs writes unconditionally on PHI2 (hardware accurate).

**Files Affected** (6 files):
- `fam65xx.hpp`: 3 markers
- `operations/control.inc.hpp`: 5 markers
- `operations/cmos.inc.hpp`: 7 markers
- `operations/memory.inc.hpp`: 9 markers
- `operations/stack.inc.hpp`: 4 markers
- `operations/wide.inc.hpp`: 19 markers

**Action Required**:
1. Search for `/* TODO_WRITE */`
2. Remove the `if (this->should_complete_write_cycle(pins))` check
3. Writes now happen unconditionally during PHI2 setup
4. The function `should_complete_write_cycle()` itself should be deleted from `fam65xx.hpp`

**Example**:
```cpp
// BEFORE (marked):
/* TODO_WRITE: Remove check */ if (this->should_complete_write_cycle(pins)) {
    this->phi2_write<...>(pins, value);
}

// AFTER:
this->bus_setup_write<...>(value);  // Always executes, no check
```

---

### 3. Read Calls - 127 Markers Added

**Search Pattern**: `/*TODO_READ*/phi2_read<`

**Purpose**: Mark all `phi2_read<>` calls that need to be split into PHI2 bus setup + PHI1 data sampling.

**Files Affected** (11 files):
- `fam65xx.hpp`: 7 markers
- `operations/control.inc.hpp`: 9 markers
- `operations/arithmetic.inc.hpp`: 5 markers
- `operations/rockwell.inc.hpp`: 1 marker
- `operations/cmos.inc.hpp`: 6 markers
- `operations/branches.inc.hpp`: 1 marker
- `operations/memory.inc.hpp`: 7 markers
- `operations/stack.inc.hpp`: 4 markers
- `operations/illegal.inc.hpp`: 10 markers
- `operations/wide.inc.hpp`: 26 markers
- `operations/addressing_modes.inc.hpp`: 51 markers (most complex!)

**Action Required**:
1. Search for `/*TODO_READ*/`
2. Split the marked call into:
   - **PHI2**: `bus_setup_read<...>(address)` - sets bus signals only
   - **PHI1**: `bus_load_reg<Reg>(pins)` or `bus_get_data(pins)` - samples data

**Example**:
```cpp
// BEFORE (marked):
case 2:
    this->/*TODO_READ*/phi2_read<AddrMode::Absolute>(pins, addr);
    this->A = FAM65XX_GET_DATA(pins);
    break;

// AFTER (split into two cycles):
case 2: // PHI2 - setup read
    this->bus_setup_read<AddrMode::Absolute>(addr);
    break;
case 3: // PHI1 - sample data
    this->bus_load_reg<Reg::A>(pins);
    break;
```

---

### 4. Write Calls - 59 Markers Added

**Search Pattern**: `/*TODO_WRITE*/phi2_write<`

**Purpose**: Mark all `phi2_write<>` calls that need to be converted to PHI2-only `bus_setup_write<>` calls.

**Files Affected** (7 files):
- `fam65xx.hpp`: 10 markers
- `operations/control.inc.hpp`: 5 markers
- `operations/cmos.inc.hpp`: 7 markers
- `operations/memory.inc.hpp`: 9 markers
- `operations/stack.inc.hpp`: 4 markers
- `operations/illegal.inc.hpp`: 5 markers
- `operations/wide.inc.hpp`: 19 markers

**Action Required**:
1. Search for `/*TODO_WRITE*/`
2. Replace `phi2_write<>` with `bus_setup_write<>`
3. Remove any accompanying `should_complete_write_cycle()` checks (already marked separately)

**Example**:
```cpp
// BEFORE (marked):
case 4:
    if (this->should_complete_write_cycle(pins)) {
        this->/*TODO_WRITE*/phi2_write<AddrMode::Absolute>(pins, addr, value);
    }
    break;

// AFTER (PHI2 only, unconditional):
case 4: // PHI2 - setup write
    this->bus_setup_write<AddrMode::Absolute>(addr, value);
    break;
case 5: // PHI1 - internal operation (if needed)
    // CPU internal work here
    break;
```

---

## Summary Statistics

| Marker Type | Count | Files | Priority |
|-------------|-------|-------|----------|
| `TODO_RDY` | 26 | 4 | Medium - Simple removal |
| `TODO_WRITE` (checks) | 47 | 6 | High - Must remove before refactoring writes |
| `TODO_READ` | 127 | 11 | **Critical** - Most complex, requires cycle splitting |
| `TODO_WRITE` (calls) | 59 | 7 | High - Simpler than reads, still manual |
| **TOTAL** | **259** | **28** | - |

---

## Recommended Order of Attack

### Step 1: Remove Obsolete Checks (Low Risk, High Reward)
1. Remove all `/* TODO_RDY */` checks (26 instances)
2. Remove all `/* TODO_WRITE */` checks for `should_complete_write_cycle` (47 instances)
3. Delete the `should_complete_write_cycle()` function itself

**Estimated Time**: 1-2 hours  
**Risk Level**: Low (simple deletions)

---

### Step 2: Convert Write Calls (Medium Complexity)
1. Replace `/*TODO_WRITE*/phi2_write<>` with `bus_setup_write<>` (59 instances)
2. Ensure writes are in PHI2 cycles (even cycle_index)

**Estimated Time**: 2-3 hours  
**Risk Level**: Medium (simple replacement, but need cycle alignment)

---

### Step 3: Split Read Calls (High Complexity)
1. Start with simple cases in `operations/arithmetic.inc.hpp` (5 markers)
2. Then `operations/branches.inc.hpp` (1 marker - easiest)
3. Move to `operations/memory.inc.hpp` (7 markers)
4. Save `operations/addressing_modes.inc.hpp` for last (51 markers - most complex)

**Estimated Time**: 8-12 hours  
**Risk Level**: High (requires cycle splitting, control flow changes)

**Strategy**:
- Convert each handler to `switch(cycle_index)` pattern first
- Then split reads into PHI2 + PHI1 pairs
- Test incrementally after each file

---

### Step 4: Convert All Handlers to switch(cycle_index)
After read/write splitting, all handlers should use consistent pattern:
```cpp
Pins handler(Pins pins) {
    switch (cycle_index) {
        case 0: /* even - PHI2 setup */ break;
        case 1: /* odd - PHI1 operation */ break;
        // ...
    }
    return pins;
}
```

**Estimated Time**: 6-8 hours  
**Risk Level**: Medium (mechanical but time-consuming)

---

### Step 5: Cleanup and Formatting
1. Run `clang-format` on all modified files
2. Remove dead code (old `phi2_read`, `phi2_write`, `phi2_read_operand` functions)
3. Update documentation comments

**Estimated Time**: 2-3 hours  
**Risk Level**: Low

---

## Files Requiring Most Attention

### Critical Files (Most Markers)
1. **`operations/addressing_modes.inc.hpp`**: 51 read markers
   - This file contains all addressing mode implementations
   - Most complex refactoring
   - All addressing modes must be cycle-accurate

2. **`operations/wide.inc.hpp`**: 64 total markers (26 read + 19 write + 19 write-check)
   - 65C816 16-bit operations
   - Complex multi-cycle operations
   - Requires careful cycle alignment

3. **`operations/transfers.inc.hpp`**: 16 RDY markers
   - Transfer operations (TAX, TAY, etc.)
   - Many RDY checks to remove
   - Relatively simple otherwise

---

## Testing Strategy

### After Each Major Change
1. **Compile**: Ensure code compiles without errors
2. **Run Klaus Test Suite**: Basic 6502 functionality
3. **Run Processor Tests**: Detailed cycle accuracy
4. **Run Lorenz Test Suite**: C64-specific behavior

### Incremental Testing
- Test after each file is refactored
- Don't move to next file until current one passes tests
- Keep a backup of working state before major changes

---

## Next Actions

### Immediate (This Session)
1. ✅ Run automated marking script (COMPLETED - 259 markers added)
2. ⏭️ Create this summary document (CURRENT)
3. ⏭️ Run `clang-format` on all marked files for baseline formatting
4. ⏭️ Begin Step 1: Remove RDY checks (26 instances)

### Short Term (Next Session)
1. Complete Step 1 & 2 (Remove checks, convert writes)
2. Begin Step 3: Split read calls (start with simple files)
3. Document any issues or patterns discovered

### Long Term (Multiple Sessions)
1. Complete all read call splitting
2. Convert all handlers to switch() pattern
3. Full test suite validation
4. Performance benchmarking

---

## Risk Assessment

### Low Risk Areas
- RDY check removal (centralized in tick())
- Write cycle check removal (obsolete function)
- Simple write call conversion

### High Risk Areas
- Addressing mode read splitting (51 instances, complex control flow)
- 65C816 wide operations (multi-byte, multi-cycle)
- Interrupt handling during refactoring
- Ensuring cycle_index increments happen at correct phase

### Mitigation
- Incremental changes with frequent testing
- Keep detailed notes of changes
- Maintain working backup branches
- Use automated formatting to reduce human error

---

## Tool Support

### Search Patterns for VSCode
```
/* TODO_RDY */         - Find RDY checks (26)
/* TODO_WRITE */       - Find write checks & calls (106)
/*TODO_READ*/          - Find read calls (127)
/*TODO_*/              - Find ALL marked locations (259)
```

### Git Strategy
```bash
# Before starting
git checkout -b phi2-phi1-refactoring
git commit -m "Add TODO markers for PHI2/PHI1 refactoring"

# After each file
git add <file>
git commit -m "Refactor <file> for PHI2/PHI1 model"

# After each phase
git tag phase-N-complete
```

---

## Conclusion

The automated marking phase is **complete**. All 259 locations requiring manual attention are now marked with searchable TODO comments. The refactoring can proceed systematically, file by file, with clear visibility of remaining work.

**Estimated Total Effort**: 20-30 hours of focused refactoring work

**Current Status**: Ready to begin manual refactoring, starting with low-risk removals.

---

*Document Generated*: 2025-11-26  
*Last Updated*: 2025-11-26  
*Status*: Automated Marking Complete ✅