/*
 * cpu_pin_layouts.h - CPU trait-specific pin layout definitions
 * 
 * This header defines pin layouts for different CPU variants based on their
 * specific features and capabilities using CPU traits.
 */

#ifndef CPU_PIN_LAYOUTS_H
#define CPU_PIN_LAYOUTS_H

#include "../../../core/chip_layout.h"
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
ChipLayout create_cpu_pin_layout();

// Specific CPU layout functions  
ChipLayout create_mos6502_layout();
ChipLayout create_mos6510_layout();
ChipLayout create_wdc_w65c02s_layout();
ChipLayout create_wdc_65c816_layout();
ChipLayout create_ricoh_2a03_layout();
ChipLayout create_rockwell_r65c02_layout();

// CPU pin state functions - get pin states from CPU and bus state  
template<const fam65xx::CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, ChipLayout* layout, bus_state_t bus_state);

// ============================================================================
// TEMPLATE FUNCTION IMPLEMENTATIONS (must be in header for templates)
// ============================================================================

#include "fam65xx.hpp"
#include "../../core/system_lines.h"
#include <type_traits>

// Generic CPU pin layout function with compile-time CPU selection
template<const fam65xx::CPUTraits& Traits>
ChipLayout create_cpu_pin_layout() {
    ChipLayout layout = {};
    
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
std::vector<PinState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, const ChipLayout* layout, bus_state_t bus_state) {
    // Map pins based on the pin layout for this CPU type
    // Get the actual pin layout for this CPU
    std::vector<PinState> states(layout->get_total_pins());

    // Initialize all pins as inactive and valid
    for (auto& state : states) {
        state = {0, false, false, 0, false, false, false, true, 0.0f, false, 0.0f};
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
        
        switch (pin.get_pin_type()) {
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
                if (pin.label == PinLabel::IRQ) {
                    state.is_active = (bus_state & BUS_BIT(BUS_IRQ_BIT)) == 0; // Active low
                    state.is_output = false;
                } else if (pin.label == PinLabel::NMI) {
                    state.is_active = (bus_state & BUS_BIT(BUS_NMI_BIT)) == 0; // Active low
                    state.is_output = false;
                } else if (pin.label == PinLabel::RES) {
                    state.is_active = (bus_state & BUS_BIT(BUS_RES_BIT)) == 0; // Active low
                    state.is_output = false;
                } else if (pin.label == PinLabel::ABORT) {
                    state.is_active = (bus_state & BUS_BIT(BUS_ABORT_BIT)) == 0; // Active low
                    state.is_output = false;
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
                if (pin.label == PinLabel::PHI0) {
                    state.is_output = false; // Input clock
                } else {
                    state.is_output = true; // Generated clocks
                }
                break;
            }
            case PinType::SPECIAL: {
                if (pin.label == PinLabel::SO) {
                    state.is_active = (bus_state & BUS_BIT(BUS_SO_BIT)) == 0; // Active low
                    state.is_output = false;
                } else if (pin.label == PinLabel::VP) {
                    state.is_active = (bus_state & BUS_BIT(BUS_VP_BIT)) != 0;
                    state.is_output = true;
                } else if (pin.label == PinLabel::ML) {
                    state.is_active = (bus_state & BUS_BIT(BUS_ML_BIT)) == 0; // Active low
                    state.is_output = true;
                } else if (pin.label == PinLabel::NC) {
                    state.is_active = false; // No connect
                    state.is_output = false;
                    state.is_tristate = true;
                }
                break;
            }
            default:
                state.is_active = false;
                break;
        }
        
        // Handle active-low pins
        if (pin.invert_logic && pin.get_pin_type() != PinType::INTERRUPT && pin.get_pin_type() != PinType::SPECIAL) {
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

// Note: PIN and PIN_LR macros are now defined in core/chip_layout.h

// ============================================================================
// MOS 6502 SPECIFIC LAYOUT IMPLEMENTATION
// ============================================================================

ChipLayout create_mos6502_layout() {
    // Start with DIP-40 base layout from core system
    ChipLayout layout = create_dip40_layout();
   
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
   
    // Pin assignments for MOS 6502 (40-pin DIP) - Using improved PIN_LR macro
    // PIN_LR(left_num, left_lbl_enum, left_inv, right_num, right_lbl_enum, right_inv, right_bit, left_bit)
    PIN_LR(1,  VSS,   false,   21, VSS,   false,   0, 0)
    PIN_LR(2,  RDY,   false,   22, A12,   false,   12, 0)
    PIN_LR(3,  PHI1,  false,   23, A13,   false,   13, 0)
    PIN_LR(4,  IRQ,   true,    24, A14,   false,   14, 0)
    PIN_LR(5,  NC,    false,   25, A15,   false,   15, 0)
    PIN_LR(6,  NMI,   true,    26, D7,    false,   7, 0)
    PIN_LR(7,  SYNC,  false,   27, D6,    false,   6, 0)
    PIN_LR(8,  VDD,   false,   28, D5,    false,   5, 0)
    PIN_LR(9,  A0,    false,   29, D4,    false,   4, 0)
    PIN_LR(10, A1,    false,   30, D3,    false,   3, 1)
    PIN_LR(11, A2,    false,   31, D2,    false,   2, 2)
    PIN_LR(12, A3,    false,   32, D1,    false,   1, 3)
    PIN_LR(13, A4,    false,   33, D0,    false,   0, 4)
    PIN_LR(14, A5,    false,   34, RW,    false,   0, 5)
    PIN_LR(15, A6,    false,   35, NC,    false,   0, 6)
    PIN_LR(16, A7,    false,   36, NC,    false,   0, 7)
    PIN_LR(17, A8,    false,   37, PHI0,  false,   0, 8)
    PIN_LR(18, A9,    false,   38, SO,    true,    0, 9)
    PIN_LR(19, A10,   false,   39, PHI2,  false,   0, 10)
    PIN_LR(20, A11,   false,   40, RES,   true,    0, 11)
   
    return layout;
}

ChipLayout create_mos6510_layout() {
    // Start with DIP-40 base layout from core system
    ChipLayout layout = create_dip40_layout();
    
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
    
    // Pin assignments for MOS 6510 (40-pin DIP) - Hardware accurate per documentation
    // PIN_LR(left_num, left_lbl_enum, left_inv, right_num, right_lbl_enum, right_inv, right_bit, left_bit)
    PIN_LR(1,  PHI0,  false,   21, VSS,   false,   0, 0)
    PIN_LR(2,  RDY,   false,   22, A12,   false,   12, 0)
    PIN_LR(3,  IRQ,   true,    23, A13,   false,   13, 0)
    PIN_LR(4,  NMI,   true,    24, P0,    false,   0, 0) // I/O Port bit 0
    PIN_LR(5,  AEC,   false,   25, P1,    false,   1, 0) // I/O Port bit 1
    PIN_LR(6,  VDD,   false,   26, P2,    false,   2, 0) // I/O Port bit 2
    PIN_LR(7,  A0,    false,   27, P3,    false,   3, 0) // I/O Port bit 3
    PIN_LR(8,  A1,    false,   28, P4,    false,   4, 1) // I/O Port bit 4
    PIN_LR(9,  A2,    false,   29, P5,    false,   5, 2) // I/O Port bit 5
    PIN_LR(10, A3,    false,   30, D7,    false,   7, 3)
    PIN_LR(11, A4,    false,   31, D6,    false,   6, 4)
    PIN_LR(12, A5,    false,   32, D5,    false,   5, 5)
    PIN_LR(13, A6,    false,   33, D4,    false,   4, 6)
    PIN_LR(14, A7,    false,   34, D3,    false,   3, 7)
    PIN_LR(15, A8,    false,   35, D2,    false,   2, 8)
    PIN_LR(16, A9,    false,   36, D1,    false,   1, 9)
    PIN_LR(17, A10,   false,   37, D0,    false,   0, 10)
    PIN_LR(18, A11,   false,   38, RW,    false,   0, 11)
    PIN_LR(19, A14,   false,   39, PHI2,  false,   0, 14)
    PIN_LR(20, A15,   false,   40, RES,   true,    0, 15)
    
    return layout;
}

ChipLayout create_wdc_w65c02s_layout() {
    // Start with DIP-40 base layout from core system
    ChipLayout layout = create_dip40_layout();
    
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
    
    // Pin assignments for WDC W65C02S (40-pin DIP) - Hardware accurate per documentation
    // PIN_LR(left_num, left_lbl_enum, left_inv, right_num, right_lbl_enum, right_inv, right_bit, left_bit)
    PIN_LR(1,  VP,    false,   21, VSS,   false,   0, 0) // Vector Pull
    PIN_LR(2,  RDY,   false,   22, A12,   false,   12, 0) // Bidirectional on 65C02
    PIN_LR(3,  PHI1,  false,   23, A13,   false,   13, 0)
    PIN_LR(4,  IRQ,   true,    24, A14,   false,   14, 0)
    PIN_LR(5,  ML,    true,    25, A15,   false,   15, 0) // Memory Lock
    PIN_LR(6,  NMI,   true,    26, D7,    false,   7, 0)
    PIN_LR(7,  SYNC,  false,   27, D6,    false,   6, 0)
    PIN_LR(8,  VDD,   false,   28, D5,    false,   5, 0)
    PIN_LR(9,  A0,    false,   29, D4,    false,   4, 0)
    PIN_LR(10, A1,    false,   30, D3,    false,   3, 1)
    PIN_LR(11, A2,    false,   31, D2,    false,   2, 2)
    PIN_LR(12, A3,    false,   32, D1,    false,   1, 3)
    PIN_LR(13, A4,    false,   33, D0,    false,   0, 4)
    PIN_LR(14, A5,    false,   34, RW,    false,   0, 5)
    PIN_LR(15, A6,    false,   35, NC,    false,   0, 6)
    PIN_LR(16, A7,    false,   36, BE,    false,   0, 7) // Bus Enable on 65C02
    PIN_LR(17, A8,    false,   37, PHI0,  false,   0, 8)
    PIN_LR(18, A9,    false,   38, SO,    true,    0, 9)
    PIN_LR(19, A10,   false,   39, PHI2,  false,   0, 10)
    PIN_LR(20, A11,   false,   40, RES,   true,    0, 11)
    
    return layout;
}

ChipLayout create_wdc_65c816_layout() {
    // Start with DIP-40 base layout from core system
    ChipLayout layout = create_dip40_layout();
    
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
    
    // Pin assignments for WDC 65C816 (40-pin DIP) - Hardware accurate per documentation
    // PIN_LR(left_num, left_lbl_enum, left_inv, right_num, right_lbl_enum, right_inv, right_bit, left_bit)
    PIN_LR(1,  VP,    false,   21, VSS,   false,   0, 0) // Vector Pull
    PIN_LR(2,  RDY,   false,   22, A12,   false,   12, 0)
    PIN_LR(3,  ABORT, true,    23, A13,   false,   13, 0) // Abort
    PIN_LR(4,  IRQ,   true,    24, A14,   false,   14, 0)
    PIN_LR(5,  ML,    true,    25, A15,   false,   15, 0) // Memory Lock
    PIN_LR(6,  NMI,   true,    26, D7,    false,   7, 0)
    PIN_LR(7,  VPA,   false,   27, D6,    false,   6, 0) // Valid Program Address
    PIN_LR(8,  VDD,   false,   28, D5,    false,   5, 0)
    PIN_LR(9,  A0,    false,   29, D4,    false,   4, 0)
    PIN_LR(10, A1,    false,   30, D3,    false,   3, 1)
    PIN_LR(11, A2,    false,   31, D2,    false,   2, 2)
    PIN_LR(12, A3,    false,   32, D1,    false,   1, 3)
    PIN_LR(13, A4,    false,   33, D0,    false,   0, 4)
    PIN_LR(14, A5,    false,   34, RW,    false,   0, 5)
    PIN_LR(15, A6,    false,   35, E,     false,   0, 6) // Emulation mode
    PIN_LR(16, A7,    false,   36, BE,    false,   0, 7) // Bus Enable
    PIN_LR(17, A8,    false,   37, PHI0,  false,   0, 8)
    PIN_LR(18, A9,    false,   38, MX,    false,   0, 9) // M/X Status
    PIN_LR(19, A10,   false,   39, PHI2,  false,   0, 10) // Corrected to PHI2 per docs
    PIN_LR(20, A11,   false,   40, RES,   true,    0, 11)
    
    return layout;
}

ChipLayout create_ricoh_2a03_layout() {
    // Start with DIP-40 base layout from core system
    ChipLayout layout = create_dip40_layout();
    
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
    
    // Pin assignments for RICOH 2A03 (40-pin DIP) - 20 lines of compact pin definitions
    // PIN_LR(left_num, left_lbl_enum, left_inv, right_num, right_lbl_enum, right_inv, right_bit, left_bit)
    PIN_LR(1,  VSS,   false,   21, VSS,   false,   0, 0)
    PIN_LR(2,  RDY,   false,   22, A12,   false,   12, 0) // Tied high internally in some revisions
    PIN_LR(3,  PHI1,  false,   23, A13,   false,   13, 0)
    PIN_LR(4,  IRQ,   true,    24, A14,   false,   14, 0)
    PIN_LR(5,  NC,    false,   25, A15,   false,   15, 0)
    PIN_LR(6,  NMI,   true,    26, D7,    false,   7, 0)
    PIN_LR(7,  SYNC,  false,   27, D6,    false,   6, 0)
    PIN_LR(8,  VDD,   false,   28, D5,    false,   5, 0)
    PIN_LR(9,  A0,    false,   29, D4,    false,   4, 0)
    PIN_LR(10, A1,    false,   30, D3,    false,   3, 1)
    PIN_LR(11, A2,    false,   31, D2,    false,   2, 2)
    PIN_LR(12, A3,    false,   32, D1,    false,   1, 3)
    PIN_LR(13, A4,    false,   33, D0,    false,   0, 4)
    PIN_LR(14, A5,    false,   34, RW,    false,   0, 5)
    PIN_LR(15, A6,    false,   35, NC,    false,   0, 6)
    PIN_LR(16, A7,    false,   36, NC,    false,   0, 7) // No BE on 2A03
    PIN_LR(17, A8,    false,   37, PHI0,  false,   0, 8)
    PIN_LR(18, A9,    false,   38, SO,    true,    0, 9)
    PIN_LR(19, A10,   false,   39, PHI2,  false,   0, 10)
    PIN_LR(20, A11,   false,   40, RES,   true,    0, 11)
    
    return layout;
}

ChipLayout create_rockwell_r65c02_layout() {
    // Start with DIP-40 base layout from core system
    ChipLayout layout = create_dip40_layout();
    
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
    
    // Pin assignments for Rockwell R65C02 (40-pin DIP) - 20 lines of compact pin definitions
    // PIN_LR(left_num, left_lbl_enum, left_inv, right_num, right_lbl_enum, right_inv, right_bit, left_bit)
    PIN_LR(1,  VSS,   false,   21, VSS,   false,   0, 0)
    PIN_LR(2,  RDY,   false,   22, A12,   false,   12, 0)
    PIN_LR(3,  PHI1,  false,   23, A13,   false,   13, 0)
    PIN_LR(4,  IRQ,   true,    24, A14,   false,   14, 0)
    PIN_LR(5,  NC,    false,   25, A15,   false,   15, 0)
    PIN_LR(6,  NMI,   true,    26, D7,    false,   7, 0)
    PIN_LR(7,  SYNC,  false,   27, D6,    false,   6, 0)
    PIN_LR(8,  VDD,   false,   28, D5,    false,   5, 0)
    PIN_LR(9,  A0,    false,   29, D4,    false,   4, 0)
    PIN_LR(10, A1,    false,   30, D3,    false,   3, 1)
    PIN_LR(11, A2,    false,   31, D2,    false,   2, 2)
    PIN_LR(12, A3,    false,   32, D1,    false,   1, 3)
    PIN_LR(13, A4,    false,   33, D0,    false,   0, 4)
    PIN_LR(14, A5,    false,   34, RW,    false,   0, 5)
    PIN_LR(15, A6,    false,   35, NC,    false,   0, 6)
    PIN_LR(16, A7,    false,   36, BE,    false,   0, 7) // Bus Enable on R65C02
    PIN_LR(17, A8,    false,   37, PHI0,  false,   0, 8)
    PIN_LR(18, A9,    false,   38, SO,    true,    0, 9)
    PIN_LR(19, A10,   false,   39, PHI2,  false,   0, 10)
    PIN_LR(20, A11,   false,   40, RES,   true,    0, 11)
    
    return layout;
}

#endif // CPU_PIN_LAYOUTS_H