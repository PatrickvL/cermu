#include "mos6502.h"
#include "../fam65xx/fam65xx_core.h"
#include "../../../core/system.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// OPCODE TABLE INITIALIZATION
// ============================================================================

void mos6502_init_opcode_table(mos6502_t* cpu) {
    if (!cpu) return;
    // Only set up the base table with decimal mode and illegal opcode support.
    // Do NOT override ADC/SBC handlers here; let fam65xx_init_opcode_table handle it based on the feature flag.
    uint32_t features = FAM65XX_FEATURE_DECIMAL_MODE | FAM65XX_FEATURE_ILLEGAL_OPCODES;
    fam65xx_init_opcode_table(&cpu->base, features);
}

// ============================================================================
// MOS 6502 IMPLEMENTATION
// ============================================================================


// Chip interface implementation
bool mos6502_create(chip_descriptor_t* desc, mos6502_t* cpu) {
    if (!desc || !cpu) {
        return false;
    }
    
    // Initialize base MOS 6502 family structure
    cpu->base.desc = desc;
    
    // Initialize CPU state
    cpu->base.pc = 0x0000;
    cpu->base.a = 0x00;
    cpu->base.x = 0x00;
    cpu->base.y = 0x00;
    cpu->base.sp = 0xFF;
    cpu->base.p = FLAG_U | FLAG_I; // Start with unused=1, interrupt disable=1
    cpu->base.address = 0x0000;
    
    // Initialize opcode table with MOS6502-specific handlers
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
    cpu->base.sp = 0xFF;
    cpu->base.p = FLAG_U | FLAG_I; // Start with unused=1, interrupt disable=1
    
    // Load reset vector
    uint8_t addr_lo = fam65xx_read_cycle(&cpu->base, 0xFFFC);
    uint8_t addr_hi = fam65xx_read_cycle(&cpu->base, 0xFFFD);
    cpu->base.pc = (addr_hi << 8) | addr_lo;
}

bool mos6502_step(mos6502_t* cpu) {
    if (!cpu) return false;
    return fam65xx_step(&cpu->base);
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

// DECIMAL MODE ARITHMETIC (Full 6502 Support)
void mos6502_adc(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
      fam65xx_t* base = &cpu->base;
    bool carry_in = fam65xx_get_flag(base, FLAG_C);
    bool decimal_mode = fam65xx_get_flag(base, FLAG_D);
    
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
        
        uint8_t result = (result_hi << 4) | result_lo;        // Set flags (N and Z are set based on binary result, not BCD)
        uint16_t binary_result = base->a + operand + (carry_in ? 1 : 0);
        fam65xx_set_flag(base, FLAG_C, carry_out);
        fam65xx_set_flag(base, FLAG_Z, (binary_result & 0xFF) == 0);
        fam65xx_set_flag(base, FLAG_N, (binary_result & 0x80) != 0);
        
        // V flag: set if sign changed unexpectedly in binary arithmetic
        bool overflow = ((base->a ^ result) & (operand ^ result) & 0x80) != 0;
        fam65xx_set_flag(base, FLAG_V, overflow);
        
        base->a = result;
    } else {        // Binary mode ADC
        uint16_t result = base->a + operand + (carry_in ? 1 : 0);
        
        fam65xx_set_flag(base, FLAG_C, result > 0xFF);
        fam65xx_set_flag(base, FLAG_Z, (result & 0xFF) == 0);
        fam65xx_set_flag(base, FLAG_N, (result & 0x80) != 0);
        
        // V flag: overflow if both inputs have same sign, but result has different sign
        bool overflow = ((base->a ^ (result & 0xFF)) & (operand ^ (result & 0xFF)) & 0x80) != 0;
        fam65xx_set_flag(base, FLAG_V, overflow);
        
        base->a = result & 0xFF;
    }
}

void mos6502_sbc(mos6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    // SBC is just ADC with the operand inverted and carry inverted
    fam65xx_t* base = &cpu->base;
    bool carry_in = fam65xx_get_flag(base, FLAG_C);
    
    // Flip carry for subtraction
    fam65xx_set_flag(base, FLAG_C, !carry_in);
    
    // Perform ADC with inverted operand
    mos6502_adc(cpu, ~operand);
}

// ============================================================================
// CHIP DESCRIPTOR
// ============================================================================

// Wrapper functions for chip descriptor interface
static void* mos6502_create_wrapper(chip_descriptor_t* desc) {
    mos6502_t* cpu = malloc(sizeof(mos6502_t));
    if (!cpu) return NULL;
    
    if (!mos6502_create(desc, cpu)) {
        free(cpu);
        return NULL;
    }
    return cpu;
}

static void mos6502_destroy_wrapper(void* chip) {
    if (chip) {
        mos6502_destroy((mos6502_t*)chip);
        free(chip);
    }
}

chip_descriptor_t mos6502_descriptor = {
    .description = "MOS6502 CPU with Decimal Mode",
    .create = mos6502_create_wrapper,
    .destroy = mos6502_destroy_wrapper,
    .bus_attach = NULL,
    .bank_change = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = mos6502_render_debug_window,
    .render_settings_window = NULL
#endif
};

// ============================================================================
// INTERCEPT FUNCTIONS
// ============================================================================

void mos6502_start_intercept(mos6502_t* cpu) {
    if (!cpu) return;
    fam65xx_start_intercept(&cpu->base);
}

void mos6502_stop_intercept(mos6502_t* cpu) {
    if (!cpu) return;
    fam65xx_stop_intercept(&cpu->base);
}

bool mos6502_is_intercepting(mos6502_t* cpu) {
    return cpu ? fam65xx_is_intercepting(&cpu->base) : false;
}
