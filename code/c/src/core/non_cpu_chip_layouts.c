/*
 * non_cpu_chip_layouts.c - Implementation of pin layouts for non-CPU chips
 * 
 * This file implements hardware-accurate pin layouts for video, audio, I/O,
 * memory, and logic chips used in vintage computer systems.
 */

#include "non_cpu_chip_layouts.h"
#include "pin_labels.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// HELPER MACROS FOR PIN DEFINITION
// ============================================================================

// Macro to create a ChipPin with enum label
#define MAKE_PIN_ENUM(num, lbl_enum, bit, inv) \
    {num, lbl_enum, bit, inv, nullptr, nullptr, false, false}

// Macro to define left and right pins for DIP packages
#define PIN_PAIR_ENUM(left_num, left_lbl, left_bit, left_inv, \
                      right_num, right_lbl, right_bit, right_inv) \
    layout.left_pins.push_back(MAKE_PIN_ENUM(left_num, left_lbl, left_bit, left_inv)); \
    layout.right_pins.push_back(MAKE_PIN_ENUM(right_num, right_lbl, right_bit, right_inv));

// ============================================================================
// VIDEO CHIP LAYOUTS
// ============================================================================

PinLayout create_mos6567_layout() {
    // Start with DIP-40 base layout
    PinLayout layout = create_dip40_layout();
    
    // Update markings for MOS 6567 VIC-II (NTSC)
    layout.markings = {
        "MOS6567",                   // part_number
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
    
    // Pin assignments for MOS 6567 VIC-II (40-pin DIP)
    // Based on VIC-II datasheet and schematics
    PIN_PAIR_ENUM(1,  PinLabel::VDD,    0, false,  21, PinLabel::VSS,    0, false)  // Power
    PIN_PAIR_ENUM(2,  PinLabel::PHI0,   0, false,  22, PinLabel::A5,     5, false)  // Clock / Address
    PIN_PAIR_ENUM(3,  PinLabel::AEC,    0, false,  23, PinLabel::A4,     4, false)  // Address Enable / Address
    PIN_PAIR_ENUM(4,  PinLabel::BA,     0, false,  24, PinLabel::A3,     3, false)  // Bus Available / Address
    PIN_PAIR_ENUM(5,  PinLabel::RW,     0, false,  25, PinLabel::A2,     2, false)  // Read/Write / Address
    PIN_PAIR_ENUM(6,  PinLabel::IRQ,    0, true,   26, PinLabel::A1,     1, false)  // Interrupt / Address
    PIN_PAIR_ENUM(7,  PinLabel::A6,     6, false,  27, PinLabel::A0,     0, false)  // Address lines
    PIN_PAIR_ENUM(8,  PinLabel::A7,     7, false,  28, PinLabel::D7,     7, false)  // Address / Data
    PIN_PAIR_ENUM(9,  PinLabel::A8,     8, false,  29, PinLabel::D6,     6, false)  // Address / Data
    PIN_PAIR_ENUM(10, PinLabel::A9,     9, false,  30, PinLabel::D5,     5, false)  // Address / Data
    PIN_PAIR_ENUM(11, PinLabel::A10,   10, false,  31, PinLabel::D4,     4, false)  // Address / Data
    PIN_PAIR_ENUM(12, PinLabel::A11,   11, false,  32, PinLabel::D3,     3, false)  // Address / Data
    PIN_PAIR_ENUM(13, PinLabel::A12,   12, false,  33, PinLabel::D2,     2, false)  // Address / Data
    PIN_PAIR_ENUM(14, PinLabel::A13,   13, false,  34, PinLabel::D1,     1, false)  // Address / Data
    PIN_PAIR_ENUM(15, PinLabel::CAS,    0, true,   35, PinLabel::D0,     0, false)  // CAS / Data
    PIN_PAIR_ENUM(16, PinLabel::RAS,    0, true,   36, PinLabel::PHI2,   0, false)  // RAS / Clock
    PIN_PAIR_ENUM(17, PinLabel::LUMA,   0, false,  37, PinLabel::COLOR,  0, false)  // Video outputs
    PIN_PAIR_ENUM(18, PinLabel::CHROMA, 0, false,  38, PinLabel::CS,     0, true)   // Video / Chip Select
    PIN_PAIR_ENUM(19, PinLabel::SYNC,   0, false,  39, PinLabel::SOUND,  0, false)  // Sync / Audio
    PIN_PAIR_ENUM(20, PinLabel::VSS,    0, false,  40, PinLabel::VCC,    0, false)  // Ground / Power
    
    return layout;
}

PinLayout create_mos6569_layout() {
    // MOS 6569 is pin-compatible with 6567 but PAL timing
    PinLayout layout = create_mos6567_layout();
    
    // Update part number for PAL version
    layout.markings.part_number = "MOS6569";
    
    return layout;
}

// ============================================================================
// AUDIO CHIP LAYOUTS
// ============================================================================

PinLayout create_mos6581_layout() {
    // Start with DIP-28 base layout
    PinLayout layout = create_dip28_layout();
    
    // Update markings for MOS 6581 SID
    layout.markings = {
        "MOS6581",                   // part_number
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
    
    // Pin assignments for MOS 6581 SID (28-pin DIP)
    // Based on SID datasheet
    PIN_PAIR_ENUM(1,  PinLabel::CAP1A,  0, false,  15, PinLabel::D0,     0, false)  // Filter cap / Data
    PIN_PAIR_ENUM(2,  PinLabel::CAP1B,  0, false,  16, PinLabel::D1,     1, false)  // Filter cap / Data
    PIN_PAIR_ENUM(3,  PinLabel::CAP2A,  0, false,  17, PinLabel::D2,     2, false)  // Filter cap / Data
    PIN_PAIR_ENUM(4,  PinLabel::CAP2B,  0, false,  18, PinLabel::D3,     3, false)  // Filter cap / Data
    PIN_PAIR_ENUM(5,  PinLabel::RES,    0, true,   19, PinLabel::D4,     4, false)  // Reset / Data
    PIN_PAIR_ENUM(6,  PinLabel::PHI2,   0, false,  20, PinLabel::D5,     5, false)  // Clock / Data
    PIN_PAIR_ENUM(7,  PinLabel::RW,     0, false,  21, PinLabel::D6,     6, false)  // Read/Write / Data
    PIN_PAIR_ENUM(8,  PinLabel::CS,     0, true,   22, PinLabel::D7,     7, false)  // Chip Select / Data
    PIN_PAIR_ENUM(9,  PinLabel::A0,     0, false,  23, PinLabel::POTY,   0, false)  // Address / Paddle Y
    PIN_PAIR_ENUM(10, PinLabel::A1,     1, false,  24, PinLabel::POTX,   0, false)  // Address / Paddle X
    PIN_PAIR_ENUM(11, PinLabel::A2,     2, false,  25, PinLabel::VCC,    0, false)  // Address / Power
    PIN_PAIR_ENUM(12, PinLabel::A3,     3, false,  26, PinLabel::EXT_IN, 0, false)  // Address / External Input
    PIN_PAIR_ENUM(13, PinLabel::A4,     4, false,  27, PinLabel::AUDIO_OUT, 0, false) // Address / Audio Output
    PIN_PAIR_ENUM(14, PinLabel::VSS,    0, false,  28, PinLabel::VDD,    0, false)  // Ground / Power
    
    return layout;
}

// ============================================================================
// I/O CHIP LAYOUTS
// ============================================================================

PinLayout create_mos6526_layout() {
    // Start with DIP-40 base layout
    PinLayout layout = create_dip40_layout();
    
    // Update markings for MOS 6526 CIA
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
    
    // Pin assignments for MOS 6526 CIA (40-pin DIP)
    // Based on CIA datasheet
    PIN_PAIR_ENUM(1,  PinLabel::VSS,    0, false,  21, PinLabel::VCC,    0, false)  // Ground / Power
    PIN_PAIR_ENUM(2,  PinLabel::PA0,    0, false,  22, PinLabel::PB7,    7, false)  // Port A/B bits
    PIN_PAIR_ENUM(3,  PinLabel::PA1,    1, false,  23, PinLabel::PB6,    6, false)  // Port A/B bits
    PIN_PAIR_ENUM(4,  PinLabel::PA2,    2, false,  24, PinLabel::PB5,    5, false)  // Port A/B bits
    PIN_PAIR_ENUM(5,  PinLabel::PA3,    3, false,  25, PinLabel::PB4,    4, false)  // Port A/B bits
    PIN_PAIR_ENUM(6,  PinLabel::PA4,    4, false,  26, PinLabel::PB3,    3, false)  // Port A/B bits
    PIN_PAIR_ENUM(7,  PinLabel::PA5,    5, false,  27, PinLabel::PB2,    2, false)  // Port A/B bits
    PIN_PAIR_ENUM(8,  PinLabel::PA6,    6, false,  28, PinLabel::PB1,    1, false)  // Port A/B bits
    PIN_PAIR_ENUM(9,  PinLabel::PA7,    7, false,  29, PinLabel::PB0,    0, false)  // Port A/B bits
    PIN_PAIR_ENUM(10, PinLabel::PB0,    0, false,  30, PinLabel::PA7,    7, false)  // Port B/A bits (mirror)
    PIN_PAIR_ENUM(11, PinLabel::PB1,    1, false,  31, PinLabel::PA6,    6, false)  // Port B/A bits
    PIN_PAIR_ENUM(12, PinLabel::PB2,    2, false,  32, PinLabel::PA5,    5, false)  // Port B/A bits
    PIN_PAIR_ENUM(13, PinLabel::PB3,    3, false,  33, PinLabel::PA4,    4, false)  // Port B/A bits
    PIN_PAIR_ENUM(14, PinLabel::PB4,    4, false,  34, PinLabel::PA3,    3, false)  // Port B/A bits
    PIN_PAIR_ENUM(15, PinLabel::PB5,    5, false,  35, PinLabel::PA2,    2, false)  // Port B/A bits
    PIN_PAIR_ENUM(16, PinLabel::PB6,    6, false,  36, PinLabel::PA1,    1, false)  // Port B/A bits
    PIN_PAIR_ENUM(17, PinLabel::PB7,    7, false,  37, PinLabel::PA0,    0, false)  // Port B/A bits
    PIN_PAIR_ENUM(18, PinLabel::PC,     0, false,  38, PinLabel::PHI2,   0, false)  // PC pin / Clock
    PIN_PAIR_ENUM(19, PinLabel::TOD,    0, false,  39, PinLabel::FLAG,   0, true)   // Time of Day / Flag
    PIN_PAIR_ENUM(20, PinLabel::VCC,    0, false,  40, PinLabel::IRQ,    0, true)   // Power / Interrupt
    
    return layout;
}

// ============================================================================
// MEMORY CHIP LAYOUTS
// ============================================================================

PinLayout create_mos2114_layout() {
    // Start with DIP-18 base layout
    PinLayout layout = create_dip18_layout();
    
    // Update markings for MOS 2114 SRAM
    layout.markings = {
        "MOS2114",                   // part_number
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
    
    // Pin assignments for MOS 2114 1K x 4 SRAM (18-pin DIP)
    PIN_PAIR_ENUM(1,  PinLabel::A6,     6, false,  10, PinLabel::A7,     7, false)  // Address lines
    PIN_PAIR_ENUM(2,  PinLabel::A5,     5, false,  11, PinLabel::A8,     8, false)  // Address lines
    PIN_PAIR_ENUM(3,  PinLabel::A4,     4, false,  12, PinLabel::A9,     9, false)  // Address lines
    PIN_PAIR_ENUM(4,  PinLabel::A3,     3, false,  13, PinLabel::WE,     0, true)   // Address / Write Enable
    PIN_PAIR_ENUM(5,  PinLabel::A0,     0, false,  14, PinLabel::CS,     0, true)   // Address / Chip Select
    PIN_PAIR_ENUM(6,  PinLabel::A1,     1, false,  15, PinLabel::D3,     3, false)  // Address / Data
    PIN_PAIR_ENUM(7,  PinLabel::A2,     2, false,  16, PinLabel::D2,     2, false)  // Address / Data
    PIN_PAIR_ENUM(8,  PinLabel::D0,     0, false,  17, PinLabel::D1,     1, false)  // Data lines
    PIN_PAIR_ENUM(9,  PinLabel::VSS,    0, false,  18, PinLabel::VCC,    0, false)  // Ground / Power
    
    return layout;
}

// ============================================================================
// LOGIC CHIP LAYOUTS
// ============================================================================

PinLayout create_74ls139_layout() {
    // Start with DIP-16 base layout
    PinLayout layout = create_dip16_layout();
    
    // Update markings for 74LS139
    layout.markings = {
        "74LS139",                   // part_number
        "Texas Instruments",         // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Pin assignments for 74LS139 Dual 2-to-4 Decoder (16-pin DIP)
    PIN_PAIR_ENUM(1,  PinLabel::G1,     0, true,   9,  PinLabel::G2,     0, true)   // Enable inputs
    PIN_PAIR_ENUM(2,  PinLabel::A1,     0, false,  10, PinLabel::A2,     1, false)  // Address inputs
    PIN_PAIR_ENUM(3,  PinLabel::B1,     1, false,  11, PinLabel::B2,     1, false)  // Address inputs
    PIN_PAIR_ENUM(4,  PinLabel::Y0,     0, true,   12, PinLabel::Y4,     4, true)   // Outputs decoder 1/2
    PIN_PAIR_ENUM(5,  PinLabel::Y1,     1, true,   13, PinLabel::Y5,     5, true)   // Outputs decoder 1/2
    PIN_PAIR_ENUM(6,  PinLabel::Y2,     2, true,   14, PinLabel::Y6,     6, true)   // Outputs decoder 1/2
    PIN_PAIR_ENUM(7,  PinLabel::Y3,     3, true,   15, PinLabel::Y7,     7, true)   // Outputs decoder 1/2
    PIN_PAIR_ENUM(8,  PinLabel::VSS,    0, false,  16, PinLabel::VCC,    0, false)  // Ground / Power
    
    return layout;
}

PinLayout create_c64_pla_layout() {
    // Start with DIP-28 base layout
    PinLayout layout = create_dip28_layout();
    
    // Update markings for C64 PLA
    layout.markings = {
        "906114-01",                 // part_number (C64 PLA part number)
        "Commodore",                 // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Pin assignments for C64 PLA (28-pin DIP)
    // Based on C64 schematics and PLA analysis
    PIN_PAIR_ENUM(1,  PinLabel::A15,   15, false,  15, PinLabel::CASRAM, 0, true)   // Address / CAS RAM
    PIN_PAIR_ENUM(2,  PinLabel::A14,   14, false,  16, PinLabel::BASIC,  0, true)   // Address / BASIC ROM
    PIN_PAIR_ENUM(3,  PinLabel::A13,   13, false,  17, PinLabel::KERNAL, 0, true)   // Address / KERNAL ROM
    PIN_PAIR_ENUM(4,  PinLabel::A12,   12, false,  18, PinLabel::CHAROM, 0, true)   // Address / CHAR ROM
    PIN_PAIR_ENUM(5,  PinLabel::BA,     0, false,  19, PinLabel::GR_W,   0, true)   // Bus Available / Graphics Write
    PIN_PAIR_ENUM(6,  PinLabel::AEC,    0, false,  20, PinLabel::IO,     0, true)   // Address Enable / I/O Select
    PIN_PAIR_ENUM(7,  PinLabel::P0,     0, false,  21, PinLabel::ROML,   0, true)   // Port 0 / ROM Low
    PIN_PAIR_ENUM(8,  PinLabel::P1,     1, false,  22, PinLabel::ROMH,   0, true)   // Port 1 / ROM High  
    PIN_PAIR_ENUM(9,  PinLabel::P2,     2, false,  23, PinLabel::GAME,   0, true)   // Port 2 / Game
    PIN_PAIR_ENUM(10, PinLabel::CHAREN, 0, false,  24, PinLabel::EXROM,  0, true)   // CHAREN / EXROM
    PIN_PAIR_ENUM(11, PinLabel::HIRAM,  0, false,  25, PinLabel::R_W,    0, false)  // HIRAM / Read/Write
    PIN_PAIR_ENUM(12, PinLabel::LORAM,  0, false,  26, PinLabel::PHI2,   0, false)  // LORAM / Clock
    PIN_PAIR_ENUM(13, PinLabel::CAS,    0, true,   27, PinLabel::A8,     8, false)  // CAS / Address
    PIN_PAIR_ENUM(14, PinLabel::VSS,    0, false,  28, PinLabel::VCC,    0, false)  // Ground / Power
    
    return layout;
}

// ============================================================================
// PIN STATE EXTRACTION FOR NON-CPU CHIPS
// ============================================================================

std::vector<PinState> get_video_chip_pin_states(void* chip, const PinLayout* layout, bus_state_t bus_state) {
    std::vector<PinState> states(layout->get_total_pins());
    
    // Initialize all pins as inactive and valid
    for (auto& state : states) {
        state = {false, false, 0, false, true};
    }
    
    if (!chip) {
        for (auto& state : states) {
            state.is_valid = false;
        }
        return states;
    }
    
    // Extract bus state components
    uint16_t addr_bus = BUS_GET_ADDR(bus_state);
    uint8_t data_bus = BUS_GET_DATA(bus_state);
    
    // Process each pin according to its label
    auto process_pin = [&](const ChipPin& pin, size_t state_index) {
        if (state_index >= states.size()) return;
        
        PinState& state = states[state_index];
        
        switch (pin.label) {
            case PinLabel::A0: case PinLabel::A1: case PinLabel::A2: case PinLabel::A3:
            case PinLabel::A4: case PinLabel::A5: case PinLabel::A6: case PinLabel::A7:
            case PinLabel::A8: case PinLabel::A9: case PinLabel::A10: case PinLabel::A11:
            case PinLabel::A12: case PinLabel::A13: case PinLabel::A14: case PinLabel::A15:
                if (pin.bit_index < 16) {
                    state.is_active = (addr_bus & (1 << pin.bit_index)) != 0;
                    state.is_output = false; // Address inputs to video chip
                    state.value = state.is_active ? 1 : 0;
                }
                break;
                
            case PinLabel::D0: case PinLabel::D1: case PinLabel::D2: case PinLabel::D3:
            case PinLabel::D4: case PinLabel::D5: case PinLabel::D6: case PinLabel::D7:
                if (pin.bit_index < 8) {
                    state.is_active = (data_bus & (1 << pin.bit_index)) != 0;
                    state.is_output = (bus_state & BUS_BIT(BUS_RW_BIT)) != 0; // Input on read, output on write
                    state.value = state.is_active ? 1 : 0;
                    state.is_tristate = true;
                }
                break;
                
            case PinLabel::RW:
                state.is_active = (bus_state & BUS_BIT(BUS_RW_BIT)) != 0;
                state.is_output = false; // Input to video chip
                break;
                
            case PinLabel::PHI2:
                state.is_active = true; // Assume clock is running
                state.is_output = false; // Input to video chip
                break;
                
            case PinLabel::CS:
                state.is_active = true; // Assume chip is selected
                state.is_output = false;
                break;
                
            case PinLabel::IRQ:
                state.is_active = (bus_state & BUS_BIT(BUS_IRQ_BIT)) == 0; // Active low
                state.is_output = true; // Output from video chip
                break;
                
            case PinLabel::VCC: case PinLabel::VDD:
                state.is_active = true;
                state.is_output = false;
                break;
                
            case PinLabel::VSS:
                state.is_active = false;
                state.is_output = false;
                break;
                
            default:
                state.is_active = false;
                break;
        }
        
        // Handle active-low pins
        if (pin.invert_logic) {
            state.is_active = !state.is_active;
        }
        
        state.value = state.is_active ? 1 : 0;
    };
    
    // Process pins from all sides
    size_t pin_index = 0;
    for (const auto& pin : layout->left_pins) {
        process_pin(pin, pin.pin_number - 1);
    }
    for (const auto& pin : layout->right_pins) {
        process_pin(pin, pin.pin_number - 1);
    }
    
    return states;
}

std::vector<PinState> get_audio_chip_pin_states(void* chip, const PinLayout* layout, bus_state_t bus_state) {
    // Similar implementation to video chip but for audio-specific pins
    return get_video_chip_pin_states(chip, layout, bus_state); // Reuse for now
}

std::vector<PinState> get_io_chip_pin_states(void* chip, const PinLayout* layout, bus_state_t bus_state) {
    // Similar implementation to video chip but for I/O-specific pins
    return get_video_chip_pin_states(chip, layout, bus_state); // Reuse for now
}

std::vector<PinState> get_memory_chip_pin_states(void* chip, const PinLayout* layout, bus_state_t bus_state) {
    // Similar implementation to video chip but for memory-specific pins
    return get_video_chip_pin_states(chip, layout, bus_state); // Reuse for now
}

std::vector<PinState> get_logic_chip_pin_states(void* chip, const PinLayout* layout, bus_state_t bus_state) {
    // Similar implementation to video chip but for logic-specific pins
    return get_video_chip_pin_states(chip, layout, bus_state); // Reuse for now
}