# PHI2/PHI1 Refactoring - Current Status

**Last Updated**: 2025-11-26  
**Current Phase**: Automated Marking Complete ✅

---

## What Has Been Completed

### ✅ Phase 0: Documentation & Planning (100% Complete)

1. **Hardware Specification** (`6502_PHI1_PHI2_CYCLE_MODEL.md`)
   - Complete 475-line cycle-by-cycle hardware model
   - Detailed PHI2 vs PHI1 phase descriptions
   - RDY behavior specification
   - Bus timing diagrams

2. **Implementation Guide** (`PHI2_PHI1_REFACTORING_COMPLETE_GUIDE.md`)
   - 446 lines of implementation details
   - Template-based `tick<Phase>()` design
   - Clean bus API specification
   - Helper function documentation

3. **Execution Plan** (`PHI2_PHI1_IMPLEMENTATION_PLAN.md`)
   - 5-phase implementation roadmap
   - Risk assessment per phase
   - Dependency order defined
   - Testing strategy outlined

4. **Regex Refactoring Guide** (`PHI2_PHI1_REGEX_REFACTORING_GUIDE.md`)
   - 371 lines of automation strategy
   - Pattern catalog (565 changes identified)
   - Safety guidelines
   - File-by-file breakdown

---

### ✅ Phase 1A: Core Infrastructure (80% Complete)

#### Completed:
1. **Memory Callbacks Removed** ✅
   - Deleted `mem_read`, `mem_write`, `mem_user_data` from CPU state
   - CPU no longer performs memory access internally
   - Revolutionary architecture: CPU only drives bus signals

2. **Template-Based tick() Functions** ✅
   - `tick<Phase::PHI2>()` implemented (even cycles)
   - `tick<Phase::PHI1>()` implemented (odd cycles)
   - Zero runtime overhead with template specialization
   - Centralized RDY handling in PHI2

3. **Bus Setup Functions** ✅
   - `bus_setup_read<AddrMode>(addr)` - PHI2 read setup
   - `bus_setup_write<AddrMode>(addr, data)` - PHI2 write setup
   - `bus_setup_dummy<AddrMode>(addr)` - PHI2 dummy cycle setup
   - All functions only set bus signals, no memory access

4. **PHI1 Helper Functions** ✅
   - `bus_load_reg<Reg>(pins)` - Single-line register load
   - `bus_get_data(pins)` - Get bus data value
   - Clean, readable PHI1 operations

5. **Automated Marking Complete** ✅
   - Python script created (`refactor_phi2_phi1.py`)
   - 259 TODO markers added across 28 files
   - All locations requiring manual work are now marked
   - Summary document created with detailed breakdown

6. **Function Renames** ✅
   - `phi2_dummy_read` → `bus_setup_dummy` (69 instances)
   - Consistent naming across codebase

#### Pending in Phase 1:
- **Remove `should_complete_write_cycle()` function** (marked with 47 TODOs)
- **Remove RDY checks from handlers** (marked with 26 TODOs)
- **Convert `phi2_read<>` calls** (marked with 127 TODOs)
- **Convert `phi2_write<>` calls** (marked with 59 TODOs)

---

## Current State: Ready for Manual Refactoring

### What's Ready
✅ **All code locations are marked** with searchable TODO comments  
✅ **Core infrastructure is in place** (tick<>, bus_setup_*, helpers)  
✅ **Documentation is comprehensive** (4 detailed documents totaling 1800+ lines)  
✅ **Strategy is clear** with step-by-step instructions  

### What's Next
The codebase is now ready for systematic manual refactoring. All 259 locations that need attention are marked and categorized.

---

## TODO Marker Breakdown

### Search Patterns (Use in VSCode)

| Pattern | Count | Purpose | Priority |
|---------|-------|---------|----------|
| `/* TODO_RDY */` | 26 | Remove RDY checks | Medium |
| `/* TODO_WRITE */` (checks) | 47 | Remove write cycle checks | High |
| `/*TODO_READ*/` | 127 | Split read into PHI2+PHI1 | **Critical** |
| `/*TODO_WRITE*/` (calls) | 59 | Convert write calls | High |
| **Total** | **259** | - | - |

---

## Files with Markers

### High Priority (Most Complex)
1. **`operations/addressing_modes.inc.hpp`** - 51 read markers
   - All addressing mode implementations
   - Most critical for cycle accuracy
   
2. **`operations/wide.inc.hpp`** - 64 total markers
   - 65C816 16-bit operations
   - Complex multi-cycle sequences

3. **`operations/transfers.inc.hpp`** - 16 RDY markers
   - Transfer operations
   - Many RDY checks to remove

