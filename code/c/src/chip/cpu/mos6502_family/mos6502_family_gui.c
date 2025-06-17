#include "mos6502_family_gui.h"
#include "mos6502_family_constants.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// SHARED MOS 6502 FAMILY GUI IMPLEMENTATION
// ============================================================================

void mos6502_family_render_cpu_registers(mos6502_family_t* cpu) {
    if (!cpu) return;
    
    igText("CPU Registers");
    igSeparator();
    
    igText("PC: $%04X", cpu->pc);
    igText("A:  $%02X (%d)", cpu->a, cpu->a);
    igText("X:  $%02X (%d)", cpu->x, cpu->x);
    igText("Y:  $%02X (%d)", cpu->y, cpu->y);
    igText("SP: $%02X", cpu->sp);
}

void mos6502_family_render_status_flags(mos6502_family_t* cpu) {
    if (!cpu) return;
    
    igText("Status Register: $%02X", cpu->p);
    igText("Flags: N V - B D I Z C");
    
    // Create a formatted string for flag display
    char flag_str[32];
    snprintf(flag_str, sizeof(flag_str), "       %c %c 1 %c %c %c %c %c",
        (cpu->p & FLAG_N) ? '1' : '0',
        (cpu->p & FLAG_V) ? '1' : '0',
        (cpu->p & FLAG_B) ? '1' : '0',
        (cpu->p & FLAG_D) ? '1' : '0',
        (cpu->p & FLAG_I) ? '1' : '0',
        (cpu->p & FLAG_Z) ? '1' : '0',
        (cpu->p & FLAG_C) ? '1' : '0');
    igText("%s", flag_str);
}

void mos6502_family_render_control_lines(mos6502_family_t* cpu) {
    if (!cpu) return;
    
    igText("Control Lines");
    igSeparator();
    
    // Check if control interface is properly initialized
    if (cpu->control_interface.get_lines && cpu->control_interface.context) {
        uint32_t control_lines = cpu->control_interface.get_lines(cpu->control_interface.context);
        igText("IRQ: %s", (control_lines & (1U << 0)) ? "ACTIVE" : "inactive");
        igText("NMI: %s", (control_lines & (1U << 1)) ? "ACTIVE" : "inactive");
        igText("RDY: %s", (control_lines & (1U << 5)) ? "STALLED" : "ready");
    } else {
        igText("IRQ: Not connected");
        igText("NMI: Not connected");
        igText("RDY: Not connected");
    }
}

void mos6502_family_render_execution_controls(mos6502_family_t* cpu, bool (*step_func)(void*), void (*reset_func)(void*)) {
    if (!cpu) return;
    
    igText("Execution Control");
    igSeparator();
    
    if (igButton("Step One Instruction", (ImVec2){0, 0})) {
        if (step_func && cpu->bus_interface.bus_read && cpu->bus_interface.context) {
            step_func(cpu);
        }
    }
    
    igSameLine(0, -1.0f);
    if (igButton("Reset CPU", (ImVec2){0, 0})) {
        if (reset_func) {
            reset_func(cpu);
        }
    }
    
    igText("Interception: %s", mos6502_family_is_intercepting(cpu) ? "ACTIVE" : "inactive");
}

void mos6502_family_render_memory_view(mos6502_family_t* cpu, uint16_t start_addr, uint16_t length) {
    if (!cpu || !cpu->bus_interface.bus_read) return;
    
    igText("Memory View ($%04X - $%04X)", start_addr, start_addr + length - 1);
    igSeparator();
    
    for (uint16_t i = 0; i < length; i += 16) {
        char line[128];
        char hex_part[64] = "";
        char ascii_part[20] = "";
        
        snprintf(line, sizeof(line), "%04X: ", start_addr + i);
        
        for (int j = 0; j < 16 && (i + j) < length; j++) {
            uint8_t value = cpu->bus_interface.bus_read(cpu->bus_interface.context, start_addr + i + j);
            char hex_byte[8];
            snprintf(hex_byte, sizeof(hex_byte), "%02X ", value);
            strncat(hex_part, hex_byte, sizeof(hex_part) - strlen(hex_part) - 1);
            
            char ascii_char = (value >= 32 && value <= 126) ? value : '.';
            strncat(ascii_part, &ascii_char, 1);
        }
        
        igText("%s%-48s %s", line, hex_part, ascii_part);
    }
}

