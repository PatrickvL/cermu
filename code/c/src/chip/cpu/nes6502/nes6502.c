#include "nes6502.h"
#include "../mos6502_family/mos6502_family_core.h"
#include "../../../core/system.h"
#include <string.h>

// ============================================================================
// NES 6502 IMPLEMENTATION
// ============================================================================

// Forward declarations for opcode handlers
static void nes6502_init_opcode_table(nes6502_t* cpu);

// Chip interface implementation
bool nes6502_create(chip_descriptor_t* desc, nes6502_t* cpu) {
    if (!desc || !cpu) {
        return false;
    }
    
    // Initialize base 6502 family structure
    cpu->base.desc = desc;
    cpu->base.intercepting = false;
    cpu->base.system_lines = NULL;
    
    // Initialize CPU state
    cpu->base.pc = 0x0000;
    cpu->base.a = 0x00;
    cpu->base.x = 0x00;
    cpu->base.y = 0x00;
    cpu->base.sp = 0xFF;
    cpu->base.p = FLAG_U | FLAG_I; // Start with unused=1, interrupt disable=1
    cpu->base.address = 0x0000;
    
    // No CPU-specific data for NES 6502
    cpu->base.cpu_specific_data = NULL;
    
    // Initialize opcode handler table
    nes6502_init_opcode_table(cpu);
    
    return true;
}

void nes6502_destroy(nes6502_t* cpu) {
    if (!cpu) return;
    
    // Nothing special to clean up for NES 6502
    // Just clear the structure
    memset(cpu, 0, sizeof(nes6502_t));
}

void nes6502_reset(nes6502_t* cpu) {
    if (!cpu) return;
    
    // Reset CPU state to power-on defaults
    cpu->base.a = 0x00;
    cpu->base.x = 0x00;
    cpu->base.y = 0x00;
    cpu->base.sp = 0xFF;
    cpu->base.p = FLAG_U | FLAG_I; // Start with unused=1, interrupt disable=1
    
    // Load reset vector
    uint8_t addr_lo = mos6502_family_read_cycle(&cpu->base, 0xFFFC);
    uint8_t addr_hi = mos6502_family_read_cycle(&cpu->base, 0xFFFD);
    cpu->base.pc = (addr_hi << 8) | addr_lo;
    
    cpu->base.intercepting = false;
}

bool nes6502_step(nes6502_t* cpu) {
    if (!cpu) return false;
    return mos6502_family_step(&cpu->base);
}

// Configuration and setup functions
void nes6502_attach_bus(nes6502_t* cpu, const bus_cycle_ops_t* bus_interface) {
    if (cpu && bus_interface) {
        cpu->base.bus_interface = *bus_interface;
    }
}

void nes6502_attach_control_lines(nes6502_t* cpu, const control_lines_interface_t* control_interface) {
    if (cpu && control_interface) {
        cpu->base.control_interface = *control_interface;
    }
}

void nes6502_attach_system_lines(nes6502_t* cpu, system_lines_t* system_lines) {
    if (cpu) {
        cpu->base.system_lines = system_lines;
    }
}

// State access functions
uint16_t nes6502_get_pc(nes6502_t* cpu) { return cpu ? cpu->base.pc : 0; }
uint8_t nes6502_get_a(nes6502_t* cpu) { return cpu ? cpu->base.a : 0; }
uint8_t nes6502_get_x(nes6502_t* cpu) { return cpu ? cpu->base.x : 0; }
uint8_t nes6502_get_y(nes6502_t* cpu) { return cpu ? cpu->base.y : 0; }
uint8_t nes6502_get_sp(nes6502_t* cpu) { return cpu ? cpu->base.sp : 0; }
uint8_t nes6502_get_p(nes6502_t* cpu) { return cpu ? cpu->base.p : 0; }

void nes6502_set_pc(nes6502_t* cpu, uint16_t value) { if (cpu) cpu->base.pc = value; }
void nes6502_set_a(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.a = value; }
void nes6502_set_x(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.x = value; }
void nes6502_set_y(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.y = value; }
void nes6502_set_sp(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.sp = value; }
void nes6502_set_p(nes6502_t* cpu, uint8_t value) { if (cpu) cpu->base.p = value; }

