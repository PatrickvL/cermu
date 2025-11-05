/*
 * cpu_pin_layouts.h - CPU trait-specific pin layout definitions
 * 
 * This header defines pin layouts for different CPU variants based on their
 * specific features and capabilities using CPU traits.
 */

#ifndef CPU_PIN_LAYOUTS_H
#define CPU_PIN_LAYOUTS_H

#include "../../../gui/chip_visualization.h"
#include "fam65xx_processor_traits.hpp"
#include "../../core/system_lines.h"

// Forward declaration of the CPU template class
namespace fam65xx {
    template<const CPUTraits& Traits>
    class fam65xx_t;
}

// ============================================================================
// CPU TRAIT-BASED PIN LAYOUT FACTORY
// ============================================================================

// CPU-specific pin layout functions - using reference template parameters like fam65xx_t
template<const fam65xx::CPUTraits& Traits>
PinLayout create_cpu_pin_layout();

// Specific CPU layout functions  
PinLayout create_mos6502_layout();
PinLayout create_mos6510_layout();
PinLayout create_wdc_w65c02s_layout();
PinLayout create_wdc_65c816_layout();
PinLayout create_ricoh_2a03_layout();
PinLayout create_rockwell_r65c02_layout();

// CPU pin state functions - get pin states from CPU and bus state  
template<const fam65xx::CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, PinLayout* layout, bus_state_t bus_state);

// ============================================================================
// TEMPLATE FUNCTION IMPLEMENTATIONS (must be in header for templates)
// ============================================================================

#include "fam65xx.hpp"
#include "../../core/system_lines.h"
#include <type_traits>

// Generic CPU pin layout function with compile-time CPU selection
template<const fam65xx::CPUTraits& Traits>
PinLayout create_cpu_pin_layout() {
    PinLayout layout = {};
    
    // MOS 6502 (NMOS) PIN LAYOUT - use create_mos6502_layout()
    if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::MOS6502)>) {
        layout = create_mos6502_layout();
        
    // MOS 6510 (C64/C128) PIN LAYOUT - use create_mos6510_layout()
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::MOS6510)>) {
        layout = create_mos6510_layout();
        
    // WDC 65C02 (CMOS) PIN LAYOUT - use create_wdc_w65c02s_layout()
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::WDC_W65C02S)>) {
        layout = create_wdc_w65c02s_layout();
        
    // WDC 65C816 (16-BIT) PIN LAYOUT - use create_wdc_65c816_layout()
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::WDC_65C816)>) {
        layout = create_wdc_65c816_layout();
        
    // NES 6502 (RICOH 2A03) PIN LAYOUT - use create_ricoh_2a03_layout()
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::RICOH_2A03)>) {
        layout = create_ricoh_2a03_layout();
        
    // ROCKWELL R65C02 PIN LAYOUT - use create_rockwell_r65c02_layout()
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::ROCKWELL_R65C02)>) {
        layout = create_rockwell_r65c02_layout();
        
    } else {
        // Fallback for unknown CPU type - set fields individually
        layout.package.width = 600.0f;                    // DIP-40 width
        layout.package.height = 2000.0f;                  // DIP-40 height
        layout.package.package_type = PackageType::DIP;
        layout.package.marker = OrientationMarker::NOTCH;
        layout.package.pin_pitch = 100.0f;
        layout.package.has_thermal_pad = false;
        layout.package.has_center_slug = false;
        layout.package.thermal_pad_size = 0.0f;
        
        layout.markings.part_number = "Unknown";
        layout.markings.manufacturer = "65xx Family";
    }
    
    return layout;
}

// ============================================================================
// CPU PIN STATE EXTRACTION WITH BUS STATE
// ============================================================================

