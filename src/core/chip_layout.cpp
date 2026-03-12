/*
 * chip_layout.cpp - Generic chip layout implementation
 * Comprehensive package support
 */

#include "core/chip_layout.hpp"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>

// ============================================================================
// CHIPPIN METHOD IMPLEMENTATIONS
// ============================================================================


uint8_t ChipPin::get_bit_index() const {
    return get_bit_index_from_label(label);
}

bool ChipPin::get_invert_logic() const {
    return get_invert_logic_from_label(label);
}

const char* ChipPin::get_group_name() const {
    return pin_type_to_group_name(get_pin_type());
}

// ============================================================================
// HELPER FUNCTIONS FOR PIN OPTIMIZATION
// ============================================================================

// Get bit index from pin label (for address/data/GPIO pins)
uint8_t get_bit_index_from_label(PinLabel label) {
    // Address pins A0-A23
    if (label >= PinLabel::A0 && label <= PinLabel::A23) {
        return (uint8_t)((int)label - (int)PinLabel::A0);
    }
    
    // Data pins D0-D15
    if (label >= PinLabel::D0 && label <= PinLabel::D15) {
        return (uint8_t)((int)label - (int)PinLabel::D0);
    }
    
    // GPIO pins PA0-PA7
    if (label >= PinLabel::PA0 && label <= PinLabel::PA7) {
        return (uint8_t)((int)label - (int)PinLabel::PA0);
    }
    
    // GPIO pins PB0-PB7
    if (label >= PinLabel::PB0 && label <= PinLabel::PB7) {
        return (uint8_t)((int)label - (int)PinLabel::PB0);
    }
    
    // GPIO pins PC0-PC7
    if (label >= PinLabel::PC0 && label <= PinLabel::PC7) {
        return (uint8_t)((int)label - (int)PinLabel::PC0);
    }
    
    // GPIO pins PD0-PD7
    if (label >= PinLabel::PD0 && label <= PinLabel::PD7) {
        return (uint8_t)((int)label - (int)PinLabel::PD0);
    }
    
    // I/O Port pins P0-P7 (6510 specific)
    if (label >= PinLabel::P0 && label <= PinLabel::P7) {
        return (uint8_t)((int)label - (int)PinLabel::P0);
    }
    
    // Memory data pins DQ0-DQ7
    if (label >= PinLabel::DQ0 && label <= PinLabel::DQ7) {
        return (uint8_t)((int)label - (int)PinLabel::DQ0);
    }
    
    // Memory address pins MA0-MA15
    if (label >= PinLabel::MA0 && label <= PinLabel::MA15) {
        return (uint8_t)((int)label - (int)PinLabel::MA0);
    }
    
    // Logic pins Q0-Q7, I0-I7, Y0-Y7
    if (label >= PinLabel::Q0 && label <= PinLabel::Q7) {
        return (uint8_t)((int)label - (int)PinLabel::Q0);
    }
    if (label >= PinLabel::I0 && label <= PinLabel::I7) {
        return (uint8_t)((int)label - (int)PinLabel::I0);
    }
    if (label >= PinLabel::Y0 && label <= PinLabel::Y7) {
        return (uint8_t)((int)label - (int)PinLabel::Y0);
    }
    
    // Select lines S0-S3
    if (label >= PinLabel::S0 && label <= PinLabel::S3) {
        return (uint8_t)((int)label - (int)PinLabel::S0);
    }
    
    // Analog pins AIN0-AIN7, AOUT0-AOUT1
    if (label >= PinLabel::AIN0 && label <= PinLabel::AIN7) {
        return (uint8_t)((int)label - (int)PinLabel::AIN0);
    }
    if (label >= PinLabel::AOUT0 && label <= PinLabel::AOUT1) {
        return (uint8_t)((int)label - (int)PinLabel::AOUT0);
    }
    
    // PWM pins PWM0-PWM3
    if (label >= PinLabel::PWM0 && label <= PinLabel::PWM3) {
        return (uint8_t)((int)label - (int)PinLabel::PWM0);
    }
    
    // Default case - cannot derive, return 0
    return 0;
}

// Get invert logic from pin label — active-low labels are declared
// before PinLabel::ACTIVE_LOW_END, enabling a simple comparison.
bool get_invert_logic_from_label(PinLabel label) {
    return label < PinLabel::ACTIVE_LOW_END;
}

