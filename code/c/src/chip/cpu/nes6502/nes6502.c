#include "nes6502.h"
#include "../mos6502_family/mos6502_family_core.h"
#include "../../../core/system.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// NES 6502 IMPLEMENTATION
// ============================================================================

// Forward declarations for opcode handlers
static void nes6502_init_opcode_table(nes6502_t* cpu);
static void nes6502_adc_impl(nes6502_t* cpu, uint8_t operand);
static void nes6502_sbc_impl(nes6502_t* cpu, uint8_t operand);

// Forward declarations for addressing mode handlers
static void nes6502_adc_immediate(mos6502_family_t* cpu);
static void nes6502_adc_zero_page(mos6502_family_t* cpu);
static void nes6502_adc_zero_page_x(mos6502_family_t* cpu);
static void nes6502_adc_absolute(mos6502_family_t* cpu);
static void nes6502_adc_absolute_x(mos6502_family_t* cpu);
static void nes6502_adc_absolute_y(mos6502_family_t* cpu);
static void nes6502_adc_indirect_x(mos6502_family_t* cpu);
static void nes6502_adc_indirect_y(mos6502_family_t* cpu);
static void nes6502_sbc_immediate(mos6502_family_t* cpu);
static void nes6502_sbc_zero_page(mos6502_family_t* cpu);
static void nes6502_sbc_zero_page_x(mos6502_family_t* cpu);
static void nes6502_sbc_absolute(mos6502_family_t* cpu);
static void nes6502_sbc_absolute_x(mos6502_family_t* cpu);
static void nes6502_sbc_absolute_y(mos6502_family_t* cpu);
static void nes6502_sbc_indirect_x(mos6502_family_t* cpu);
static void nes6502_sbc_indirect_y(mos6502_family_t* cpu);

