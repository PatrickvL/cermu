/*
 * chip_layout.cpp - Generic chip layout implementation
 * Comprehensive package support
 */

#include "chip_layout.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>

// ============================================================================
// HELPER FUNCTIONS FOR STANDARD PACKAGE LAYOUTS
// ============================================================================

// Pin maker helper functions
ChipPin make_power_pin(uint8_t num, const char* label) {
    return {num, label, PinType::POWER, 0, false, "POWER", nullptr, false, false};
}

ChipPin make_ground_pin(uint8_t num, const char* label) {
    return {num, label, PinType::POWER, 0, false, "POWER", nullptr, false, false};
}

ChipPin make_address_pin(uint8_t num, const char* label, uint8_t bit) {
    return {num, label, PinType::ADDRESS, bit, false, "ADDR", nullptr, false, false};
}

ChipPin make_data_pin(uint8_t num, const char* label, uint8_t bit) {
    return {num, label, PinType::DATA, bit, false, "DATA", nullptr, false, false};
}

ChipPin make_control_pin(uint8_t num, const char* label, bool active_low) {
    return {num, label, PinType::CONTROL, 0, active_low, "CONTROL", nullptr, false, false};
}

ChipPin make_clock_pin(uint8_t num, const char* label) {
    return {num, label, PinType::CLOCK, 0, false, "CLOCK", nullptr, false, false};
}

ChipPin make_interrupt_pin(uint8_t num, const char* label, bool active_low) {
    return {num, label, PinType::INTERRUPT, 0, active_low, "INTERRUPT", nullptr, false, false};
}

ChipPin make_gpio_pin(uint8_t num, const char* label, const char* port) {
    return {num, label, PinType::IO_PORT, 0, false, port ? port : "GPIO", nullptr, false, false};
}

ChipPin make_analog_pin(uint8_t num, const char* label) {
    return {num, label, PinType::ANALOG, 0, false, "ANALOG", nullptr, false, false};
}

ChipPin make_differential_pin(uint8_t num, const char* label, bool positive) {
    return {num, label, PinType::DIFFERENTIAL, 0, false, "DIFF", nullptr, positive, !positive};
}

ChipPin make_nc_pin(uint8_t num) {
    return {num, "NC", PinType::NO_CONNECT, 0, false, "NC", nullptr, false, false};
}

// ============================================================================
// PinLayout method implementations
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

static const char* get_package_type_charptr(PackageType package_type) {
    return get_package_type_string(package_type).c_str();
}

