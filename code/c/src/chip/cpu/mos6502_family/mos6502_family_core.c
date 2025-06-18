#include "mos6502_family_core.h"
#include "../../../core/system.h"

// ============================================================================
// SHARED MOS 6502 FAMILY CORE IMPLEMENTATION
// ============================================================================

// Core memory and cycle functions (shared by all family members)
uint8_t mos6502_family_read_cycle(mos6502_family_t* cpu, uint16_t address) {
    return cpu->bus_interface.bus_read(cpu->bus_interface.context, address);
}

void mos6502_family_write_cycle(mos6502_family_t* cpu, uint16_t address, uint8_t value) {
    cpu->bus_interface.bus_write(cpu->bus_interface.context, address, value);
}

void mos6502_family_opcode_dispatch(mos6502_family_t* cpu, uint8_t opcode) {
    // All opcodes have handlers - no NULL check needed for performance
    cpu->opcode_handlers[opcode](cpu);
}

// Stack operations (shared)
void mos6502_family_push(mos6502_family_t* cpu, uint8_t value) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    mos6502_family_write_cycle(cpu, 0x0100 | cpu->sp, value);
    cpu->sp--;
}

uint8_t mos6502_family_pull(mos6502_family_t* cpu) {
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    (void)mos6502_family_read_cycle(cpu, 0x0100 | cpu->sp); // Dummy read
    cpu->sp++;
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    return mos6502_family_read_cycle(cpu, 0x0100 | cpu->sp);
}

// Interrupt handling (shared)
void mos6502_family_interrupt_sequence(mos6502_family_t* cpu, uint8_t status_flags, uint16_t vector_addr) {
    // Push program counter (high byte first)
    mos6502_family_push(cpu, (cpu->pc >> 8) & 0xFF);
    mos6502_family_push(cpu, cpu->pc & 0xFF);
    // Push status register with specified flags
    mos6502_family_push(cpu, status_flags | FLAG_U); // Always set unused flag
      // Set interrupt disable flag
    mos6502_family_set_flag(cpu, FLAG_I, true);
    
    // Load interrupt vector
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, vector_addr);
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, vector_addr + 1);
    
    cpu->pc = (addr_hi << 8) | addr_lo;
}

void mos6502_family_interrupt_handler(mos6502_family_t* cpu) {
    // Check for NMI first (higher priority)
    if (M6502_TEST_NMI(cpu)) {
        mos6502_family_interrupt_sequence(cpu, cpu->p, 0xFFFA); // NMI vector
        return;
    }
    
    // Check for IRQ (if not masked)
    if (M6502_TEST_IRQ(cpu) && !mos6502_family_get_flag(cpu, FLAG_I)) {
        mos6502_family_interrupt_sequence(cpu, cpu->p, 0xFFFE); // IRQ vector
        return;
    }
    
    // No interrupt - continue with next instruction
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
}

// Interception support (shared)
void mos6502_family_start_intercept(mos6502_family_t* cpu) {
    cpu->intercepting = true;
}

void mos6502_family_stop_intercept(mos6502_family_t* cpu) {
    cpu->intercepting = false;
}

bool mos6502_family_is_intercepting(mos6502_family_t* cpu) {
    return cpu->intercepting;
}

// Single step execution (shared)
bool mos6502_family_step(mos6502_family_t* cpu) {
    if (!cpu) return false;
    
    // Check if ready line is asserted (for RDY support)
    if (!M6502_TEST_RDY(cpu)) {
        return false; // CPU is halted
    }
    
    // Handle interrupts first
    if (M6502_TEST_IRQ(cpu) || M6502_TEST_NMI(cpu)) {
        mos6502_family_interrupt_handler(cpu);
        return true;
    }
    
    // Fetch and execute next instruction
    MOS6502_FAMILY_INTRA_CYCLE(cpu);
    uint8_t opcode = mos6502_family_read_cycle(cpu, cpu->pc++);
    mos6502_family_opcode_dispatch(cpu, opcode);
    
    return true;
}