// Convert pin type to group name string
const char* pin_type_to_group_name(PinType type) {
    switch(type) {
        case PinType::POWER:        return "POWER";
        case PinType::CLOCK:        return "CLOCK";
        case PinType::ADDRESS:      return "ADDR";
        case PinType::DATA:         return "DATA";
        case PinType::CONTROL:      return "CONTROL";
        case PinType::INTERRUPT:    return "INTERRUPT";
        case PinType::SPECIAL:      return "SPECIAL";
        case PinType::IO_PORT:      return "GPIO";
        case PinType::PORT_A:       return "PORTA";
        case PinType::PORT_B:       return "PORTB";
        case PinType::PORT_C:       return "PORTC";
        case PinType::PORT_D:       return "PORTD";
        case PinType::ANALOG:       return "ANALOG";
        case PinType::DIFFERENTIAL: return "DIFF";
        case PinType::VIDEO:        return "VIDEO";
        case PinType::AUDIO:        return "AUDIO";
        case PinType::MEMORY:       return "MEMORY";
        case PinType::LOGIC:        return "LOGIC";
        case PinType::SERIAL:       return "SERIAL";
        case PinType::TIMER:        return "TIMER";
        case PinType::NO_CONNECT:   return "NC";
        default:                    return "UNKNOWN";
    }
}


// ============================================================================
// HELPER FUNCTIONS FOR STANDARD PACKAGE LAYOUTS
// ============================================================================

ChipPin make_pin(uint8_t num, PinLabel label, bool positive) {
    ChipPin pin = make_pin(num, label);
    pin.is_differential_pos = positive;
    pin.is_differential_neg = !positive;
    return pin;
}

// ============================================================================
// ChipLayout method implementations
// ============================================================================

// Helper function to convert PackageType enum to string
const std::string get_package_type_string(PackageType package_type) {
    // get_package_name
    switch(package_type) {
        case PackageType::DIP: return "DIP";
        case PackageType::SOIC: return "SOIC";
        case PackageType::SOP: return "SOP";
        case PackageType::SSOP: return "SSOP";
        case PackageType::TSSOP: return "TSSOP";
        case PackageType::PLCC: return "PLCC";
        case PackageType::QFP: return "QFP";
        case PackageType::LQFP: return "LQFP";
        case PackageType::TQFP: return "TQFP";
        case PackageType::QFN: return "QFN";
        case PackageType::DFN: return "DFN";
        case PackageType::BGA: return "BGA";
        case PackageType::LGA: return "LGA";
        case PackageType::SIP: return "SIP";
        case PackageType::TO220: return "TO-220";
        case PackageType::TO92: return "TO-92";
        case PackageType::SOT23: return "SOT-23";
        case PackageType::SOT223: return "SOT-223";
        case PackageType::CUSTOM: return "Custom";
        default: return "IC";
    }
}


bool get_package_name_shows_pin_count(PackageType package_type) {
    switch (package_type) {
        case PackageType::TO220:
        case PackageType::TO92:
        case PackageType::SOT23:
        case PackageType::SOT223:
            return false;
        default:
            return true;
    }
}

std::string ChipLayout::get_package_name() const {
    if (get_package_name_shows_pin_count(package.package_type)) {
        size_t pin_count = get_total_pins();
        return get_package_type_string(package.package_type) + "-" + std::to_string(pin_count);
    } else {
        return get_package_type_string(package.package_type);
    }
}

