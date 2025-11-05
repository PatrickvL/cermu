/*
 * non_cpu_chip_layouts.h - ChipLayout definitions for video, audio, I/O, and logic chips
 * 
 * Hardware-accurate pin layouts for chips other than CPUs:
 * - MOS6526 CIA (Complex Interface Adapter)
 * - MOS6581 SID (Sound Interface Device) 
 * - MOS6567/6569 VIC-II (Video Interface Chip)
 * - RAM chips (SRAM/DRAM)
 * - PLA (Programmable Logic Array)
 */

#ifndef NON_CPU_CHIP_LAYOUTS_H
#define NON_CPU_CHIP_LAYOUTS_H

#include "chip_layout.h"
#include "pin_macros.h"

// ============================================================================
// MOS6526 CIA LAYOUT (40-pin DIP)
// ============================================================================

ChipLayout create_mos6526_layout() {
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
    PIN_LR(1,  VSS,     21, IRQ)    // Ground / Interrupt Request
    PIN_LR(2,  PA0,     22, RW)     // Port A Bit 0 / Read/Write
    PIN_LR(3,  PA1,     23, CS)     // Port A Bit 1 / Chip Select
    PIN_LR(4,  PA2,     24, FLAG)   // Port A Bit 2 / Flag Input
    PIN_LR(5,  PA3,     25, PHI2)   // Port A Bit 3 / Clock
    PIN_LR(6,  PA4,     26, SP)     // Port A Bit 4 / Serial Port
    PIN_LR(7,  PA5,     27, CNT)    // Port A Bit 5 / Counter
    PIN_LR(8,  PA6,     28, A0)     // Port A Bit 6 / Address 0
    PIN_LR(9,  PA7,     29, A1)     // Port A Bit 7 / Address 1
    PIN_LR(10, PB0,     30, A2)     // Port B Bit 0 / Address 2
    PIN_LR(11, PB1,     31, A3)     // Port B Bit 1 / Address 3
    PIN_LR(12, PB2,     32, D0)     // Port B Bit 2 / Data 0
    PIN_LR(13, PB3,     33, D1)     // Port B Bit 3 / Data 1
    PIN_LR(14, PB4,     34, D2)     // Port B Bit 4 / Data 2
    PIN_LR(15, PB5,     35, D3)     // Port B Bit 5 / Data 3
    PIN_LR(16, PB6,     36, D4)     // Port B Bit 6 / Data 4
    PIN_LR(17, PB7,     37, D5)     // Port B Bit 7 / Data 5
    PIN_LR(18, PC,      38, D6)     // Serial Port / Data 6
    PIN_LR(19, TOD,     39, D7)     // Time of Day / Data 7
    PIN_LR(20, VDD,     40, RES)    // +5V Power / Reset
    
    return layout;
}

// ============================================================================
// MOS6581 SID LAYOUT (28-pin DIP)
// ============================================================================

ChipLayout create_mos6581_layout() {
    // Start with DIP-28 base layout
    ChipLayout layout = create_dip28_layout();
    
    // Update package info for MOS6581 SID
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
    
    // Clear default pins and create hardware-accurate MOS6581 SID pinout (28-pin DIP)
    layout.left_pins.clear();
    layout.right_pins.clear();
    
    // Left side pins (1-14)
    layout.left_pins.push_back(make_pin(1,  PinLabel::CAP1A, nullptr));   // Filter Cap 1A
    layout.left_pins.push_back(make_pin(2,  PinLabel::CAP1B, nullptr));   // Filter Cap 1B
    layout.left_pins.push_back(make_pin(3,  PinLabel::CAP2A, nullptr));   // Filter Cap 2A
    layout.left_pins.push_back(make_pin(4,  PinLabel::CAP2B, nullptr));   // Filter Cap 2B
    layout.left_pins.push_back(make_pin(5,  PinLabel::RES, nullptr));     // Reset
    layout.left_pins.push_back(make_pin(6,  PinLabel::PHI2, nullptr));    // Clock
    layout.left_pins.push_back(make_pin(7,  PinLabel::RW, nullptr));      // Read/Write
    layout.left_pins.push_back(make_pin(8,  PinLabel::CS, nullptr));      // Chip Select
    layout.left_pins.push_back(make_pin(9,  PinLabel::A0, nullptr));      // Address 0
    layout.left_pins.push_back(make_pin(10, PinLabel::A1, nullptr));      // Address 1
    layout.left_pins.push_back(make_pin(11, PinLabel::A2, nullptr));      // Address 2
    layout.left_pins.push_back(make_pin(12, PinLabel::A3, nullptr));      // Address 3
    layout.left_pins.push_back(make_pin(13, PinLabel::A4, nullptr));      // Address 4
    layout.left_pins.push_back(make_pin(14, PinLabel::VSS, nullptr));     // Ground
    
    // Right side pins (15-28)
    layout.right_pins.push_back(make_pin(28, PinLabel::VDD, nullptr));    // +12V Power
    layout.right_pins.push_back(make_pin(27, PinLabel::AUDIO_OUT, nullptr)); // Audio Output
    layout.right_pins.push_back(make_pin(26, PinLabel::EXT_IN, nullptr)); // External Input
    layout.right_pins.push_back(make_pin(25, PinLabel::VCC, nullptr));    // +5V Power
    layout.right_pins.push_back(make_pin(24, PinLabel::POTX, nullptr));   // Paddle X
    layout.right_pins.push_back(make_pin(23, PinLabel::POTY, nullptr));   // Paddle Y
    layout.right_pins.push_back(make_pin(22, PinLabel::D7, nullptr));     // Data 7
    layout.right_pins.push_back(make_pin(21, PinLabel::D6, nullptr));     // Data 6
    layout.right_pins.push_back(make_pin(20, PinLabel::D5, nullptr));     // Data 5
    layout.right_pins.push_back(make_pin(19, PinLabel::D4, nullptr));     // Data 4
    layout.right_pins.push_back(make_pin(18, PinLabel::D3, nullptr));     // Data 3
    layout.right_pins.push_back(make_pin(17, PinLabel::D2, nullptr));     // Data 2
    layout.right_pins.push_back(make_pin(16, PinLabel::D1, nullptr));     // Data 1
    layout.right_pins.push_back(make_pin(15, PinLabel::D0, nullptr));     // Data 0
    
    return layout;
}

