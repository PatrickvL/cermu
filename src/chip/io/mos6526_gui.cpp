#include "mos6526.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#include "../../systems/c64/c64.h"  // Need this to access C64 structure
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <stdio.h>
#include <stddef.h>  // For offsetof
#include <memory>

// Forward declarations
static const char* mos6526_get_cia_name(mos6526_t* cia);

// ============================================================================
// MOS6526 CIA LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_mos6526_layout() {
    // Start with DIP-40 base layout
    ChipLayout layout = create_dip40_layout();
    
    // Clear default pins from create_dip40_layout() and add hardware-accurate MOS6526 pins
    layout.left_pins.clear();
    layout.right_pins.clear();
    
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
    // Right-hand pins (21-40) are numbered bottom-up, not top-down
    PIN_LR(layout,  1, VSS,  RES, 40)    // Ground / Reset
    PIN_LR(layout,  2, PA0,  D7, 39)     // Port A Bit 0 / Data 7
    PIN_LR(layout,  3, PA1,  D6, 38)     // Port A Bit 1 / Data 6
    PIN_LR(layout,  4, PA2,  D5, 37)     // Port A Bit 2 / Data 5
    PIN_LR(layout,  5, PA3,  D4, 36)     // Port A Bit 3 / Data 4
    PIN_LR(layout,  6, PA4,  D3, 35)     // Port A Bit 4 / Data 3
    PIN_LR(layout,  7, PA5,  D2, 34)     // Port A Bit 5 / Data 2
    PIN_LR(layout,  8, PA6,  D1, 33)     // Port A Bit 6 / Data 1
    PIN_LR(layout,  9, PA7,  D0, 32)     // Port A Bit 7 / Data 0
    PIN_LR(layout, 10, PB0,  A3, 31)     // Port B Bit 0 / Address 3
    PIN_LR(layout, 11, PB1,  A2, 30)     // Port B Bit 1 / Address 2
    PIN_LR(layout, 12, PB2,  A1, 29)     // Port B Bit 2 / Address 1
    PIN_LR(layout, 13, PB3,  A0, 28)     // Port B Bit 3 / Address 0
    PIN_LR(layout, 14, PB4,  CNT, 27)    // Port B Bit 4 / Counter
    PIN_LR(layout, 15, PB5,  SP, 26)     // Port B Bit 5 / Serial Port
    PIN_LR(layout, 16, PB6,  PHI2, 25)   // Port B Bit 6 / Clock
    PIN_LR(layout, 17, PB7,  FLAG, 24)   // Port B Bit 7 / Flag Input
    PIN_LR(layout, 18, PC,   CS, 23)     // Serial Port / Chip Select
    PIN_LR(layout, 19, TOD,  RW, 22)     // Time of Day / Read/Write
    PIN_LR(layout, 20, VDD,  IRQ, 21)    // +5V Power / Interrupt Request
    
    return layout;
}

// ============================================================================
// MOS6526 CIA GUI DEBUG WINDOW
// ============================================================================

// Helper function to get CIA pin states for visualization
static std::vector<PinSignalState> get_cia_pin_states(mos6526_t* cia, const ChipLayout* layout, bus_state_t bus_state) {
    std::vector<PinSignalState> pin_states;
    if (!cia || !layout) return pin_states;
    
    int total_pins = layout->get_total_pins();
    pin_states.resize(total_pins);
    
    // Initialize all pins as inactive by default
    for (int i = 0; i < total_pins; i++) {
        pin_states[i] = PinSignalState{
            .pin_number = static_cast<uint8_t>(i + 1),
            .signal_level = false,
            .drive_direction = false,
            .signal_value = 0,
            .high_impedance = true,
            .has_pullup = false,
            .has_pulldown = false,
            .signal_valid = true,
            .analog_voltage = 0.0f,
            .is_pwm = false,
            .pwm_duty_cycle = 0.0f
        };
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
            pin_states[pin_idx].high_impedance = !drive_direction;
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
            pin_states[pin_idx].high_impedance = !drive_direction;
        }
    }
    
    // IRQ pin (pin 21, index 20)
    if (20 < total_pins) {
        bool irq_active = (cia->reg[ICR] & 0x80) != 0;
        pin_states[20].signal_level = !irq_active; // IRQ is active low
        pin_states[20].signal_valid = true;
        pin_states[20].drive_direction = true;
        pin_states[20].high_impedance = false;
    }
    
    // Power pins are always active
    pin_states[0].signal_level = false;  // VSS (Ground, pin 1)
    pin_states[0].high_impedance = false;
    pin_states[19].signal_level = true;  // VDD (+5V, pin 20)
    pin_states[19].high_impedance = false;
    if (39 < total_pins) {
        pin_states[39].signal_level = true; // RES (Reset, pin 40) - active high when not reset
        pin_states[39].high_impedance = false;
    }
    
    return pin_states;
}

