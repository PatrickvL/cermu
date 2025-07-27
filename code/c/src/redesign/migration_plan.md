
# Migration Plan: Integrating Redesign Concepts into the Main C64 Emulator Project (July 2025)

## 1. Architectural Inspiration from Redesign Reference

**Key redesign ideas still present in the reference files and not yet fully applied:**
- **Nostradamus Distributor Pattern:** Stackless, threaded CPU opcode dispatch using handler return values (see `c64_cpu_integration_example.c`).
- **Centralized, branchless chip select logic:** Precomputed arrays and encoded selectors for ultra-fast bus-to-chip mapping (`c64_bus_optimized_header.h`, `c64_bus_implementation.c`).
- **Unified bus state threading:** All chips (VIC-II, SID, CIA) advance cycles and handle I/O using a shared bus state structure.
- **Macro-based handler declarations:** Central macro for opcode handler signatures and a centralized FOOTER macro for future-proofing handler changes.
- **Register-based calling conventions:** For performance, especially in handler and bus interfaces.
- **System tick functions:** All chips advance timing and interrupt state in a single tick, with bus state passed through.
- **Feature flags for incremental rollout:** Allow toggling between legacy and redesign logic.

## 2. Migration Status (Updated January 2025)

**Successfully completed:**
- ✅ **Unified bus state threading**: Complete [`bus_state_t`](../../core/bus_cycle_interface.h:1) implementation across all layers
- ✅ **Bus layer refactoring**: Full switch-based chip dispatch implemented in [`c64_bus_cpu_read()`](../../systems/c64/c64_bus.c:79) and [`c64_bus_cpu_write()`](../../systems/c64/c64_bus.c:132)
- ✅ **Encoded chip select arrays**: Implemented with [`encode_chip_rw()`](../../systems/c64/c64_bus.h:204) and [`decode_read_chip()`](../../systems/c64/c64_bus.h:211) / [`decode_write_chip()`](../../systems/c64/c64_bus.h:215)
- ✅ **PLA integration**: Complete implementation with [`c64_bus_generate_all_pla_modes()`](../../systems/c64/c64_bus.c:379) generating all 32 memory modes
- ✅ **Adapter interfaces**: Full adapter pattern implementation for CPU integration via [`c64_bus_init_adapters()`](../../systems/c64/c64_bus.c:490)
- ✅ **Project builds**: All code compiles successfully with new architecture

**Partially completed:**
- 🔶 **CPU opcode handlers**: Standard function call dispatch implemented in [`fam65xx_next_instruction_dispatch()`](../../chip/cpu/fam65xx/fam65xx_core.h:251), but missing stackless threaded execution
- 🔶 **Macro-based signatures**: [`FAM65XX_OPCODE_PROTO()`](../../chip/cpu/fam65xx/fam65xx_core.h:305) macro exists but only configured for legacy mode (not redesign mode)
- 🔶 **FOOTER macros**: [`FAM65XX_OPCODE_FOOTER()`](../../chip/cpu/fam65xx/fam65xx_core.h:318) implemented but not using return-based dispatch

**Not yet implemented:**
- ❌ **Stackless threaded CPU dispatch**: Current implementation uses traditional function calls rather than Nostradamus pattern
- ❌ **Return-based handler dispatch**: Handlers use [`FAM65XX_NEXT_INSTRUCTION()`](../../chip/cpu/fam65xx/fam65xx_core.h:295) instead of returning next handler pointer
- ❌ **REDESIGN mode activation**: Code has `#ifdef REDESIGN` branches but they're not enabled
- ❌ **Comprehensive testing**: Limited test coverage for new architecture components
- ❌ **Performance benchmarking**: No comparative performance analysis conducted
- ❌ **Documentation updates**: Code comments still reference legacy architecture patterns

## 3. Migration Steps

### Step 1: Preparation
- Map current bus, chip, and CPU layers to redesign patterns.
- Document existing implementations and identify refactoring targets.
- Set up feature flags for toggling new/legacy code.

### Step 2: Bus Layer Refactoring
- Refactor ACID-based chip select logic to use encoded, branchless chip select arrays.
- Integrate PLA logic with new chip select arrays and mode switching.
- Update bus access points to use unified system tick and cycle functions.

### Step 3: Chip Layer Refactoring
- Ensure all chip cycle and I/O functions use unified bus state threading.
- Integrate chip advance cycle functions for proper timing and interrupt handling.