// CPU pin state function with compile-time CPU selection  
template<const fam65xx::CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, const PinLayout* layout, bus_state_t bus_state) {
    // Map pins based on the pin layout for this CPU type
    // Get the actual pin layout for this CPU
    std::vector<PinState> states(layout->get_total_pins());

    // Initialize all pins as inactive and valid
    for (auto& state : states) {
        state = {false, false, 0, false, true};
    }
    
    if (!cpu) {
        // Mark all states as invalid if no CPU
        for (auto& state : states) {
            state.is_valid = false;
        }
        return states;
    }
    
    // Extract bus state components
    uint16_t addr_bus = BUS_GET_ADDR(bus_state);
    uint8_t data_bus = BUS_GET_DATA(bus_state);
    
    // Process each pin according to its type and position
    auto process_pin = [&](const ChipPin& pin, size_t state_index) {
        if (state_index >= states.size()) return;
        
        PinState& state = states[state_index];
        
        PinType pin_type = pin.get_pin_type();
        switch (pin_type) {
            case PinType::ADDRESS: {
                if (pin.bit_index < 16) {
                    state.is_active = (addr_bus & (1 << pin.bit_index)) != 0;
                    state.is_output = true;
                    state.value = state.is_active ? 1 : 0;
                }
                break;
            }
            case PinType::DATA: {
                if (pin.bit_index < 8) {
                    state.is_active = (data_bus & (1 << pin.bit_index)) != 0;
                    state.is_output = (bus_state & BUS_BIT(BUS_RW_BIT)) == 0; // Output on write
                    state.value = state.is_active ? 1 : 0;
                    state.is_tristate = !state.is_output;
                }
                break;
            }
            case PinType::CONTROL: {
                if (pin.label == PinLabel::RW) {
                    state.is_active = (bus_state & BUS_BIT(BUS_RW_BIT)) != 0;
                    state.is_output = true;
                } else if (pin.label == PinLabel::SYNC) {
                    state.is_active = (bus_state & BUS_BIT(BUS_SYNC_BIT)) != 0;
                    state.is_output = true;
                } else if (pin.label == PinLabel::RDY) {
                    state.is_active = (bus_state & BUS_BIT(BUS_RDY_BIT)) != 0;
                    state.is_output = false; // Input to CPU
                } else if (pin.label == PinLabel::AEC) {
                    state.is_active = (bus_state & BUS_BIT(BUS_AEC_BIT)) != 0;
                    state.is_output = true;
                } else if (pin.label == PinLabel::BE) {
                    state.is_active = (bus_state & BUS_BIT(BUS_BE_BIT)) != 0;
                    state.is_output = false; // Input to CPU
                } else if (pin.label == PinLabel::BA) {
                    state.is_active = (bus_state & BUS_BIT(BUS_BA_BIT)) != 0;
                    state.is_output = true;
                }
                break;
            }
            case PinType::INTERRUPT: {
                switch (pin.label) {
                    case PinLabel::IRQ:
                        state.is_active = (bus_state & BUS_BIT(BUS_IRQ_BIT)) == 0; // Active low
                        state.is_output = false;
                        break;
                    case PinLabel::NMI:
                        state.is_active = (bus_state & BUS_BIT(BUS_NMI_BIT)) == 0; // Active low
                        state.is_output = false;
                        break;
                    case PinLabel::RES:
                        state.is_active = (bus_state & BUS_BIT(BUS_RES_BIT)) == 0; // Active low
                        state.is_output = false;
                        break;
                    case PinLabel::ABRT:
                        state.is_active = (bus_state & BUS_BIT(BUS_ABORT_BIT)) == 0; // Active low
                        state.is_output = false;
                        break;
                    default:
                        break;
                }
                break;
            }
            case PinType::POWER: {
                // Power pins always active
                state.is_active = true;
                state.is_output = false;
                break;
            }
            case PinType::CLOCK: {
                // Clock pins - would need actual clock state from bus
                // For now, assume active during valid cycles
                state.is_active = true;
                switch (pin.label) {
                    case PinLabel::PHI0:
                        state.is_output = false; // Input clock
                        break;
                    case PinLabel::PHI1:
                    case PinLabel::PHI2:
                    case PinLabel::E:
                        state.is_output = true; // Generated clocks
                        break;
                    default:
                        state.is_output = true;
                        break;
                }
                break;
            }
            case PinType::SPECIAL: {
                switch (pin.label) {
                    case PinLabel::SO:
                        state.is_active = (bus_state & BUS_BIT(BUS_SO_BIT)) == 0; // Active low
                        state.is_output = false;
                        break;
                    case PinLabel::VP:
                    case PinLabel::VPA:
                        state.is_active = (bus_state & BUS_BIT(BUS_VP_BIT)) != 0;
                        state.is_output = true;
                        break;
                    case PinLabel::VDA:
                        state.is_active = (bus_state & BUS_BIT(BUS_VDA_BIT)) != 0;
                        state.is_output = true;
                        break;
                    case PinLabel::ML:
                        state.is_active = (bus_state & BUS_BIT(BUS_ML_BIT)) == 0; // Active low
                        state.is_output = true;
                        break;
                    case PinLabel::E:
                        state.is_active = (bus_state & BUS_BIT(BUS_E_BIT)) != 0;
                        state.is_output = true;
                        break;
                    case PinLabel::MX:
                        state.is_active = (bus_state & BUS_BIT(BUS_MX_BIT)) != 0;
                        state.is_output = true;
                        break;
                    case PinLabel::NC:
                        state.is_active = false; // No connect
                        state.is_output = false;
                        state.is_tristate = true;
                        break;
                    default:
                        break;
                }
                break;
            }
            case PinType::IO_PORT: {
                // 6510 I/O port pins (P0-P5) - would need actual port state
                // For now, set to inactive (would be controlled by DDR and DATA registers)
                state.is_active = false;
                state.is_output = false; // Depends on DDR register
                state.is_tristate = true; // Can be input or output
                break;
            }
            default:
                state.is_active = false;
                break;
        }
        
        // Handle active-low pins
        if (pin.invert_logic && pin_type != PinType::INTERRUPT && pin_type != PinType::SPECIAL) {
            state.is_active = !state.is_active;
        }
        
        state.value = state.is_active ? 1 : 0;
    };
    
    // Process pins from all sides
    size_t pin_index = 0;
    for (const auto& pin : layout->left_pins) {
        process_pin(pin, pin.pin_number - 1); // Pin numbers are 1-based
    }
    for (const auto& pin : layout->right_pins) {
        process_pin(pin, pin.pin_number - 1);
    }
    for (const auto& pin : layout->top_pins) {
        process_pin(pin, pin.pin_number - 1);
    }
    for (const auto& pin : layout->bottom_pins) {
        process_pin(pin, pin.pin_number - 1);
    }    
    for (const auto& pin : layout->grid_pins) {
        process_pin(pin, pin.pin_number - 1);
    }
    
    return states;
}

