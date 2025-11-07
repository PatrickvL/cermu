#include "mos6526.h"
#include "../../gui/imgui_interface.h"
#include "../../gui/generic_chip_gui.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#include "../../systems/c64/c64.h"  // Need this to access C64 structure
#include <imgui.h>
#include <stdio.h>
#include <stddef.h>  // For offsetof

// Forward declarations
static const char* mos6526_get_cia_name(mos6526_t* cia);
static ChipLayout get_cia_layout(void* chip);
static void get_cia_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, PinSignalState* pin_states);
static void render_cia_specific_content(void* chip);

// ============================================================================
// MOS6526 CIA LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_mos6526_layout() {
    // Start with DIP-40 base layout
    ChipLayout layout = create_dip40_layout();
    
    // Update package info for MOS6526 CIA
    layout.markings = {
        "MOS6526",                   // part_number
        "MOS Technology",            // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate MOS6526 CIA pinout (40-pin DIP)
    PIN_LR(layout, 1, VSS,   IRQ, 21)    // Ground / Interrupt Request
    PIN_LR(layout, 2, PA0,   RW, 22)     // Port A Bit 0 / Read/Write
    PIN_LR(layout, 3, PA1,   CS, 23)     // Port A Bit 1 / Chip Select
    PIN_LR(layout, 4, PA2,   FLAG, 24)   // Port A Bit 2 / Flag Input
    PIN_LR(layout, 5, PA3,   PHI2, 25)   // Port A Bit 3 / Clock
    PIN_LR(layout, 6, PA4,   SP, 26)     // Port A Bit 4 / Serial Port
    PIN_LR(layout, 7, PA5,   CNT, 27)    // Port A Bit 5 / Counter
    PIN_LR(layout, 8, PA6,   A0, 28)     // Port A Bit 6 / Address 0
    PIN_LR(layout, 9, PA7,   A1, 29)     // Port A Bit 7 / Address 1
    PIN_LR(layout, 10, PB0,  A2, 30)     // Port B Bit 0 / Address 2
    PIN_LR(layout, 11, PB1,  A3, 31)     // Port B Bit 1 / Address 3
    PIN_LR(layout, 12, PB2,  D0, 32)     // Port B Bit 2 / Data 0
    PIN_LR(layout, 13, PB3,  D1, 33)     // Port B Bit 3 / Data 1
    PIN_LR(layout, 14, PB4,  D2, 34)     // Port B Bit 4 / Data 2
    PIN_LR(layout, 15, PB5,  D3, 35)     // Port B Bit 5 / Data 3
    PIN_LR(layout, 16, PB6,  D4, 36)     // Port B Bit 6 / Data 4
    PIN_LR(layout, 17, PB7,  D5, 37)     // Port B Bit 7 / Data 5
    PIN_LR(layout, 18, PC,   D6, 38)     // Serial Port / Data 6
    PIN_LR(layout, 19, TOD,  D7, 39)     // Time of Day / Data 7
    PIN_LR(layout, 20, VDD,  RES, 40)    // +5V Power / Reset
    
    return layout;
}

// ============================================================================
// MOS6526 CIA GUI DEBUG WINDOW
// ============================================================================

// Callback functions for generic chip GUI
static ChipLayout get_cia_layout(void* chip) {
    return create_mos6526_layout();
}