const bool get_package_name_shows_pin_count(PackageType package_type) {
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

std::string PinLayout::get_package_name() const {
    if (get_package_name_shows_pin_count(package.package_type)) {
        size_t pin_count = get_total_pins();
        return get_package_type_string(package.package_type) + "-" + std::to_string(pin_count);
    } else {
        return get_package_type_string(package.package_type);
    }
}

// DIP layouts
PinLayout create_dip8_layout() {
    PinLayout layout = {};
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
    
    layout.left_pins = {
        make_power_pin(1, "VCC"),
        make_gpio_pin(2, "IN1"),
        make_gpio_pin(3, "IN2"),
        make_ground_pin(4, "GND")
    };
    
    layout.right_pins = {
        make_power_pin(8, "VDD"),
        make_gpio_pin(7, "OUT1"),
        make_gpio_pin(6, "OUT2"),
        make_ground_pin(5, "VSS")
    };
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

PinLayout create_dip14_layout() {
    PinLayout layout = {};
    layout.package = {
        300.0f,                      // width (mil) - DIP14 narrow body width
        748.0f,                      // height (mil) - DIP14 body length
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // Left side (pins 1-7)
    for (uint8_t i = 1; i <= 7; i++) {
        char label[8];
        if (i == 7) {
            layout.left_pins.push_back(make_ground_pin(7, "GND"));
        } else {
            snprintf(label, sizeof(label), "P%d", i);
            layout.left_pins.push_back(make_gpio_pin(i, label, "PORT"));
        }
    }
    
    // Right side (pins 8-14)
    for (uint8_t i = 14; i >= 8; i--) {
        char label[8];
        if (i == 14) {
            layout.right_pins.push_back(make_power_pin(14, "VCC"));
        } else {
            snprintf(label, sizeof(label), "P%d", i);
            layout.right_pins.push_back(make_gpio_pin(i, label, "PORT"));
        }
    }
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_dip16_layout() {
    PinLayout layout = {};
    layout.package = {
        250.0f,                      // width (mil) - DIP16 narrow body (6.35mm)
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
        char label[8];
        snprintf(label, sizeof(label), "P%d", i);
        layout.left_pins.push_back(make_gpio_pin(i, label, "PORTA"));
    }
    
    for (uint8_t i = 16; i >= 9; i--) {
        char label[8];
        snprintf(label, sizeof(label), "P%d", i);
        layout.right_pins.push_back(make_gpio_pin(i, label, "PORTB"));
    }
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_dip20_layout() {
    PinLayout layout = {};
    layout.package = {
        250.0f,                      // width (mil) - DIP20 narrow body (6.35mm)
        1000.0f,                     // height (mil) - DIP20 body length (25.4mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    // Implementation similar to above
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_dip24_layout() {
    PinLayout layout = {};
    layout.package = {
        250.0f,                      // width (mil) - DIP24 narrow body (6.35mm)
        1200.0f,                     // height (mil) - DIP24 body length (30.48mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_dip28_layout() {
    PinLayout layout = {};
    layout.package = {
        250.0f,                      // width (mil) - DIP28 narrow body (6.35mm)
        1400.0f,                     // height (mil) - DIP28 body length (35.56mm)
        PackageType::DIP,            // package_type
        OrientationMarker::NOTCH,    // marker
        100.0f,                      // pin_pitch (mil) - standard DIP pitch
        false,                       // has_thermal_pad
        false,                       // has_center_slug
        0.0f                         // thermal_pad_size
    };
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_dip40_layout() {
    PinLayout layout = {};
    
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
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

// SOIC layouts
PinLayout create_soic8_layout() {
    PinLayout layout = {};
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
        make_gpio_pin(1, "IN1", nullptr),
        make_gpio_pin(2, "IN2", nullptr),
        make_gpio_pin(3, "IN3", nullptr),
        make_ground_pin(4, "GND")
    };
    
    layout.right_pins = {
        make_power_pin(8, "VCC"),
        make_gpio_pin(7, "OUT1", nullptr),
        make_gpio_pin(6, "OUT2", nullptr),
        make_gpio_pin(5, "OUT3", nullptr)
    };
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_soic14_layout() {
    PinLayout layout = {};
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
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_soic16_layout() {
    PinLayout layout = {};
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
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_soic28_layout() {
    PinLayout layout = {};
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
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

// PLCC layouts
PinLayout create_plcc28_layout() {
    PinLayout layout = {};
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
        char label[8];
        snprintf(label, sizeof(label), "L%d", i);
        layout.left_pins.push_back(make_gpio_pin(i, label, "LEFT"));
    }
    
    for (int i = 8; i <= 14; i++) {
        char label[8];
        snprintf(label, sizeof(label), "B%d", i);
        layout.bottom_pins.push_back(make_gpio_pin(i, label, "BOTTOM"));
    }
    
    for (int i = 15; i <= 21; i++) {
        char label[8];
        snprintf(label, sizeof(label), "R%d", i);
        layout.right_pins.push_back(make_gpio_pin(i, label, "RIGHT"));
    }
    
    for (int i = 22; i <= 28; i++) {
        char label[8];
        snprintf(label, sizeof(label), "T%d", i);
        layout.top_pins.push_back(make_gpio_pin(i, label, "TOP"));
    }
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_plcc44_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_plcc68_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

// QFP layouts
PinLayout create_qfp32_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_qfp44_layout() {
    PinLayout layout = {};
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
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_qfp64_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_qfp100_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_qfp144_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

// QFN layouts
PinLayout create_qfn16_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_qfn24_layout() {
    PinLayout layout = {};
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
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_qfn32_layout() {
    PinLayout layout = {};
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
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_qfn48_layout() {
    PinLayout layout = {};
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
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

// BGA layouts
PinLayout create_bga64_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_bga100_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_bga256_layout() {
    PinLayout layout = {};
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
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

// Power packages
PinLayout create_to220_layout() {
    PinLayout layout = {};
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
        make_gpio_pin(1, "IN", "INPUT"),
        make_ground_pin(2, "GND"),
        make_gpio_pin(3, "OUT", "OUTPUT")
    };
    
    layout.markings = {
        "LM7805",                    // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        "+5V Regulator",             // custom_text
        true,                        // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

PinLayout create_to92_layout() {
    PinLayout layout = {};
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
        make_gpio_pin(1, "E", "EMITTER"),
        make_gpio_pin(2, "B", "BASE"),
        make_gpio_pin(3, "C", "COLLECTOR")
    };
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_sot23_layout() {
    PinLayout layout = {};
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
        make_gpio_pin(1, "B", "BASE"),
        make_gpio_pin(2, "E", "EMITTER")
    };
    
    layout.right_pins = {
        make_gpio_pin(3, "C", "COLLECTOR")
    };
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_sot223_layout() {
    PinLayout layout = {};
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
        make_gpio_pin(1, "IN", "INPUT"),
        make_ground_pin(2, "GND"),
        make_gpio_pin(3, "OUT", "OUTPUT")
    };
    
    // Pin 4 is the large thermal tab (typically connected to OUT)
    layout.top_pins = {
        make_gpio_pin(4, "TAB", "THERMAL")
    };
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

// SIP layouts
PinLayout create_sip8_layout() {
    PinLayout layout = {};
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
        char label[8];
        snprintf(label, sizeof(label), "P%d", i);
        layout.bottom_pins.push_back(make_gpio_pin(i, label, nullptr));
    }
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    return layout;
}

PinLayout create_sip9_layout() {
    PinLayout layout = {};
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
    layout.bottom_pins.push_back(make_gpio_pin(1, "COM", "COMMON"));
    for (uint8_t i = 2; i <= 9; i++) {
        char label[8];
        snprintf(label, sizeof(label), "R%d", i);
        layout.bottom_pins.push_back(make_gpio_pin(i, label, "RESISTOR"));
    }
    
    layout.markings = {
        nullptr,                     // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        "10K Resistor Network",      // custom_text
        false,                       // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

// Custom layout builders
PinLayout create_custom_dip(uint8_t total_pins, const char* part_name) {
    PinLayout layout = {};
    
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
        char label[8];
        snprintf(label, sizeof(label), "P%d", i);
        layout.left_pins.push_back(make_gpio_pin(i, label, nullptr));
    }
    
    for (uint8_t i = total_pins; i > pins_per_side; i--) {
        char label[8];
        snprintf(label, sizeof(label), "P%d", i);
        layout.right_pins.push_back(make_gpio_pin(i, label, nullptr));
    }
    
    layout.markings = {};
    return layout;
}

PinLayout create_custom_qfp(uint8_t total_pins, const char* part_name) {
    PinLayout layout = {};
    
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
        char label[8];
        snprintf(label, sizeof(label), "P%d", i);
        layout.left_pins.push_back(make_gpio_pin(i, label));
    }
    
    for (uint8_t i = pins_per_side + 1; i <= 2 * pins_per_side; i++) {
        char label[8]; 
        snprintf(label, sizeof(label), "P%d", i);
        layout.top_pins.push_back(make_gpio_pin(i, label));
    }
    
    for (uint8_t i = 2 * pins_per_side + 1; i <= 3 * pins_per_side; i++) {
        char label[8];
        snprintf(label, sizeof(label), "P%d", i);
        layout.right_pins.push_back(make_gpio_pin(i, label));
    }
    
    for (uint8_t i = 3 * pins_per_side + 1; i <= total_pins; i++) {
        char label[8];
        snprintf(label, sizeof(label), "P%d", i);
        layout.bottom_pins.push_back(make_gpio_pin(i, label));
    }
    
    layout.markings = {
        part_name,                   // part_number
        nullptr,                     // manufacturer
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        part_name != nullptr,        // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}

PinLayout create_custom_bga(uint8_t rows, uint8_t cols, const char* part_name) {
    PinLayout layout = {};
    
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
            char row_letter = 'A' + row;
            uint8_t col_number = col + 1;
            uint8_t pin_number = row * cols + col + 1;
            
            char label[8];
            snprintf(label, sizeof(label), "%c%d", row_letter, col_number);
            layout.grid_pins.push_back(make_gpio_pin(pin_number, label));
        }
    }
    
    layout.markings = {
        part_name,                   // part_number
        nullptr,                     // manufacturer  
        nullptr,                     // package_variant
        nullptr,                     // date_code
        nullptr,                     // lot_number
        nullptr,                     // custom_text
        part_name != nullptr,        // show_part_number
        false,                       // show_manufacturer
        false,                       // show_package_variant
        false                        // show_date_code
    };
    
    return layout;
}