#include "mos6510.h"
#include "../../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// MOS6510 CPU GUI DEBUG WINDOW
// ============================================================================

void mos6510_render_debug_window(void* chip, bool* show_window) {
    mos6510_t* cpu = (mos6510_t*)chip;
    if (!cpu) return;
    
    if (!*show_window) return;
    
    if (!igBegin("MOS6510 CPU Debug", show_window, 0)) {
        igEnd();
        return;
    }

    // CPU State Section
    igText("CPU State");
    igSeparator();
    
    igText("PC: $%04X", cpu->pc);
    igText("A:  $%02X (%d)", cpu->a, cpu->a);
    igText("X:  $%02X (%d)", cpu->x, cpu->x);
    igText("Y:  $%02X (%d)", cpu->y, cpu->y);
    igText("SP: $%02X", cpu->sp);
    
    igSeparator();
    
    // Status Register with individual flags
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

    igSeparator();

    // I/O Ports Section
    igText("I/O Ports ($0000-$0001)");
    igSeparator();
    
    igText("DDR ($0000): $%02X", cpu->io_port[0]);
    igText("Port ($0001): $%02X", cpu->io_port[1]);
    
    // Show individual port bits
    igText("Port Bits: 7 6 5 4 3 2 1 0");
    char port_bits[32];
    snprintf(port_bits, sizeof(port_bits), "           %c %c %c %c %c %c %c %c",
        (cpu->io_port[1] & 0x80) ? '1' : '0',
        (cpu->io_port[1] & 0x40) ? '1' : '0',
        (cpu->io_port[1] & 0x20) ? '1' : '0',
        (cpu->io_port[1] & 0x10) ? '1' : '0',
        (cpu->io_port[1] & 0x08) ? '1' : '0',
        (cpu->io_port[1] & 0x04) ? '1' : '0',
        (cpu->io_port[1] & 0x02) ? '1' : '0',
        (cpu->io_port[1] & 0x01) ? '1' : '0');
    igText("%s", port_bits);

    igSeparator();
    
    // Control Lines Section
    igText("Control Lines");
    igSeparator();
#if 0 // TODO : fix this part crashing    
    uint32_t control_lines = CPU_CONTROL_LINES(cpu);
    igText("IRQ: %s", (control_lines & MOS6510_MASK_IRQ) ? "ACTIVE" : "inactive");
    igText("NMI: %s", (control_lines & MOS6510_MASK_NMI) ? "ACTIVE" : "inactive");
    igText("RDY: %s", (control_lines & MOS6510_MASK_RDY) ? "STALLED" : "ready");
#endif
    igSeparator();

    // Execution Control Section
    igText("Execution Control");
    igSeparator();
    
    if (igButton("Step One Instruction", (ImVec2){0, 0})) {
#if 1 // TODO : fix this part crashing    
        mos6510_step(cpu);
#endif
    }
    igSameLine(0, -1.0f);
    if (igButton("Reset CPU", (ImVec2){0, 0})) {
#if 1 // TODO : fix this part crashing    
        mos6510_reset(cpu);
#endif
    }
    
    igText("Interception: %s", mos6510_is_intercepting() ? "ACTIVE" : "inactive");

    igEnd();
}

// ============================================================================
// MOS6510 CPU GUI SETTINGS WINDOW
// ============================================================================

void mos6510_render_settings_window(void* chip, bool* show_window) {
    mos6510_t* cpu = (mos6510_t*)chip;
    
    if (!*show_window) return;
    
    if (!igBegin("MOS6510 CPU Settings", show_window, 0)) {
        igEnd();
        return;
    }

    igText("CPU Configuration");
    igSeparator();
    
    igText("CPU Type: MOS6510");
    igText("Architecture: 8-bit 6502 compatible");
    igText("Clock Speed: ~1 MHz");
    
    igSeparator();
    
    // CPU behavior settings
    static bool decimal_mode_support = true;
    igCheckbox("Decimal Mode Support", &decimal_mode_support);
    
    static bool illegal_opcodes = true;
    igCheckbox("Illegal Opcodes", &illegal_opcodes);

    igSeparator();

    igText("I/O Port Configuration");
    igSeparator();
    
    igText("I/O Port Default Values:");
    
    static int ddr_value = 0x2F;
    static int port_value = 0x37;
    
    igSliderInt("DDR Default", &ddr_value, 0, 255, "$%02X", 0);
    igSliderInt("Port Default", &port_value, 0, 255, "$%02X", 0);
    
    if (igButton("Apply Defaults", (ImVec2){0, 0})) {
        cpu->io_port[0] = (uint8_t)ddr_value;
        cpu->io_port[1] = (uint8_t)port_value;
    }

    igSeparator();

    igText("Debug Options");
    igSeparator();
    
    static bool log_instructions = false;
    static bool break_on_brk = true;
    static bool break_on_jam = true;
    
    igCheckbox("Log Instructions", &log_instructions);
    igCheckbox("Break on BRK", &break_on_brk);
    igCheckbox("Break on JAM", &break_on_jam);
    
    if (igButton("Clear Instruction Log", (ImVec2){0, 0})) {
        // TODO: Implement instruction logging
    }

    igEnd();
}