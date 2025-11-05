/*
 * chip_layouts.cpp - Hardware-accurate chip layouts for all chip types
 * Using generic ChipLayout system with proper PinLabel enums
 */

#include "chip_layout.h"

// ============================================================================
// HELPER MACROS FOR CHIP PIN CREATION
// ============================================================================

#define MAKE_PIN_ENUM(num, label_enum, bit, inv) \
    {num, label_enum, bit, inv, nullptr, nullptr, false, false}

#define PIN_PAIR_ENUM(left_num, left_label, left_bit, left_inv, \
                      right_num, right_label, right_bit, right_inv) \
    layout.left_pins.push_back(MAKE_PIN_ENUM(left_num, left_label, left_bit, left_inv)); \
    layout.right_pins.push_back(MAKE_PIN_ENUM(right_num, right_label, right_bit, right_inv));

// ============================================================================
// CPU CHIP LAYOUTS (65xx Family)
// ============================================================================

PinLayout create_mos6502_chip_layout() {
    PinLayout layout = create_dip40_layout();
    
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
    
    // Hardware-accurate pin assignments for MOS 6502
    PIN_PAIR_ENUM(1,  PinLabel::VSS,    0, false,  21, PinLabel::VSS,    0, false)  // Ground
    PIN_PAIR_ENUM(2,  PinLabel::RDY,    0, false,  22, PinLabel::A12,    12, false) // Ready / Address
    PIN_PAIR_ENUM(3,  PinLabel::PHI1,   0, false,  23, PinLabel::A13,    13, false) // Clock / Address
    PIN_PAIR_ENUM(4,  PinLabel::IRQ,    0, true,   24, PinLabel::A14,    14, false) // Interrupt / Address
    PIN_PAIR_ENUM(5,  PinLabel::NC,     0, false,  25, PinLabel::A15,    15, false) // NC / Address
    PIN_PAIR_ENUM(6,  PinLabel::NMI,    0, true,   26, PinLabel::D7,     7, false)  // NMI / Data
    PIN_PAIR_ENUM(7,  PinLabel::SYNC,   0, false,  27, PinLabel::D6,     6, false)  // Sync / Data
    PIN_PAIR_ENUM(8,  PinLabel::VDD,    0, false,  28, PinLabel::D5,     5, false)  // Power / Data
    PIN_PAIR_ENUM(9,  PinLabel::A0,     0, false,  29, PinLabel::D4,     4, false)  // Address / Data
    PIN_PAIR_ENUM(10, PinLabel::A1,     1, false,  30, PinLabel::D3,     3, false)  // Address / Data
    PIN_PAIR_ENUM(11, PinLabel::A2,     2, false,  31, PinLabel::D2,     2, false)  // Address / Data
    PIN_PAIR_ENUM(12, PinLabel::A3,     3, false,  32, PinLabel::D1,     1, false)  // Address / Data
    PIN_PAIR_ENUM(13, PinLabel::A4,     4, false,  33, PinLabel::D0,     0, false)  // Address / Data
    PIN_PAIR_ENUM(14, PinLabel::A5,     5, false,  34, PinLabel::RW,     0, false)  // Address / R/W
    PIN_PAIR_ENUM(15, PinLabel::A6,     6, false,  35, PinLabel::NC,     0, false)  // Address / NC
    PIN_PAIR_ENUM(16, PinLabel::A7,     7, false,  36, PinLabel::NC,     0, false)  // Address / NC
    PIN_PAIR_ENUM(17, PinLabel::A8,     8, false,  37, PinLabel::PHI0,   0, false)  // Address / Clock
    PIN_PAIR_ENUM(18, PinLabel::A9,     9, false,  38, PinLabel::SO,     0, true)   // Address / Set Overflow
    PIN_PAIR_ENUM(19, PinLabel::A10,    10, false, 39, PinLabel::PHI2,   0, false)  // Address / Clock
    PIN_PAIR_ENUM(20, PinLabel::A11,    11, false, 40, PinLabel::RES,    0, true)   // Address / Reset
    
    return layout;
}