// Use global renderer for CIA chip visualization
static ChipLayout& get_cia_layout() {
    static ChipLayout layout = create_mos6526_layout();
    return layout;
}

void mos6526_render_debug_content(void* chip) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia || !cia->desc) return;
    
#ifdef IMGUI_VERSION
    const char* cia_name = mos6526_get_cia_name(cia);

    // Create two-column layout: chip visualization on left, debugging info on right
    ImVec2 window_size = ImGui::GetContentRegionAvail();
    
    // Left column: Chip Visualization (fixed width ~250px)
    ImVec2 chip_viz_size = ImVec2(250.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Chip Visualization");
        ImGui::Separator();
        
        // Calculate chip center for visualization
        ImVec2 chip_center = ImGui::GetCursorScreenPos();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        chip_center.x += content_region.x * 0.5f;
        chip_center.y += 200.0f; // Space for the chip
        
        // Get global renderer and chip layout
        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_cia_layout();
        
        // Get current pin states from CIA
        std::vector<PinSignalState> pin_states = get_cia_pin_states(cia, &layout, 0 /* bus_state */);
        
        // Render the chip using global renderer
        renderer.render(layout, chip_center, pin_states, cia_name);
    }
    ImGui::EndChild();
    
    ImGui::SameLine(0, 5.0f); // Small gap between columns
    
    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0); // Remaining width minus left column and gap
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        // CIA State Section
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
    ImGui::EndChild();
#endif
}

// ============================================================================
// MOS6526 CIA GUI SETTINGS
// ============================================================================

void mos6526_render_settings_content(void* chip) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia || !cia->desc) return;
    
#ifdef IMGUI_VERSION
    const char* cia_name = mos6526_get_cia_name(cia);

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
    ImGui::Text("CNT Output: %s", cia->cnt_output_state ? "HIGH" : "LOW");
    ImGui::Text("SP Output: %d", cia->sp_output_bit ? 1 : 0);
    ImGui::Text("SPMODE: %s", (cia->reg[CRA] & CRA_SPMODE) ? "Output" : "Input");
    
    // Reset to single column at the end
    ImGui::Columns(1, NULL, false);
#endif
}

// Helper function to determine CIA type based on interrupt line
static const char* mos6526_get_cia_name(mos6526_t* cia) {
    // Determine CIA type based on which interrupt line it raises
    // CIA1 raises IRQ (BUS_MASK_IRQ), CIA2 raises NMI (BUS_MASK_NMI)
    if (cia->configured_interrupt_bit == BUS_IRQ_BIT) {
        return "CIA1 ($DC00)";
    } else if (cia->configured_interrupt_bit == BUS_NMI_BIT) {
        return "CIA2 ($DD00)";
    } else {
        return "CIA"; // Generic fallback for other systems
    }
}

// ============================================================================
// MOS6526 CIA LAYOUT WINDOW (standalone pinout diagram)
// ============================================================================

void mos6526_render_layout_content(void* chip) {
    mos6526_t* cia = (mos6526_t*)chip;
    if (!cia) return;

#ifdef IMGUI_VERSION
    const char* cia_name = mos6526_get_cia_name(cia);

    ChipVisualization& renderer = GetGlobalChipRenderer();
    ChipLayout& layout = get_cia_layout();
    std::vector<PinSignalState> pin_states = get_cia_pin_states(cia, &layout, 0);

    ImVec2 size = renderer.get_recommended_size(layout);
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 center = {cursor.x + size.x * 0.5f, cursor.y + size.y * 0.5f};
    ImGui::Dummy(size);
    renderer.render(layout, center, pin_states, cia_name);
#endif
}
