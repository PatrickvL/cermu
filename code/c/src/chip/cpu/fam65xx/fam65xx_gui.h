#ifndef FAM65XX_GUI_H
#define FAM65XX_GUI_H

#include "fam65xx_core.h"
#include "../../../gui/cimgui_interface.h"

// ============================================================================
// SHARED MOS 6502 FAMILY GUI FUNCTIONS
// ============================================================================

// Common CPU state display functions
void fam65xx_render_cpu_registers(fam65xx_t* cpu);
void fam65xx_render_status_flags(fam65xx_t* cpu);
void fam65xx_render_control_lines(fam65xx_t* cpu);
void fam65xx_render_execution_controls(fam65xx_t* cpu, bool (*step_func)(void*), void (*reset_func)(void*));

// Memory debugging helpers
void fam65xx_render_memory_view(fam65xx_t* cpu, uint16_t start_addr, uint16_t length);
void fam65xx_render_stack_view(fam65xx_t* cpu);

// Disassembly view
void fam65xx_render_disassembly(fam65xx_t* cpu, int num_instructions);

// Generic debug window framework
typedef struct {
    const char* cpu_type_name;
    bool has_decimal_mode;
    bool has_io_ports;
    bool has_extended_opcodes;
    void (*render_cpu_specific)(void* cpu_instance);
} fam65xx_gui_config_t;

void fam65xx_render_debug_window(void* chip, bool* show_window, const fam65xx_gui_config_t* config);
void fam65xx_render_settings_window(void* chip, bool* show_window, const fam65xx_gui_config_t* config);

// Flag display helpers
void fam65xx_render_flag_bits(uint8_t flags, const char* flag_names);
void fam65xx_render_port_bits(uint8_t port_value, const char* label);

#endif // FAM65XX_GUI_H
