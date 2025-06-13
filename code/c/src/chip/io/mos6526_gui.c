#include "mos6526.h"
#include "../../gui/cimgui_interface.h"
#include "../../systems/c64/c64.h"  // Need this to access C64 structure
#ifndef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#endif
#include <cimgui.h>
#include <stdio.h>
#include <stddef.h>  // For offsetof

// Forward declaration
static const char* mos6526_get_cia_name(mos6526_t* cia);

// ============================================================================
// MOS6526 CIA GUI DEBUG WINDOW
// ============================================================================

void mos6526_render_debug_window(void* chip, bool* show_window) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia || !cia->desc) return;
    
    if (!*show_window) return;
      // Push unique ID to prevent conflicts between CIA1 and CIA2
    igPushID_Int((int)(uintptr_t)cia);
      char window_title[128];
    const char* cia_name = mos6526_get_cia_name(cia);
    snprintf(window_title, sizeof(window_title), "%s Debug", cia_name);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        igPopID();
        return;
    }

    // Show which CIA this is
    igText("Complex Interface Adapter - %s", cia_name);
    igSeparator();

    // CIA State Section
    igText("CIA State");
    igSeparator();
    
    // Port A and B
    igText("Data Ports");
    igSeparator();
    
    igText("Port A Data (PRA): $%02X", cia->reg[PRA]);
    igText("Port A DDR (DDRA): $%02X", cia->reg[DDRA]);
    igText("Port B Data (PRB): $%02X", cia->reg[PRB]);
    igText("Port B DDR (DDRB): $%02X", cia->reg[DDRB]);
    
    igSeparator();
    
    // Timers
    igText("Timers");
    igSeparator();
    
    uint16_t timer_a = (cia->reg[TA_HI] << 8) | cia->reg[TA_LO];
    igText("Timer A: %04X", timer_a);
    igText("Timer A Control: $%02X", cia->reg[CRA]);
    igText("Timer A Running: %s", (cia->reg[CRA] & 0x01) ? "YES" : "NO");
    
    uint16_t timer_b = (cia->reg[TB_HI] << 8) | cia->reg[TB_LO];
    igText("Timer B: %04X", timer_b);
    igText("Timer B Control: $%02X", cia->reg[CRB]);
    igText("Timer B Running: %s", (cia->reg[CRB] & 0x01) ? "YES" : "NO");
    
    igSeparator();
    
    // Time of Day Clock
    igText("Time of Day Clock");
    igSeparator();
    
    igText("TOD 10ths: $%02X", cia->reg[TOD_10THS]);
    igText("TOD Seconds: $%02X", cia->reg[TOD_SEC]);
    igText("TOD Minutes: $%02X", cia->reg[TOD_MIN]);
    igText("TOD Hours: $%02X", cia->reg[TOD_HR]);
    
    igSeparator();
    
    // Interrupts
    igText("Interrupt Control");
    igSeparator();
    igText("ICR: $%02X", cia->reg[ICR]);
    igText("IRQ Active: %s", (cia->reg[ICR] & 0x80) ? "YES" : "NO");
    
    igSeparator();
    
    // Serial Data Register
    igText("Serial Data Register: $%02X", cia->reg[SDR]);

    igEnd();
    igPopID();
}

// ============================================================================
// MOS6526 CIA GUI SETTINGS WINDOW
// ============================================================================