PinLayout create_mos6510_chip_layout() {
    PinLayout layout = create_dip40_layout();
    
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
    
    // Hardware-accurate pin assignments for MOS 6510 (C64 CPU)
    PIN_PAIR_ENUM(1,  PinLabel::PHI0,   0, false,  21, PinLabel::VSS,    0, false)  // Clock / Ground
    PIN_PAIR_ENUM(2,  PinLabel::RDY,    0, false,  22, PinLabel::A12,    12, false) // Ready / Address
    PIN_PAIR_ENUM(3,  PinLabel::IRQ,    0, true,   23, PinLabel::A13,    13, false) // Interrupt / Address
    PIN_PAIR_ENUM(4,  PinLabel::NMI,    0, true,   24, PinLabel::A14,    14, false) // NMI / Address
    PIN_PAIR_ENUM(5,  PinLabel::AEC,    0, false,  25, PinLabel::A15,    15, false) // Address Enable / Address
    PIN_PAIR_ENUM(6,  PinLabel::VDD,    0, false,  26, PinLabel::D7,     7, false)  // Power / Data
    PIN_PAIR_ENUM(7,  PinLabel::A0,     0, false,  27, PinLabel::D6,     6, false)  // Address / Data
    PIN_PAIR_ENUM(8,  PinLabel::A1,     1, false,  28, PinLabel::D5,     5, false)  // Address / Data
    PIN_PAIR_ENUM(9,  PinLabel::A2,     2, false,  29, PinLabel::D4,     4, false)  // Address / Data
    PIN_PAIR_ENUM(10, PinLabel::A3,     3, false,  30, PinLabel::D3,     3, false)  // Address / Data
    PIN_PAIR_ENUM(11, PinLabel::A4,     4, false,  31, PinLabel::D2,     2, false)  // Address / Data
    PIN_PAIR_ENUM(12, PinLabel::A5,     5, false,  32, PinLabel::D1,     1, false)  // Address / Data
    PIN_PAIR_ENUM(13, PinLabel::A6,     6, false,  33, PinLabel::D0,     0, false)  // Address / Data
    PIN_PAIR_ENUM(14, PinLabel::A7,     7, false,  34, PinLabel::RW,     0, false)  // Address / R/W
    PIN_PAIR_ENUM(15, PinLabel::A8,     8, false,  35, PinLabel::NC,     0, false)  // Address / NC
    PIN_PAIR_ENUM(16, PinLabel::A9,     9, false,  36, PinLabel::NC,     0, false)  // Address / NC
    PIN_PAIR_ENUM(17, PinLabel::A10,    10, false, 37, PinLabel::PHI2,   0, false)  // Address / Clock
    PIN_PAIR_ENUM(18, PinLabel::A11,    11, false, 38, PinLabel::RW,     0, false)  // Address / R/W
    PIN_PAIR_ENUM(19, PinLabel::SYNC,   0, false,  39, PinLabel::PHI1,   0, false)  // Sync / Clock
    PIN_PAIR_ENUM(20, PinLabel::VSS,    0, false,  40, PinLabel::RES,    0, true)   // Ground / Reset
    
    return layout;
}

// ============================================================================
// VIDEO CHIP LAYOUTS (VIC-II)
// ============================================================================

