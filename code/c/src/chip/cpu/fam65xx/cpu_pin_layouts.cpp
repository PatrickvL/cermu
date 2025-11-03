/*
 * cpu_pin_layouts.cpp - CPU trait-specific pin layout implementations
 */

#include "cpu_pin_layouts.h"
#include "fam65xx.hpp"
#include "../../core/system_lines.h"

using namespace fam65xx;

// ============================================================================
// GENERIC CPU PIN LAYOUT FUNCTION WITH COMPILE-TIME SELECTION
// ============================================================================

template<const CPUTraits& Traits>
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
    
    // MOS 6510 (C64/C128) PIN LAYOUT  
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::MOS6510)>) {
    PinLayout layout;
    
    layout.package = {
        .width = 200.0f,
        .height = 400.0f,
        .total_pins = 40,
        .has_notch = true,
        .package_name = "DIP-40 (MOS 6510)"
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
        {35, "AEC",   PinType::CONTROL,   0, false}, // Address Enable Control - 6510 specific
        {36, "NC",    PinType::SPECIAL,   0, false},
        {37, "φ0",    PinType::CLOCK,     0, false},
        {38, "SO",    PinType::SPECIAL,   0, true},
        {39, "φ2",    PinType::CLOCK,     0, false},
        {40, "RES",   PinType::INTERRUPT, 0, true}
    };
    
    // WDC 65C02 (CMOS) PIN LAYOUT
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::WDC_W65C02S)>) {
        layout.package = {
        .width = 200.0f,
        .height = 400.0f,
        .total_pins = 40,
        .has_notch = true,
        .package_name = "DIP-40 (WDC 65C02)"
    };
    
    // Left side pins (1-20, top to bottom)
    layout.left_pins = {
        {1,  "VSS",   PinType::POWER,     0, false},
        {2,  "RDY",   PinType::CONTROL,   0, false}, // Bidirectional on 65C02
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
        {36, "BE",    PinType::CONTROL,   0, false}, // Bus Enable on some variants
        {37, "φ0",    PinType::CLOCK,     0, false},
        {38, "SO",    PinType::SPECIAL,   0, true},
        {39, "φ2",    PinType::CLOCK,     0, false},
        {40, "RES",   PinType::INTERRUPT, 0, true}
    };
    
    return layout;

    // WDC 65C816 (16-BIT) PIN LAYOUT  
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::WDC_65C816)>) {
        layout.package = {
        .width = 200.0f,
        .height = 400.0f,
        .total_pins = 40,
        .has_notch = true,
        .package_name = "DIP-40 (WDC 65C816)"
    };
    
    // Left side pins (1-20, top to bottom)
    layout.left_pins = {
        {1,  "VPB",   PinType::CONTROL,   0, false}, // Vector Pull Bar
        {2,  "RDY",   PinType::CONTROL,   0, false},
        {3,  "ABRT",  PinType::INTERRUPT, 0, true}, // Abort
        {4,  "IRQ",   PinType::INTERRUPT, 0, true},
        {5,  "ML",    PinType::CONTROL,   0, true}, // Memory Lock
        {6,  "NMI",   PinType::INTERRUPT, 0, true},
        {7,  "VP",    PinType::CONTROL,   0, false}, // Vector Pull
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
        {35, "E",     PinType::CONTROL,   0, false}, // Enable
        {36, "BE",    PinType::CONTROL,   0, false}, // Bus Enable
        {37, "PHI2",  PinType::CLOCK,     0, false},
        {38, "MX",    PinType::CONTROL,   0, false}, // Memory/Index size
        {39, "VDA",   PinType::CONTROL,   0, false}, // Valid Data Address
        {40, "RES",   PinType::INTERRUPT, 0, true}
    };
    
    // NES 6502 (RICOH 2A03) PIN LAYOUT
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::RICOH_2A03)>) {
        layout.package = {
            .width = 200.0f,
            .height = 400.0f,
            .total_pins = 40,
            .has_notch = true,
        .package_name = "DIP-40 (RICOH 2A03)"
    };
    
    // Left side pins (1-20, top to bottom)
    layout.left_pins = {
        {1,  "VSS",   PinType::POWER,     0, false},
        {2,  "RDY",   PinType::CONTROL,   0, false}, // Tied high internally in some revisions
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
        {35, "NC",    PinType::SPECIAL,   0, false}, // 2A03 specific
        {36, "NC",    PinType::SPECIAL,   0, false},
        {37, "φ0",    PinType::CLOCK,     0, false},
        {38, "SO",    PinType::SPECIAL,   0, true},
        {39, "φ2",    PinType::CLOCK,     0, false},
        {40, "RES",   PinType::INTERRUPT, 0, true}
    };
    
    // ROCKWELL R65C02 PIN LAYOUT
    } else if constexpr (std::is_same_v<std::remove_cv_t<decltype(Traits)>, decltype(fam65xx::ROCKWELL_R65C02)>) {
        layout.package = {
        .width = 200.0f,
        .height = 400.0f,
        .total_pins = 40,
        .has_notch = true,
        .package_name = "DIP-40 (Rockwell R65C02)"
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
        {36, "BE",    PinType::CONTROL,   0, false}, // Bus Enable - Rockwell variant
        {37, "φ0",    PinType::CLOCK,     0, false},
        {38, "SO",    PinType::SPECIAL,   0, true},
        {39, "φ2",    PinType::CLOCK,     0, false},
        {40, "RES",   PinType::INTERRUPT, 0, true}
    };
    
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

// ============================================================================
// CPU PIN STATE EXTRACTION WITH BUS STATE
// ============================================================================

template<const CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx_t<Traits>* cpu, bus_state_t bus_state) {
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
    
    // Extract bus state components
    uint16_t addr_bus = BUS_GET_ADDR(bus_state);
    uint8_t data_bus = BUS_GET_DATA(bus_state);
    
    // Map pins based on the pin layout for this CPU type
    PinLayout layout = create_cpu_pin_layout<Traits>();
    
    // Process each pin according to its type and position
    auto process_pin = [&](const ChipPin& pin, size_t state_index) {
        if (state_index >= states.size()) return;
        
        PinState& state = states[state_index];
        
        switch (pin.type) {
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
                if (strcmp(pin.label, "RW") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_RW_BIT)) != 0;
                    state.is_output = true;
                } else if (strcmp(pin.label, "SYNC") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_SYNC_BIT)) != 0;
                    state.is_output = true;
                } else if (strcmp(pin.label, "RDY") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_RDY_BIT)) != 0;
                    state.is_output = false; // Input to CPU
                } else if (strcmp(pin.label, "AEC") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_AEC_BIT)) != 0;
                    state.is_output = true;
                } else if (strcmp(pin.label, "BE") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_BE_BIT)) != 0;
                    state.is_output = false; // Input to CPU
                } else if (strcmp(pin.label, "BA") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_BA_BIT)) != 0;
                    state.is_output = true;
                }
                break;
            }
            case PinType::INTERRUPT: {
                if (strcmp(pin.label, "IRQ") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_IRQ_BIT)) == 0; // Active low
                    state.is_output = false;
                } else if (strcmp(pin.label, "NMI") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_NMI_BIT)) == 0; // Active low
                    state.is_output = false;
                } else if (strcmp(pin.label, "RES") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_RES_BIT)) == 0; // Active low
                    state.is_output = false;
                } else if (strcmp(pin.label, "ABRT") == 0) {
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
                if (strcmp(pin.label, "φ0") == 0) {
                    state.is_output = false; // Input clock
                } else {
                    state.is_output = true; // Generated clocks
                }
                break;
            }
            case PinType::SPECIAL: {
                if (strcmp(pin.label, "SO") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_SO_BIT)) == 0; // Active low
                    state.is_output = false;
                } else if (strcmp(pin.label, "VP") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_VP_BIT)) != 0;
                    state.is_output = true;
                } else if (strcmp(pin.label, "ML") == 0) {
                    state.is_active = (bus_state & BUS_BIT(BUS_ML_BIT)) == 0; // Active low
                    state.is_output = true;
                } else if (strcmp(pin.label, "NC") == 0) {
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
        if (pin.invert_logic && pin.type != PinType::INTERRUPT && pin.type != PinType::SPECIAL) {
            state.is_active = !state.is_active;
        }
        
        state.value = state.is_active ? 1 : 0;
    };
    
    // Process pins from all sides
    size_t pin_index = 0;
    for (const auto& pin : layout.left_pins) {
        process_pin(pin, pin.pin_number - 1); // Pin numbers are 1-based
    }
    for (const auto& pin : layout.right_pins) {
        process_pin(pin, pin.pin_number - 1);
    }
    for (const auto& pin : layout.top_pins) {
        process_pin(pin, pin.pin_number - 1);
    }
    for (const auto& pin : layout.bottom_pins) {
        process_pin(pin, pin.pin_number - 1);
    }
    
    return states;
}

