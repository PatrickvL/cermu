#include "chip/io/mos6526.hpp"
#include "core/chip_layout.hpp"
#include "core/pin_macros.hpp"
// Native Dear ImGui C++ - conditional compilation for GUI availability
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif
#include <cstdio>
#include <cstddef>  // For offsetof
#include <memory>

#ifdef CERMU_HAS_GUI

// ============================================================================
// MOS6526 CIA layout virtuals
// ============================================================================

ChipLayout* mos6526_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        // Start with DIP-40 base layout
        ChipLayout layout = create_dip40_layout();

        // Clear default pins and add hardware-accurate MOS6526 pins
        layout.left_pins.clear();
        layout.right_pins.clear();

        // Update package info for MOS6526 CIA
        layout.markings = {
            "MOS6526",                   // part_number
            "MOS Technology",            // manufacturer
            {},                     // package_variant
            {},                     // date_code
            {},                     // lot_number
            {},                     // custom_text
            true,                        // show_part_number
            true,                        // show_manufacturer
            false,                       // show_package_variant
            false                        // show_date_code
        };

        // Hardware-accurate MOS6526 CIA pinout (40-pin DIP)
        // Per MOS 6526 Complex Interface Adapter datasheet
        PIN_LR(layout,  1, VSS,  _RES, 40)   // gnd / reset
        PIN_LR(layout,  2, PA0,  CNT, 39)    // port A lo / counter in
        PIN_LR(layout,  3, PA1,  SP, 38)     // / serial port
        PIN_LR(layout,  4, PA2,  A0, 37)     // / reg sel lo
        PIN_LR(layout,  5, PA3,  A1, 36)
        PIN_LR(layout,  6, PA4,  A2, 35)
        PIN_LR(layout,  7, PA5,  A3, 34)     // / reg sel hi
        PIN_LR(layout,  8, PA6,  D0, 33)     // / data lo
        PIN_LR(layout,  9, PA7,  D1, 32)     // port A hi
        PIN_LR(layout, 10, PB0,  D2, 31)     // port B lo
        PIN_LR(layout, 11, PB1,  D3, 30)
        PIN_LR(layout, 12, PB2,  D4, 29)
        PIN_LR(layout, 13, PB3,  D5, 28)
        PIN_LR(layout, 14, PB4,  D6, 27)
        PIN_LR(layout, 15, PB5,  D7, 26)     // / data hi
        PIN_LR(layout, 16, PB6,  PHI2, 25)   // / clock
        PIN_LR(layout, 17, PB7,  FLAG, 24)   // port B hi / flag in
        PIN_LR(layout, 18, PC,   _CS, 23)    // periph ctrl / chip sel
        PIN_LR(layout, 19, TOD,  RW, 22)     // TOD clock / R/W
        PIN_LR(layout, 20, VDD,  _IRQ, 21)   // +5V / interrupt

        return layout;
    }();
    return &layout;
}

// Helper function to get CIA pin states for visualization
static std::vector<PinSignalState> get_cia_pin_states(mos6526_t* cia, const ChipLayout* layout, bus_state_t bus_state) {
    if (!cia || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto pin_states = populate_pin_states_from_bus(*layout, bus_state);
    int total_pins = static_cast<int>(pin_states.size());

    // CIA specific: Port A pins (PA0-PA7) from CIA registers
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 1; // PA0 is pin 2, so index 1
        if (pin_idx < total_pins) {
            bool pin_active = (cia->regs_[CIA_REG_PRA] & (1 << i)) != 0;
            bool drive_dir = (cia->regs_[CIA_REG_DDRA] & (1 << i)) != 0;
            pin_states[pin_idx].signal_level = pin_active;
            pin_states[pin_idx].drive_direction = drive_dir;
            pin_states[pin_idx].high_impedance = !drive_dir;
        }
    }

    // Port B pins (PB0-PB7)
    for (int i = 0; i < 8; i++) {
        int pin_idx = i + 9; // PB0 is pin 10, so index 9
        if (pin_idx < total_pins) {
            bool pin_active = (cia->regs_[CIA_REG_PRB] & (1 << i)) != 0;
            bool drive_dir = (cia->regs_[CIA_REG_DDRB] & (1 << i)) != 0;
            pin_states[pin_idx].signal_level = pin_active;
            pin_states[pin_idx].drive_direction = drive_dir;
            pin_states[pin_idx].high_impedance = !drive_dir;
        }
    }

    // IRQ pin (pin 21, index 20) — CIA drives IRQ as output
    if (20 < total_pins) {
        bool irq_active = (cia->regs_[CIA_REG_ICR] & 0x80) != 0;
        pin_states[20].signal_level = !irq_active; // Active low
        pin_states[20].drive_direction = true;
        pin_states[20].high_impedance = false;
    }

    return pin_states;
}