static void get_cia_pin_states(void* chip, ChipLayout* layout, bus_state_t bus_state, PinSignalState* pin_states) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia || !layout || !pin_states) return;
    
    int total_pins = layout->get_total_pins();
    
    // Initialize all pins as inactive by default
    for (int i = 0; i < total_pins; i++) {
        pin_states[i].pin_number = 0;
        pin_states[i].signal_level = false;
        pin_states[i].drive_direction = false;
        pin_states[i].signal_value = 0;
        pin_states[i].high_impedance = false;
        pin_states[i].has_pullup = false;
        pin_states[i].has_pulldown = false;
        pin_states[i].signal_valid = true;
        pin_states[i].analog_voltage = 0.0f;
        pin_states[i].is_pwm = false;
        pin_states[i].pwm_duty_cycle = 0.0f;
    }
    
    // Set pin states based on CIA registers
    // Port A pins (PA0-PA7, pins 2-9)
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 1; // PA0 is pin 2, so index 1
        if (pin_idx < total_pins) {
            bool pin_active = (cia->reg[PRA] & (1 << i)) != 0;
            bool drive_direction = (cia->reg[DDRA] & (1 << i)) != 0;
            pin_states[pin_idx].signal_level = pin_active;
            pin_states[pin_idx].signal_valid = true;
            pin_states[pin_idx].drive_direction = drive_direction;
        }
    }
    
    // Port B pins (PB0-PB7, pins 10-17)
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 9; // PB0 is pin 10, so index 9
        if (pin_idx < total_pins) {
            bool pin_active = (cia->reg[PRB] & (1 << i)) != 0;
            bool drive_direction = (cia->reg[DDRB] & (1 << i)) != 0;
            pin_states[pin_idx].signal_level = pin_active;
            pin_states[pin_idx].signal_valid = true;
            pin_states[pin_idx].drive_direction = drive_direction;
        }
    }
    
    // IRQ pin (pin 21, index 20)
    if (20 < total_pins) {
        bool irq_active = (cia->reg[ICR] & 0x80) != 0;
        pin_states[20].signal_level = irq_active;
        pin_states[20].signal_valid = true;
    }
    
    // Power pins are always active
    pin_states[0].signal_level = false; // VSS (Ground, pin 1)
    pin_states[19].signal_level = true; // VDD (+5V, pin 20)
    if (39 < total_pins) {
        pin_states[39].signal_level = false; // RES (Reset, pin 40)
    }
}

static void render_cia_specific_content(void* chip) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia) return;
    
    // This replaces the right column content from the original function
    const char* cia_name = mos6526_get_cia_name(cia);
    ImGui::Text("Complex Interface Adapter - %s", cia_name);
    ImGui::Separator();

    // CIA State Section
    ImGui::Text("CIA State");
    ImGui::Separator();
    
    // Port A and B
    ImGui::Text("Data Ports");
    ImGui::Separator();
    
    ImGui::Text("Port A Data (PRA): $%02X", cia->reg[PRA]);
    ImGui::Text("Port A DDR (DDRA): $%02X", cia->reg[DDRA]);
    ImGui::Text("Port B Data (PRB): $%02X", cia->reg[PRB]);
    ImGui::Text("Port B DDR (DDRB): $%02X", cia->reg[DDRB]);
    
    ImGui::Separator();
    
    // Timers
    ImGui::Text("Timers");
    ImGui::Separator();
    
    uint16_t timer_a = (cia->reg[TA_HI] << 8) | cia->reg[TA_LO];
    ImGui::Text("Timer A: %04X", timer_a);
    ImGui::Text("Timer A Control: $%02X", cia->reg[CRA]);
    ImGui::Text("Timer A Running: %s", (cia->reg[CRA] & 0x01) ? "YES" : "NO");
    
    uint16_t timer_b = (cia->reg[TB_HI] << 8) | cia->reg[TB_LO];
    ImGui::Text("Timer B: %04X", timer_b);
    ImGui::Text("Timer B Control: $%02X", cia->reg[CRB]);
    ImGui::Text("Timer B Running: %s", (cia->reg[CRB] & 0x01) ? "YES" : "NO");
    
    ImGui::Separator();
    
    // Time of Day Clock
    ImGui::Text("Time of Day Clock");
    ImGui::Separator();
    
    ImGui::Text("TOD 10ths: $%02X", cia->reg[TOD_10THS]);
    ImGui::Text("TOD Seconds: $%02X", cia->reg[TOD_SEC]);
    ImGui::Text("TOD Minutes: $%02X", cia->reg[TOD_MIN]);
    ImGui::Text("TOD Hours: $%02X", cia->reg[TOD_HR]);
    
    ImGui::Separator();
    
    // Interrupts
    ImGui::Text("Interrupt Control");
    ImGui::Separator();
    ImGui::Text("ICR: $%02X", cia->reg[ICR]);
    ImGui::Text("IRQ Active: %s", (cia->reg[ICR] & 0x80) ? "YES" : "NO");
    
    ImGui::Separator();
    
    // Serial Data Register
    ImGui::Text("Serial Data Register: $%02X", cia->reg[SDR]);
}

