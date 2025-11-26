# PHI2/PHI1 Regex-Based Refactoring Guide

## Summary of Changes Required

- **124 instances** of `phi2_read<`
- **54 instances** of `phi2_write<`
- **67 instances** of `phi2_dummy_read<`
- **~40 instances** of `should_complete_write_cycle()`
- **~180+ instances** of RDY checks that need removal

---

## Step 1: Simple Renames (Safe - No Logic Change)

These are straightforward function name changes:

### 1.1 Rename phi2_dummy_read → bus_setup_dummy

**Find (Regex)**:
```regex
this->phi2_dummy_read<
```

**Replace With**:
```
this->bus_setup_dummy<
```

**Instances**: 67  
**Files**: All operation files  
**Risk**: ⭐ LOW - Direct rename, no logic change

---

## Step 2: Remove RDY Checks from Handlers

The new architecture centralizes RDY checking in `tick<Phase::PHI2>`, so all handler-level checks must be removed.

### 2.1 Remove RDY checks after bus_setup calls

**Pattern 1 - After bus_setup_dummy**:
```regex
pins = this->bus_setup_dummy<(.+?)>\(pins\);\s*if \(!?FAM65XX_GET_RDY\(pins\)\) \{\s*return pins;\s*\}
```

**Replace With**:
```
pins = this->bus_setup_dummy<$1>(pins);
```

**Pattern 2 - After any bus setup with RDY return**:
```regex
if \(!FAM65XX_GET_RDY\(pins\)\) \{\s*return pins;\s*\}
```

**Replace With**:
```
(delete - remove entire if block)
```

---

## Step 3: Complex Refactoring (Requires Manual Review)

These changes affect control flow and require understanding the context.

### 3.1 phi2_read → bus_setup_read + bus_load_reg

**Old Pattern**:
```cpp
pins = this->phi2_read<Addr::PC>(pins, REG_DL);
if (FAM65XX_GET_RDY(pins)) {
    this->inc(REG_PC);
    cycle_index++;
}
```

**New Pattern** (PHI2/PHI1 split required):
```cpp
// Case 0 (PHI2): Set up bus
pins = this->bus_setup_read<Addr::PC>(pins);
return pins;

// Case 1 (PHI1): Load data and process  
this->bus_load_reg(REG_DL, pins);
this->inc(REG_PC);
cycle_index++;
```

**Manual Steps Required**:
1. Identify which cycle (even/odd) the code is in
2. Split PHI2 (bus setup) from PHI1 (data processing)
3. Move all logic AFTER the read to the PHI1 cycle
4. Remove RDY check (centralized in tick)

**Note**: This CANNOT be done with simple regex - requires understanding of handler flow.

### 3.2 phi2_write → bus_setup_write

**Old Pattern**:
```cpp
if (this->should_complete_write_cycle(pins)) {
    pins = this->phi2_write<Addr::SP>(pins, this->get(REG_A));
    this->dec(REG_S);
    cycle_index++;
}
```

**New Pattern**:
```cpp
// PHI2 cycle:
pins = this->bus_setup_write<Addr::SP>(pins, this->get(REG_A));
return pins;

// PHI1 cycle:
this->dec(REG_S);
cycle_index++;
```

**Manual Steps Required**:
1. Remove `should_complete_write_cycle()` check
2. Split write cycle into PHI2 (bus setup) and PHI1 (post-write logic)
3. Move register updates to PHI1 cycle

### 3.3 Remove should_complete_write_cycle() Entirely

**Find**:
```regex
if \(this->should_complete_write_cycle\(pins\)\) \{
```

**Action**: Remove the if statement wrapper, keeping the inner code

**Reason**: RDY is now centralized, writes always proceed when tick<PHI2> is called

---

## Step 4: Handler Structure Changes

ALL handlers must be converted to use `switch(cycle_index)` pattern.

### 4.1 Current Pattern (Varied)

Handlers currently use inconsistent patterns:
- Some use if/else chains
- Some use cycle_index checks inline
- Some have mixed logic

### 4.2 Target Pattern (Consistent)

**Every handler must use this structure**:

```cpp
bus_state_t handler_name(bus_state_t pins) {
    switch (cycle_index) {
        case 0: // PHI2: Bus setup
            pins = bus_setup_[read|write|dummy]<Addr::X>(pins [, data]);
            return pins;  // NO cycle_index++ in PHI2
            
        case 1: // PHI1: Process result
            bus_load_reg(REG_X, pins);  // If read
            // Perform operations
            cycle_index++;  // ALWAYS in PHI1
            return pins;
            
        case 2: // Next PHI2...
            // Continue pattern
    }
    return pins;
}
```

**Manual Conversion Required**: This cannot be automated with regex.

---

## Step 5: File-by-File Refactoring Order

Based on complexity and dependencies:

### Priority 1: Core Infrastructure (fam65xx.hpp)
- [ ] Fix `rmw_operation_helper()` (lines 510-681)
- [ ] Fix `fetch_opcode()` (line 859)

