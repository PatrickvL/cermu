#include "mos6502.h"
#include "../../../core/system.h"

// ============================================================================
// MOS 6502 IMPLEMENTATION
// ============================================================================

// Forward declarations for opcode handlers
static void mos6502_init_opcode_table(mos6502_t* cpu);

// Chip interface implementation
bool mos6502_create(chip_descriptor_t* desc, mos6502_t* cpu) {
    if (!desc || !cpu) {
        return false;
    }
      // Initialize base MOS 6502 family structure
    cpu->base.desc = desc;
    cpu->base.intercepting = false;
    cpu->base.system_lines = NULL;
    
    // Initialize CPU state
    cpu->base.pc = 0x0000;
    cpu->base.a = 0x00;
    cpu->base.x = 0x00;
    cpu->base.y = 0x00;
    cpu->base.sp = 0xFF;
    cpu->base.p = MOS6502_FLAG_U | MOS6502_FLAG_I; // Start with unused=1, interrupt disable=1
    cpu->base.address = 0x0000;
    
    // No CPU-specific data for standard MOS 6502
    cpu->base.cpu_specific_data = NULL;
    
    // Initialize opcode handler table
    mos6502_init_opcode_table(cpu);
    
    return true;
}

void mos6502_destroy(mos6502_t* cpu) {
    if (!cpu) return;
    
    // Nothing special to clean up for standard 6502
    // Just clear the structure
    memset(cpu, 0, sizeof(mos6502_t));
}

void mos6502_reset(mos6502_t* cpu) {
    if (!cpu) return;
    
    // Reset CPU state to power-on defaults
    cpu->base.a = 0x00;
    cpu->base.x = 0x00;
    cpu->base.y = 0x00;
    cpu->base.sp = 0xFF;    cpu->base.p = MOS6502_FLAG_U | MOS6502_FLAG_I; // Start with unused=1, interrupt disable=1
    
    // Load reset vector
    uint8_t addr_lo = mos6502_read_cycle(&cpu->base, 0xFFFC);
    uint8_t addr_hi = mos6502_read_cycle(&cpu->base, 0xFFFD);
    cpu->base.pc = (addr_hi << 8) | addr_lo;
    
    cpu->base.intercepting = false;
}

bool mos6502_step(mos6502_t* cpu) {
    if (!cpu) return false;
    return mos6502_step(&cpu->base);
}

// Configuration and setup functions
void mos6502_attach_bus(mos6502_t* cpu, const bus_cycle_ops_t* bus_interface) {
    if (cpu && bus_interface) {
        cpu->base.bus_interface = *bus_interface;
    }
}

void mos6502_attach_control_lines(mos6502_t* cpu, const control_lines_interface_t* control_interface) {
    if (cpu && control_interface) {
        cpu->base.control_interface = *control_interface;
    }
}

void mos6502_attach_system_lines(mos6502_t* cpu, system_lines_t* system_lines) {
    if (cpu) {
        cpu->base.system_lines = system_lines;
    }
}

// State access functions
uint16_t mos6502_get_pc(mos6502_t* cpu) { return cpu ? cpu->base.pc : 0; }
uint8_t mos6502_get_a(mos6502_t* cpu) { return cpu ? cpu->base.a : 0; }
uint8_t mos6502_get_x(mos6502_t* cpu) { return cpu ? cpu->base.x : 0; }
uint8_t mos6502_get_y(mos6502_t* cpu) { return cpu ? cpu->base.y : 0; }
uint8_t mos6502_get_sp(mos6502_t* cpu) { return cpu ? cpu->base.sp : 0; }
uint8_t mos6502_get_p(mos6502_t* cpu) { return cpu ? cpu->base.p : 0; }

void mos6502_set_pc(mos6502_t* cpu, uint16_t value) { if (cpu) cpu->base.pc = value; }
void mos6502_set_a(mos6502_t* cpu, uint8_t value) { if (cpu) cpu->base.a = value; }
void mos6502_set_x(mos6502_t* cpu, uint8_t value) { if (cpu) cpu->base.x = value; }
void mos6502_set_y(mos6502_t* cpu, uint8_t value) { if (cpu) cpu->base.y = value; }
void mos6502_set_sp(mos6502_t* cpu, uint8_t value) { if (cpu) cpu->base.sp = value; }
void mos6502_set_p(mos6502_t* cpu, uint8_t value) { if (cpu) cpu->base.p = value; }

// Memory access functions
uint8_t mos6502_read_memory(mos6502_t* cpu, uint16_t address) {
    return cpu ? mos6502_read_cycle(&cpu->base, address) : 0;
}

void mos6502_write_memory(mos6502_t* cpu, uint16_t address, uint8_t value) {
    if (cpu) {
        mos6502_write_cycle(&cpu->base, address, value);
    }
}

// Test and debug support
void mos6502_start_intercept(mos6502_t* cpu) {
    if (cpu) mos6502_start_intercept(&cpu->base);
}

void mos6502_stop_intercept(mos6502_t* cpu) {
    if (cpu) mos6502_stop_intercept(&cpu->base);
}

bool mos6502_is_intercepting(mos6502_t* cpu) {
    return cpu ? mos6502_is_intercepting(&cpu->base) : false;
}