### Step 4: CPU Layer Refactoring
- Refactor opcode handlers to use macro-based signatures and centralized FOOTER macros.
- Implement stackless threaded dispatch loop (Nostradamus pattern).
- Update handler table and dispatch logic for consistency with redesign.

### Step 5: System Integration
- Update system initialization and cleanup to use new bus and chip structures.
- Migrate cartridge and PLA integration to use new signal and mode management.
- Refactor legacy code to use new interfaces.

### Step 6: Testing & Validation
- Expand and modernize unit and integration tests for bus, chip, and CPU layers.
- Validate chip selection, PLA mode switching, memory access, and cycle timing.
- Benchmark performance before and after migration.

### Step 7: Documentation & Training
- Update code comments, README, and developer docs.
- Provide migration guides and best practices for contributors.

### Step 8: Incremental Rollout
- Migrate in stages: bus layer, then chips, then CPU.
- Maintain legacy compatibility during transition.
- Flag new code for easy toggling with macros and feature flags.
- Gradually phase out legacy code as new modules are validated.

## 4. Macro Usage Example

**Handler Declaration Macro:**
```c
#define OPCODE_HANDLER_PROTO(name) REGISTER_CALL void* name(cpu_state_t* cpu, bus_state_t* bus_state)
```

**FOOTER Macro:**
```c
#define OPCODE_FOOTER(cpu, bus) return get_next_handler(cpu, bus)
```

**Usage in Handler:**
```c
OPCODE_HANDLER_PROTO(cpu_lda_abs) {
    // ... handler code ...
    OPCODE_FOOTER(cpu, bus);
}
```

## 5. Working List for Remaining Tasks

### High Priority - Core Architecture Completion

1. **Enable REDESIGN mode and implement stackless threaded dispatch**
   - Activate `#ifdef REDESIGN` branches in [`fam65xx_core.h`](../../chip/cpu/fam65xx/fam65xx_core.h:287)
   - Refactor [`FAM65XX_OPCODE_PROTO()`](../../chip/cpu/fam65xx/fam65xx_core.h:305) to return `void*` instead of `void`
   - Update [`FAM65XX_OPCODE_FOOTER()`](../../chip/cpu/fam65xx/fam65xx_core.h:318) to return next handler pointer
   - Implement `get_next_handler()` function for Nostradamus pattern
   - Update all opcode handlers in [`fam65xx_arithmetic.inc`](../../chip/cpu/fam65xx/fam65xx_arithmetic.inc:1), [`fam65xx_control.inc`](../../chip/cpu/fam65xx/fam65xx_control.inc:1), etc.

2. **Complete CPU handler table migration**
   - Convert all 256 opcode handlers to use return-based dispatch
   - Ensure register-based calling conventions for performance
   - Validate that inline assembly optimizations work correctly

3. **Integrate redesign reference architecture**
   - Merge optimizations from [`c64_bus_implementation.c`](../c64_bus_implementation.c:1) into main codebase
   - Apply `FORCE_INLINE` and `REGISTER_CALL` optimizations
   - Implement ultra-fast chip selection with branchless I/O detection

### Medium Priority - Testing and Validation

4. **Expand test coverage**
   - Create unit tests for new bus state threading
   - Add integration tests for PLA mode switching
   - Implement cycle-accurate timing tests
   - Validate chip selection behavior across all 32 PLA modes

5. **Performance benchmarking**
   - Compare legacy vs redesign performance
   - Measure impact of stackless dispatch
   - Profile memory access patterns
   - Validate register calling convention benefits

### Low Priority - Documentation and Cleanup

6. **Update documentation**
   - Revise code comments to reflect new architecture
   - Update README with redesign benefits
   - Create migration guide for contributors
   - Document performance improvements

7. **Code cleanup**
   - Remove legacy compatibility code
   - Simplify build configuration
   - Clean up temporary debugging code

## 6. Architectural Differences Between Current and Target

### Current Implementation (Legacy Mode)
- **CPU Dispatch**: Function call-based with stack growth per instruction
- **Handler Signature**: `void handler(fam65xx_t* cpu)`
- **Next Instruction**: Recursive calls via `FAM65XX_NEXT_INSTRUCTION()`
- **Bus Interface**: Adapter pattern with function pointers
- **Chip Access**: Switch-based dispatch (implemented)
- **PLA Integration**: Complete with encoded arrays (implemented)