void mos6526_render_settings_window(void* chip, bool* show_window) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia || !cia->desc) return;
    
    if (!*show_window) return;
    
    // Push unique ID to prevent conflicts between CIA1 and CIA2
    igPushID_Int((int)(uintptr_t)cia);
    
    char window_title[128];
    const char* cia_name = mos6526_get_cia_name(cia);
    snprintf(window_title, sizeof(window_title), "%s Settings", cia_name);
    
    if (!igBegin(window_title, show_window, 0)) {
        igEnd();
        igPopID();
        return;
    }

    // Show which CIA this is
    igText("Complex Interface Adapter - %s Configuration", cia_name);
    igSeparator();
    igText("Chip Type: MOS6526 CIA");

    // CIA State Section
    igText("Extended CIA Debug Information");
    igSeparator();
    
    // Port A and B with detailed information
    igText("Data Ports (Detailed)");
    igSeparator();
    
    igText("Port A Data (PRA): $%02X", cia->reg[PRA]);
    igText("Port A DDR (DDRA): $%02X", cia->reg[DDRA]);
    igText("Port A Value: $%02X", cia->port_a_value);
    igSeparator();
    
    igText("Port B Data (PRB): $%02X", cia->reg[PRB]);
    igText("Port B DDR (DDRB): $%02X", cia->reg[DDRB]);
    igText("Port B Internal DDR: $%02X", cia->reg[IDDRB_OFFSET]);
    igText("Port B Value: $%02X", cia->port_b_value);
    
    igSeparator();
    
    // Timers with latches
    igText("Timers (with Latches)");
    igSeparator();
    
    uint16_t timer_a = (cia->reg[TA_HI] << 8) | cia->reg[TA_LO];
    uint16_t timer_a_latch = (cia->reg[TIMER_OFFSET + TA_HI] << 8) | cia->reg[TIMER_OFFSET + TA_LO];
    igText("Timer A: $%04X", timer_a);
    igText("Timer A Latch: $%04X", timer_a_latch);
    igText("Timer A Control: $%02X", cia->reg[CRA]);
    igText("Timer A Running: %s", (cia->reg[CRA] & CRA_START) ? "YES" : "NO");
    
    uint16_t timer_b = (cia->reg[TB_HI] << 8) | cia->reg[TB_LO];
    uint16_t timer_b_latch = (cia->reg[TIMER_OFFSET + TB_HI] << 8) | cia->reg[TIMER_OFFSET + TB_LO];
    igText("Timer B: $%04X", timer_b);
    igText("Timer B Latch: $%04X", timer_b_latch);
    igText("Timer B Control: $%02X", cia->reg[CRB]);
    igText("Timer B Running: %s", (cia->reg[CRB] & CRB_START) ? "YES" : "NO");
    
    igSeparator();
    
    // Time of Day Clock with detailed information
    igText("Time of Day Clock (Detailed)");
    igSeparator();
    
    igText("TOD 10ths: $%02X", cia->reg[TOD_10THS]);
    igText("TOD Seconds: $%02X", cia->reg[TOD_SEC]);
    igText("TOD Minutes: $%02X", cia->reg[TOD_MIN]);
    igText("TOD Hours: $%02X", cia->reg[TOD_HR]);
    igText("TOD Running: %s", cia->is_running_tod ? "YES" : "NO");
    igText("TOD Cycles: %d", cia->tod_cycles);
    igText("TOD Read Delta: %u", cia->read_tod_delta);
    igText("TOD Write Delta: %u", cia->write_tod_delta);
    
    igSeparator();
    
    // Alarm registers
    igText("Alarm Registers");
    igSeparator();
    
    igText("Alarm 10ths: $%02X", cia->reg[ALARM_OFFSET + TOD_10THS]);
    igText("Alarm Seconds: $%02X", cia->reg[ALARM_OFFSET + TOD_SEC]);
    igText("Alarm Minutes: $%02X", cia->reg[ALARM_OFFSET + TOD_MIN]);
    igText("Alarm Hours: $%02X", cia->reg[ALARM_OFFSET + TOD_HR]);
    
    igSeparator();
    
    // Interrupts with detailed information
    igText("Interrupt Control (Detailed)");
    igSeparator();
    
    igText("ICR: $%02X", cia->reg[ICR]);
    igText("Interrupt Mask: $%02X", cia->interrupt_mask);
    igText("Delayed IRQ: %s", cia->delayed_irq ? "YES" : "NO");
    igText("IRQ Active: %s", (cia->reg[ICR] & ICR_IRQ) ? "YES" : "NO");
    igText("Timer A IRQ: %s", (cia->reg[ICR] & ICR_TA) ? "YES" : "NO");
    igText("Timer B IRQ: %s", (cia->reg[ICR] & ICR_TB) ? "YES" : "NO");
    igText("Alarm IRQ: %s", (cia->reg[ICR] & ICR_ALRM) ? "YES" : "NO");
    igText("Serial IRQ: %s", (cia->reg[ICR] & ICR_SP) ? "YES" : "NO");
    igText("Flag IRQ: %s", (cia->reg[ICR] & ICR_FLG) ? "YES" : "NO");
    
    igSeparator();
    
    // Serial Data Register with detailed information
    igText("Serial Data (Detailed)");
    igSeparator();
    
    igText("SDR: $%02X", cia->reg[SDR]);
    igText("Shift Register: $%02X", cia->reg[SHIFT_OFFSET]);
    igText("Serial Shift: %d", cia->serial_shift);
    
    igEnd();
    igPopID();
}

// Helper function to determine CIA type based on system context
static const char* mos6526_get_cia_name(mos6526_t* cia) {
    if (!cia->bus) return "CIA"; // Generic CIA if no bus context
    
    // For C64 system: CIA1 is at 0xDC00, CIA2 is at 0xDD00
    // Access the C64 structure through the bus to determine which CIA this is
    c64_bus_t* bus = (c64_bus_t*)cia->bus;
    
    // The C64 bus should have a reference back to the C64 system
    c64_t* c64 = (c64_t*)bus->c64;
    if (!c64) return "CIA"; // Generic CIA if no c64 context
    
    // Now we can directly compare pointers to determine which CIA this is
    if (cia == c64->cia1) {
        return "CIA1 ($DC00)";
    } else if (cia == c64->cia2) {
        return "CIA2 ($DD00)";
    } else {
        return "CIA"; // Generic fallback for other systems
    }
}
