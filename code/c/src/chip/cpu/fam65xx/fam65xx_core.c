#include "fam65xx_core.h"
#include "../../../core/system.h"
#include <string.h>  // For memcpy

// ============================================================================
// SHARED MOS 6502 FAMILY CORE IMPLEMENTATION
// ============================================================================

// Core memory and cycle functions (shared by all family members)
uint8_t fam65xx_read_cycle(fam65xx_t* cpu, uint16_t address) {
    cpu->bus_interface.cycle_tick(cpu->bus_interface.context);  // Bus cycle
    return cpu->bus_interface.bus_read(cpu->bus_interface.context, address);
}

void fam65xx_write_cycle(fam65xx_t* cpu, uint16_t address, uint8_t value) {
    cpu->bus_interface.cycle_tick(cpu->bus_interface.context);  // Bus cycle
    cpu->bus_interface.bus_write(cpu->bus_interface.context, address, value);
}

// Stack operations (shared)
void fam65xx_push(fam65xx_t* cpu, uint8_t value) {

    fam65xx_write_cycle(cpu, 0x0100 | cpu->sp, value);  // Bus write cycle
    cpu->sp--;
}

uint8_t fam65xx_pull(fam65xx_t* cpu) {
    (void)fam65xx_read_cycle(cpu, 0x0100 | cpu->sp);  // T1: Dummy read
    cpu->sp++;
    return fam65xx_read_cycle(cpu, 0x0100 | cpu->sp);  // T2: Stack read
}

// Interrupt handling (shared)
void fam65xx_interrupt_sequence(fam65xx_t* cpu, uint8_t status_flags, uint16_t vector_addr) {
    // Push program counter (high byte first)
    fam65xx_push(cpu, (cpu->pc >> 8) & 0xFF);
    fam65xx_push(cpu, cpu->pc & 0xFF);
    // Push status register with specified flags
    fam65xx_push(cpu, status_flags | FLAG_U); // Always set unused flag
      // Set interrupt disable flag
    fam65xx_set_flag(cpu, FLAG_I, true);
    
    // Load interrupt vector

    uint8_t addr_lo = fam65xx_read_cycle(cpu, vector_addr);  // Bus read cycle

    uint8_t addr_hi = fam65xx_read_cycle(cpu, vector_addr + 1);  // Bus read cycle
    
    cpu->pc = (addr_hi << 8) | addr_lo;
}

void fam65xx_interrupt_handler(fam65xx_t* cpu) {
    // Check for NMI first (higher priority)
    if (FAM65XX_TEST_NMI(cpu)) {
        fam65xx_interrupt_sequence(cpu, cpu->p, 0xFFFA); // NMI vector
        return;
    }
    
    // Check for IRQ (if not masked)
    if (FAM65XX_TEST_IRQ(cpu) && !fam65xx_get_flag(cpu, FLAG_I)) {
        fam65xx_interrupt_sequence(cpu, cpu->p, 0xFFFE); // IRQ vector
        return;
    }
}

// Interception support (shared) - proper handler replacement mechanism
void fam65xx_intercept_stub(fam65xx_t* cpu) {
    // This stub is hit when threaded dispatch tries to execute the next instruction
    // Restore the original handlers and stop interception
    if (fam65xx_is_intercepting(cpu)) {
        fam65xx_stop_intercept(cpu);
    }
    // Decrement PC since it was incremented during opcode fetch for the next instruction
    cpu->pc--;
    // Threaded dispatch ends here - this completes the single step
}

bool fam65xx_is_intercepting(fam65xx_t* cpu) {
    return cpu ? cpu->opcode_handlers[0] == fam65xx_intercept_stub : false;
}

void fam65xx_start_intercept(fam65xx_t* cpu) {
    if (!cpu) return;
    
    if (fam65xx_is_intercepting(cpu)) return;

    // Save current handlers and replace all with intercept stubs
    memcpy(cpu->saved_opcode_handlers, cpu->opcode_handlers, sizeof(cpu->opcode_handlers));
    for (int i = 0; i < 256; i++) {
        cpu->opcode_handlers[i] = fam65xx_intercept_stub;
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
    if (!cpu) return false;
    
    if (fam65xx_is_intercepting(cpu)) return false; // Cannot step while intercepting
    
    // Single step implementation using interception mechanism
    // This ensures only one instruction executes before returning control

    // Fetch the opcode and get the real handler BEFORE starting interception
    // Do the opcode fetch without performing an extra bus cycle_tick as would
    // be done by fam65xx_read_cycle, because during stepping the preceding opcode
    // fetch (as done by FAM65XX_OPCODE_FOOTER) already performed the cycle_tick.
    uint8_t opcode = cpu->bus_interface.bus_read(cpu->bus_interface.context, cpu->pc++);
    fam65xx_opcode_handler_t handler = cpu->opcode_handlers[opcode];
    
    // Start interception to catch the next instruction after this one
    fam65xx_start_intercept(cpu);
    
    // Execute the actual instruction handler
    // The handler will call the footer macro which does threaded dispatch to the next instruction
    // Since interception is active, the next instruction will be the intercept stub
    // The intercept stub will automatically restore handlers and stop interception
    handler(cpu);
    
    // The intercept stub has already restored handlers, so we're done
    return true;
}