// DIP layouts
ChipLayout create_dip8_layout() {
    ChipLayout layout = {};
    layout.package = {
        300.0f,                      // width (mil) - DIP8 narrow body width
        374.0f,                      // height (mil) - DIP8 body length
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    PIN_LR(layout, 1, VCC,  VDD, 8);
    PIN_LR(layout, 2, PA0,  PB0, 7);
    PIN_LR(layout, 3, PA1,  PB1, 6);
    PIN_LR(layout, 4, GND,  VSS, 5);
    
    layout.markings = {
        {},                     // part_number
        {},                     // manufacturer
        {},                     // package_variant
        {},                     // date_code
        {},                     // lot_number
        {},                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

ChipLayout create_dip14_layout() {
    ChipLayout layout = {};
    layout.package = {
        300.0f,                      // width (mil) - DIP14 narrow body for logic chips (7.62mm)
        748.0f,                      // height (mil) - DIP14 body length (19.00mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // Left side (pins 1-7)
    for (uint8_t i = 1; i <= 7; i++) {
        if (i == 7) {
            layout.left_pins.push_back(make_pin(7, PinLabel::GND));
        } else {
            layout.left_pins.push_back(make_pin(i, PinLabel::PA0, "PORT"));
        }
    }
    
    // Right side (pins 8-14)
    for (uint8_t i = 14; i >= 8; i--) {
        if (i == 14) {
            layout.right_pins.push_back(make_pin(14, PinLabel::VCC));
        } else {
            layout.right_pins.push_back(make_pin(i, PinLabel::PB0, "PORT"));
        }
    }
        return layout;
}

ChipLayout create_dip16_layout() {
    ChipLayout layout = {};
    layout.package = {
        300.0f,                      // width (mil) - DIP16 narrow body for logic chips (7.62mm)
        800.0f,                      // height (mil) - DIP16 body length (20.32mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 8 pins per side
    for (uint8_t i = 1; i <= 8; i++) {
        layout.left_pins.push_back(make_pin(i, PinLabel::PA0, "PORTA"));
    }
    
    for (uint8_t i = 16; i >= 9; i--) {
        layout.right_pins.push_back(make_pin(i, PinLabel::PB0, "PORTB"));
    }
    
    return layout;
}

ChipLayout create_dip20_layout() {
    ChipLayout layout = {};
    layout.package = {
        300.0f,                      // width (mil) - DIP20 narrow body for logic chips (7.62mm)
        1000.0f,                     // height (mil) - DIP20 body length (25.4mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    return layout;
}

ChipLayout create_dip18_layout() {
    ChipLayout layout = {};
    layout.package = {
        300.0f,                      // width (mil) - DIP18 narrow body for 2114 SRAM (7.62mm)
        900.0f,                      // height (mil) - DIP18 body length (22.86mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 9 pins per side
    for (uint8_t i = 1; i <= 9; i++) {
        layout.left_pins.push_back(make_pin(i, PinLabel::MA0));
    }
    
    for (uint8_t i = 18; i >= 10; i--) {
        layout.right_pins.push_back(make_pin(i, PinLabel::DQ0));
    }
    
    return layout;
}

ChipLayout create_dip24_layout() {
    ChipLayout layout = {};
    layout.package = {
        600.0f,                      // width (mil) - DIP24 wide body for ROM chips (15.24mm)
        1200.0f,                     // height (mil) - DIP24 body length (30.48mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    return layout;
}

ChipLayout create_dip28_layout() {
    ChipLayout layout = {};
    layout.package = {
        600.0f,                      // width (mil) - DIP28 wide body for C64 chips (15.24mm)
        1400.0f,                     // height (mil) - DIP28 body length (35.56mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    return layout;
}

ChipLayout create_dip40_layout() {
    ChipLayout layout = {};
    layout.package = {
        600.0f,                      // width (mil) - DIP40 wide body width
        2000.0f,                     // height (mil) - DIP40 body length
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    return layout;
}

// SOIC layouts
ChipLayout create_soic8_layout() {
    ChipLayout layout = {};
    layout.package = {
        153.5f,                      // width (mil) - SOIC8 narrow body
        193.0f,                      // height (mil) - SOIC8 body length
        PackageType::SOIC,           // package_type
        OrientationMarker::DOT,      // marker
        50.0f,                       // pin_pitch (mil) - standard SOIC pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    layout.left_pins = {
        make_pin(1, PinLabel::PA0),
        make_pin(2, PinLabel::PA1),
        make_pin(3, PinLabel::PA2),
        make_pin(4, PinLabel::GND)
    };
    
    layout.right_pins = {
        make_pin(8, PinLabel::VCC),
        make_pin(7, PinLabel::PB0),
        make_pin(6, PinLabel::PB1),
        make_pin(5, PinLabel::PB2)
    };
    
    return layout;
}

ChipLayout create_soic14_layout() {
    ChipLayout layout = {};
    layout.package = {
        153.5f,                      // width (mil) - SOIC14 narrow body (3.9mm)
        340.6f,                      // height (mil) - SOIC14 body length (8.65mm)
        PackageType::SOIC,           // package_type
        OrientationMarker::DOT,      // marker
        50.0f,                       // pin_pitch (mil) - standard SOIC pitch (1.27mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    return layout;
}

ChipLayout create_soic16_layout() {
    ChipLayout layout = {};
    layout.package = {
        153.5f,                      // width (mil) - SOIC16 narrow body (3.9mm)
        389.8f,                      // height (mil) - SOIC16 body length (9.9mm)
        PackageType::SOIC,           // package_type
        OrientationMarker::DOT,      // marker
        50.0f,                       // pin_pitch (mil) - standard SOIC pitch (1.27mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    return layout;
}

ChipLayout create_soic28_layout() {
    ChipLayout layout = {};
    layout.package = {
        295.3f,                      // width (mil) - SOIC28 wide body (7.5mm)
        704.7f,                      // height (mil) - SOIC28 body length (17.9mm)
        PackageType::SOIC,           // package_type
        OrientationMarker::DOT,      // marker
        50.0f,                       // pin_pitch (mil) - standard SOIC pitch (1.27mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    return layout;
}

// PLCC layouts
ChipLayout create_plcc28_layout() {
    ChipLayout layout = {};
    layout.package = {
        450.0f,                      // width (mil) - PLCC28 square body
        450.0f,                      // height (mil) - PLCC28 square body
        PackageType::PLCC,           // package_type
        OrientationMarker::CHAMFER,  // marker
        50.0f,                       // pin_pitch (mil) - standard PLCC pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // PLCC has pins on all 4 sides: 7 per side
    for (int i = 1; i <= 7; i++) {
        layout.left_pins.push_back(make_pin(i, PinLabel::PA0, "LEFT"));
    }
    
    for (int i = 8; i <= 14; i++) {
        layout.bottom_pins.push_back(make_pin(i, PinLabel::PA1, "BOTTOM"));
    }
    
    for (int i = 15; i <= 21; i++) {
        layout.right_pins.push_back(make_pin(i, PinLabel::PA2, "RIGHT"));
    }
    
    for (int i = 22; i <= 28; i++) {
        layout.top_pins.push_back(make_pin(i, PinLabel::PA3, "TOP"));
    }
    
    return layout;
}

ChipLayout create_plcc44_layout() {
    ChipLayout layout = {};
    layout.package = {
        689.0f,                      // width (mil) - PLCC44 standard square
        689.0f,                      // height (mil) - PLCC44 standard square
        PackageType::PLCC,           // package_type
        OrientationMarker::CHAMFER,  // marker
        50.0f,                       // pin_pitch (mil) - standard PLCC pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 11 pins per side
    return layout;
}

ChipLayout create_plcc68_layout() {
    ChipLayout layout = {};
    layout.package = {
        950.0f,                      // width (mil) - PLCC68 square body
        950.0f,                      // height (mil) - PLCC68 square body
        PackageType::PLCC,           // package_type
        OrientationMarker::CHAMFER,  // marker
        50.0f,                       // pin_pitch (mil) - standard PLCC pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 17 pins per side
    return layout;
}

// QFP layouts
ChipLayout create_qfp32_layout() {
    ChipLayout layout = {};
    layout.package = {
        275.6f,                      // width (mil) - QFP32 7×7mm
        275.6f,                      // height (mil) - QFP32 7×7mm
        PackageType::QFP,            // package_type
        OrientationMarker::CHAMFER,  // marker
        31.5f,                       // pin_pitch (mil) - 0.8mm pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 8 pins per side
    return layout;
}

ChipLayout create_qfp44_layout() {
    ChipLayout layout = {};
    layout.package = {
        393.7f,                      // width (mil) - QFP44 10×10mm
        393.7f,                      // height (mil) - QFP44 10×10mm
        PackageType::QFP,            // package_type
        OrientationMarker::CHAMFER,  // marker
        31.5f,                       // pin_pitch (mil) - QFP44 (0.8mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    return layout;
}

ChipLayout create_qfp64_layout() {
    ChipLayout layout = {};
    layout.package = {
        393.7f,                      // width (mil) - QFP64 10×10mm
        393.7f,                      // height (mil) - QFP64 10×10mm
        PackageType::QFP,            // package_type
        OrientationMarker::CHAMFER,  // marker
        19.7f,                       // pin_pitch (mil) - 0.5mm pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 16 pins per side
    return layout;
}

ChipLayout create_qfp100_layout() {
    ChipLayout layout = {};
    layout.package = {
        551.2f,                      // width (mil) - QFP100 14×14mm
        551.2f,                      // height (mil) - QFP100 14×14mm
        PackageType::QFP,            // package_type
        OrientationMarker::CHAMFER,  // marker
        19.7f,                       // pin_pitch (mil) - 0.5mm pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 25 pins per side
    return layout;
}

ChipLayout create_qfp144_layout() {
    ChipLayout layout = {};
    layout.package = {
        787.4f,                      // width (mil) - QFP144 20×20mm
        787.4f,                      // height (mil) - QFP144 20×20mm
        PackageType::QFP,            // package_type
        OrientationMarker::CHAMFER,  // marker
        19.7f,                       // pin_pitch (mil) - 0.5mm pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 36 pins per side
    return layout;
}

// QFN layouts
ChipLayout create_qfn16_layout() {
    ChipLayout layout = {};
    layout.package = {
        157.5f,                      // width (mil) - QFN16 4×4mm
        157.5f,                      // height (mil) - QFN16 4×4mm
        PackageType::QFN,            // package_type
        OrientationMarker::DOT,      // marker
        19.7f,                       // pin_pitch (mil) - 0.5mm pitch
        true,                        // has_thermal_pad
        true,                        // has_center_slug
        0.6f                         // thermal_pad_size
    };
    
    // 4 pins per side
    return layout;
}

ChipLayout create_qfn24_layout() {
    ChipLayout layout = {};
    layout.package = {
        157.5f,                      // width (mil) - QFN24 4×4mm
        157.5f,                      // height (mil) - QFN24 4×4mm
        PackageType::QFN,            // package_type
        OrientationMarker::DOT,      // marker
        19.7f,                       // pin_pitch (mil) - QFN24 (0.5mm)
        true,                        // has_thermal_pad
        true,                        // has_center_slug
        0.6f                         // thermal_pad_size
    };
    
    return layout;
}

ChipLayout create_qfn32_layout() {
    ChipLayout layout = {};
    layout.package = {
        196.9f,                      // width (mil) - QFN32 5×5mm
        196.9f,                      // height (mil) - QFN32 5×5mm
        PackageType::QFN,            // package_type
        OrientationMarker::DOT,      // marker
        19.7f,                       // pin_pitch (mil) - 0.5mm pitch
        true,                        // has_thermal_pad
        true,                        // has_center_slug
        0.65f                        // thermal_pad_size
    };
    
    return layout;
}

ChipLayout create_qfn48_layout() {
    ChipLayout layout = {};
    layout.package = {
        275.6f,                      // width (mil) - QFN48 7×7mm
        275.6f,                      // height (mil) - QFN48 7×7mm
        PackageType::QFN,            // package_type
        OrientationMarker::DOT,      // marker
        19.7f,                       // pin_pitch (mil) - QFN48 (0.5mm)
        true,                        // has_thermal_pad
        true,                        // has_center_slug
        0.65f                        // thermal_pad_size
    };
    
    return layout;
}

// BGA layouts
ChipLayout create_bga64_layout() {
    ChipLayout layout = {};
    layout.package = {
        315.0f,                      // width (mil) - BGA64 8×8mm
        315.0f,                      // height (mil) - BGA64 8×8mm
        PackageType::BGA,            // package_type
        OrientationMarker::TRIANGLE, // marker
        31.5f,                       // pin_pitch (mil) - 0.8mm pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 8x8 grid
    return layout;
}

ChipLayout create_bga100_layout() {
    ChipLayout layout = {};
    layout.package = {
        393.7f,                      // width (mil) - BGA100 (10.0mm)
        393.7f,                      // height (mil) - BGA100 (10.0mm)
        PackageType::BGA,            // package_type
        OrientationMarker::TRIANGLE, // marker
        31.5f,                       // pin_pitch (mil) - BGA100 (0.8mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 10x10 grid
    return layout;
}

ChipLayout create_bga256_layout() {
    ChipLayout layout = {};
    layout.package = {
        669.3f,                      // width (mil) - BGA256 (17.0mm)
        669.3f,                      // height (mil) - BGA256 (17.0mm)
        PackageType::BGA,            // package_type
        OrientationMarker::TRIANGLE, // marker
        31.5f,                       // pin_pitch (mil) - BGA256 (0.8mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // 16x16 grid
    return layout;
}

// Power packages
ChipLayout create_to220_layout() {
    ChipLayout layout = {};
    layout.package = {
        393.7f,                      // width (mil) - TO-220 body width (10.0mm)
        342.5f,                      // height (mil) - TO-220 body height (8.7mm)
        PackageType::TO220,          // package_type
        OrientationMarker::NONE,     // marker
        100.0f,                      // pin_pitch (mil) - standard 100 mil
        true,                        // has_thermal_pad
        true,                        // has_center_slug
        0.7f                         // thermal_pad_size
    };
    
    layout.bottom_pins = {
        make_pin(1, PinLabel::PA0, "INPUT"),
        make_pin(2, PinLabel::GND),
        make_pin(3, PinLabel::PA1, "OUTPUT")
    };
    
    layout.markings = {
        "LM7805",                    // part_number
        {},                     // manufacturer
        {},                     // package_variant
        {},                     // date_code
        {},                     // lot_number
        "+5V Regulator",             // custom_text
        true,                        // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

ChipLayout create_to92_layout() {
    ChipLayout layout = {};
    layout.package = {
        177.2f,                      // width (mil) - TO-92 body diameter (4.5mm)
        177.2f,                      // height (mil) - TO-92 body height (4.5mm)
        PackageType::TO92,           // package_type
        OrientationMarker::NONE,     // marker
        50.0f,                       // pin_pitch (mil) - standard 50 mil
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    layout.bottom_pins = {
        make_pin(1, PinLabel::PA0, "EMITTER"),
        make_pin(2, PinLabel::PA1, "BASE"),
        make_pin(3, PinLabel::PA2, "COLLECTOR")
    };
    
    return layout;
}

ChipLayout create_sot23_layout() {
    ChipLayout layout = {};
    layout.package = {
        118.1f,                      // width (mil) - SOT-23 body width (3.0mm)
        55.1f,                       // height (mil) - SOT-23 body height (1.4mm)
        PackageType::SOT23,          // package_type
        OrientationMarker::DOT,      // marker
        37.4f,                       // pin_pitch (mil) - between pins on same side (0.95mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    layout.left_pins = {
        make_pin(1, PinLabel::PA0, "BASE"),
        make_pin(2, PinLabel::PA1, "EMITTER")
    };
    
    layout.right_pins = {
        make_pin(3, PinLabel::PA2, "COLLECTOR")
    };
    
    return layout;
}

ChipLayout create_sot223_layout() {
    ChipLayout layout = {};
    layout.package = {
        255.9f,                      // width (mil) - SOT-223 (6.5mm)
        137.8f,                      // height (mil) - SOT-223 (3.5mm)
        PackageType::SOT223,         // package_type
        OrientationMarker::DOT,      // marker
        90.6f,                       // pin_pitch (mil) - SOT-223 (2.3mm)
        true,                        // has_thermal_pad
        false,                       // has_center_slug
        0.5f                         // thermal_pad_size
    };
    
    layout.bottom_pins = {
        make_pin(1, PinLabel::PA0, "INPUT"),
        make_pin(2, PinLabel::GND),
        make_pin(3, PinLabel::PA1, "OUTPUT")
    };
    
    // Pin 4 is the large thermal tab (typically connected to OUT)
    layout.top_pins = {
        make_pin(4, PinLabel::PA2, "THERMAL")
    };
    
    return layout;
}

// SIP layouts
ChipLayout create_sip8_layout() {
    ChipLayout layout = {};
    layout.package = {
        100.0f,                      // width (mil) - SIP standard width (2.54mm)
        800.0f,                      // height (mil) - SIP-8 length (20.32mm)
        PackageType::SIP,            // package_type
        OrientationMarker::DOT,      // marker
        100.0f,                      // pin_pitch (mil) - 100 mil standard
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // All pins on one side (bottom)
    for (uint8_t i = 1; i <= 8; i++) {
        layout.bottom_pins.push_back(make_pin(i, PinLabel::PA0));
    }
    
    return layout;
}

ChipLayout create_sip9_layout() {
    ChipLayout layout = {};
    layout.package = {
        100.0f,                      // width (mil) - SIP standard width (2.54mm)
        900.0f,                      // height (mil) - SIP-9 length (22.86mm)
        PackageType::SIP,            // package_type
        OrientationMarker::DOT,      // marker
        100.0f,                      // pin_pitch (mil) - 100 mil standard
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // Common pin 1, resistor array
    layout.bottom_pins.push_back(make_pin(1, PinLabel::PA0, "COMMON"));
    for (uint8_t i = 2; i <= 9; i++) {
        layout.bottom_pins.push_back(make_pin(i, PinLabel::PA1, "RESISTOR"));
    }
    
    layout.markings = {
        {},                     // part_number
        {},                     // manufacturer
        {},                     // package_variant
        {},                     // date_code
        {},                     // lot_number
        "10K Resistor Network",      // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

// Custom layout builders
ChipLayout create_custom_dip(uint8_t total_pins, const char* part_name) {
    ChipLayout layout = {};
    
    uint8_t pins_per_side = total_pins / 2;
    
    // Calculate proper DIP dimensions based on pin count
    float width = (total_pins <= 20) ? 300.0f : 600.0f;  // 300 or 600 mil width
    float length = (pins_per_side - 1) * 100.0f + 300.0f; // Pin pitch × (pins-1) + end margins
    
    layout.package = {
        width,                       // width (mil) - DIP width (300 or 600 mil)
        length,                      // height (mil) - DIP length based on pin count
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - 100 mil standard
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // Generate generic pins
    for (uint8_t i = 1; i <= pins_per_side; i++) {
        layout.left_pins.push_back(make_pin(i, PinLabel::PA0));
    }
    
    for (uint8_t i = total_pins; i > pins_per_side; i--) {
        layout.right_pins.push_back(make_pin(i, PinLabel::PB0));
    }
    
    return layout;
}

ChipLayout create_custom_qfp(uint8_t total_pins, const char* part_name) {
    ChipLayout layout = {};
    
    uint8_t pins_per_side = total_pins / 4;
    float size_mil = 393.7f + pins_per_side * 31.5f; // QFP size in mil based on pin count
    
    layout.package = {
        size_mil,                    // width (mil)
        size_mil,                    // height (mil) 
        PackageType::QFP,            // package_type
        OrientationMarker::DOT,      // marker
        19.7f,                       // pin_pitch (mil, typical QFP 0.5mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // Generate pins for each side
    for (uint8_t i = 1; i <= pins_per_side; i++) {
        layout.left_pins.push_back(make_pin(i, PinLabel::PA0));
    }
    
    for (uint8_t i = pins_per_side + 1; i <= 2 * pins_per_side; i++) {
        layout.top_pins.push_back(make_pin(i, PinLabel::PA1));
    }
    
    for (uint8_t i = 2 * pins_per_side + 1; i <= 3 * pins_per_side; i++) {
        layout.right_pins.push_back(make_pin(i, PinLabel::PA2));
    }
    
    for (uint8_t i = 3 * pins_per_side + 1; i <= total_pins; i++) {
        layout.bottom_pins.push_back(make_pin(i, PinLabel::PA3));
    }
    
    layout.markings = {
        part_name ? part_name : std::string_view{},  // part_number
        {},                     // manufacturer
        {},                     // package_variant
        {},                     // date_code
        {},                     // lot_number
        {},                     // custom_text
        part_name != nullptr,        // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

ChipLayout create_custom_bga(uint8_t rows, uint8_t cols, const char* part_name) {
    ChipLayout layout = {};
    
    float width_mil = 393.7f + cols * 50.0f;   // BGA size based on grid
    float height_mil = 393.7f + rows * 50.0f;
    
    layout.package = {
        width_mil,                   // width (mil)
        height_mil,                  // height (mil)
        PackageType::BGA,            // package_type
        OrientationMarker::DOT,      // marker
        50.0f,                       // pin_pitch (mil, typical BGA 1.27mm)
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // Generate grid pins (A1, A2, ..., B1, B2, ...)
    for (uint8_t row = 0; row < rows; row++) {
        for (uint8_t col = 0; col < cols; col++) {
            uint8_t pin_number = row * cols + col + 1;
            layout.grid_pins.push_back(make_pin(pin_number, PinLabel::PA0));
        }
    }
    
    layout.markings = {
        part_name ? part_name : std::string_view{},  // part_number
        {},                     // manufacturer  
        {},                     // package_variant
        {},                     // date_code
        {},                     // lot_number
        {},                     // custom_text
        part_name != nullptr,        // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

// ============================================================================
// ENUM-TO-STRING CONVERSION FUNCTIONS
// ============================================================================

// ============================================================================
// GENERIC BUS STATE → PIN SIGNAL POPULATION
// ============================================================================

std::vector<PinSignalState> populate_pin_states_from_bus(
    const ChipLayout& layout, bus_state_t bus_state) {

    size_t total_pins = layout.get_total_pins();
    std::vector<PinSignalState> states(total_pins);

    // Initialize all pins as inactive and valid
    for (size_t i = 0; i < total_pins; i++) {
        states[i] = {
            static_cast<uint8_t>(i + 1), // pin_number (1-based)
            false,   // signal_level
            false,   // drive_direction
            0,       // signal_value
            true,    // high_impedance (default tri-state)
            false,   // has_pullup
            false,   // has_pulldown
            true,    // signal_valid
            0.0f,    // analog_voltage
            false,   // is_pwm
            0.0f     // pwm_duty_cycle
        };
    }

    uint16_t addr_bus = BUS_GET_ADDR(bus_state);
    uint8_t data_bus = BUS_GET_DATA(bus_state);

    // Process a single pin: extract signal level from bus_state based on
    // the pin's PinType and PinLabel metadata.
    auto process_pin = [&](const ChipPin& pin) {
        if (pin.pin_number == 0 || pin.pin_number > total_pins)
            return;
        PinSignalState& state = states[pin.pin_number - 1];

        switch (pin.get_pin_type()) {
        case PinType::ADDRESS: {
            uint8_t bit_index = pin.get_bit_index();
            if (bit_index < 16) {
                state.signal_level = (addr_bus & (1 << bit_index)) != 0;
                state.high_impedance = false;
            }
            break;
        }
        case PinType::DATA: {
            uint8_t bit_index = pin.get_bit_index();
            if (bit_index < 8) {
                state.signal_level = (data_bus & (1 << bit_index)) != 0;
                state.drive_direction =
                    !BUS_GET_BIT(bus_state, BUS_RW_BIT); // Output on write
                state.high_impedance = !state.drive_direction;
            }
            break;
        }
        case PinType::POWER: {
            // VDD/VCC = high, VSS/GND = low
            state.signal_level =
                (pin.label == PinLabel::VDD || pin.label == PinLabel::VCC);
            state.high_impedance = false;
            break;
        }
        case PinType::CLOCK: {
            state.signal_level = true;
            state.high_impedance = false;
            break;
        }
        case PinType::NO_CONNECT: {
            state.high_impedance = true;
            break;
        }
        default: {
            // CONTROL, INTERRUPT, SPECIAL — check for bus bit mapping
            auto [bus_bit, is_input, invert] = get_pin_bus_mapping(pin.label);
            if (bus_bit >= 0) {
                bool bit_set = BUS_GET_BIT(bus_state, bus_bit);
                state.signal_level = invert ? !bit_set : bit_set;
                state.drive_direction = !is_input;
                state.high_impedance = false;
            }
            // Pins with no mapping (IO_PORT, SPECIAL, ANALOG without bus bits)
            // are left at defaults for chip-specific overlay.
            break;
        }
        }

        state.signal_value = state.signal_level ? 1 : 0;
    };

    // Process pins from all sides
    for (const auto& pin : layout.left_pins)
        process_pin(pin);
    for (const auto& pin : layout.right_pins)
        process_pin(pin);
    for (const auto& pin : layout.top_pins)
        process_pin(pin);
    for (const auto& pin : layout.bottom_pins)
        process_pin(pin);
    for (const auto& pin : layout.grid_pins)
        process_pin(pin);

    return states;
}