### Medium Priority
4. **`operations/illegal.inc.hpp`** - 15 markers (10 read + 5 write)
5. **`operations/memory.inc.hpp`** - 25 markers (7 read + 9 write + 9 checks)
6. **`operations/stack.inc.hpp`** - 12 markers (4 read + 4 write + 4 checks)
7. **`operations/cmos.inc.hpp`** - 20 markers (6 read + 7 write + 7 checks)

### Lower Priority
8. **`operations/control.inc.hpp`** - 19 markers
9. **`operations/arithmetic.inc.hpp`** - 7 markers
10. **`fam65xx.hpp`** - 21 markers (core CPU file)
11. **Other files** - Remaining markers

---

## Recommended Execution Order

### Step 1: Low-Risk Removals (2-3 hours)
**Goal**: Remove obsolete checks to clean up codebase

1. Search for `/* TODO_RDY */` (26 instances)
   - Remove `if (!FAM65XX_GET_RDY(pins))` checks
   - Remove associated early returns
   
2. Search for `/* TODO_WRITE */` in context of `should_complete_write_cycle` (47 instances)
   - Remove `if (this->should_complete_write_cycle(pins))` checks
   - Writes now happen unconditionally

3. Delete `should_complete_write_cycle()` function from `fam65xx.hpp`

**Why First**: These are simple deletions with minimal risk. Cleaning them up first makes subsequent refactoring clearer.

---

### Step 2: Convert Write Calls (3-4 hours)
**Goal**: Convert all write operations to new model

1. Search for `/*TODO_WRITE*/phi2_write<` (59 instances)
2. Replace with `bus_setup_write<AddrMode>(addr, value)`
3. Ensure in PHI2 cycles (even cycle_index)
4. Convert handler to `switch(cycle_index)` pattern if needed

**Example Pattern**:
```cpp
// BEFORE:
case 4:
    if (this->should_complete_write_cycle(pins)) {
        this->/*TODO_WRITE*/phi2_write<AddrMode::ZeroPage>(pins, addr, value);
    }
    break;

// AFTER:
case 4: // PHI2 - setup write
    this->bus_setup_write<AddrMode::ZeroPage>(addr, value);
    break;
case 5: // PHI1 - internal operation
    // Any CPU-internal work here
    break;
```

**Why Second**: Writes are simpler than reads (no data sampling needed). This builds confidence before tackling complex read splitting.

---

### Step 3: Split Read Calls (10-15 hours)
**Goal**: Split all reads into PHI2 setup + PHI1 sampling

**Recommended File Order** (easiest → hardest):

1. **`operations/branches.inc.hpp`** (1 marker) - Start here!
   - Single simple read to split
   - Good practice for the pattern

2. **`operations/arithmetic.inc.hpp`** (5 markers)
   - Straightforward ALU operations
   - Clear cycle boundaries

3. **`operations/rockwell.inc.hpp`** (1 marker)
   - Single BBR/BBS instruction

4. **`operations/memory.inc.hpp`** (7 markers)
   - LDA/LDX/LDY operations
   - Well-understood patterns

5. **`operations/stack.inc.hpp`** (4 markers)
   - PHA/PLA operations
   - Stack-specific but clear

6. **`operations/control.inc.hpp`** (9 markers)
   - JSR/RTS/RTI operations
   - More complex control flow

7. **`operations/cmos.inc.hpp`** (6 markers)
   - CMOS-specific operations

8. **`operations/illegal.inc.hpp`** (10 markers)
   - Undocumented operations
   - Well-tested patterns

9. **`operations/wide.inc.hpp`** (26 markers)
   - 65C816 16-bit operations
   - Most complex multi-byte sequences

10. **`operations/addressing_modes.inc.hpp`** (51 markers) - **Save for last!**
    - Most critical file
    - Most complex control flow
    - Requires all previous experience

**Splitting Pattern**:
```cpp
// BEFORE (single cycle):
case 2:
    this->/*TODO_READ*/phi2_read<AddrMode::Absolute>(pins, addr);
    this->A = FAM65XX_GET_DATA(pins);
    break;

// AFTER (two cycles):
case 2: // PHI2 - setup read
    this->bus_setup_read<AddrMode::Absolute>(addr);
    break;
case 3: // PHI1 - sample data and operate
    this->bus_load_reg<Reg::A>(pins);
    // Any additional PHI1 work here
    break;
```

**Why Third**: Reads are the most complex change, requiring cycle splitting and control flow adjustments. Doing writes first provides practice with the new model.

---