// Chip interface implementation
bool nes6502_create(chip_descriptor_t* desc, nes6502_t* cpu) {
    if (!desc || !cpu) {
        return false;
    }
    
    // Initialize base 6502 family structure
    cpu->base.desc = desc;
    cpu->base.system_lines = NULL;
    
    // Initialize CPU state
    cpu->base.pc = 0x0000;
    cpu->base.a = 0x00;
    cpu->base.x = 0x00;
    cpu->base.y = 0x00;
    cpu->base.sp = 0xFF;
    cpu->base.p = FLAG_U | FLAG_I; // Start with unused=1, interrupt disable=1
    cpu->base.address = 0x0000;
      // Initialize with complete family opcode table
    mos6502_family_init_opcode_table(&cpu->base);
    
    // NES 6502 specific overrides: Disable decimal mode behavior
    // Override all ADC opcodes to use binary-only arithmetic
    mos6502_family_override_opcode(&cpu->base, 0x69, (mos6502_family_opcode_handler_t)nes6502_adc_immediate);  // ADC #nn
    mos6502_family_override_opcode(&cpu->base, 0x65, (mos6502_family_opcode_handler_t)nes6502_adc_zero_page);   // ADC $nn
    mos6502_family_override_opcode(&cpu->base, 0x75, (mos6502_family_opcode_handler_t)nes6502_adc_zero_page_x); // ADC $nn,X
    mos6502_family_override_opcode(&cpu->base, 0x6D, (mos6502_family_opcode_handler_t)nes6502_adc_absolute);    // ADC $nnnn
    mos6502_family_override_opcode(&cpu->base, 0x7D, (mos6502_family_opcode_handler_t)nes6502_adc_absolute_x);  // ADC $nnnn,X
    mos6502_family_override_opcode(&cpu->base, 0x79, (mos6502_family_opcode_handler_t)nes6502_adc_absolute_y);  // ADC $nnnn,Y
    mos6502_family_override_opcode(&cpu->base, 0x61, (mos6502_family_opcode_handler_t)nes6502_adc_indirect_x);  // ADC ($nn,X)
    mos6502_family_override_opcode(&cpu->base, 0x71, (mos6502_family_opcode_handler_t)nes6502_adc_indirect_y);  // ADC ($nn),Y
    
    // Override all SBC opcodes to use binary-only arithmetic
    mos6502_family_override_opcode(&cpu->base, 0xE9, (mos6502_family_opcode_handler_t)nes6502_sbc_immediate);  // SBC #nn
    mos6502_family_override_opcode(&cpu->base, 0xE5, (mos6502_family_opcode_handler_t)nes6502_sbc_zero_page);   // SBC $nn
    mos6502_family_override_opcode(&cpu->base, 0xF5, (mos6502_family_opcode_handler_t)nes6502_sbc_zero_page_x); // SBC $nn,X
    mos6502_family_override_opcode(&cpu->base, 0xED, (mos6502_family_opcode_handler_t)nes6502_sbc_absolute);    // SBC $nnnn
    mos6502_family_override_opcode(&cpu->base, 0xFD, (mos6502_family_opcode_handler_t)nes6502_sbc_absolute_x);  // SBC $nnnn,X
    mos6502_family_override_opcode(&cpu->base, 0xF9, (mos6502_family_opcode_handler_t)nes6502_sbc_absolute_y);  // SBC $nnnn,Y
    mos6502_family_override_opcode(&cpu->base, 0xE1, (mos6502_family_opcode_handler_t)nes6502_sbc_indirect_x);  // SBC ($nn,X)
    mos6502_family_override_opcode(&cpu->base, 0xF1, (mos6502_family_opcode_handler_t)nes6502_sbc_indirect_y);  // SBC ($nn),Y
    mos6502_family_override_opcode(&cpu->base, 0xEB, (mos6502_family_opcode_handler_t)nes6502_sbc_immediate);  // SBC #nn (illegal)
  
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

// Binary-only ADC implementation (NES characteristic)
static void nes6502_adc_impl(nes6502_t* cpu, uint8_t operand) {
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

// Binary-only SBC implementation (NES characteristic)
static void nes6502_sbc_impl(nes6502_t* cpu, uint8_t operand) {
    if (!cpu) return;
    
    // SBC = ADC with inverted operand and inverted carry
    mos6502_family_t* base = &cpu->base;
    bool carry_in = mos6502_family_get_flag(base, FLAG_C);
    
    // Flip carry for subtraction (borrow = !carry)
    mos6502_family_set_flag(base, FLAG_C, !carry_in);
    
    // Perform ADC with inverted operand (binary mode only)
    nes6502_adc_impl(cpu, ~operand);
}

// ADC addressing mode handlers
static void nes6502_adc_immediate(mos6502_family_t* cpu) {
    uint8_t operand = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    nes6502_adc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_adc_zero_page(mos6502_family_t* cpu) {
    uint8_t addr = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t operand = mos6502_family_read_cycle(cpu, addr);
    nes6502_adc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_adc_zero_page_x(mos6502_family_t* cpu) {
    uint8_t addr = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    mos6502_family_read_cycle(cpu, addr); // Dummy read
    addr = (addr + cpu->x) & 0xFF;
    uint8_t operand = mos6502_family_read_cycle(cpu, addr);
    nes6502_adc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_adc_absolute(mos6502_family_t* cpu) {
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint8_t operand = mos6502_family_read_cycle(cpu, addr);
    nes6502_adc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_adc_absolute_x(mos6502_family_t* cpu) {
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint16_t final_addr = addr + cpu->x;
    if ((addr & 0xFF00) != (final_addr & 0xFF00)) {
        mos6502_family_read_cycle(cpu, (addr & 0xFF00) | (final_addr & 0xFF)); // Dummy read on page cross
    }
    uint8_t operand = mos6502_family_read_cycle(cpu, final_addr);
    nes6502_adc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_adc_absolute_y(mos6502_family_t* cpu) {
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint16_t final_addr = addr + cpu->y;
    if ((addr & 0xFF00) != (final_addr & 0xFF00)) {
        mos6502_family_read_cycle(cpu, (addr & 0xFF00) | (final_addr & 0xFF)); // Dummy read on page cross
    }
    uint8_t operand = mos6502_family_read_cycle(cpu, final_addr);
    nes6502_adc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_adc_indirect_x(mos6502_family_t* cpu) {
    uint8_t zp_addr = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    mos6502_family_read_cycle(cpu, zp_addr); // Dummy read
    zp_addr = (zp_addr + cpu->x) & 0xFF;
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, zp_addr);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint8_t operand = mos6502_family_read_cycle(cpu, addr);
    nes6502_adc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_adc_indirect_y(mos6502_family_t* cpu) {
    uint8_t zp_addr = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, zp_addr);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint16_t final_addr = addr + cpu->y;
    if ((addr & 0xFF00) != (final_addr & 0xFF00)) {
        mos6502_family_read_cycle(cpu, (addr & 0xFF00) | (final_addr & 0xFF)); // Dummy read on page cross
    }
    uint8_t operand = mos6502_family_read_cycle(cpu, final_addr);
    nes6502_adc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

// SBC addressing mode handlers
static void nes6502_sbc_immediate(mos6502_family_t* cpu) {
    uint8_t operand = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    nes6502_sbc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_sbc_zero_page(mos6502_family_t* cpu) {
    uint8_t addr = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t operand = mos6502_family_read_cycle(cpu, addr);
    nes6502_sbc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_sbc_zero_page_x(mos6502_family_t* cpu) {
    uint8_t addr = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    mos6502_family_read_cycle(cpu, addr); // Dummy read
    addr = (addr + cpu->x) & 0xFF;
    uint8_t operand = mos6502_family_read_cycle(cpu, addr);
    nes6502_sbc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_sbc_absolute(mos6502_family_t* cpu) {
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint8_t operand = mos6502_family_read_cycle(cpu, addr);
    nes6502_sbc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_sbc_absolute_x(mos6502_family_t* cpu) {
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint16_t final_addr = addr + cpu->x;
    if ((addr & 0xFF00) != (final_addr & 0xFF00)) {
        mos6502_family_read_cycle(cpu, (addr & 0xFF00) | (final_addr & 0xFF)); // Dummy read on page cross
    }
    uint8_t operand = mos6502_family_read_cycle(cpu, final_addr);
    nes6502_sbc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_sbc_absolute_y(mos6502_family_t* cpu) {
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint16_t final_addr = addr + cpu->y;
    if ((addr & 0xFF00) != (final_addr & 0xFF00)) {
        mos6502_family_read_cycle(cpu, (addr & 0xFF00) | (final_addr & 0xFF)); // Dummy read on page cross
    }
    uint8_t operand = mos6502_family_read_cycle(cpu, final_addr);
    nes6502_sbc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_sbc_indirect_x(mos6502_family_t* cpu) {
    uint8_t zp_addr = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    mos6502_family_read_cycle(cpu, zp_addr); // Dummy read
    zp_addr = (zp_addr + cpu->x) & 0xFF;
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, zp_addr);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint8_t operand = mos6502_family_read_cycle(cpu, addr);
    nes6502_sbc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

static void nes6502_sbc_indirect_y(mos6502_family_t* cpu) {
    uint8_t zp_addr = mos6502_family_read_cycle(cpu, cpu->pc);
    cpu->pc++;
    uint8_t addr_lo = mos6502_family_read_cycle(cpu, zp_addr);
    uint8_t addr_hi = mos6502_family_read_cycle(cpu, (zp_addr + 1) & 0xFF);
    uint16_t addr = (addr_hi << 8) | addr_lo;
    uint16_t final_addr = addr + cpu->y;
    if ((addr & 0xFF00) != (final_addr & 0xFF00)) {
        mos6502_family_read_cycle(cpu, (addr & 0xFF00) | (final_addr & 0xFF)); // Dummy read on page cross
    }
    uint8_t operand = mos6502_family_read_cycle(cpu, final_addr);
    nes6502_sbc_impl((nes6502_t*)cpu, operand);
    MOS6502_FAMILY_OPCODE_FOOTER(cpu);
    
}

// ============================================================================
// CHIP DESCRIPTOR
// ============================================================================

// Wrapper functions for chip descriptor interface
static void* nes6502_create_wrapper(chip_descriptor_t* desc) {
    nes6502_t* cpu = malloc(sizeof(nes6502_t));
    if (!cpu) return NULL;
    
    if (!nes6502_create(desc, cpu)) {
        free(cpu);
        return NULL;
    }
    return cpu;
}

static void nes6502_destroy_wrapper(void* chip) {
    if (chip) {
        nes6502_destroy((nes6502_t*)chip);
        free(chip);
    }
}

chip_descriptor_t nes6502_descriptor = {
    .description = "NES 6502 CPU (No Decimal Mode)",
    .create = nes6502_create_wrapper,
    .destroy = nes6502_destroy_wrapper,
    .bus_attach = NULL,
    .read = NULL,  // NES 6502 doesn't have special read behavior
    .write = NULL, // NES 6502 doesn't have special write behavior
    .bank_change = NULL,
    .get_rwcb_context = NULL,
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    .render_debug_window = nes6502_render_debug_window,
    .render_settings_window = NULL
#endif
};
