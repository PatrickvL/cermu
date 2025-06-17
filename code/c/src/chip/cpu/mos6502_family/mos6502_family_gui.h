#ifndef MOS6502_FAMILY_GUI_H
#define MOS6502_FAMILY_GUI_H

#include "mos6502_family_core.h"
#include "../../../gui/cimgui_interface.h"

// ============================================================================
// SHARED MOS 6502 FAMILY GUI FUNCTIONS
// ============================================================================

// Common CPU state display functions
void mos6502_family_render_cpu_registers(mos6502_family_t* cpu);
void mos6502_family_render_status_flags(mos6502_family_t* cpu);
void mos6502_family_render_control_lines(mos6502_family_t* cpu);
void mos6502_family_render_execution_controls(mos6502_family_t* cpu, bool (*step_func)(void*), void (*reset_func)(void*));

// Memory debugging helpers
void mos6502_family_render_memory_view(mos6502_family_t* cpu, uint16_t start_addr, uint16_t length);
void mos6502_family_render_stack_view(mos6502_family_t* cpu);

// Disassembly view
void mos6502_family_render_disassembly(mos6502_family_t* cpu, int num_instructions);

// Generic debug window framework
typedef struct {
    const char* cpu_type_name;
    bool has_decimal_mode;
    bool has_io_ports;
    bool has_extended_opcodes;
    void (*render_cpu_specific)(void* cpu_instance);
} mos6502_family_gui_config_t;

void mos6502_family_render_debug_window(void* chip, bool* show_window, const mos6502_family_gui_config_t* config);
void mos6502_family_render_settings_window(void* chip, bool* show_window, const mos6502_family_gui_config_t* config);

// Flag display helpers
void mos6502_family_render_flag_bits(uint8_t flags, const char* flag_names);
void mos6502_family_render_port_bits(uint8_t port_value, const char* label);

#endif // MOS6502_FAMILY_GUI_H