void mos6526_render_debug_window(void* chip, bool* show_window) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia || !cia->desc) return;
    
    if (!*show_window) return;
    
    // Push unique ID to prevent conflicts between CIA1 and CIA2
    ImGui::PushID((int)(uintptr_t)cia);
    
    // Create generic chip GUI config
    chip_gui_config_t config = generic_chip_gui_get_default_config("MOS6526", "CIA");
    config.get_layout = get_cia_layout;
    config.get_pin_states = get_cia_pin_states;
    
    // Create generic chip GUI instance
    generic_chip_gui_t* gui = generic_chip_gui_create(cia, &config);
    if (!gui) {
        ImGui::PopID();
        return;
    }
    
    char window_title[128];
    const char* cia_name = mos6526_get_cia_name(cia);
    snprintf(window_title, sizeof(window_title), "%s Debug", cia_name);
    
    // Use generic chip GUI render function
    generic_chip_gui_render_debug_panel(gui, NULL, window_title, show_window, render_cia_specific_content);
    
    // Cleanup
    generic_chip_gui_destroy(gui);
    ImGui::PopID();
}

// ============================================================================
// MOS6526 CIA GUI SETTINGS WINDOW
// ============================================================================

void mos6526_render_settings_window(void* chip, bool* show_window) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia || !cia->desc) return;
    
    if (!*show_window) return;
    
    // Push unique ID to prevent conflicts between CIA1 and CIA2
    ImGui::PushID((int)(uintptr_t)cia);
    
    char window_title[128];
    const char* cia_name = mos6526_get_cia_name(cia);
    snprintf(window_title, sizeof(window_title), "%s Settings", cia_name);
    
    if (!ImGui::Begin(window_title, show_window, 0)) {
        ImGui::End();
        ImGui::PopID();
        return;
    }

    // Show which CIA this is
    ImGui::Text("Complex Interface Adapter - %s Configuration", cia_name);
    ImGui::Separator();
    ImGui::Text("Chip Type: MOS6526 CIA");

    // CIA State Section
    ImGui::Text("Extended CIA Debug Information");
    ImGui::Separator();
    
    // Port A and B with detailed information
    ImGui::Text("Data Ports (Detailed)");
    ImGui::Separator();
    
    ImGui::Text("Port A Data (PRA): $%02X", cia->reg[PRA]);
    ImGui::Text("Port A DDR (DDRA): $%02X", cia->reg[DDRA]);
    ImGui::Text("Port A Value: $%02X", cia->port_a_value);
    ImGui::Separator();
    
    ImGui::Text("Port B Data (PRB): $%02X", cia->reg[PRB]);
    ImGui::Text("Port B DDR (DDRB): $%02X", cia->reg[DDRB]);
    ImGui::Text("Port B Internal DDR: $%02X", cia->reg[IDDRB_OFFSET]);
    ImGui::Text("Port B Value: $%02X", cia->port_b_value);
    
    ImGui::Separator();
    
    // Timers with latches
    ImGui::Text("Timers (with Latches)");
    ImGui::Separator();
    
    uint16_t timer_a = (cia->reg[TA_HI] << 8) | cia->reg[TA_LO];
    uint16_t timer_a_latch = (cia->reg[TIMER_OFFSET + TA_HI] << 8) | cia->reg[TIMER_OFFSET + TA_LO];
    ImGui::Text("Timer A: $%04X", timer_a);
    ImGui::Text("Timer A Latch: $%04X", timer_a_latch);
    ImGui::Text("Timer A Control: $%02X", cia->reg[CRA]);
    ImGui::Text("Timer A Running: %s", (cia->reg[CRA] & CRA_START) ? "YES" : "NO");
    
    uint16_t timer_b = (cia->reg[TB_HI] << 8) | cia->reg[TB_LO];
    uint16_t timer_b_latch = (cia->reg[TIMER_OFFSET + TB_HI] << 8) | cia->reg[TIMER_OFFSET + TB_LO];
    ImGui::Text("Timer B: $%04X", timer_b);
    ImGui::Text("Timer B Latch: $%04X", timer_b_latch);
    ImGui::Text("Timer B Control: $%02X", cia->reg[CRB]);
    ImGui::Text("Timer B Running: %s", (cia->reg[CRB] & CRB_START) ? "YES" : "NO");
    
    ImGui::Separator();
    
    // Time of Day Clock with detailed information
    ImGui::Text("Time of Day Clock (Detailed)");
    ImGui::Separator();
    
    ImGui::Text("TOD 10ths: $%02X", cia->reg[TOD_10THS]);
    ImGui::Text("TOD Seconds: $%02X", cia->reg[TOD_SEC]);
    ImGui::Text("TOD Minutes: $%02X", cia->reg[TOD_MIN]);
    ImGui::Text("TOD Hours: $%02X", cia->reg[TOD_HR]);
    ImGui::Text("TOD Running: %s", cia->is_running_tod ? "YES" : "NO");
    ImGui::Text("TOD Cycles: %d", cia->tod_cycles);
    ImGui::Text("TOD Read Delta: %u", cia->read_tod_delta);
    ImGui::Text("TOD Write Delta: %u", cia->write_tod_delta);
    
    ImGui::Separator();
    
    // Alarm registers
    ImGui::Text("Alarm Registers");
    ImGui::Separator();
    
    ImGui::Text("Alarm 10ths: $%02X", cia->reg[ALARM_OFFSET + TOD_10THS]);
    ImGui::Text("Alarm Seconds: $%02X", cia->reg[ALARM_OFFSET + TOD_SEC]);
    ImGui::Text("Alarm Minutes: $%02X", cia->reg[ALARM_OFFSET + TOD_MIN]);
    ImGui::Text("Alarm Hours: $%02X", cia->reg[ALARM_OFFSET + TOD_HR]);
    
    ImGui::Separator();
    
    // Interrupts with detailed information
    ImGui::Text("Interrupt Control (Detailed)");
    ImGui::Separator();
    
    ImGui::Text("ICR: $%02X", cia->reg[ICR]);
    ImGui::Text("Interrupt Mask: $%02X", cia->interrupt_mask);
    ImGui::Text("Delayed IRQ: %s", cia->delayed_irq ? "YES" : "NO");
    ImGui::Text("IRQ Active: %s", (cia->reg[ICR] & ICR_IRQ) ? "YES" : "NO");
    ImGui::Text("Timer A IRQ: %s", (cia->reg[ICR] & ICR_TA) ? "YES" : "NO");
    ImGui::Text("Timer B IRQ: %s", (cia->reg[ICR] & ICR_TB) ? "YES" : "NO");
    ImGui::Text("Alarm IRQ: %s", (cia->reg[ICR] & ICR_ALRM) ? "YES" : "NO");
    ImGui::Text("Serial IRQ: %s", (cia->reg[ICR] & ICR_SP) ? "YES" : "NO");
    ImGui::Text("Flag IRQ: %s", (cia->reg[ICR] & ICR_FLG) ? "YES" : "NO");
    
    ImGui::Separator();
    
    // Serial Data Register with detailed information
    ImGui::Text("Serial Data (Detailed)");
    ImGui::Separator();
    
    ImGui::Text("SDR: $%02X", cia->reg[SDR]);
    ImGui::Text("Shift Register: $%02X", cia->reg[SHIFT_OFFSET]);
    ImGui::Text("Serial Shift: %d", cia->serial_shift);
    
    // Reset to single column at the end
    ImGui::Columns(1, NULL, false);
    
    ImGui::End();
    ImGui::PopID();
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