// ============================================================================
// MOS6567/6569 VIC-II LAYOUT (40-pin DIP)
// ============================================================================

ChipLayout create_vicii_layout() {
    // Start with DIP-40 base layout
    ChipLayout layout = create_dip40_layout();
    
    // Update package info for VIC-II
    layout.markings = {
        "MOS6567/6569",              // part_number
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
    
    // Hardware-accurate MOS6567/6569 VIC-II pinout (40-pin DIP)
    PIN_LR(1,  VDD,     21, VSS)    // +5V Power / Ground
    PIN_LR(2,  PHI0,    22, A5)     // Clock Input / Address 5
    PIN_LR(3,  AEC,     23, A4)     // Address Enable / Address 4
    PIN_LR(4,  BA,      24, A3)     // Bus Available / Address 3
    PIN_LR(5,  RW,      25, A2)     // Read/Write / Address 2
    PIN_LR(6,  IRQ,     26, A1)     // Interrupt / Address 1
    PIN_LR(7,  A6,      27, A0)     // Address 6 / Address 0
    PIN_LR(8,  A7,      28, D7)     // Address 7 / Data 7
    PIN_LR(9,  A8,      29, D6)     // Address 8 / Data 6
    PIN_LR(10, A9,      30, D5)     // Address 9 / Data 5
    PIN_LR(11, A10,     31, D4)     // Address 10 / Data 4
    PIN_LR(12, A11,     32, D3)     // Address 11 / Data 3
    PIN_LR(13, A12,     33, D2)     // Address 12 / Data 2
    PIN_LR(14, A13,     34, D1)     // Address 13 / Data 1
    PIN_LR(15, CAS,     35, D0)     // Column Addr Strobe / Data 0
    PIN_LR(16, RAS,     36, PHI2)   // Row Addr Strobe / Clock
    PIN_LR(17, LUMA,    37, COLOR)  // Luminance / Color Signal
    PIN_LR(18, CHROMA,  38, CS)     // Chrominance / Chip Select
    PIN_LR(19, CSYNC,   39, SOUND)  // Composite Sync / Sound
    PIN_LR(20, VSS,     40, VCC)    // Ground / +5V Power
    
    return layout;
}

// ============================================================================
// GENERIC RAM LAYOUT (18-pin DIP for SRAM)
// ============================================================================

ChipLayout create_ram_layout() {
    // Start with DIP-18 base layout
    ChipLayout layout = create_dip18_layout();
    
    // Update package info for RAM
    layout.markings = {
        "SRAM",                      // part_number
        "Generic",                   // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        true,                        // show_part_number
        true,                        // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    // Clear default pins and create generic SRAM pinout (18-pin DIP)
    layout.left_pins.clear();
    layout.right_pins.clear();
    
    // Left side pins (1-9)
    layout.left_pins.push_back(make_pin(1, PinLabel::A6, nullptr));       // Address 6
    layout.left_pins.push_back(make_pin(2, PinLabel::A5, nullptr));       // Address 5
    layout.left_pins.push_back(make_pin(3, PinLabel::A4, nullptr));       // Address 4
    layout.left_pins.push_back(make_pin(4, PinLabel::A3, nullptr));       // Address 3
    layout.left_pins.push_back(make_pin(5, PinLabel::A0, nullptr));       // Address 0
    layout.left_pins.push_back(make_pin(6, PinLabel::A1, nullptr));       // Address 1
    layout.left_pins.push_back(make_pin(7, PinLabel::A2, nullptr));       // Address 2
    layout.left_pins.push_back(make_pin(8, PinLabel::D0, nullptr));       // Data 0
    layout.left_pins.push_back(make_pin(9, PinLabel::VSS, nullptr));      // Ground
    
    // Right side pins (10-18)
    layout.right_pins.push_back(make_pin(18, PinLabel::VCC, nullptr));    // +5V Power
    layout.right_pins.push_back(make_pin(17, PinLabel::D1, nullptr));     // Data 1
    layout.right_pins.push_back(make_pin(16, PinLabel::D2, nullptr));     // Data 2
    layout.right_pins.push_back(make_pin(15, PinLabel::D3, nullptr));     // Data 3
    layout.right_pins.push_back(make_pin(14, PinLabel::CS, nullptr));     // Chip Select
    layout.right_pins.push_back(make_pin(13, PinLabel::WE, nullptr));     // Write Enable
    layout.right_pins.push_back(make_pin(12, PinLabel::A9, nullptr));     // Address 9
    layout.right_pins.push_back(make_pin(11, PinLabel::A8, nullptr));     // Address 8
    layout.right_pins.push_back(make_pin(10, PinLabel::A7, nullptr));     // Address 7
    
    return layout;
}

// ============================================================================
// C64 PLA LAYOUT (28-pin DIP)
// ============================================================================

ChipLayout create_pla_layout() {
    // Start with DIP-28 base layout
    ChipLayout layout = create_dip28_layout();
    
    // Update package info for C64 PLA
    layout.markings = {
        "906114-01",                 // part_number
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
    
    // Clear default pins and create hardware-accurate C64 PLA pinout (28-pin DIP)
    layout.left_pins.clear();
    layout.right_pins.clear();
    
    // Left side pins (1-14)
    layout.left_pins.push_back(make_pin(1,  PinLabel::A15, nullptr));     // Address 15
    layout.left_pins.push_back(make_pin(2,  PinLabel::A14, nullptr));     // Address 14
    layout.left_pins.push_back(make_pin(3,  PinLabel::A13, nullptr));     // Address 13
    layout.left_pins.push_back(make_pin(4,  PinLabel::A12, nullptr));     // Address 12
    layout.left_pins.push_back(make_pin(5,  PinLabel::BA, nullptr));      // Bus Available
    layout.left_pins.push_back(make_pin(6,  PinLabel::AEC, nullptr));     // Address Enable
    layout.left_pins.push_back(make_pin(7,  PinLabel::P0, nullptr));      // 6510 Port 0
    layout.left_pins.push_back(make_pin(8,  PinLabel::P1, nullptr));      // 6510 Port 1
    layout.left_pins.push_back(make_pin(9,  PinLabel::P2, nullptr));      // 6510 Port 2
    layout.left_pins.push_back(make_pin(10, PinLabel::CHAREN, nullptr));  // Character Enable
    layout.left_pins.push_back(make_pin(11, PinLabel::HIRAM, nullptr));   // High RAM
    layout.left_pins.push_back(make_pin(12, PinLabel::LORAM, nullptr));   // Low RAM
    layout.left_pins.push_back(make_pin(13, PinLabel::CAS, nullptr));     // Column Addr Strobe
    layout.left_pins.push_back(make_pin(14, PinLabel::VSS, nullptr));     // Ground
    
    // Right side pins (15-28)
    layout.right_pins.push_back(make_pin(28, PinLabel::VCC, nullptr));    // +5V Power
    layout.right_pins.push_back(make_pin(27, PinLabel::A8, nullptr));     // Address 8
    layout.right_pins.push_back(make_pin(26, PinLabel::PHI2, nullptr));   // Clock
    layout.right_pins.push_back(make_pin(25, PinLabel::RW, nullptr));     // Read/Write
    layout.right_pins.push_back(make_pin(24, PinLabel::EXROM, nullptr));  // External ROM
    layout.right_pins.push_back(make_pin(23, PinLabel::GAME, nullptr));   // Game Line
    layout.right_pins.push_back(make_pin(22, PinLabel::ROMH, nullptr));   // ROM High
    layout.right_pins.push_back(make_pin(21, PinLabel::ROML, nullptr));   // ROM Low
    layout.right_pins.push_back(make_pin(20, PinLabel::IO, nullptr));     // I/O Select
    layout.right_pins.push_back(make_pin(19, PinLabel::GRW, nullptr));    // Graphics R/W
    layout.right_pins.push_back(make_pin(18, PinLabel::CHAROM, nullptr)); // Character ROM
    layout.right_pins.push_back(make_pin(17, PinLabel::KERNAL, nullptr)); // KERNAL ROM
    layout.right_pins.push_back(make_pin(16, PinLabel::BASIC, nullptr));  // BASIC ROM
    layout.right_pins.push_back(make_pin(15, PinLabel::CASRAM_PLA, nullptr)); // CAS RAM
    
    return layout;
}

#endif // NON_CPU_CHIP_LAYOUTS_H