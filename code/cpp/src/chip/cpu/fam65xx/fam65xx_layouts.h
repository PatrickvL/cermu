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
#include "../../../core/system_lines.h"

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
std::vector<PinSignalState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, ChipLayout* layout, bus_state_t bus_state);

// ============================================================================
// TEMPLATE FUNCTION IMPLEMENTATIONS (must be in header for templates)
// ============================================================================

#include "fam65xx.hpp"
#include <type_traits>

// Generic CPU pin layout function with compile-time CPU selection
template<const fam65xx::CPUTraits& Traits>
ChipLayout create_cpu_pin_layout() {
    ChipLayout layout = {};
    
    // Compare by vendor and model strings instead of types
    // MOS 6502 (NMOS) PIN LAYOUT - use create_mos6502_layout()
    if constexpr (Traits == fam65xx::MOS6502) {
        layout = create_mos6502_layout();
        
    // MOS 6510 (C64/C128) PIN LAYOUT - use create_mos6510_layout()
    } else if constexpr (Traits == fam65xx::MOS6510) {
        layout = create_mos6510_layout();
        
    // WDC 65C02 (CMOS) PIN LAYOUT - use create_wdc_w65c02s_layout()
    } else if constexpr (Traits == fam65xx::WDC_W65C02S) {
        layout = create_wdc_w65c02s_layout();
        
    // WDC 65C816 (16-BIT) PIN LAYOUT - use create_wdc_65c816_layout()
    } else if constexpr (Traits == fam65xx::WDC_65C816) {
        layout = create_wdc_65c816_layout();
        
    // NES 6502 (RICOH 2A03) PIN LAYOUT - use create_ricoh_2a03_layout()
    } else if constexpr (Traits == fam65xx::RICOH_2A03) {
        layout = create_ricoh_2a03_layout();
        
    // ROCKWELL R65C02 PIN LAYOUT - use create_rockwell_r65c02_layout()
    } else if constexpr (Traits == fam65xx::ROCKWELL_R65C02) {
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
std::vector<PinSignalState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, const ChipLayout* layout, bus_state_t bus_state) {
    // Map pins based on the pin layout for this CPU type
    // Get the actual pin layout for this CPU
    std::vector<PinSignalState> states(layout->get_total_pins());

    // Initialize all pins as inactive and valid
    for (auto& state : states) {
        state = {0, false, false, 0, false, false, false, true, 0.0f, false, 0.0f};
    }
    
    if (!cpu) {
        // Mark all states as invalid if no CPU
        for (auto& state : states) {
            state.signal_valid = false;
        }
        return states;
    }
    
    // Extract bus state components
    uint16_t addr_bus = BUS_GET_ADDR(bus_state);
    uint8_t data_bus = BUS_GET_DATA(bus_state);
    
    // Process each pin according to its type and position
    auto process_pin = [&](const ChipPin& pin, size_t state_index) {
        if (state_index >= states.size()) return;
        
        PinSignalState& state = states[state_index];
        
        switch (pin.get_pin_type()) {
            case PinType::ADDRESS: {
                uint8_t bit_index = pin.get_bit_index();
                if (bit_index < 16) {
                    state.signal_level = (addr_bus & (1 << bit_index)) != 0;
                    state.drive_direction = true;
                    state.signal_value = state.signal_level ? 1 : 0;
                }
                break;
            }
            case PinType::DATA: {
                uint8_t bit_index = pin.get_bit_index();
                if (bit_index < 8) {
                    state.signal_level = (data_bus & (1 << bit_index)) != 0;
                    state.drive_direction = (bus_state & BUS_BIT(BUS_RW_BIT)) == 0; // Output on write
                    state.signal_value = state.signal_level ? 1 : 0;
                    state.high_impedance = !state.drive_direction;
                }
                break;
            }
            case PinType::CONTROL: {
                if (pin.label == PinLabel::RW) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_RW_BIT)) != 0;
                    state.drive_direction = true;
                } else if (pin.label == PinLabel::SYNC) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_SYNC_BIT)) != 0;
                    state.drive_direction = true;
                } else if (pin.label == PinLabel::RDY) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_RDY_BIT)) != 0;
                    state.drive_direction = false; // Input to CPU
                } else if (pin.label == PinLabel::AEC) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_AEC_BIT)) != 0;
                    state.drive_direction = true;
                } else if (pin.label == PinLabel::BE) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_BE_BIT)) != 0;
                    state.drive_direction = false; // Input to CPU
                } else if (pin.label == PinLabel::BA) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_BA_BIT)) != 0;
                    state.drive_direction = true;
                }
                break;
            }
            case PinType::INTERRUPT: {
                if (pin.label == PinLabel::IRQ) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_IRQ_BIT)) == 0; // Active low
                    state.drive_direction = false;
                } else if (pin.label == PinLabel::NMI) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_NMI_BIT)) == 0; // Active low
                    state.drive_direction = false;
                } else if (pin.label == PinLabel::RES) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_RES_BIT)) == 0; // Active low
                    state.drive_direction = false;
                } else if (pin.label == PinLabel::ABORT) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_ABORT_BIT)) == 0; // Active low
                    state.drive_direction = false;
                }
                break;
            }
            case PinType::POWER: {
                // Power pins always active
                state.signal_level = true;
                state.drive_direction = false;
                break;
            }
            case PinType::CLOCK: {
                // Clock pins - would need actual clock state from bus
                // For now, assume active during valid cycles
                state.signal_level = true;
                if (pin.label == PinLabel::PHI0) {
                    state.drive_direction = false; // Input clock
                } else {
                    state.drive_direction = true; // Generated clocks
                }
                break;
            }
            case PinType::SPECIAL: {
                if (pin.label == PinLabel::SO) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_SO_BIT)) == 0; // Active low
                    state.drive_direction = false;
                } else if (pin.label == PinLabel::VP) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_VP_BIT)) != 0;
                    state.drive_direction = true;
                } else if (pin.label == PinLabel::ML) {
                    state.signal_level = (bus_state & BUS_BIT(BUS_ML_BIT)) == 0; // Active low
                    state.drive_direction = true;
                } else if (pin.label == PinLabel::NC) {
                    state.signal_level = false; // No connect
                    state.drive_direction = false;
                    state.high_impedance = true;
                }
                break;
            }
            default:
                state.signal_level = false;
                break;
        }
        
        // Handle active-low pins
        if (pin.get_invert_logic() && pin.get_pin_type() != PinType::INTERRUPT && pin.get_pin_type() != PinType::SPECIAL) {
            state.signal_level = !state.signal_level;
        }
        
        state.signal_value = state.signal_level ? 1 : 0;
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
   
    // Pin assignments for MOS 6502 (40-pin DIP) - Using simplified PIN_LR macro
    PIN_LR(layout,  1, VSS,   VSS, 21)
    PIN_LR(layout,  2, RDY,   A12, 22)
    PIN_LR(layout,  3, PHI1,  A13, 23)
    PIN_LR(layout,  4, IRQ,   A14, 24)
    PIN_LR(layout,  5, NC,    A15, 25)
    PIN_LR(layout,  6, NMI,   D7, 26)
    PIN_LR(layout,  7, SYNC,  D6, 27)
    PIN_LR(layout,  8, VDD,   D5, 28)
    PIN_LR(layout,  9, A0,    D4, 29)
    PIN_LR(layout, 10, A1,    D3, 30)
    PIN_LR(layout, 11, A2,    D2, 31)
    PIN_LR(layout, 12, A3,    D1, 32)
    PIN_LR(layout, 13, A4,    D0, 33)
    PIN_LR(layout, 14, A5,    RW, 34)
    PIN_LR(layout, 15, A6,    NC, 35)
    PIN_LR(layout, 16, A7,    NC, 36)
    PIN_LR(layout, 17, A8,    PHI0, 37)
    PIN_LR(layout, 18, A9,    SO, 38)
    PIN_LR(layout, 19, A10,   PHI2, 39)
    PIN_LR(layout, 20, A11,   RES, 40)
   
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
    PIN_LR(layout,  1, PHI0,  VSS, 21)
    PIN_LR(layout,  2, RDY,   A12, 22)
    PIN_LR(layout,  3, IRQ,   A13, 23)
    PIN_LR(layout,  4, NMI,   P0, 24)    // I/O Port bit 0
    PIN_LR(layout,  5, AEC,   P1, 25)    // I/O Port bit 1
    PIN_LR(layout,  6, VDD,   P2, 26)    // I/O Port bit 2
    PIN_LR(layout,  7, A0,    P3, 27)    // I/O Port bit 3
    PIN_LR(layout,  8, A1,    P4, 28)    // I/O Port bit 4
    PIN_LR(layout,  9, A2,    P5, 29)    // I/O Port bit 5
    PIN_LR(layout, 10, A3,    D7, 30)
    PIN_LR(layout, 11, A4,    D6, 31)
    PIN_LR(layout, 12, A5,    D5, 32)
    PIN_LR(layout, 13, A6,    D4, 33)
    PIN_LR(layout, 14, A7,    D3, 34)
    PIN_LR(layout, 15, A8,    D2, 35)
    PIN_LR(layout, 16, A9,    D1, 36)
    PIN_LR(layout, 17, A10,   D0, 37)
    PIN_LR(layout, 18, A11,   RW, 38)
    PIN_LR(layout, 19, A14,   PHI2, 39)
    PIN_LR(layout, 20, A15,   RES, 40)
    
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
    PIN_LR(layout,  1, VP,    VSS, 21)   // Vector Pull
    PIN_LR(layout,  2, RDY,   A12, 22)   // Bidirectional on 65C02
    PIN_LR(layout,  3, PHI1,  A13, 23)
    PIN_LR(layout,  4, IRQ,   A14, 24)
    PIN_LR(layout,  5, ML,    A15, 25)   // Memory Lock
    PIN_LR(layout,  6, NMI,   D7, 26)
    PIN_LR(layout,  7, SYNC,  D6, 27)
    PIN_LR(layout,  8, VDD,   D5, 28)
    PIN_LR(layout,  9, A0,    D4, 29)
    PIN_LR(layout, 10, A1,    D3, 30)
    PIN_LR(layout, 11, A2,    D2, 31)
    PIN_LR(layout, 12, A3,    D1, 32)
    PIN_LR(layout, 13, A4,    D0, 33)
    PIN_LR(layout, 14, A5,    RW, 34)
    PIN_LR(layout, 15, A6,    NC, 35)
    PIN_LR(layout, 16, A7,    BE, 36)    // Bus Enable on 65C02
    PIN_LR(layout, 17, A8,    PHI0, 37)
    PIN_LR(layout, 18, A9,    SO, 38)
    PIN_LR(layout, 19, A10,   PHI2, 39)
    PIN_LR(layout, 20, A11,   RES, 40)
    
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
    PIN_LR(layout,  1, VPB,    VSS, 21)   // Vector Pull Bar
    PIN_LR(layout,  2, RDY,    A12, 22)
    PIN_LR(layout,  3, ABORT,  A13, 23)   // Abort
    PIN_LR(layout,  4, IRQ,    A14, 24)
    PIN_LR(layout,  5, ML,     A15, 25)   // Memory Lock
    PIN_LR(layout,  6, NMI,    D7, 26)
    PIN_LR(layout,  7, VPA,    D6, 27)    // Valid Program Address
    PIN_LR(layout,  8, VDD,    D5, 28)
    PIN_LR(layout,  9, A0,     D4, 29)
    PIN_LR(layout, 10, A1,     D3, 30)
    PIN_LR(layout, 11, A2,     D2, 31)
    PIN_LR(layout, 12, A3,     D1, 32)
    PIN_LR(layout, 13, A4,     D0, 33)
    PIN_LR(layout, 14, A5,     RW, 34)
    PIN_LR(layout, 15, A6,     E, 35)      // Emulation mode
    PIN_LR(layout, 16, A7,     BE, 36)     // Bus Enable
    PIN_LR(layout, 17, A8,     PHI0, 37)
    PIN_LR(layout, 18, A9,     MX, 38)     // M/X Status
    PIN_LR(layout, 19, A10,    VDA, 39)    // Valid Data Address
    PIN_LR(layout, 20, A11,    RES, 40)
    
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
    
    // Pin assignments for RICOH 2A03 (40-pin DIP)
    PIN_LR(layout,  1, VSS,   VSS, 21)
    PIN_LR(layout,  2, RDY,   A12, 22)   // Tied high internally in some revisions
    PIN_LR(layout,  3, PHI1,  A13, 23)
    PIN_LR(layout,  4, IRQ,   A14, 24)
    PIN_LR(layout,  5, NC,    A15, 25)
    PIN_LR(layout,  6, NMI,   D7, 26)
    PIN_LR(layout,  7, SYNC,  D6, 27)
    PIN_LR(layout,  8, VDD,   D5, 28)
    PIN_LR(layout,  9, A0,    D4, 29)
    PIN_LR(layout, 10, A1,    D3, 30)
    PIN_LR(layout, 11, A2,    D2, 31)
    PIN_LR(layout, 12, A3,    D1, 32)
    PIN_LR(layout, 13, A4,    D0, 33)
    PIN_LR(layout, 14, A5,    RW, 34)
    PIN_LR(layout, 15, A6,    NC, 35)
    PIN_LR(layout, 16, A7,    NC, 36)    // No BE on 2A03
    PIN_LR(layout, 17, A8,    PHI0, 37)
    PIN_LR(layout, 18, A9,    SO, 38)
    PIN_LR(layout, 19, A10,   PHI2, 39)
    PIN_LR(layout, 20, A11,   RES, 40)
    
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
    
    // Pin assignments for Rockwell R65C02 (40-pin DIP)
    PIN_LR(layout,  1, VSS,   VSS, 21)
    PIN_LR(layout,  2, RDY,   A12, 22)
    PIN_LR(layout,  3, PHI1,  A13, 23)
    PIN_LR(layout,  4, IRQ,   A14, 24)
    PIN_LR(layout,  5, NC,    A15, 25)
    PIN_LR(layout,  6, NMI,   D7, 26)
    PIN_LR(layout,  7, SYNC,  D6, 27)
    PIN_LR(layout,  8, VDD,   D5, 28)
    PIN_LR(layout,  9, A0,    D4, 29)
    PIN_LR(layout, 10, A1,    D3, 30)
    PIN_LR(layout, 11, A2,    D2, 31)
    PIN_LR(layout, 12, A3,    D1, 32)
    PIN_LR(layout, 13, A4,    D0, 33)
    PIN_LR(layout, 14, A5,    RW, 34)
    PIN_LR(layout, 15, A6,    NC, 35)
    PIN_LR(layout, 16, A7,    BE, 36)    // Bus Enable on R65C02
    PIN_LR(layout, 17, A8,    PHI0, 37)
    PIN_LR(layout, 18, A9,    SO, 38)
    PIN_LR(layout, 19, A10,   PHI2, 39)
    PIN_LR(layout, 20, A11,   RES, 40)
    
    return layout;
}

#endif // CPU_PIN_LAYOUTS_H