### Step 4: Convert to switch(cycle_index) Pattern (6-8 hours)
**Goal**: Ensure all handlers use consistent pattern

After read/write splitting, convert any remaining if-else chains to switch statements:

```cpp
Pins handler(Pins pins) {
    switch (cycle_index) {
        case 0: // PHI2
            this->bus_setup_read<...>(addr);
            break;
        case 1: // PHI1
            this->bus_load_reg<Reg::A>(pins);
            break;
        case 2: // PHI2
            this->bus_setup_write<...>(addr, value);
            break;
        case 3: // PHI1
            // Internal operation
            this->fetch_next_opcode(pins);
            break;
    }
    return pins;
}
```

**Why Fourth**: Once reads/writes are split, the switch pattern becomes natural. This ensures consistency and readability.

---

### Step 5: Cleanup & Testing (3-4 hours)
**Goal**: Remove dead code and validate

1. Delete old functions:
   - `phi2_read<>()`
   - `phi2_write<>()`
   - `phi2_read_operand()`
   - `should_complete_write_cycle()`

2. Run test suites:
   - Klaus 6502 test suite
   - ProcessorTests JSON suite
   - Lorenz C64 test suite

3. Fix any failing tests

4. Update documentation comments

**Why Last**: Only remove dead code after everything is refactored. Testing validates all changes together.

---

## Testing Strategy

### After Each File
1. Compile to check syntax
2. Run basic smoke tests

### After Each Step
1. Full compilation
2. Run Klaus test suite (basic 6502)
3. Run a subset of ProcessorTests

### After Complete Refactoring
1. Full test suite (all 10,000+ tests)
2. Performance benchmarking
3. Cycle accuracy validation

---

## Risk Assessment

### Low Risk (Steps 1-2)
- Simple deletions and replacements
- Well-defined patterns
- Easy to validate

### Medium Risk (Step 4)
- Mechanical but time-consuming
- Consistent pattern reduces errors

### High Risk (Step 3)
- Complex control flow changes
- Cycle boundary splitting
- Addressing modes are critical for accuracy

### Mitigation
- Incremental approach (one file at a time)
- Frequent testing after each file
- Start with simple files to build confidence
- Save most complex (addressing_modes.inc.hpp) for last

---

## Time Estimates

| Phase | Duration | Complexity |
|-------|----------|------------|
| Step 1: Remove checks | 2-3 hours | Low |
| Step 2: Convert writes | 3-4 hours | Medium |
| Step 3: Split reads | 10-15 hours | High |
| Step 4: Switch pattern | 6-8 hours | Medium |
| Step 5: Cleanup & test | 3-4 hours | Low |
| **Total** | **24-34 hours** | - |

This is focused development time. With breaks and testing, expect 4-6 full days of work.

---

## Success Criteria

### Functional Requirements ✅
- [ ] All TODO markers resolved
- [ ] All test suites pass
- [ ] Cycle-accurate behavior maintained
- [ ] No regressions in existing tests

### Code Quality ✅
- [ ] Consistent switch(cycle_index) pattern
- [ ] Clean separation of PHI2 (bus setup) vs PHI1 (operation)
- [ ] No dead code remaining
- [ ] Documentation updated

### Architecture ✅
- [ ] CPU never calls memory directly
- [ ] External code performs all memory access
- [ ] RDY handled centrally in tick<PHI2>()
- [ ] Writes happen unconditionally (hardware accurate)

---

## Next Immediate Actions

1. **Begin Step 1**: Remove RDY checks
   - Open VSCode search
   - Search for `/* TODO_RDY */`
   - Systematically remove each check
   - Compile and test after each file

2. **Track Progress**: Update this document as each step completes

3. **Document Issues**: Note any unexpected problems or patterns

---

## Current Blockers

**None** - Ready to proceed with manual refactoring.

All automation is complete, documentation is comprehensive, and the path forward is clear.

---

## Resources

### Documentation
- `6502_PHI1_PHI2_CYCLE_MODEL.md` - Hardware reference
- `PHI2_PHI1_REFACTORING_COMPLETE_GUIDE.md` - Implementation guide
- `PHI2_PHI1_AUTOMATED_CHANGES_SUMMARY.md` - Detailed marker breakdown
- This file - Current status and next steps

### Tools
- `refactor_phi2_phi1.py` - Automated marking script (already run)
- VSCode search with regex patterns
- Git for incremental commits

### Test Suites
- Klaus 6502 functional test
- ProcessorTests JSON suite (10,000+ tests)
- Lorenz C64 test suite

---

**Status**: 🟢 Ready for Step 1 (Remove RDY checks)

*Last Updated*: 2025-11-26 15:10 UTC