# PHI2/PHI1 Implementation Plan - Execution Order

## Key Improvements from User Feedback

1. **Remove data_reg from bus_setup_read()**:
   - PHI2: `bus_setup_read<Addr::PC>(pins)` - no data_reg argument
   - PHI1: `this->bus_load_reg(REG_ABL, pins)` - single-line load

2. **Add bus helper functions**:
   - `bus_load_reg(data_reg, pins)` - combines get + load in one call
   - `bus_get_data(pins)` - for when you need the value
   - All bus interactions use `bus_*` prefix

3. **Cleaner separation**:
   - PHI2 handlers: Only call `bus_setup_*()` functions
   - PHI1 handlers: Call `bus_load_reg()` or `bus_get_data()` functions

## API Changes Summary

### Old API (phi2_read with data_reg):
```cpp
pins = phi2_read<Addr::PC>(pins, REG_ABL);  // Reads AND stores
if (FAM65XX_GET_RDY(pins)) {
    // continue
}
```

### New API (separate bus setup and data load):
```cpp
// PHI2 (even cycle):
pins = bus_setup_read<Addr::PC>(pins);  // Only sets up bus

// PHI1 (odd cycle):
this->bus_load_reg(REG_ABL, pins);  // ✅ Clean 1-line load
```

### For calculations that need the value:
```cpp
// PHI1 (odd cycle):
uint8_t value = this->bus_get_data(pins);  // Get value
this->set(REG_A, value + 1);  // Use in expression
```

## Critical Dependencies

The refactoring must be done in this exact order to maintain compilability:

### Step 1: Add New Helper Functions
- Add `bus_load_reg(data_reg, pins)` - combines set + FAM65XX_GET_DATA
- Add `bus_get_data(pins)` - wraps FAM65XX_GET_DATA
- Add new `bus_setup()`, `bus_setup_read()`, `bus_setup_write()`, `bus_setup_dummy()`
- Keep old `phi2_*` functions temporarily for backward compatibility
- New functions do NOT perform memory access, only set pins
- New functions do NOT take data_reg argument

### Step 2: Update RMW Helper to Not Use should_complete_write_cycle()
- Rewrite `rmw_operation_helper()` to use tick()'s RDY check instead
- Remove all `should_complete_write_cycle()` calls

### Step 3: Remove should_complete_write_cycle() Function
- Now safe to remove since nothing uses it

### Step 4: Rewrite tick() With Centralized RDY
- Add PHI2/PHI1 phase detection (even/odd cycle_index)
- Single RDY check in PHI2 phase only
- Call handler without RDY checks

### Step 5: Update Addressing Modes
- Change `phi2_read<Addr::PC>(pins, REG_ABL)` → `bus_setup_read<Addr::PC>(pins)`
- In PHI1 cycle: Add `this->bus_load_reg(REG_ABL, pins)`
- Change all `phi2_write` → `bus_setup_write`
- Change all `phi2_dummy_read` → `bus_setup_dummy`
- Remove ALL `FAM65XX_GET_RDY()` checks from handlers
- Split cycles into PHI2 (even) and PHI1 (odd) pairs

### Step 6: Update Operations
- Same changes as addressing modes
- Replace direct `FAM65XX_GET_DATA(pins)` with `bus_load_reg()` or `bus_get_data()`

### Step 7: Remove Old phi2_* Functions
- Now safe since all code uses bus_setup_*

### Step 8: Remove Memory Callbacks
- Remove `mem_read`, `mem_write`, `mem_user_data`
- Remove `set_memory_callbacks()`
- Remove `phi2_read_impl()` and `phi2_write_impl()`

### Step 9: Update Test Harnesses
- Add external memory access between PHI2 and PHI1 ticks

## Current Status

**DECISION NEEDED**: This is a massive refactoring affecting 50+ files.

**OPTIONS**:

### Option A: Full Refactoring (High Risk)
- Do complete architectural change now
- High risk of breaking existing functionality
- Difficult to debug if issues arise
- Estimated: 2-3 weeks

### Option B: Incremental with Feature Flag (Low Risk)
- Add compile-time flag to enable new architecture
- Keep both implementations working
- Test incrementally
- Remove old code once stable
- Estimated: 3-4 weeks but safer

### Option C: Documentation Only (No Risk)
- Complete and perfect all documentation ✅ DONE
- Provide clear implementation guide
- Leave actual implementation for later
- Can be done systematically when ready

## Recommendation

Given the scope (1000+ lines of changes across 50+ files), we should:

1. **Document the complete architecture** ✅ DONE
2. **Create detailed implementation guide** ✅ DONE
3. **Wait for user approval** before proceeding
4. If approved: Use **Option B (Feature Flag)** for safety

## Documentation Completed

✅ **6502_PHI1_PHI2_CYCLE_MODEL.md** - Hardware specification
✅ **PHI2_PHI1_REFACTORING_COMPLETE_GUIDE.md** - Implementation guide  
✅ **PHI2_PHI1_IMPLEMENTATION_PLAN.md** - Execution roadmap

All documentation includes the improved API with `bus_get_data()` helper.