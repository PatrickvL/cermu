

Migration Plan: Integrating Redesign Concepts into the Main C64 Emulator Project (July 2025)

## Overview
This document provides a clear migration strategy for modernizing the main C64 emulator project by selectively adopting ideas and techniques from the reference implementations in `code/c/src/redesign`. The redesign files are **not** to be updated directly; they serve as inspiration only. The goal is to refactor and incrementally improve the main project, not to rewrite or replace large portions of code except where necessary (notably, chip select logic).

---

## 1. Current State of the Main Project

**Bus Layer:**
- Uses a `c64_bus_t` structure with address, data, control lines, and system lines.
- Optimized callback arrays and ACID-to-chip mappings for fast chip selection.
- PLA logic generates all 32 memory modes; cartridge EXROM/GAME signals are integrated.
- Feature flags and toggling between legacy and new logic are supported.

**CPU Layer:**
- Handler tables and function pointers for opcode dispatch.
- Macro-based handler signatures and centralized FOOTER macros are present.
- Stackless dispatch loop using shared bus state.

**Chip Layer:**
- VIC, SID, CIA chips advance cycles and I/O using shared bus state.
- Bus layer provides adapter interfaces for cycle operations and control lines.
- Non-CPU cycle functions tick all chips for proper timing and interrupt handling.

**Macros & Feature Flags:**
- Macros for handler signatures and footers are used and can be updated centrally.
- Feature flags (`#ifdef REDESIGN`) allow toggling new/legacy code.

**Testing & Build:**
- Unit and integration tests for bus, chip, and CPU layers.
- CMake build scripts and documentation are present.

**ACID vs. Chip Select:**
- ACID-based chip select logic is used for bus-to-chip mapping.
- PLA logic is integrated and can be refactored to use encoded chip select arrays.

---

## 2. Redesign Reference: Key Ideas & Techniques

- **Highly optimized, branchless chip select logic** (precomputed arrays, encoded selectors)
- **Register-based calling conventions** for performance
- **Threaded, stackless CPU dispatch** (Nostradamus Distributor pattern)
- **Unified bus state threading** for cycle-accurate emulation
- **Macro-based handler declarations** for future-proofing
- **Centralized system tick and chip cycle advancement**
- **Feature flags for incremental rollout**

---

## 3. Migration Strategy: Refactoring & Incremental Adoption

### General Principles
- **Do not rewrite or replace code unnecessarily.** Most redesign concepts are already present in the main project, sometimes in more advanced form.
- **Focus on refactoring and unification.** Only the chip select logic (ACID → encoded branchless arrays) requires a major replacement.
- **Incremental migration.** Use feature flags and macros to toggle new logic and maintain legacy compatibility during transition.

### Migration Steps

#### Step 1: Preparation
- Map current bus, chip, and CPU layers to redesign equivalents.
- Document existing implementations and identify areas for refactoring.
- Set up feature flags for toggling new/legacy code.

#### Step 2: Bus Layer Refactoring
- Refactor ACID-based chip select logic to use encoded, branchless chip select arrays as in the redesign.
- Integrate PLA logic with new chip select arrays and mode switching.
- Update bus access points to use unified system tick and cycle functions.

#### Step 3: Chip Layer Refactoring
- Ensure all chip cycle and I/O functions use unified bus state threading and match redesign prototypes.
- Integrate chip advance cycle functions for proper timing and interrupt handling.

#### Step 4: CPU Layer Refactoring
- Ensure all opcode handlers use macro-based signatures and centralized FOOTER macros.
- Confirm stackless dispatch and shared bus state usage.
- Update handler table and dispatch logic for consistency with redesign.

#### Step 5: System Integration
- Update system initialization and cleanup to use new bus and chip structures.
- Migrate cartridge and PLA integration to use new signal and mode management.
- Refactor any legacy code that interacts with bus, chips, or CPU to use new interfaces.

#### Step 6: Testing & Validation
- Expand and modernize unit and integration tests for bus, chip, and CPU layers.
- Validate chip selection, PLA mode switching, memory access, and cycle timing.
- Benchmark performance before and after migration.

#### Step 7: Documentation & Training
- Update code comments, README, and developer docs to reflect new architecture.
- Provide migration guides and best practices for contributors.

#### Step 8: Incremental Rollout
- Migrate in stages: bus layer, then chips, then CPU.
- Maintain legacy compatibility during transition.
- Flag new code for easy toggling with macros and feature flags.
- Gradually phase out legacy code as new modules are validated.

---

## 4. Migration Checklist
- [x] Redesign files provide reference architecture and techniques
- [ ] Main project bus layer refactored to use encoded chip select arrays
- [ ] Main project chip layer unified for bus state threading and cycle advancement
- [ ] Macro-based handler signatures and footers applied to all opcode handlers
- [ ] Main project CMake build and documentation updated
- [ ] System-wide validation and benchmarking in main project
- [ ] Comprehensive unit and integration tests in main project