// Memory access functions
uint8_t nes6502_read_memory(nes6502_t* cpu, uint16_t address) {
    return cpu ? mos6502_family_read_cycle(&cpu->base, address) : 0;
}

void nes6502_write_memory(nes6502_t* cpu, uint16_t address, uint8_t value) {
    if (cpu) {
        mos6502_family_write_cycle(&cpu->base, address, value);
    }
}

// Test and debug support
void nes6502_start_intercept(nes6502_t* cpu) {
    if (cpu) mos6502_family_start_intercept(&cpu->base);
}

void nes6502_stop_intercept(nes6502_t* cpu) {
    if (cpu) mos6502_family_stop_intercept(&cpu->base);
}

bool nes6502_is_intercepting(nes6502_t* cpu) {
    return cpu ? mos6502_family_is_intercepting(&cpu->base) : false;
}

// ============================================================================
// NES 6502 OPCODE HANDLERS (No Decimal Mode)
// ============================================================================

// ADC without decimal mode support (NES characteristic)
static void nes6502_adc(nes6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    mos6502_family_t* base = &cpu->base;
    bool carry_in = mos6502_family_get_flag(base, FLAG_C);
    
    // NES 6502: Always binary mode, decimal flag is ignored
    uint16_t result = base->a + operand + (carry_in ? 1 : 0);
    
    mos6502_family_set_flag(base, FLAG_C, result > 0xFF);
    mos6502_family_set_flag(base, FLAG_Z, (result & 0xFF) == 0);
    mos6502_family_set_flag(base, FLAG_N, (result & 0x80) != 0);
    
    // V flag: overflow if both inputs have same sign, but result has different sign
    bool overflow = ((base->a ^ (result & 0xFF)) & (operand ^ (result & 0xFF)) & 0x80) != 0;
    mos6502_family_set_flag(base, FLAG_V, overflow);
    
    base->a = result & 0xFF;
}

// SBC without decimal mode support (NES characteristic)
static void nes6502_sbc(nes6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    // SBC is just ADC with the operand inverted and carry inverted
    mos6502_family_t* base = &cpu->base;
    bool carry_in = mos6502_family_get_flag(base, FLAG_C);
    
    // Flip carry for subtraction
    mos6502_family_set_flag(base, FLAG_C, !carry_in);
    
    // Perform ADC with inverted operand (binary mode only)
    nes6502_adc(cpu, ~operand);
}

// Basic instruction handlers (examples)
static void nes6502_brk(nes6502_t* cpu) {
    mos6502_family_t* base = &cpu->base;
    
    // BRK instruction: push PC+2, push status with B flag set, jump to IRQ vector
    base->pc++; // Skip the signature byte
    
    // Push return address (PC+1)
    mos6502_family_push(base, (base->pc >> 8) & 0xFF);
    mos6502_family_push(base, base->pc & 0xFF);
    
    // Push status register with B flag set
    mos6502_family_push(base, base->p | FLAG_B);
    
    // Set interrupt disable flag
    mos6502_family_set_flag(base, FLAG_I, true);
    
    // Jump to IRQ vector
    uint8_t addr_lo = mos6502_family_read_cycle(base, 0xFFFE);
    uint8_t addr_hi = mos6502_family_read_cycle(base, 0xFFFF);
    base->pc = (addr_hi << 8) | addr_lo;
}

static void nes6502_nop(nes6502_t* cpu) {
    // No operation - just continue to next instruction
    (void)cpu; // Suppress unused parameter warning
}

// Initialize the opcode handler table
static void nes6502_init_opcode_table(nes6502_t* cpu) {
    if (!cpu) return;
    
    // Initialize all opcodes to NULL (will be handled as NOPs)
    for (int i = 0; i < 256; i++) {
        cpu->base.opcode_handlers[i] = NULL;
    }
    
    // Set up basic opcodes (more would be added for complete implementation)
    cpu->base.opcode_handlers[0x00] = (mos6502_family_opcode_handler_t)nes6502_brk;
    cpu->base.opcode_handlers[0xEA] = (mos6502_family_opcode_handler_t)nes6502_nop;
    
    // TODO: Add complete opcode table for all 256 instructions
    // Key difference from standard 6502: ADC/SBC always work in binary mode
}
