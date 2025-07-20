
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

## 2. Migration Status

**Already applied in the main project:**
- Unified bus state threading (`bus_state_t` used everywhere).
- Macro-based handler signatures and centralized FOOTER macros (partially applied).
- Refactored bus and chip layers to use encoded chip select arrays and cycle advancement.
- All chip cycle and I/O functions use bus state threading.
- Feature flags and macros for toggling new/legacy logic.
- Project builds after migration and refactoring.

**Not yet fully applied:**
- Full stackless threaded CPU dispatch (Nostradamus pattern) for all opcode handlers.
- Complete macro-based handler signatures and centralized FOOTER macros for all CPU handlers.
- Comprehensive unit/integration tests for bus, chip, and CPU layers.
- Full migration of PLA logic to encoded chip select arrays in all bus access points.
- Documentation and code comments updated to reflect redesign architecture.
- Performance benchmarking and validation against reference test suites.

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

## 5. Migration Checklist

- [x] Redesign files provide reference architecture and techniques
- [x] Main project bus layer refactored to use encoded chip select arrays and unified bus state threading
- [x] Main project chip layer unified for bus state threading and cycle advancement (VIC-II, SID, CIA)
- [x] Macro-based handler signatures and centralized FOOTER macros applied to opcode handlers (partially)
- [x] Global replacement of bus_cycle_t with bus_state_t completed
- [x] c64_bus_s struct refactored to use bus_state_t state field
- [x] All code updated to use new bus state threading; no lingering references to legacy fields
- [x] Project builds after migration and refactoring
- [ ] Full stackless threaded CPU dispatch for all opcode handlers
- [ ] Complete macro-based handler signatures and centralized FOOTER macros for all CPU handlers
- [ ] Comprehensive unit/integration tests for bus, chip, and CPU layers
- [ ] Full migration of PLA logic to encoded chip select arrays in all bus access points
- [ ] Documentation and code comments updated to reflect redesign architecture
- [ ] Performance benchmarking and validation against reference test suites

## 6. Next Actions

- Refactor opcode handlers and dispatch logic for stackless execution.
- Expand and modernize test coverage.
- Finalize documentation and migration guides.
- Validate cycle accuracy, interrupt handling, and cartridge/PLA integration.