void mos6502_family_render_stack_view(mos6502_family_t* cpu) {
    if (!cpu) return;
    
    igText("Stack View (Page $01)");
    igSeparator();
    
    // Show stack around current SP
    uint16_t stack_base = 0x0100;
    uint16_t current_sp = stack_base | cpu->sp;
    
    for (int i = -4; i <= 4; i++) {
        uint16_t addr = current_sp + i;
        if ((addr & 0xFF00) == 0x0100) { // Stay in stack page
            uint8_t value = cpu->bus_interface.bus_read(cpu->bus_interface.context, addr);
            const char* marker = (i == 0) ? " <- SP" : "";
            igText("$%04X: $%02X%s", addr, value, marker);
        }
    }
}

void mos6502_family_render_disassembly(mos6502_family_t* cpu, int num_instructions) {
    if (!cpu) return;
    
    igText("Disassembly (PC = $%04X)", cpu->pc);
    igSeparator();
    
    // This is a simplified disassembly - a real implementation would need
    // a full 6502 instruction decoder
    uint16_t addr = cpu->pc;
    for (int i = 0; i < num_instructions; i++) {
        if (addr == cpu->pc) {
            igText("-> $%04X: $%02X", addr, cpu->bus_interface.bus_read(cpu->bus_interface.context, addr));
        } else {
            igText("   $%04X: $%02X", addr, cpu->bus_interface.bus_read(cpu->bus_interface.context, addr));
        }
        addr++; // Simplified - real implementation would decode instruction length
    }
}

void mos6502_family_render_debug_window(void* chip, bool* show_window, const mos6502_family_gui_config_t* config) {
    if (!chip || !show_window || !config) return;
    
    mos6502_family_t* cpu = (mos6502_family_t*)chip;
    if (!cpu->desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Debug", cpu->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    // Common CPU state
    mos6502_family_render_cpu_registers(cpu);
    igSeparator();
    mos6502_family_render_status_flags(cpu);
    igSeparator();
    
    // CPU-specific sections
    if (config->render_cpu_specific) {
        config->render_cpu_specific(chip);
        igSeparator();
    }
    
    // Control lines
    mos6502_family_render_control_lines(cpu);
    igSeparator();
    
    // Execution controls (would need to be passed proper function pointers)
    mos6502_family_render_execution_controls(cpu, NULL, NULL);

    igEnd();
}

void mos6502_family_render_settings_window(void* chip, bool* show_window, const mos6502_family_gui_config_t* config) {
    if (!chip || !show_window || !config) return;
    
    mos6502_family_t* cpu = (mos6502_family_t*)chip;
    if (!cpu->desc) return;
    
    if (!*show_window) return;
    
    char window_title[128];
    snprintf(window_title, sizeof(window_title), "%s Settings", cpu->desc->description);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        return;
    }

    igText("CPU Configuration");
    igSeparator();
    
    igText("CPU Type: %s", config->cpu_type_name);
    igText("Architecture: 8-bit 6502 family");
    igText("Decimal Mode: %s", config->has_decimal_mode ? "Supported" : "Not supported");
    igText("I/O Ports: %s", config->has_io_ports ? "Present" : "Standard");
    igText("Extended Opcodes: %s", config->has_extended_opcodes ? "Supported" : "Standard");
    
    igSeparator();
    
    // CPU behavior settings
    static bool log_instructions = false;
    static bool break_on_brk = true;
    static bool break_on_illegal = false;
    
    igCheckbox("Log Instructions", &log_instructions);
    igCheckbox("Break on BRK", &break_on_brk);
    if (config->has_extended_opcodes) {
        igCheckbox("Break on Illegal Opcodes", &break_on_illegal);
    }

    igEnd();
}

void mos6502_family_render_flag_bits(uint8_t flags, const char* flag_names) {
    char bits[32];
    int bit_count = strlen(flag_names);
    
    for (int i = 0; i < bit_count; i++) {
        bits[i*2] = (flags & (0x80 >> i)) ? '1' : '0';
        bits[i*2 + 1] = ' ';
    }
    bits[bit_count*2 - 1] = '\0';
    
    igText("%s", flag_names);
    igText("%s", bits);
}

void mos6502_family_render_port_bits(uint8_t port_value, const char* label) {
    igText("%s: $%02X", label, port_value);
    igText("Bits: 7 6 5 4 3 2 1 0");
    
    char port_bits[32];
    snprintf(port_bits, sizeof(port_bits), "      %c %c %c %c %c %c %c %c",
        (port_value & 0x80) ? '1' : '0',
        (port_value & 0x40) ? '1' : '0',
        (port_value & 0x20) ? '1' : '0',
        (port_value & 0x10) ? '1' : '0',
        (port_value & 0x08) ? '1' : '0',
        (port_value & 0x04) ? '1' : '0',
        (port_value & 0x02) ? '1' : '0',
        (port_value & 0x01) ? '1' : '0');
    igText("%s", port_bits);
}
