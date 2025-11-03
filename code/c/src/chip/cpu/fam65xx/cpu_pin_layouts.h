/*
 * cpu_pin_layouts.h - CPU trait-specific pin layout definitions
 * 
 * This header defines pin layouts for different CPU variants based on their
 * specific features and capabilities using CPU traits.
 */

#ifndef CPU_PIN_LAYOUTS_H
#define CPU_PIN_LAYOUTS_H

#include "chip_visualization.h"
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

// CPU pin state functions - get pin states from CPU and bus state  
template<const fam65xx::CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, bus_state_t bus_state);

// ============================================================================
// TEMPLATE FUNCTION IMPLEMENTATIONS (must be in header for templates)
// ============================================================================

#include "fam65xx.hpp"
#include "../../core/system_lines.h"
#include <type_traits>

// Generic CPU pin layout function with compile-time CPU selection
template<const fam65xx::CPUTraits& Traits>
PinLayout create_cpu_pin_layout() {
    PinLayout layout;
    
    // MOS 6502 (NMOS) PIN LAYOUT
    if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::MOS6502)>) {
        layout.package = {
            .width = 200.0f,
            .height = 400.0f,
            .total_pins = 40,
            .has_notch = true,
            .package_name = "DIP-40 (MOS 6502)"
        };
        
        // Left side pins (1-20, top to bottom)
        layout.left_pins = {
            {1,  "VSS",   PinType::POWER,     0, false},
            {2,  "RDY",   PinType::CONTROL,   0, false},
            {3,  "φ1",    PinType::CLOCK,     0, false},
            {4,  "IRQ",   PinType::INTERRUPT, 0, true},
            {5,  "NC",    PinType::SPECIAL,   0, false},
            {6,  "NMI",   PinType::INTERRUPT, 0, true},
            {7,  "SYNC",  PinType::CONTROL,   0, false},
            {8,  "VCC",   PinType::POWER,     0, false},
            {9,  "A0",    PinType::ADDRESS,   0, false},
            {10, "A1",    PinType::ADDRESS,   1, false},
            {11, "A2",    PinType::ADDRESS,   2, false},
            {12, "A3",    PinType::ADDRESS,   3, false},
            {13, "A4",    PinType::ADDRESS,   4, false},
            {14, "A5",    PinType::ADDRESS,   5, false},
            {15, "A6",    PinType::ADDRESS,   6, false},
            {16, "A7",    PinType::ADDRESS,   7, false},
            {17, "A8",    PinType::ADDRESS,   8, false},
            {18, "A9",    PinType::ADDRESS,   9, false},
            {19, "A10",   PinType::ADDRESS,   10, false},
            {20, "A11",   PinType::ADDRESS,   11, false}
        };
        
        // Right side pins (21-40, top to bottom)
        layout.right_pins = {
            {21, "VSS",   PinType::POWER,     0, false},
            {22, "A12",   PinType::ADDRESS,   12, false},
            {23, "A13",   PinType::ADDRESS,   13, false},
            {24, "A14",   PinType::ADDRESS,   14, false},
            {25, "A15",   PinType::ADDRESS,   15, false},
            {26, "D7",    PinType::DATA,      7, false},
            {27, "D6",    PinType::DATA,      6, false},
            {28, "D5",    PinType::DATA,      5, false},
            {29, "D4",    PinType::DATA,      4, false},
            {30, "D3",    PinType::DATA,      3, false},
            {31, "D2",    PinType::DATA,      2, false},
            {32, "D1",    PinType::DATA,      1, false},
            {33, "D0",    PinType::DATA,      0, false},
            {34, "RW",    PinType::CONTROL,   0, false},
            {35, "NC",    PinType::SPECIAL,   0, false}, 
            {36, "NC",    PinType::SPECIAL,   0, false}, // No BE on original 6502
            {37, "φ0",    PinType::CLOCK,     0, false},
            {38, "SO",    PinType::SPECIAL,   0, true},
            {39, "φ2",    PinType::CLOCK,     0, false},
            {40, "RES",   PinType::INTERRUPT, 0, true}
        };
        
    // TODO: Add other CPU types here - this is just a test with MOS6502
    } else {
        // Fallback for unknown CPU type
        layout.package = {
            .width = 200.0f,
            .height = 400.0f,
            .total_pins = 40,
            .has_notch = true,
            .package_name = "DIP-40 (Unknown CPU)"
        };
    }
    
    return layout;
}

// CPU pin state function with compile-time CPU selection  
template<const fam65xx::CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx::fam65xx_t<Traits>* cpu, bus_state_t bus_state) {
    std::vector<PinState> states(40); // Assume 40-pin package
    
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
    
    // Extract basic CPU state
    uint16_t address = BUS_GET_ADDR(bus_state);
    uint8_t data = BUS_GET_DATA(bus_state);
    
    // Set address pins (A0-A15)
    for (int i = 0; i < 16; i++) {
        states[8 + i] = {(address & (1 << i)) != 0, false, (uint8_t)i, false, true};  // A0-A15
    }
    
    // Set data pins (D0-D7)  
    for (int i = 0; i < 8; i++) {
        states[33 - i] = {(data & (1 << i)) != 0, false, (uint8_t)i, false, true};  // D0-D7
    }
    
    // Set control signals from bus state
    states[33] = {BUS_GET_BIT(bus_state, BUS_RW_BIT), false, 0, false, true};     // RW
    states[1] = {BUS_GET_BIT(bus_state, BUS_RDY_BIT), false, 0, false, true};     // RDY  
    states[6] = {BUS_GET_BIT(bus_state, BUS_SYNC_BIT), false, 0, false, true};    // SYNC
    states[39] = {!BUS_GET_BIT(bus_state, BUS_RES_BIT), false, 0, true, true};    // RES (active low)
    states[3] = {!BUS_GET_BIT(bus_state, BUS_IRQ_BIT), false, 0, true, true};     // IRQ (active low)
    states[5] = {!BUS_GET_BIT(bus_state, BUS_NMI_BIT), false, 0, true, true};     // NMI (active low)
    
    return states;
}

#endif // CPU_PIN_LAYOUTS_H