// ============================================================================
// DECIMAL MODE ARITHMETIC (Full 6502 Support)
// ============================================================================

void mos6502_adc(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    mos6502_family_t* base = &cpu->base;
    bool carry_in = mos6502_get_flag(base, MOS6502_FLAG_C);
    bool decimal_mode = mos6502_get_flag(base, MOS6502_FLAG_D);
    
    if (decimal_mode) {
        // Decimal mode ADC - convert to BCD
        uint8_t acc_lo = base->a & 0x0F;
        uint8_t acc_hi = (base->a >> 4) & 0x0F;
        uint8_t op_lo = operand & 0x0F;
        uint8_t op_hi = (operand >> 4) & 0x0F;
        
        // Add low nibbles
        uint8_t result_lo = acc_lo + op_lo + (carry_in ? 1 : 0);
        bool carry_to_hi = false;
        if (result_lo > 9) {
            result_lo -= 10;
            carry_to_hi = true;
        }
        
        // Add high nibbles
        uint8_t result_hi = acc_hi + op_hi + (carry_to_hi ? 1 : 0);
        bool carry_out = false;
        if (result_hi > 9) {
            result_hi -= 10;
            carry_out = true;
        }
        
        uint8_t result = (result_hi << 4) | result_lo;
          // Set flags (N and Z are set based on binary result, not BCD)
        uint16_t binary_result = base->a + operand + (carry_in ? 1 : 0);
        mos6502_set_flag(base, MOS6502_FLAG_C, carry_out);
        mos6502_set_flag(base, MOS6502_FLAG_Z, (binary_result & 0xFF) == 0);
        mos6502_set_flag(base, MOS6502_FLAG_N, (binary_result & 0x80) != 0);
        
        // V flag: set if sign changed unexpectedly in binary arithmetic
        bool overflow = ((base->a ^ result) & (operand ^ result) & 0x80) != 0;
        mos6502_set_flag(base, MOS6502_FLAG_V, overflow);
        
        base->a = result;
    } else {        // Binary mode ADC
        uint16_t result = base->a + operand + (carry_in ? 1 : 0);
        
        mos6502_set_flag(base, MOS6502_FLAG_C, result > 0xFF);
        mos6502_set_flag(base, MOS6502_FLAG_Z, (result & 0xFF) == 0);
        mos6502_set_flag(base, MOS6502_FLAG_N, (result & 0x80) != 0);
        
        // V flag: overflow if both inputs have same sign, but result has different sign
        bool overflow = ((base->a ^ (result & 0xFF)) & (operand ^ (result & 0xFF)) & 0x80) != 0;
        mos6502_set_flag(base, MOS6502_FLAG_V, overflow);
        
        base->a = result & 0xFF;
    }
}

void mos6502_sbc(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    // SBC is just ADC with the operand inverted and carry inverted
    mos6502_family_t* base = &cpu->base;
    bool carry_in = mos6502_get_flag(base, MOS6502_FLAG_C);
    
    // Flip carry for subtraction
    mos6502_set_flag(base, MOS6502_FLAG_C, !carry_in);
    
    // Perform ADC with inverted operand
    mos6502_adc(cpu, ~operand);
}

// ============================================================================
// OPCODE HANDLER STUBS (Would be filled out with complete instruction set)
// ============================================================================

// Basic instruction handlers (examples)
static void mos6502_brk(mos6502_t* cpu) {
    mos6502_family_t* base = &cpu->base;
    
    // BRK instruction: push PC+2, push status with B flag set, jump to IRQ vector
    base->pc++; // Skip the signature byte
    
    // Push return address (PC+1)
    mos6502_push(base, (base->pc >> 8) & 0xFF);
    mos6502_push(base, base->pc & 0xFF);
    
    // Push status register with B flag set
    mos6502_push(base, base->p | MOS6502_FLAG_B);
    
    // Set interrupt disable flag
    mos6502_set_flag(base, MOS6502_FLAG_I, true);
    
    // Jump to IRQ vector
    uint8_t addr_lo = mos6502_read_cycle(base, 0xFFFE);
    uint8_t addr_hi = mos6502_read_cycle(base, 0xFFFF);
    base->pc = (addr_hi << 8) | addr_lo;
}

static void mos6502_nop(mos6502_t* cpu) {
    // No operation - just continue to next instruction
    (void)cpu; // Suppress unused parameter warning
}

// Initialize the opcode handler table
static void mos6502_init_opcode_table(mos6502_t* cpu) {
    if (!cpu) return;
    
    // Initialize all opcodes to NULL (will be handled as NOPs)
    for (int i = 0; i < 256; i++) {
        cpu->base.opcode_handlers[i] = NULL;
    }
      // Set up basic opcodes (more would be added for complete implementation)
    cpu->base.opcode_handlers[0x00] = (mos6502_opcode_handler_t)mos6502_brk;
    cpu->base.opcode_handlers[0xEA] = (mos6502_opcode_handler_t)mos6502_nop;
    
    // TODO: Add complete opcode table for all 256 instructions
    // This would include all arithmetic, logical, memory, and control instructions
}