// Fallback version for when bus state is not available
template<const CPUTraits& Traits>
std::vector<PinState> get_cpu_pin_states(fam65xx_t<Traits>* cpu) {
    // Create a default bus state from CPU registers
    bus_state_t bus_state = 0;
    
    if (cpu) {
        uint16_t addr_bus = cpu->get(REG_AB);
        uint8_t data_bus = cpu->get(REG_DL);
        
        BUS_SET_ADDR(bus_state, addr_bus);
        BUS_SET_DATA(bus_state, data_bus);
        
        // Set some default control signals
        bus_state |= BUS_BIT(BUS_RW_BIT);   // Default to read
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // Default to ready
        bus_state |= BUS_BIT(BUS_RES_BIT);  // Default to not reset
        bus_state |= BUS_BIT(BUS_IRQ_BIT);  // Default to no IRQ
        bus_state |= BUS_BIT(BUS_NMI_BIT);  // Default to no NMI
    }
    
    return get_cpu_pin_states<Traits>(cpu, bus_state);
}

// Explicit template instantiations - trying to match the exact linker expectations
// Based on linker errors, the calls expect &CPUTraits pattern
template PinLayout create_cpu_pin_layout<fam65xx::MOS6502>();
template PinLayout create_cpu_pin_layout<fam65xx::MOS6510>();  
template PinLayout create_cpu_pin_layout<fam65xx::WDC_W65C02S>();
template PinLayout create_cpu_pin_layout<fam65xx::WDC_65C816>();
template PinLayout create_cpu_pin_layout<fam65xx::RICOH_2A03>();
template PinLayout create_cpu_pin_layout<fam65xx::ROCKWELL_R65C02>();

template std::vector<PinState> get_cpu_pin_states<fam65xx::MOS6502>(fam65xx::fam65xx_t<fam65xx::MOS6502>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::MOS6510>(fam65xx::fam65xx_t<fam65xx::MOS6510>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::WDC_W65C02S>(fam65xx::fam65xx_t<fam65xx::WDC_W65C02S>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::WDC_65C816>(fam65xx::fam65xx_t<fam65xx::WDC_65C816>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::RICOH_2A03>(fam65xx::fam65xx_t<fam65xx::RICOH_2A03>* cpu, bus_state_t bus_state);
template std::vector<PinState> get_cpu_pin_states<fam65xx::ROCKWELL_R65C02>(fam65xx::fam65xx_t<fam65xx::ROCKWELL_R65C02>* cpu, bus_state_t bus_state);