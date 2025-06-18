#ifndef NES6502_H
#define NES6502_H

#include "../mos6502_family/mos6502_family_core.h"
#include "../mos6502_family/mos6502_family_constants.h"

// ============================================================================
// NES 6502 CPU (6502 variant used in Nintendo Entertainment System)
// Key difference: Decimal mode is disabled (D flag exists but does nothing)
// ============================================================================

// External descriptor for chip management
extern chip_descriptor_t nes6502_descriptor;

// NES 6502 doesn't have additional hardware beyond the standard 6502 family
typedef struct {
    mos6502_family_t base; // Must be first member for casting
    // No additional hardware for NES 6502
} nes6502_t;

// ============================================================================
// NES 6502 SPECIFIC FUNCTIONS
// ============================================================================

// Chip interface functions
bool nes6502_create(chip_descriptor_t* desc, nes6502_t* cpu);
void nes6502_destroy(nes6502_t* cpu);
void nes6502_reset(nes6502_t* cpu);
bool nes6502_step(nes6502_t* cpu);

// Configuration and setup
void nes6502_attach_bus(nes6502_t* cpu, const bus_cycle_ops_t* bus_interface);
void nes6502_attach_control_lines(nes6502_t* cpu, const control_lines_interface_t* control_interface);
void nes6502_attach_system_lines(nes6502_t* cpu, system_lines_t* system_lines);
void nes6502_attach_ram(nes6502_t* cpu, const access_callback_t* ram_access);

// State access (for debugger, test harness, etc.)
uint16_t nes6502_get_pc(nes6502_t* cpu);
uint8_t nes6502_get_a(nes6502_t* cpu);
uint8_t nes6502_get_x(nes6502_t* cpu);
uint8_t nes6502_get_y(nes6502_t* cpu);
uint8_t nes6502_get_sp(nes6502_t* cpu);
uint8_t nes6502_get_p(nes6502_t* cpu);

void nes6502_set_pc(nes6502_t* cpu, uint16_t value);
void nes6502_set_a(nes6502_t* cpu, uint8_t value);
void nes6502_set_x(nes6502_t* cpu, uint8_t value);
void nes6502_set_y(nes6502_t* cpu, uint8_t value);
void nes6502_set_sp(nes6502_t* cpu, uint8_t value);
void nes6502_set_p(nes6502_t* cpu, uint8_t value);

// Memory access functions
uint8_t nes6502_read_memory(nes6502_t* cpu, uint16_t address);
void nes6502_write_memory(nes6502_t* cpu, uint16_t address, uint8_t value);

// Test and debug support
void nes6502_start_intercept(nes6502_t* cpu);
void nes6502_stop_intercept(nes6502_t* cpu);
bool nes6502_is_intercepting(nes6502_t* cpu);

// GUI support
void nes6502_render_debug_window(void* chip, bool* show_window);

#endif // NES6502_H