// ============================================================================
// COMPACT PIN DEFINITION MACROS FOR DIP-40 LAYOUTS
// ============================================================================

// Macro to create a ChipPin with all members properly initialized using enum labels
#define MAKE_PIN(num, lbl_enum, bit, inv) \
    {num, PinLabel::lbl_enum, bit, inv, nullptr, nullptr, false, false}

// Macro to define left and right pins in one line for DIP-40 packages using enum labels
#define PIN_PAIR(left_num, left_lbl_enum, left_bit, left_inv, \
                 right_num, right_lbl_enum, right_bit, right_inv) \
    layout.left_pins.push_back(MAKE_PIN(left_num, left_lbl_enum, left_bit, left_inv)); \
    layout.right_pins.push_back(MAKE_PIN(right_num, right_lbl_enum, right_bit, right_inv));

// ============================================================================
// MOS 6502 SPECIFIC LAYOUT IMPLEMENTATION
// ============================================================================

PinLayout create_mos6502_layout() {
    // Start with DIP-40 base layout from core system
    PinLayout layout = create_dip40_layout();
   
    // Customize for MOS 6502 - assign pin labels and types
    // The create_dip40_layout() provides the physical package structure
   
    // Update package info for MOS 6502 - properly initialize all fields
    layout.markings = {
        "MOS6502",                   // part_number
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
   
    // Pin assignments for MOS 6502 (40-pin DIP) - Hardware accurate pin definitions
    PIN_PAIR(1,  VSS,   0, false,   21, VSS,   0, false)
    PIN_PAIR(2,  RDY,   0, false,   22, A12,   12, false)
    PIN_PAIR(3,  PHI1,  0, false,   23, A13,   13, false)
    PIN_PAIR(4,  IRQ,   0, true,    24, A14,   14, false)
    PIN_PAIR(5,  NC,    0, false,   25, A15,   15, false)
    PIN_PAIR(6,  NMI,   0, true,    26, D7,    7, false)
    PIN_PAIR(7,  SYNC,  0, false,   27, D6,    6, false)
    PIN_PAIR(8,  VDD,   0, false,   28, D5,    5, false)
    PIN_PAIR(9,  A0,    0, false,   29, D4,    4, false)
    PIN_PAIR(10, A1,    1, false,   30, D3,    3, false)
    PIN_PAIR(11, A2,    2, false,   31, D2,    2, false)
    PIN_PAIR(12, A3,    3, false,   32, D1,    1, false)
    PIN_PAIR(13, A4,    4, false,   33, D0,    0, false)
    PIN_PAIR(14, A5,    5, false,   34, RW,    0, false)
    PIN_PAIR(15, A6,    6, false,   35, NC,    0, false)
    PIN_PAIR(16, A7,    7, false,   36, NC,    0, false)
    PIN_PAIR(17, A8,    8, false,   37, PHI0,  0, false)
    PIN_PAIR(18, A9,    9, false,   38, SO,    0, true)
    PIN_PAIR(19, A10,   10, false,  39, PHI2,  0, false)
    PIN_PAIR(20, A11,   11, false,  40, RES,   0, true)
   
    return layout;
}

PinLayout create_mos6510_layout() {
    // Start with DIP-40 base layout from core system
    PinLayout layout = create_dip40_layout();
    
    // Update package info for MOS 6510 - properly initialize all fields
    layout.markings = {
        "MOS6510",                   // part_number
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
    
    // Pin assignments for MOS 6510 (40-pin DIP) - Hardware accurate with I/O port pins
    PIN_PAIR(1,  PHI0,  0, false,   21, VSS,   0, false)
    PIN_PAIR(2,  RDY,   0, false,   22, A11,   11, false)
    PIN_PAIR(3,  IRQ,   0, true,    23, A10,   10, false)
    PIN_PAIR(4,  NMI,   0, true,    24, P5,    5, false)
    PIN_PAIR(5,  AEC,   0, false,   25, P4,    4, false)
    PIN_PAIR(6,  VDD,   0, false,   26, P3,    3, false)
    PIN_PAIR(7,  A0,    0, false,   27, P2,    2, false)
    PIN_PAIR(8,  A1,    1, false,   28, P1,    1, false)
    PIN_PAIR(9,  A2,    2, false,   29, P0,    0, false)
    PIN_PAIR(10, A3,    3, false,   30, D7,    7, false)
    PIN_PAIR(11, A4,    4, false,   31, D6,    6, false)
    PIN_PAIR(12, A5,    5, false,   32, D5,    5, false)
    PIN_PAIR(13, A6,    6, false,   33, D4,    4, false)
    PIN_PAIR(14, A7,    7, false,   34, D3,    3, false)
    PIN_PAIR(15, A8,    8, false,   35, D2,    2, false)
    PIN_PAIR(16, A9,    9, false,   36, D1,    1, false)
    PIN_PAIR(17, A12,   12, false,  37, D0,    0, false)
    PIN_PAIR(18, A13,   13, false,  38, RW,    0, false)
    PIN_PAIR(19, A14,   14, false,  39, PHI2,  0, false)
    PIN_PAIR(20, A15,   15, false,  40, RES,   0, true)
    
    return layout;
}

PinLayout create_wdc_w65c02s_layout() {
    // Start with DIP-40 base layout from core system
    PinLayout layout = create_dip40_layout();
    
    // Update package info for WDC W65C02S - properly initialize all fields
    layout.markings = {
        "W65C02S",                   // part_number
        "Western Design Center",     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Pin assignments for WDC W65C02S (40-pin DIP) - Hardware accurate pin definitions
    PIN_PAIR(1,  VP,    0, false,   21, VSS,   0, false)
    PIN_PAIR(2,  RDY,   0, false,   22, A12,   12, false)
    PIN_PAIR(3,  PHI1,  0, false,   23, A13,   13, false)
    PIN_PAIR(4,  IRQ,   0, true,    24, A14,   14, false)
    PIN_PAIR(5,  ML,    0, true,    25, A15,   15, false)
    PIN_PAIR(6,  NMI,   0, true,    26, D7,    7, false)
    PIN_PAIR(7,  SYNC,  0, false,   27, D6,    6, false)
    PIN_PAIR(8,  VDD,   0, false,   28, D5,    5, false)
    PIN_PAIR(9,  A0,    0, false,   29, D4,    4, false)
    PIN_PAIR(10, A1,    1, false,   30, D3,    3, false)
    PIN_PAIR(11, A2,    2, false,   31, D2,    2, false)
    PIN_PAIR(12, A3,    3, false,   32, D1,    1, false)
    PIN_PAIR(13, A4,    4, false,   33, D0,    0, false)
    PIN_PAIR(14, A5,    5, false,   34, RW,    0, false)
    PIN_PAIR(15, A6,    6, false,   35, NC,    0, false)
    PIN_PAIR(16, A7,    7, false,   36, BE,    0, false)
    PIN_PAIR(17, A8,    8, false,   37, PHI0,  0, false)
    PIN_PAIR(18, A9,    9, false,   38, SO,    0, true)
    PIN_PAIR(19, A10,   10, false,  39, PHI2,  0, false)
    PIN_PAIR(20, A11,   11, false,  40, RES,   0, true)
    
    return layout;
}

PinLayout create_wdc_65c816_layout() {
    // Start with DIP-40 base layout from core system
    PinLayout layout = create_dip40_layout();
    
    // Update package info for WDC 65C816 - properly initialize all fields
    layout.markings = {
        "W65C816S",                  // part_number
        "Western Design Center",     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Pin assignments for WDC 65C816 (40-pin DIP) - Hardware accurate per datasheet
    PIN_PAIR(1,  VP,    0, false,   21, VSS,   0, false)
    PIN_PAIR(2,  RDY,   0, false,   22, A12,   12, false)
    PIN_PAIR(3,  ABRT,  0, true,    23, A13,   13, false)
    PIN_PAIR(4,  IRQ,   0, true,    24, A14,   14, false)
    PIN_PAIR(5,  ML,    0, true,    25, A15,   15, false)
    PIN_PAIR(6,  NMI,   0, true,    26, D7,    7, false)
    PIN_PAIR(7,  VPA,   0, false,   27, D6,    6, false)
    PIN_PAIR(8,  VDD,   0, false,   28, D5,    5, false)
    PIN_PAIR(9,  A0,    0, false,   29, D4,    4, false)
    PIN_PAIR(10, A1,    1, false,   30, D3,    3, false)
    PIN_PAIR(11, A2,    2, false,   31, D2,    2, false)
    PIN_PAIR(12, A3,    3, false,   32, D1,    1, false)
    PIN_PAIR(13, A4,    4, false,   33, D0,    0, false)
    PIN_PAIR(14, A5,    5, false,   34, RW,    0, false)
    PIN_PAIR(15, A6,    6, false,   35, E,     0, false)
    PIN_PAIR(16, A7,    7, false,   36, BE,    0, false)
    PIN_PAIR(17, A8,    8, false,   37, PHI0,  0, false)
    PIN_PAIR(18, A9,    9, false,   38, MX,    0, false)
    PIN_PAIR(19, A10,   10, false,  39, VDA,   0, false)
    PIN_PAIR(20, VSS,   0, false,   40, RES,   0, true)
    
    return layout;
}

PinLayout create_ricoh_2a03_layout() {
    // Start with DIP-40 base layout from core system
    PinLayout layout = create_dip40_layout();
    
    // Update package info for RICOH 2A03 (NES processor) - properly initialize all fields
    layout.markings = {
        "RP2A03",                    // part_number
        "Ricoh",                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Pin assignments for RICOH 2A03 (40-pin DIP) - Hardware accurate NES processor
    PIN_PAIR(1,  VSS,   0, false,   21, VSS,   0, false)
    PIN_PAIR(2,  RDY,   0, false,   22, A12,   12, false)
    PIN_PAIR(3,  PHI1,  0, false,   23, A13,   13, false)
    PIN_PAIR(4,  IRQ,   0, true,    24, A14,   14, false)
    PIN_PAIR(5,  NC,    0, false,   25, A15,   15, false)
    PIN_PAIR(6,  NMI,   0, true,    26, D7,    7, false)
    PIN_PAIR(7,  SYNC,  0, false,   27, D6,    6, false)
    PIN_PAIR(8,  VDD,   0, false,   28, D5,    5, false)
    PIN_PAIR(9,  A0,    0, false,   29, D4,    4, false)
    PIN_PAIR(10, A1,    1, false,   30, D3,    3, false)
    PIN_PAIR(11, A2,    2, false,   31, D2,    2, false)
    PIN_PAIR(12, A3,    3, false,   32, D1,    1, false)
    PIN_PAIR(13, A4,    4, false,   33, D0,    0, false)
    PIN_PAIR(14, A5,    5, false,   34, RW,    0, false)
    PIN_PAIR(15, A6,    6, false,   35, NC,    0, false)
    PIN_PAIR(16, A7,    7, false,   36, NC,    0, false)
    PIN_PAIR(17, A8,    8, false,   37, PHI0,  0, false)
    PIN_PAIR(18, A9,    9, false,   38, SO,    0, true)
    PIN_PAIR(19, A10,   10, false,  39, PHI2,  0, false)
    PIN_PAIR(20, A11,   11, false,  40, RES,   0, true)
    
    return layout;
}

PinLayout create_rockwell_r65c02_layout() {
    // Start with DIP-40 base layout from core system
    PinLayout layout = create_dip40_layout();
    
    // Update package info for Rockwell R65C02 - properly initialize all fields
    layout.markings = {
        "R65C02",                    // part_number
        "Rockwell",                  // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Pin assignments for Rockwell R65C02 (40-pin DIP) - Hardware accurate pin definitions
    PIN_PAIR(1,  PinLabel::VSS,   0, false,   21, PinLabel::VSS,   0, false)
    PIN_PAIR(2,  PinLabel::RDY,   0, false,   22, PinLabel::A12,   12, false)
    PIN_PAIR(3,  PinLabel::PHI1,  0, false,   23, PinLabel::A13,   13, false)
    PIN_PAIR(4,  PinLabel::IRQ,   0, true,    24, PinLabel::A14,   14, false)
    PIN_PAIR(5,  PinLabel::ML,    0, true,    25, PinLabel::A15,   15, false) // Memory Lock on R65C02
    PIN_PAIR(6,  PinLabel::NMI,   0, true,    26, PinLabel::D7,    7, false)
    PIN_PAIR(7,  PinLabel::SYNC,  0, false,   27, PinLabel::D6,    6, false)
    PIN_PAIR(8,  PinLabel::VDD,   0, false,   28, PinLabel::D5,    5, false)
    PIN_PAIR(9,  PinLabel::A0,    0, false,   29, PinLabel::D4,    4, false)
    PIN_PAIR(10, PinLabel::A1,    1, false,   30, PinLabel::D3,    3, false)
    PIN_PAIR(11, PinLabel::A2,    2, false,   31, PinLabel::D2,    2, false)
    PIN_PAIR(12, PinLabel::A3,    3, false,   32, PinLabel::D1,    1, false)
    PIN_PAIR(13, PinLabel::A4,    4, false,   33, PinLabel::D0,    0, false)
    PIN_PAIR(14, PinLabel::A5,    5, false,   34, PinLabel::RW,    0, false)
    PIN_PAIR(15, PinLabel::A6,    6, false,   35, PinLabel::NC,    0, false)
    PIN_PAIR(16, PinLabel::A7,    7, false,   36, PinLabel::BE,    0, false) // Bus Enable on R65C02
    PIN_PAIR(17, PinLabel::A8,    8, false,   37, PinLabel::PHI0,  0, false)
    PIN_PAIR(18, PinLabel::A9,    9, false,   38, PinLabel::SO,    0, true)
    PIN_PAIR(19, PinLabel::A10,   10, false,  39, PinLabel::PHI2,  0, false)
    PIN_PAIR(20, PinLabel::A11,   11, false,  40, PinLabel::RES,   0, true)
    
    return layout;
}

#endif // CPU_PIN_LAYOUTS_H