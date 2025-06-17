#ifndef MOS6502_H
#define MOS6502_H

#include "../mos6502_family/mos6502_family_core.h"
#include "../mos6502_family/mos6502_family_constants.h"

// ============================================================================
// MOS 6502 CPU (Standard 6502 with Decimal Mode Support)
// ============================================================================

// MOS 6502 specific configuration
#define MOS6502_HAS_DECIMAL_MODE 1
#define MOS6502_HAS_IO_PORTS 0
#define MOS6502_HAS_ILLEGAL_OPCODES 1

// MOS 6502 doesn't have additional hardware beyond the standard MOS 6502 family
typedef struct {
    mos6502_family_t base; // Must be first member for casting
    // No additional hardware for standard MOS 6502
} mos6502_t;

// ============================================================================
// MOS 6502 SPECIFIC FUNCTIONS
// ============================================================================

// Chip interface functions
bool mos6502_create(chip_descriptor_t* desc, mos6502_t* cpu);
void mos6502_destroy(mos6502_t* cpu);
void mos6502_reset(mos6502_t* cpu);
bool mos6502_step(mos6502_t* cpu);

// Configuration and setup
void mos6502_attach_bus(mos6502_t* cpu, const bus_cycle_ops_t* bus_interface);
void mos6502_attach_control_lines(mos6502_t* cpu, const control_lines_interface_t* control_interface);
void mos6502_attach_system_lines(mos6502_t* cpu, system_lines_t* system_lines);

// State access (for debugger, test harness, etc.)
uint16_t mos6502_get_pc(mos6502_t* cpu);
uint8_t mos6502_get_a(mos6502_t* cpu);
uint8_t mos6502_get_x(mos6502_t* cpu);
uint8_t mos6502_get_y(mos6502_t* cpu);
uint8_t mos6502_get_sp(mos6502_t* cpu);
uint8_t mos6502_get_p(mos6502_t* cpu);

void mos6502_set_pc(mos6502_t* cpu, uint16_t value);
void mos6502_set_a(mos6502_t* cpu, uint8_t value);
void mos6502_set_x(mos6502_t* cpu, uint8_t value);
void mos6502_set_y(mos6502_t* cpu, uint8_t value);
void mos6502_set_sp(mos6502_t* cpu, uint8_t value);
void mos6502_set_p(mos6502_t* cpu, uint8_t value);

// Memory access functions
uint8_t mos6502_read_memory(mos6502_t* cpu, uint16_t address);
void mos6502_write_memory(mos6502_t* cpu, uint16_t address, uint8_t value);

// Test and debug support
void mos6502_start_intercept(mos6502_t* cpu);
void mos6502_stop_intercept(mos6502_t* cpu);
bool mos6502_is_intercepting(mos6502_t* cpu);

// GUI support
void mos6502_render_debug_window(void* chip, bool* show_window);

// ============================================================================
// ARITHMETIC OPERATIONS WITH DECIMAL MODE SUPPORT
// ============================================================================

// ADC with full decimal mode support (the main difference from 6510)
void mos6502_adc(mos6502_t* cpu, uint8_t operand);

// SBC with full decimal mode support  
void mos6502_sbc(mos6502_t* cpu, uint8_t operand);

// Helper function for decimal mode conversion
uint8_t mos6502_decimal_adjust_add(uint8_t binary_result, bool* carry_out);
uint8_t mos6502_decimal_adjust_sub(uint8_t binary_result, bool* carry_out);

#endif // MOS6502_H
