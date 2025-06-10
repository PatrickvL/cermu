#include "mos6526.h"
#include "../../gui/cimgui_interface.h"
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>

// ============================================================================
// MOS6526 CIA GUI DEBUG WINDOW
// ============================================================================

void mos6526_render_debug_window(void* chip, bool* show_window) {
    mos6526_t* cia = (mos6526_t*)chip;
    
    if (!*show_window) return;
    
    if (!igBegin("MOS6526 CIA Debug", show_window, 0)) {
        igEnd();
        return;
    }

    // CIA State Section
    igText("CIA State");
    igSeparator();
    
    // Port A and B
    igText("Data Ports");
    igSeparator();
    
    igText("Port A Data (PRA): $%02X", cia->pra);
    igText("Port A DDR (DDRA): $%02X", cia->ddra);
    igText("Port B Data (PRB): $%02X", cia->prb);
    igText("Port B DDR (DDRB): $%02X", cia->ddrb);
    
    igSeparator();
    
    // Timers
    igText("Timers");
    igSeparator();
    
    igText("Timer A: %04X", cia->timer_a);
    igText("Timer A Control: $%02X", cia->cra);
    igText("Timer A Running: %s", (cia->cra & 0x01) ? "YES" : "NO");
    
    igText("Timer B: %04X", cia->timer_b);
    igText("Timer B Control: $%02X", cia->crb);
    igText("Timer B Running: %s", (cia->crb & 0x01) ? "YES" : "NO");
    
    igSeparator();
    
    // Time of Day Clock
    igText("Time of Day Clock");
    igSeparator();
    
    igText("TOD 10ths: $%02X", cia->tod_10ths);
    igText("TOD Seconds: $%02X", cia->tod_sec);
    igText("TOD Minutes: $%02X", cia->tod_min);
    igText("TOD Hours: $%02X", cia->tod_hr);
    
    igSeparator();
    
    // Interrupts
    igText("Interrupt Control");
    igSeparator();
      igText("ICR: $%02X", cia->icr);
    igText("IRQ Active: %s", (cia->icr & 0x80) ? "YES" : "NO");
    
    igSeparator();
    
    // Serial Data Register
    igText("Serial Data Register: $%02X", cia->sdr);

    igEnd();
}

// ============================================================================
// MOS6526 CIA GUI SETTINGS WINDOW
// ============================================================================

void mos6526_render_settings_window(void* chip, bool* show_window) {
    mos6526_t* cia = (mos6526_t*)chip;
    
    if (!*show_window) return;
    
    if (!igBegin("MOS6526 CIA Settings", show_window, 0)) {
        igEnd();
        return;
    }

    igText("CIA Configuration");
    igSeparator();
      igText("Chip Type: MOS6526 CIA");
    
    igSeparator();
    
    // Timer Settings
    igText("Timer Configuration");
    igSeparator();
    
    static bool timer_a_enabled = true;
    static bool timer_b_enabled = true;
    
    igCheckbox("Timer A Enabled", &timer_a_enabled);
    igCheckbox("Timer B Enabled", &timer_b_enabled);
    
    igSeparator();
    
    // Port Configuration
    igText("Port Configuration");
    igSeparator();
    
    static int port_a_direction = 0x00;
    static int port_b_direction = 0x00;
    
    igSliderInt("Port A Direction", &port_a_direction, 0, 255, "$%02X", 0);
    igSliderInt("Port B Direction", &port_b_direction, 0, 255, "$%02X", 0);
    
    if (igButton("Apply Port Settings", (ImVec2){0, 0})) {
        cia->ddra = (uint8_t)port_a_direction;
        cia->ddrb = (uint8_t)port_b_direction;
    }
    
    igSeparator();
    
    // Interrupt Settings
    igText("Interrupt Configuration");
    igSeparator();
    
    static bool irq_enabled = true;
    igCheckbox("IRQ Enabled", &irq_enabled);
      if (igButton("Reset CIA", (ImVec2){0, 0})) {
        // Reset CIA state
        cia->pra = 0x00;
        cia->prb = 0x00;
        cia->ddra = 0x00;
        cia->ddrb = 0x00;
        cia->timer_a = 0xFFFF;
        cia->timer_b = 0xFFFF;
        cia->cra = 0x00;
        cia->crb = 0x00;
        cia->icr = 0x00;
        cia->sdr = 0x00;
        cia->tod_10ths = 0x00;
        cia->tod_sec = 0x00;
        cia->tod_min = 0x00;
        cia->tod_hr = 0x00;
        cia->tod_latched = false;
    }

    igEnd();
}