### Priority 2: Simple Operations (Low Risk)
- [ ] `flags.inc.hpp` - Flag operations (CLC, SEC, etc.) - 7 handlers
- [ ] `transfers.inc.hpp` - Register transfers (TAX, TXA, etc.) - 16 handlers

### Priority 3: Addressing Modes (Medium Risk)
- [ ] `addressing_modes.inc.hpp` - All 18 addressing modes

### Priority 4: Complex Operations (High Risk)
- [ ] `stack.inc.hpp` - Stack operations (PHA, PLA, etc.) - 8 handlers
- [ ] `control.inc.hpp` - Control flow (JMP, JSR, RTS, RTI, BRK) - 8 handlers  
- [ ] `branches.inc.hpp` - Branch operations - 9 handlers
- [ ] `memory.inc.hpp` - Load/store operations - 6 handlers
- [ ] `arithmetic.inc.hpp` - ADC, SBC, CMP, etc. - 6 handlers

### Priority 5: Special Cases (Highest Risk)
- [ ] `rmw.inc.hpp` - RMW operations using helper - 6 handlers
- [ ] `illegal.inc.hpp` - Illegal opcodes - 20+ handlers
- [ ] `cmos.inc.hpp` - 65C02 enhancements - 10 handlers
- [ ] `wide.inc.hpp` - 65C816 16-bit operations - 15 handlers
- [ ] `rockwell.inc.hpp` - Rockwell bit manipulation - 16 handlers

---

## Step 6: Validation Checklist

After each file refactoring:

- [ ] **Compilation**: File compiles without errors
- [ ] **RDY checks removed**: No `if (FAM65XX_GET_RDY(pins))` in handlers
- [ ] **Switch pattern**: All handlers use `switch(cycle_index)`
- [ ] **PHI2 cycles**: Even cycles only call `bus_setup_*()` functions
- [ ] **PHI1 cycles**: Odd cycles use `bus_load_reg()` or `bus_get_data()`
- [ ] **cycle_index**: PHI2 handlers NEVER increment, PHI1 handlers ALWAYS increment by 1
- [ ] **Transitions**: Use `transition_to_fetch()` or `transition_to_operation()`

---

## Example: Complete Before/After

### BEFORE (flags.inc.hpp - op_clc)

```cpp
bus_state_t op_clc(bus_state_t pins) {
    /* Dummy cycle for internal operation */
    pins = this->phi2_dummy_read<Addr::PC>(pins);
    if (!FAM65XX_GET_RDY(pins)) {
        return pins;
    }
    this->update_flag(FLAG_C, false);
    this->transition_to_fetch();
    return pins;
}
```

### AFTER (flags.inc.hpp - op_clc)

```cpp
bus_state_t op_clc(bus_state_t pins) {
    switch (cycle_index) {
        case 0: // PHI2: Dummy read
            pins = this->bus_setup_dummy<Addr::PC>(pins);
            return pins;
            
        case 1: // PHI1: Clear carry flag
            this->update_flag(FLAG_C, false);
            this->transition_to_fetch();
            return pins;
    }
    return pins;
}
```

**Changes**:
1. ✅ Added `switch(cycle_index)`
2. ✅ Renamed `phi2_dummy_read` → `bus_setup_dummy`
3. ✅ Removed RDY check
4. ✅ Split PHI2 (case 0) and PHI1 (case 1)
5. ✅ PHI2 returns immediately (no cycle_index++)
6. ✅ PHI1 has logic and transition

---

## Automated Regex Scripts

For use with sed, perl, or VSCode find/replace:

### Script 1: Rename phi2_dummy_read
```bash
find code/cpp/src/chip/cpu/fam65xx -name "*.hpp" -type f -exec sed -i 's/this->phi2_dummy_read</this->bus_setup_dummy</g' {} +
```

### Script 2: Remove simple RDY checks after bus_setup_dummy
```bash
# This is complex - recommend manual review for each case
```

**⚠️ WARNING**: Automated scripts should be used CAREFULLY with version control. Always review changes before committing.

---

## Estimated Effort

| Task | Instances | Automation | Manual | Time |
|------|-----------|------------|--------|------|
| Rename phi2_dummy_read | 67 | 100% | 0% | 5 min |
| Remove RDY checks | ~180 | 80% | 20% | 30 min |
| phi2_read refactor | 124 | 0% | 100% | 4 hours |
| phi2_write refactor | 54 | 0% | 100% | 2 hours |
| should_complete_write_cycle | 40 | 50% | 50% | 1 hour |
| Handler structure | 100+ | 0% | 100% | 8 hours |
| **TOTAL** | **565** | **~15%** | **~85%** | **~16 hours** |

**Recommendation**: Even with regex assistance, this is a 2-3 day focused effort requiring careful manual review at each step.

---

*Last Updated: 2025-11-26*
*Status: Ready for systematic application*