PinLayout create_mos6567_chip_layout() {
    PinLayout layout = create_dip40_layout();
    
    layout.markings = {
        "MOS6567",                   // part_number
        "MOS Technology",            // manufacturer
        "VIC-II",                    // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        "Video Interface Chip",      // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        true,                        // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate pin assignments for MOS 6567 VIC-II (NTSC)
    PIN_PAIR_ENUM(1,  PinLabel::VDD,     0, false,  21, PinLabel::VSS,     0, false)  // Power / Ground
    PIN_PAIR_ENUM(2,  PinLabel::PHI0,    0, false,  22, PinLabel::A5,      5, false)  // Clock / Address
    PIN_PAIR_ENUM(3,  PinLabel::AEC,     0, false,  23, PinLabel::A4,      4, false)  // Address Enable / Address
    PIN_PAIR_ENUM(4,  PinLabel::BA,      0, false,  24, PinLabel::A3,      3, false)  // Bus Available / Address
    PIN_PAIR_ENUM(5,  PinLabel::RW,      0, false,  25, PinLabel::A2,      2, false)  // R/W / Address
    PIN_PAIR_ENUM(6,  PinLabel::IRQ,     0, true,   26, PinLabel::A1,      1, false)  // Interrupt / Address
    PIN_PAIR_ENUM(7,  PinLabel::LUMA,    0, false,  27, PinLabel::A0,      0, false)  // Luminance / Address
    PIN_PAIR_ENUM(8,  PinLabel::CHROMA,  0, false,  28, PinLabel::A6,      6, false)  // Chrominance / Address
    PIN_PAIR_ENUM(9,  PinLabel::CSYNC,   0, false,  29, PinLabel::A7,      7, false)  // Composite Sync / Address
    PIN_PAIR_ENUM(10, PinLabel::COLOR_CLK, 0, false, 30, PinLabel::A8,      8, false)  // Color Clock / Address
    PIN_PAIR_ENUM(11, PinLabel::D0,      0, false,  31, PinLabel::A9,      9, false)  // Data / Address
    PIN_PAIR_ENUM(12, PinLabel::D1,      1, false,  32, PinLabel::A10,     10, false) // Data / Address
    PIN_PAIR_ENUM(13, PinLabel::D2,      2, false,  33, PinLabel::A11,     11, false) // Data / Address
    PIN_PAIR_ENUM(14, PinLabel::D3,      3, false,  34, PinLabel::A12,     12, false) // Data / Address
    PIN_PAIR_ENUM(15, PinLabel::D4,      4, false,  35, PinLabel::A13,     13, false) // Data / Address
    PIN_PAIR_ENUM(16, PinLabel::D5,      5, false,  36, PinLabel::RAS,     0, false)  // Data / Row Address Strobe
    PIN_PAIR_ENUM(17, PinLabel::D6,      6, false,  37, PinLabel::CAS,     0, false)  // Data / Column Address Strobe
    PIN_PAIR_ENUM(18, PinLabel::D7,      7, false,  38, PinLabel::PHI2,    0, false)  // Data / Clock
    PIN_PAIR_ENUM(19, PinLabel::CS,      0, false,  39, PinLabel::LIGHT_PEN, 0, false) // Chip Select / Light Pen
    PIN_PAIR_ENUM(20, PinLabel::VSS,     0, false,  40, PinLabel::DOT_CLK, 0, false)  // Ground / Dot Clock
    
    return layout;
}

PinLayout create_mos6569_chip_layout() {
    PinLayout layout = create_mos6567_chip_layout(); // Same pinout as 6567
    
    layout.markings.part_number = "MOS6569";
    layout.markings.custom_text = "Video Interface Chip (PAL)";
    
    return layout;
}

// ============================================================================
// AUDIO CHIP LAYOUTS (SID)
// ============================================================================

PinLayout create_mos6581_chip_layout() {
    PinLayout layout = create_dip28_layout();
    
    layout.markings = {
        "MOS6581",                   // part_number
        "MOS Technology",            // manufacturer
        "SID",                       // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        "Sound Interface Device",    // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        true,                        // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate pin assignments for MOS 6581 SID (28-pin DIP)
    layout.left_pins = {
        MAKE_PIN_ENUM(1,  PinLabel::FILTER_OUT, 0, false),
        MAKE_PIN_ENUM(2,  PinLabel::VREF,       0, false),
        MAKE_PIN_ENUM(3,  PinLabel::AUDIO_OUT,  0, false),
        MAKE_PIN_ENUM(4,  PinLabel::AUDIO_IN,   0, false),
        MAKE_PIN_ENUM(5,  PinLabel::VDD,        0, false),
        MAKE_PIN_ENUM(6,  PinLabel::PHI2,       0, false),
        MAKE_PIN_ENUM(7,  PinLabel::RW,         0, false),
        MAKE_PIN_ENUM(8,  PinLabel::CS,         0, false),
        MAKE_PIN_ENUM(9,  PinLabel::A0,         0, false),
        MAKE_PIN_ENUM(10, PinLabel::A1,         1, false),
        MAKE_PIN_ENUM(11, PinLabel::A2,         2, false),
        MAKE_PIN_ENUM(12, PinLabel::A3,         3, false),
        MAKE_PIN_ENUM(13, PinLabel::A4,         4, false),
        MAKE_PIN_ENUM(14, PinLabel::VSS,        0, false)
    };
    
    layout.right_pins = {
        MAKE_PIN_ENUM(28, PinLabel::VDD,        0, false),
        MAKE_PIN_ENUM(27, PinLabel::D7,         7, false),
        MAKE_PIN_ENUM(26, PinLabel::D6,         6, false),
        MAKE_PIN_ENUM(25, PinLabel::D5,         5, false),
        MAKE_PIN_ENUM(24, PinLabel::D4,         4, false),
        MAKE_PIN_ENUM(23, PinLabel::D3,         3, false),
        MAKE_PIN_ENUM(22, PinLabel::D2,         2, false),
        MAKE_PIN_ENUM(21, PinLabel::D1,         1, false),
        MAKE_PIN_ENUM(20, PinLabel::D0,         0, false),
        MAKE_PIN_ENUM(19, PinLabel::NC,         0, false),
        MAKE_PIN_ENUM(18, PinLabel::NC,         0, false),
        MAKE_PIN_ENUM(17, PinLabel::NC,         0, false),
        MAKE_PIN_ENUM(16, PinLabel::NC,         0, false),
        MAKE_PIN_ENUM(15, PinLabel::VSS,        0, false)
    };
    
    return layout;
}

// ============================================================================
// I/O CHIP LAYOUTS (CIA)
// ============================================================================

PinLayout create_mos6526_chip_layout() {
    PinLayout layout = create_dip40_layout();
    
    layout.markings = {
        "MOS6526",                   // part_number
        "MOS Technology",            // manufacturer
        "CIA",                       // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        "Complex Interface Adapter", // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        true,                        // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate pin assignments for MOS 6526 CIA
    PIN_PAIR_ENUM(1,  PinLabel::VSS,    0, false,  21, PinLabel::VDD,     0, false)  // Ground / Power
    PIN_PAIR_ENUM(2,  PinLabel::PA0,    0, false,  22, PinLabel::PB7,     7, false)  // Port A / Port B
    PIN_PAIR_ENUM(3,  PinLabel::PA1,    1, false,  23, PinLabel::PB6,     6, false)  // Port A / Port B
    PIN_PAIR_ENUM(4,  PinLabel::PA2,    2, false,  24, PinLabel::PB5,     5, false)  // Port A / Port B
    PIN_PAIR_ENUM(5,  PinLabel::PA3,    3, false,  25, PinLabel::PB4,     4, false)  // Port A / Port B
    PIN_PAIR_ENUM(6,  PinLabel::PA4,    4, false,  26, PinLabel::PB3,     3, false)  // Port A / Port B
    PIN_PAIR_ENUM(7,  PinLabel::PA5,    5, false,  27, PinLabel::PB2,     2, false)  // Port A / Port B
    PIN_PAIR_ENUM(8,  PinLabel::PA6,    6, false,  28, PinLabel::PB1,     1, false)  // Port A / Port B
    PIN_PAIR_ENUM(9,  PinLabel::PA7,    7, false,  29, PinLabel::PB0,     0, false)  // Port A / Port B
    PIN_PAIR_ENUM(10, PinLabel::PB0,    0, false,  30, PinLabel::PA7,     7, false)  // Port B / Port A
    PIN_PAIR_ENUM(11, PinLabel::PB1,    1, false,  31, PinLabel::PA6,     6, false)  // Port B / Port A
    PIN_PAIR_ENUM(12, PinLabel::PB2,    2, false,  32, PinLabel::PA5,     5, false)  // Port B / Port A
    PIN_PAIR_ENUM(13, PinLabel::PB3,    3, false,  33, PinLabel::PA4,     4, false)  // Port B / Port A
    PIN_PAIR_ENUM(14, PinLabel::PB4,    4, false,  34, PinLabel::PA3,     3, false)  // Port B / Port A
    PIN_PAIR_ENUM(15, PinLabel::PB5,    5, false,  35, PinLabel::PA2,     2, false)  // Port B / Port A
    PIN_PAIR_ENUM(16, PinLabel::PB6,    6, false,  36, PinLabel::PA1,     1, false)  // Port B / Port A
    PIN_PAIR_ENUM(17, PinLabel::PB7,    7, false,  37, PinLabel::PA0,     0, false)  // Port B / Port A
    PIN_PAIR_ENUM(18, PinLabel::CNT,    0, false,  38, PinLabel::CS,      0, false)  // Counter / Chip Select
    PIN_PAIR_ENUM(19, PinLabel::SP,     0, false,  39, PinLabel::PHI2,    0, false)  // Serial Port / Clock
    PIN_PAIR_ENUM(20, PinLabel::VSS,    0, false,  40, PinLabel::FLAG,    0, false)  // Ground / Flag
    
    return layout;
}

// ============================================================================
// MEMORY CHIP LAYOUTS (RAM)
// ============================================================================

PinLayout create_static_ram_chip_layout() {
    PinLayout layout = create_dip18_layout();
    
    layout.markings = {
        "2114",                      // part_number
        "Generic",                   // manufacturer
        "SRAM",                      // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        "1K×4 Static RAM",           // custom_text
        true,                        // show_part_number
        false,                       // show_manufacturer
        true,                        // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate pin assignments for generic SRAM (18-pin DIP)
    layout.left_pins = {
        MAKE_PIN_ENUM(1,  PinLabel::MA6,    6, false),
        MAKE_PIN_ENUM(2,  PinLabel::MA5,    5, false),
        MAKE_PIN_ENUM(3,  PinLabel::MA4,    4, false),
        MAKE_PIN_ENUM(4,  PinLabel::MA3,    3, false),
        MAKE_PIN_ENUM(5,  PinLabel::MA2,    2, false),
        MAKE_PIN_ENUM(6,  PinLabel::MA1,    1, false),
        MAKE_PIN_ENUM(7,  PinLabel::MA0,    0, false),
        MAKE_PIN_ENUM(8,  PinLabel::DQ0,    0, false),
        MAKE_PIN_ENUM(9,  PinLabel::VSS,    0, false)
    };
    
    layout.right_pins = {
        MAKE_PIN_ENUM(18, PinLabel::VDD,    0, false),
        MAKE_PIN_ENUM(17, PinLabel::DQ3,    3, false),
        MAKE_PIN_ENUM(16, PinLabel::DQ2,    2, false),
        MAKE_PIN_ENUM(15, PinLabel::DQ1,    1, false),
        MAKE_PIN_ENUM(14, PinLabel::MA7,    7, false),
        MAKE_PIN_ENUM(13, PinLabel::MA8,    8, false),
        MAKE_PIN_ENUM(12, PinLabel::MA9,    9, false),
        MAKE_PIN_ENUM(11, PinLabel::WE,     0, true),
        MAKE_PIN_ENUM(10, PinLabel::CS,     0, true)
    };
    
    return layout;
}

// ============================================================================
// LOGIC CHIP LAYOUTS (PLA)
// ============================================================================

PinLayout create_pla_chip_layout() {
    PinLayout layout = create_dip28_layout();
    
    layout.markings = {
        "82S100",                    // part_number
        "Generic",                   // manufacturer
        "PLA",                       // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        "Programmable Logic Array",  // custom_text
        true,                        // show_part_number
        false,                       // show_manufacturer
        true,                        // show_package_variant
        false                        // show_date_code
    };
    
    // Hardware-accurate pin assignments for generic PLA (28-pin DIP)
    layout.left_pins = {
        MAKE_PIN_ENUM(1,  PinLabel::I0,     0, false),
        MAKE_PIN_ENUM(2,  PinLabel::I1,     1, false),
        MAKE_PIN_ENUM(3,  PinLabel::I2,     2, false),
        MAKE_PIN_ENUM(4,  PinLabel::I3,     3, false),
        MAKE_PIN_ENUM(5,  PinLabel::I4,     4, false),
        MAKE_PIN_ENUM(6,  PinLabel::I5,     5, false),
        MAKE_PIN_ENUM(7,  PinLabel::I6,     6, false),
        MAKE_PIN_ENUM(8,  PinLabel::I7,     7, false),
        MAKE_PIN_ENUM(9,  PinLabel::G,      0, true),
        MAKE_PIN_ENUM(10, PinLabel::A8,     8, false),
        MAKE_PIN_ENUM(11, PinLabel::A9,     9, false),
        MAKE_PIN_ENUM(12, PinLabel::A10,    10, false),
        MAKE_PIN_ENUM(13, PinLabel::A11,    11, false),
        MAKE_PIN_ENUM(14, PinLabel::VSS,    0, false)
    };
    
    layout.right_pins = {
        MAKE_PIN_ENUM(28, PinLabel::VDD,    0, false),
        MAKE_PIN_ENUM(27, PinLabel::Y7,     7, false),
        MAKE_PIN_ENUM(26, PinLabel::Y6,     6, false),
        MAKE_PIN_ENUM(25, PinLabel::Y5,     5, false),
        MAKE_PIN_ENUM(24, PinLabel::Y4,     4, false),
        MAKE_PIN_ENUM(23, PinLabel::Y3,     3, false),
        MAKE_PIN_ENUM(22, PinLabel::Y2,     2, false),
        MAKE_PIN_ENUM(21, PinLabel::Y1,     1, false),
        MAKE_PIN_ENUM(20, PinLabel::Y0,     0, false),
        MAKE_PIN_ENUM(19, PinLabel::A12,    12, false),
        MAKE_PIN_ENUM(18, PinLabel::A13,    13, false),
        MAKE_PIN_ENUM(17, PinLabel::A14,    14, false),
        MAKE_PIN_ENUM(16, PinLabel::A15,    15, false),
        MAKE_PIN_ENUM(15, PinLabel::CS,     0, true)
    };
    
    return layout;
}

// ============================================================================
// LAYOUT FACTORY FUNCTIONS
// ============================================================================

PinLayout get_chip_layout(const char* chip_name) {
    if (strcmp(chip_name, "MOS6502") == 0) {
        return create_mos6502_chip_layout();
    } else if (strcmp(chip_name, "MOS6510") == 0) {
        return create_mos6510_chip_layout();
    } else if (strcmp(chip_name, "MOS6567") == 0) {
        return create_mos6567_chip_layout();
    } else if (strcmp(chip_name, "MOS6569") == 0) {
        return create_mos6569_chip_layout();
    } else if (strcmp(chip_name, "MOS6581") == 0) {
        return create_mos6581_chip_layout();
    } else if (strcmp(chip_name, "MOS6526") == 0) {
        return create_mos6526_chip_layout();
    } else if (strcmp(chip_name, "SRAM") == 0) {
        return create_static_ram_chip_layout();
    } else if (strcmp(chip_name, "PLA") == 0) {
        return create_pla_chip_layout();
    } else {
        // Return generic DIP-40 layout for unknown chips
        return create_dip40_layout();
    }
}