std::vector<PinSignalState> mos6526_t::get_layout_pin_states(ChipLayout& layout) {
    return get_cia_pin_states(this, &layout, bus_snapshot_);
}

// Helper function to determine CIA type based on interrupt line
static const char* mos6526_get_cia_name(const mos6526_t* cia) {
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

const char* mos6526_t::get_layout_chip_name() const {
    return mos6526_get_cia_name(this);
}

// ============================================================================
// MOS6526 CIA GUI SETTINGS
// ============================================================================

void mos6526_t::render_settings_content() {
    mos6526_t* cia = this;
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
    
    ImGui::Text("Port A Data (CIA_REG_PRA): $%02X", cia->regs_[CIA_REG_PRA]);
    ImGui::Text("Port A DDR (CIA_REG_DDRA): $%02X", cia->regs_[CIA_REG_DDRA]);
    ImGui::Text("Port A Value: $%02X", cia->port_a_value);
    ImGui::Separator();
    
    ImGui::Text("Port B Data (CIA_REG_PRB): $%02X", cia->regs_[CIA_REG_PRB]);
    ImGui::Text("Port B DDR (CIA_REG_DDRB): $%02X", cia->regs_[CIA_REG_DDRB]);
    ImGui::Text("Port B Internal DDR: $%02X", cia->regs_[IDDRB_OFFSET]);
    ImGui::Text("Port B Value: $%02X", cia->port_b_value);
    
    ImGui::Separator();
    
    // Timers with latches
    ImGui::Text("Timers (with Latches)");
    ImGui::Separator();
    
    uint16_t timer_a_latch = (cia->regs_[TIMER_OFFSET + CIA_REG_TA_HI] << 8) | cia->regs_[TIMER_OFFSET + CIA_REG_TA_LO];
    ImGui::Text("Timer A: $%04X", cia->timer_counter_[A]);
    ImGui::Text("Timer A Latch: $%04X", timer_a_latch);
    ImGui::Text("Timer A Control: $%02X", cia->regs_[CIA_REG_CRA]);
    ImGui::Text("Timer A Running: %s", (cia->regs_[CIA_REG_CRA] & CRA_START) ? "YES" : "NO");
    
    uint16_t timer_b_latch = (cia->regs_[TIMER_OFFSET + CIA_REG_TB_HI] << 8) | cia->regs_[TIMER_OFFSET + CIA_REG_TB_LO];
    ImGui::Text("Timer B: $%04X", cia->timer_counter_[B]);
    ImGui::Text("Timer B Latch: $%04X", timer_b_latch);
    ImGui::Text("Timer B Control: $%02X", cia->regs_[CIA_REG_CRB]);
    ImGui::Text("Timer B Running: %s", (cia->regs_[CIA_REG_CRB] & CRB_START) ? "YES" : "NO");
    
    ImGui::Separator();
    
    // Time of Day Clock with detailed information
    ImGui::Text("Time of Day Clock (Detailed)");
    ImGui::Separator();
    
    ImGui::Text("TOD 10ths: $%02X", cia->regs_[CIA_REG_TOD_10THS]);
    ImGui::Text("TOD Seconds: $%02X", cia->regs_[CIA_REG_TOD_SEC]);
    ImGui::Text("TOD Minutes: $%02X", cia->regs_[CIA_REG_TOD_MIN]);
    ImGui::Text("TOD Hours: $%02X", cia->regs_[CIA_REG_TOD_HR]);
    ImGui::Text("TOD Running: %s", cia->is_running_tod ? "YES" : "NO");
    ImGui::Text("TOD Cycles: %d", cia->tod_cycles);
    ImGui::Text("TOD Read Delta: %u", cia->read_tod_delta);
    ImGui::Text("TOD Write Delta: %u", cia->write_tod_delta);
    
    ImGui::Separator();
    
    // Alarm registers
    ImGui::Text("Alarm Registers");
    ImGui::Separator();
    
    ImGui::Text("Alarm 10ths: $%02X", cia->regs_[ALARM_OFFSET + CIA_REG_TOD_10THS]);
    ImGui::Text("Alarm Seconds: $%02X", cia->regs_[ALARM_OFFSET + CIA_REG_TOD_SEC]);
    ImGui::Text("Alarm Minutes: $%02X", cia->regs_[ALARM_OFFSET + CIA_REG_TOD_MIN]);
    ImGui::Text("Alarm Hours: $%02X", cia->regs_[ALARM_OFFSET + CIA_REG_TOD_HR]);
    
    ImGui::Separator();
    
    // Interrupts with detailed information
    ImGui::Text("Interrupt Control (Detailed)");
    ImGui::Separator();
    
    ImGui::Text("CIA_REG_ICR: $%02X", cia->regs_[CIA_REG_ICR]);
    ImGui::Text("Interrupt Mask: $%02X", cia->interrupt_mask);
    ImGui::Text("IRQ Active: %s", (cia->regs_[CIA_REG_ICR] & ICR_IRQ) ? "YES" : "NO");
    ImGui::Text("Timer A IRQ: %s", (cia->regs_[CIA_REG_ICR] & ICR_TA) ? "YES" : "NO");
    ImGui::Text("Timer B IRQ: %s", (cia->regs_[CIA_REG_ICR] & ICR_TB) ? "YES" : "NO");
    ImGui::Text("Alarm IRQ: %s", (cia->regs_[CIA_REG_ICR] & ICR_ALRM) ? "YES" : "NO");
    ImGui::Text("Serial IRQ: %s", (cia->regs_[CIA_REG_ICR] & ICR_SP) ? "YES" : "NO");
    ImGui::Text("Flag IRQ: %s", (cia->regs_[CIA_REG_ICR] & ICR_FLG) ? "YES" : "NO");
    
    ImGui::Separator();
    
    // Serial Data Register with detailed information
    ImGui::Text("Serial Data (Detailed)");
    ImGui::Separator();
    
    ImGui::Text("CIA_REG_SDR: $%02X", cia->regs_[CIA_REG_SDR]);
    ImGui::Text("Shifter: $%04X", cia->shifter);
    ImGui::Text("SR Bits: %d", cia->sr_bits);
    ImGui::Text("CIA_REG_SDR Valid: %s", cia->sdr_valid ? "YES" : "NO");
    ImGui::Text("CIA_REG_SDR Delay: $%05X", cia->sdr_delay);
    ImGui::Text("CNT Output: %s", cia->cnt_output_state ? "HIGH" : "LOW");
    ImGui::Text("SP Output: %d", cia->sp_output_bit ? 1 : 0);
    ImGui::Text("SPMODE: %s", (cia->regs_[CIA_REG_CRA] & CRA_SPMODE) ? "Output" : "Input");
    
    // Reset to single column at the end
    ImGui::Columns(1, NULL, false);
}

#endif // CERMU_HAS_GUI