---

## 5. Conclusion & Next Steps

Migrating to the redesign concepts will:
- Improve performance and maintainability
- Enable stackless, cycle-accurate emulation
- Simplify future redesigns via macro-based handler declarations and centralized logic

**Next Steps:**
- Use redesign files as reference only; do not update them directly
- Apply redesign ideas and techniques to main project files via refactoring
- Begin with bus layer migration (chip select logic), then chips, then CPU
- Validate each step with tests and benchmarks
- Document migration progress and any issues

For further details or code-by-code mapping, request a deep-dive migration guide.

Step 2: Bus Layer Migration
Goal: Update main project bus structures, chip select logic, and PLA integration using redesign techniques.

Step 3: Chip Layer Migration
Goal: Refactor main project chip implementations (VIC, SID, CIA) to use bus state threading and modern prototypes.

Step 4: System Integration
Goal: Integrate new bus and chip patterns into main project system initialization, cartridge, and PLA logic.

Step 5: Testing & Validation
Goal: Expand and modernize main project test coverage and benchmarking, following redesign reference patterns.

Step 6: CMake Build Integration
Goal: Ensure main project CMakeLists.txt and build documentation reflect redesign best practices.

Step 2: Bus Layer Migration
Replace legacy bus structures with c64_bus_t and related types. Integrate chip select arrays and PLA mode logic:
- Migrate PLA logic to use chip_select_per_bank_per_mode and encode_chip_select.
- Update all bus access points to use the new chip selection and system tick functions.
- Replace legacy read/write cycles with c64_bus_read_cycle and c64_bus_write_cycle.
- Ensure all chips use the unified bus state.

Step 3: Chip Layer Migration
Refactor chip implementations (VIC, SID, CIA) to use the new bus state threading:
- Update chip cycle and I/O functions to match the prototypes and calling conventions in the redesign.
- Integrate chip advance cycle functions for proper timing and interrupt handling.

Step 4: System Integration
Update system initialization and cleanup to use new bus and chip structures:
- Migrate cartridge and PLA integration to use new signal and mode management.
- Refactor any legacy code that interacts with the bus, chips, or CPU to use the new interfaces.

Step 5: Testing & Validation
Unit Test Each Layer:

Bus: Validate chip selection, PLA mode switching, and memory access.
Chips: Confirm correct cycle timing, register access, and interrupt signaling.
CPU: Ensure stackless dispatch, correct instruction execution, and interrupt handling.
Integration Test Full System:

Boot C64 ROMs and run standard test suites (Klaus, VICE, etc.).
Validate cycle accuracy and compatibility with cartridges and peripherals.
Performance Benchmarking:

Compare execution speed and memory usage before and after migration.
Step 4: Documentation & Training
Document new architecture:

Update code comments, README, and developer docs.
Provide migration guides for contributors.
Train team members:

Hold code walkthroughs and Q&A sessions.
Share best practices for threaded dispatch and bus state threading.
Step 5: Incremental Rollout
Migrate in stages:
Start with bus layer, then chips, then CPU.
Maintain legacy compatibility during transition.
Flag new code with #ifdef REDESIGN or similar for easy toggling.
Gradually phase out legacy code as new modules are validated.
Summary Table
| Area         | Legacy Approach                | Redesign Approach                | Migration Action                |
|--------------|-------------------------------|----------------------------------|---------------------------------|
| Bus          | Conditional logic, slower     | Precomputed, branchless, fast    | Replace bus structs/functions   |
| Chip         | Separate cycles, no threading | Unified bus state, threaded      | Refactor chip code              |
| CPU          | Recursive/stacked dispatch    | Stackless threaded dispatch      | Rewrite opcode handlers/dispatch|
| PLA/Cartridge| Ad-hoc logic                  | Unified mode/signals             | Integrate new PLA management    |

Conclusion
Migrating to the redesign will:

Greatly improve performance and maintainability
Enable true stackless, cycle-accurate emulation
Simplify integration of new chips and features
Next Steps:

Begin with bus layer migration, then chips, then CPU.
Validate each step with tests and benchmarks.
Document and train as you go.
If you want a more detailed checklist or code-by-code mapping, let me know!


Do write up a migration plan document please.

And in it I can already suggest that you use a macro to declare the opcode handler signatures with, so that the header can be redeclared centrally without having to modify all 256 handlers if another redesign would come to pass. Similarly, the current FOOTER macro can be redeclared for the new purpose (this would be one of the steps in the plan)