### Target Implementation (Redesign Mode)
- **CPU Dispatch**: Stackless threaded execution (Nostradamus pattern)
- **Handler Signature**: `REGISTER_CALL void* handler(fam65xx_t* cpu, bus_state_t* bus_state)`
- **Next Instruction**: Return pointer to next handler function
- **Bus Interface**: Direct integration with unified bus state
- **Chip Access**: Branchless I/O detection with ultra-fast selection
- **System Ticking**: Combined chip advance cycle with bus state threading

### Key Benefits of Target Architecture
- **Performance**: 0.5-1 cycle faster per instruction due to stackless execution
- **Memory**: Reduced stack pressure and better cache locality
- **Maintainability**: Unified bus state threading across all components
- **Flexibility**: Register-based calling conventions for optimal performance

## 7. Migration Checklist (Updated)
- [x] ✅ Redesign files provide reference architecture and techniques
- [x] ✅ Main project bus layer refactored to use encoded chip select arrays and unified bus state threading
- [x] ✅ Main project chip layer unified for bus state threading and cycle advancement (VIC-II, SID, CIA)
- [x] ✅ Macro-based handler signatures and centralized FOOTER macros applied to opcode handlers (legacy mode)
- [x] ✅ Global replacement of bus_cycle_t with bus_state_t completed
- [x] ✅ c64_bus_s struct refactored to use bus_state_t state field
- [x] ✅ All code updated to use new bus state threading; no lingering references to legacy fields
- [x] ✅ Project builds after migration and refactoring
- [x] ✅ PLA logic fully integrated with encoded chip select arrays
- [x] ✅ Adapter interfaces implemented for CPU integration
- [ ] ❌ Enable REDESIGN mode and activate stackless threaded CPU dispatch
- [ ] ❌ Complete macro-based handler signatures for redesign mode (return-based)
- [ ] ❌ Implement get_next_handler() function for Nostradamus pattern
- [ ] ❌ Update all 256 opcode handlers to use return-based dispatch
- [ ] ❌ Integrate ultra-fast optimizations from redesign reference
- [ ] ❌ Comprehensive unit/integration tests for new architecture
- [ ] ❌ Performance benchmarking and validation against reference test suites
- [ ] ❌ Documentation and code comments updated to reflect redesign architecture

## 8. Next Actions (Immediate Priority)

### Phase 1: Enable Stackless Threaded Dispatch (1-2 days)
1. **Activate REDESIGN mode**
   - Define `REDESIGN` preprocessor symbol in build configuration
   - Verify all `#ifdef REDESIGN` branches activate correctly

2. **Update CPU handler macros**
   - Modify `FAM65XX_OPCODE_PROTO()` to return `void*` with `bus_state_t*` parameter
   - Implement `get_next_handler()` function for fetching next opcode handler
   - Update `FAM65XX_OPCODE_FOOTER()` to return next handler pointer

3. **Convert core opcode handlers**
   - Start with arithmetic operations in `fam65xx_arithmetic.inc`
   - Update control flow operations in `fam65xx_control.inc`
   - Ensure all handlers use return-based dispatch

### Phase 2: Integration and Optimization (2-3 days)
4. **Merge redesign optimizations**
   - Integrate branchless I/O detection from reference implementation
   - Apply `REGISTER_CALL` optimizations for performance
   - Implement ultra-fast chip selection patterns

5. **Validate and test**
   - Run basic functionality tests
   - Verify PLA mode switching still works correctly
   - Check interrupt handling and timing accuracy

### Phase 3: Testing and Documentation (1-2 days)
6. **Comprehensive testing**
   - Create performance benchmarks comparing legacy vs redesign
   - Add unit tests for new dispatch mechanism
   - Validate cycle-accurate behavior

7. **Documentation updates**
   - Update code comments to reflect new architecture
   - Create developer migration guide
   - Document performance improvements achieved

## 9. Success Criteria

The migration will be considered complete when:
- ✅ All 256 CPU opcode handlers use stackless threaded dispatch
- ✅ Performance benchmarks show 0.5-1 cycle improvement per instruction
- ✅ All existing functionality remains intact (no regressions)
- ✅ Code builds cleanly with REDESIGN mode enabled
- ✅ Comprehensive test suite validates new architecture
- ✅ Documentation reflects the completed redesign

## 10. Estimated Timeline

**Total estimated effort: 4-7 days**
- Phase 1 (Core dispatch): 1-2 days
- Phase 2 (Integration): 2-3 days
- Phase 3 (Testing/docs): 1-2 days

The migration is approximately **85% complete**, with the major architectural components successfully implemented. The remaining work focuses on enabling the final performance optimizations and completing the CPU dispatch refactoring.