#include "fam65xx_core.h"
#include "../../../core/system.h"
#include <string.h>  // For memcpy
#include <stdio.h>   // For printf (debug)

// ============================================================================
// SHARED MOS 6502 FAMILY CORE IMPLEMENTATION
// ============================================================================

// Interrupt handler - called when IRQ or NMI lines are active
void fam65xx_interrupt_handler(fam65xx_t* cpu) {
    // Check for NMI first (higher priority)
    if (FAM65XX_TEST_NMI(cpu)) {
        fam65xx_nmi(cpu); // NMI handler
        return;
    }
    // Check for IRQ (if not masked)
    if (FAM65XX_TEST_IRQ(cpu) && !fam65xx_get_flag(cpu, FLAG_I)) {
        fam65xx_irq(cpu); // IRQ handler
        return;
    }
}

// Interception support (shared) - proper handler replacement mechanism
FAM65XX_OPCODE_PROTO(fam65xx_op_intercept_stub) {
    printf("fam65xx_op_intercept_stub: INTERCEPT HIT!\n");
    // This stub is hit when threaded dispatch tries to execute the next instruction
    // Restore the original handlers and stop interception
    if (fam65xx_is_intercepting(cpu)) {
        printf("fam65xx_op_intercept_stub: Stopping interception\n");
        fam65xx_stop_intercept(cpu);
    }
    // Decrement PC since it was incremented during opcode fetch for the next instruction
    printf("fam65xx_op_intercept_stub: Decrementing PC from $%04X to $%04X\n", cpu->pc, cpu->pc - 1);
    cpu->pc--;
    // Threaded dispatch ends here - this completes the single step
    printf("fam65xx_op_intercept_stub: Single step completed\n");
}

bool fam65xx_is_intercepting(fam65xx_t* cpu) {
    return cpu ? cpu->opcode_handlers[0] == fam65xx_op_intercept_stub : false;
}

void fam65xx_start_intercept(fam65xx_t* cpu) {
    if (!cpu) return;
    
    if (fam65xx_is_intercepting(cpu)) return;

    // Save current handlers and replace all with intercept stubs
    memcpy(cpu->saved_opcode_handlers, cpu->opcode_handlers, sizeof(cpu->opcode_handlers));
    for (int i = 0; i < 256; i++) {
        cpu->opcode_handlers[i] = fam65xx_op_intercept_stub;
    }
}

void fam65xx_stop_intercept(fam65xx_t* cpu) {
    if (!cpu) return;
    
    if (!fam65xx_is_intercepting(cpu)) return;

    // Restore original handlers from saved copy
    memcpy(cpu->opcode_handlers, cpu->saved_opcode_handlers, sizeof(cpu->opcode_handlers));
}

// Single step execution (shared) - bypasses threaded dispatch for single instructions
bool fam65xx_step(fam65xx_t* cpu) {
    if (!cpu) {
        return false;
    }
    
    if (fam65xx_is_intercepting(cpu)) {
        return false; // Cannot step while intercepting
    }
    
    // To perform a single step, we use the interception mechanism. This ensures
    // that all bus interactions, including CPU stalls, are handled correctly
    // by the underlying bus implementation, as if in free-running execution.

    // --- How CPU Stalling (Cycle Stretching) Works ---
    // In a C64, the VIC-II chip can halt the CPU to take over the bus for
    // graphics rendering. It does this using two main signals:
    // 1. AEC (Address Enable Control): When VIC pulls AEC low, the CPU is
    //    disconnected from the address bus.
    // 2. BA (Bus Available, connected to the CPU's RDY pin): When VIC pulls
    //    BA low, it signals the CPU to halt on its next READ cycle.
    //
    // The `c64_bus_read_cycle` function in `c64_bus.c` calls into the main
    // system cycle handler (`c64_non_cpu_cycle`). This handler centralizes
    // all bus contention logic. It will loop, ticking the VIC, CIAs, etc.,
    // until the VIC releases the bus (AEC and/or BA/RDY lines go high),
    // accurately simulating the CPU being "frozen" while the rest of the
    // system runs. Write cycles are correctly handled by only waiting for AEC,
    // not the RDY line, as per 6502/6510 hardware behavior.

    // --- Stepping Logic ---
    // 1. We fetch the opcode for the instruction at the current PC. This read
    //    operation will correctly stall if the VIC has control of the bus.
    uint8_t opcode = cpu->bus_interface.bus_read_cycle(cpu->bus_interface.context, cpu->pc);
    
    fam65xx_opcode_handler_t handler = cpu->opcode_handlers[opcode];
    if (!handler) {
        return false;
    }

    // 2. We start interception. This replaces all 256 opcode handlers with a
    //    stub that will halt execution and restore the original handlers.
    fam65xx_start_intercept(cpu);

    // 3. We execute the real handler for the fetched opcode. All memory
    //    accesses within this handler will correctly stall if/when required.
    //    The handler finishes with a macro that tries to dispatch the *next*
    //    instruction.
    handler(cpu);

    // 4. The dispatch to the next instruction is caught by our intercept stub.
    //    The stub calls `fam65xx_stop_intercept`, restoring the real handlers
    //    and completing the single step.
    return true;
}
