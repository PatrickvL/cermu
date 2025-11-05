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

// Hardware-accurate CIA chip layout (MOS 6526 - 40-pin DIP)
static void render_cia_chip_layout(mos6526_t* cia) {
    if (igCollapsingHeader_BoolPtr("Hardware Layout - MOS 6526 CIA", NULL, ImGuiTreeNodeFlags_DefaultOpen)) {
        igIndent(16.0f);
        
        igText("Package: 40-pin DIP");
        igText("Complex Interface Adapter (CIA)");
        igSeparator();
        
        // Two-column layout for pins
        igColumns(2, "cia_pinout", true);
        igText("LEFT SIDE:");
        igText("1  - VSS (Ground)");
        igText("2  - PA0 (Port A Bit 0)");
        igText("3  - PA1 (Port A Bit 1)");
        igText("4  - PA2 (Port A Bit 2)");
        igText("5  - PA3 (Port A Bit 3)");
        igText("6  - PA4 (Port A Bit 4)");
        igText("7  - PA5 (Port A Bit 5)");
        igText("8  - PA6 (Port A Bit 6)");
        igText("9  - PA7 (Port A Bit 7)");
        igText("10 - PB0 (Port B Bit 0)");
        igText("11 - PB1 (Port B Bit 1)");
        igText("12 - PB2 (Port B Bit 2)");
        igText("13 - PB3 (Port B Bit 3)");
        igText("14 - PB4 (Port B Bit 4)");
        igText("15 - PB5 (Port B Bit 5)");
        igText("16 - PB6 (Port B Bit 6)");
        igText("17 - PB7 (Port B Bit 7)");
        igText("18 - PC (Serial Port)");
        igText("19 - TOD (Time of Day)");
        igText("20 - VCC (+5V)");
        
        igNextColumn();
        igText("RIGHT SIDE:");
        igText("21 - IRQ (Interrupt Request)");
        igText("22 - R/W (Read/Write)");
        igText("23 - CS (Chip Select)");
        igText("24 - FLAG (Flag Input)");
        igText("25 - PHI2 (Clock)");
        igText("26 - SP (Serial Port)");
        igText("27 - CNT (Serial Counter)");
        igText("28 - A0 (Address)");
        igText("29 - A1 (Address)");
        igText("30 - A2 (Address)");
        igText("31 - A3 (Address)");
        igText("32 - D0 (Data)");
        igText("33 - D1 (Data)");
        igText("34 - D2 (Data)");
        igText("35 - D3 (Data)");
        igText("36 - D4 (Data)");
        igText("37 - D5 (Data)");
        igText("38 - D6 (Data)");
        igText("39 - D7 (Data)");
        igText("40 - RES (Reset)");
        
        igColumns(1, NULL, false);
        igUnindent(16.0f);
    }
}

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

    // Create two-column layout: chip visualization on left, debugging info on right
    igColumns(2, "cia_debug_columns", true);
    
    // Left column: Hardware chip layout
    render_cia_chip_layout(cia);
    
    igNextColumn();
    
    // Right column: Register information
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

    // Reset to single column at the end
    igColumns(1, NULL, false);
    
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
    
    // Reset to single column at the end
    igColumns(1, NULL, false);
    
    igEnd();
    igPopID();
}

// Helper function to determine CIA type based on system context
static const char* mos6526_get_cia_name(mos6526_t* cia) {
    if (!cia->bus_interface.context) return "CIA"; // Generic CIA if no bus context
    
    // For C64 system: CIA1 is at 0xDC00, CIA2 is at 0xDD00
    // Access the C64 structure through the bus to determine which CIA this is
    c64_bus_t* bus = (c64_bus_t*)cia->bus_interface.context;
    
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
