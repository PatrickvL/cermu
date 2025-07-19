
Migration Plan: Integrating code/c/src/redesign into Existing Project

Overview
The redesign in code/c/src/redesign introduces:

Highly optimized bus and chip selection logic
Register-based calling conventions for performance
Threaded, stackless CPU dispatch (Nostradamus Distributor pattern)
Unified bus state threading for cycle-accurate emulation

This migration plan outlines steps to transition from the legacy codebase to the new architecture, minimizing disruption and maximizing maintainability and performance.

Step 1: CPU Layer Migration
Begin migration with the CPU layer to establish the new stackless, threaded opcode dispatch model:

- Refactor opcode handlers to return the next handler instead of calling it (Nostradamus Distributor pattern).
- Use a shared bus state pointer for all handlers.
- Update handler tables and dispatch logic to use a centralized handler table and loop.
- Ensure interrupt and flow control logic matches the new pattern.
- Migrate register calling conventions for performance-critical functions.
- Update function pointer types and handler signatures.

**Macro Recommendation:**
Implement a macro for declaring opcode handler signatures, e.g.:
    #define OPCODE_HANDLER_PROTO(name) REGISTER_CALL void* name(cpu_state_t* cpu, aiemu_bus_state_t* bus_state)
Use this macro for all handler declarations and definitions. Future changes to the handler signature require only macro modification, not updates to all 256 handlers.

**FOOTER Macro Repurposing:**
Repurpose the existing FOOTER macro to support the new stackless dispatch pattern, e.g.:
    #define OPCODE_FOOTER(cpu, bus) return get_next_handler(cpu, bus)
Use this macro at the end of each handler for consistency and future-proofing.

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
[ ] Integrate new bus structures and chip select logic.
[ ] Refactor chip implementations for bus state threading.
[ ] Redefine opcode handler signatures using macros.
[ ] Refactor CPU dispatch for stackless execution.
[ ] Update system integration and PLA/cartridge logic.
[ ] Test and validate each migration step.
[ ] Document changes and train contributors.
[ ] Roll out incrementally with feature flags.
5. Conclusion
Migrating to the redesign will:

Improve performance and maintainability.
Enable stackless, cycle-accurate emulation.
Simplify future redesigns via macro-based handler declarations and centralized FOOTER logic.
Next Steps:

Begin with bus layer migration, then chips, then CPU. Validate each step and document as you go.

For further details or code-by-code mapping, request a deep-dive migration guide.