Task Completed
Migration Plan: Integrating code/c/src/redesign Architecture into Existing C64 Emulator
1. Introduction
This migration plan outlines the steps required to transition the existing C64 emulator codebase to the new architecture found in code/c/src/redesign. The redesign introduces optimized bus logic, unified chip selection, stackless threaded CPU dispatch, and macro-based handler declarations for future-proofing.

2. Migration Steps
A. Preparation
Review and Document Existing Code:
Map current bus, chip, and CPU layers to their equivalents in the redesign.
Set Up Feature Flags:
Use #ifdef REDESIGN or similar to allow toggling between legacy and new code during migration.
B. Bus Layer Migration
Replace Legacy Bus Structures:

Integrate c64_bus_t and related types from c64_bus_optimized_header.h.
Update all bus access points to use the new chip selection and system tick functions.
Integrate PLA and Chip Select Logic:

Migrate PLA logic to use chip_select_per_bank_per_mode and encode_chip_select.
Refactor memory access to use c64_bus_read_cycle and c64_bus_write_cycle.
Update System Line and Cartridge Signal Handling:

Use new system line masks and cartridge signal functions for accurate mode switching.
C. Chip Layer Migration
Refactor Chip Implementations:

Update VIC, SID, CIA, and cartridge code to use unified bus state threading.
Ensure all chip cycle and I/O functions match the prototypes and calling conventions in the redesign.
Integrate Chip Advance Cycle Functions:

Use vic_advance_cycle, sid_advance_cycle, and cia_advance_cycle for proper timing and interrupt handling.
D. CPU Layer Migration
Adopt Macro-Based Handler Declarations:

Define a macro for opcode handler signatures, e.g.:
#define OPCODE_HANDLER_PROTO(name) REGISTER_CALL void* name(cpu_state_t* cpu, c64_bus_state_t* bus_state)

c


Use this macro for all handler declarations and definitions.
Future changes to the handler signature require only macro modification.
Refactor Opcode Handlers for Stackless Dispatch:

Change handlers to return the next handler instead of calling it.
Use a shared bus state pointer for all handlers.
Update Handler Table and Dispatch Logic:

Replace legacy dispatch with the handler table and loop from c64_cpu_integration_example.c.
Ensure interrupt and flow control logic matches the new pattern.
Redefine FOOTER Macro for New Purpose:

Update the FOOTER macro to match the new stackless dispatch pattern:
#define OPCODE_FOOTER(cpu, bus) return get_next_handler(cpu, bus)

c


Use this macro at the end of each handler for consistency and future-proofing.
E. System Integration
Update Initialization and Cleanup:
Use new bus and chip structures for system setup and teardown.
Migrate Cartridge and PLA Integration:
Use new signal and mode management functions.
F. Testing & Validation
Unit Test Each Layer:
Validate chip selection, PLA mode switching, memory access, and cycle timing.
Integration Test Full System:
Boot C64 ROMs and run standard test suites.
Performance Benchmarking:
Compare execution speed and memory usage before and after migration.
G. Documentation & Training
Document New Architecture:
Update code comments, README, and developer docs.
Provide migration guides for contributors.
Train Team Members:
Hold code walkthroughs and Q&A sessions.
H. Incremental Rollout
Migrate in Stages:
Start with bus layer, then chips, then CPU.
Maintain legacy compatibility during transition.
Flag New Code for Easy Toggling:
Use macros and feature flags to switch between legacy and redesign.
3. Macro Usage Example
Handler Declaration Macro:

#define OPCODE_HANDLER_PROTO(name) REGISTER_CALL void* name(cpu_state_t* cpu, c64_bus_state_t* bus_state)

c


FOOTER Macro:

#define OPCODE_FOOTER(cpu, bus) return get_next_handler(cpu, bus)

c


Usage in Handler:

OPCODE_HANDLER_PROTO(cpu_lda_abs) {
    // ... handler code ...
    OPCODE_FOOTER(cpu, bus);
}

c


4. Migration Checklist
[x] Redesign files provide reference architecture and techniques
[ ] Main project bus, chip, and CPU layers refactored using redesign ideas
[ ] Macro-based handler signatures and footers applied to main project
[ ] Main project CMake build and documentation updated
[ ] System-wide validation and benchmarking in main project
[ ] Comprehensive unit and integration tests in main project
5. Conclusion
Migrating to the redesign will:

Improve performance and maintainability.
Enable stackless, cycle-accurate emulation.
Simplify future redesigns via macro-based handler declarations and centralized FOOTER logic.

Next Steps:
- Use redesign files as reference only; do not update them directly
- Apply redesign ideas and techniques to main project files
- Refactor main project bus, chip, and CPU layers for maintainability and performance
- Update build and test infrastructure in main project
- Document migration progress and any issues

For further details or code-by-code mapping, request a deep-dive migration guide.