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
// CHIPPIN METHOD IMPLEMENTATIONS
// ============================================================================

// Implementation of ChipPin::get_pin_type() method
PinType ChipPin::get_pin_type() const {
    switch (label) {
        case PinLabel::VDD:
        case PinLabel::VSS:
        case PinLabel::VCC:
        case PinLabel::GND:
            return PinType::POWER;
            
        case PinLabel::PHI0:
        case PinLabel::PHI1:
        case PinLabel::PHI2:
        case PinLabel::XTAL1:
        case PinLabel::XTAL2:
        case PinLabel::OSC_IN:
        case PinLabel::OSC_OUT:
            return PinType::CLOCK;
            
        case PinLabel::A0: case PinLabel::A1: case PinLabel::A2: case PinLabel::A3:
        case PinLabel::A4: case PinLabel::A5: case PinLabel::A6: case PinLabel::A7:
        case PinLabel::A8: case PinLabel::A9: case PinLabel::A10: case PinLabel::A11:
        case PinLabel::A12: case PinLabel::A13: case PinLabel::A14: case PinLabel::A15:
        case PinLabel::A16: case PinLabel::A17: case PinLabel::A18: case PinLabel::A19:
        case PinLabel::A20: case PinLabel::A21: case PinLabel::A22: case PinLabel::A23:
            return PinType::ADDRESS;
            
        case PinLabel::D0: case PinLabel::D1: case PinLabel::D2: case PinLabel::D3:
        case PinLabel::D4: case PinLabel::D5: case PinLabel::D6: case PinLabel::D7:
        case PinLabel::D8: case PinLabel::D9: case PinLabel::D10: case PinLabel::D11:
        case PinLabel::D12: case PinLabel::D13: case PinLabel::D14: case PinLabel::D15:
            return PinType::DATA;
            
        case PinLabel::RW:
        case PinLabel::SYNC:
        case PinLabel::RDY:
        case PinLabel::AEC:
        case PinLabel::BE:
        case PinLabel::BA:
        case PinLabel::CS:
        case PinLabel::CS0: case PinLabel::CS1: case PinLabel::CS2:
        case PinLabel::OE:
        case PinLabel::WE:
        case PinLabel::E:
        case PinLabel::MX:
            return PinType::CONTROL;
            
        case PinLabel::IRQ:
        case PinLabel::NMI:
        case PinLabel::RES:
        case PinLabel::ABORT:
            return PinType::INTERRUPT;
            
        case PinLabel::SO:
        case PinLabel::VP:
        case PinLabel::VPB:
        case PinLabel::VPA:
        case PinLabel::VDA:
        case PinLabel::ML:
        case PinLabel::TEST:
            return PinType::SPECIAL;
            
        case PinLabel::P0: case PinLabel::P1: case PinLabel::P2: case PinLabel::P3:
        case PinLabel::P4: case PinLabel::P5: case PinLabel::P6: case PinLabel::P7:
        case PinLabel::PA0: case PinLabel::PA1: case PinLabel::PA2: case PinLabel::PA3:
        case PinLabel::PA4: case PinLabel::PA5: case PinLabel::PA6: case PinLabel::PA7:
        case PinLabel::PB0: case PinLabel::PB1: case PinLabel::PB2: case PinLabel::PB3:
        case PinLabel::PB4: case PinLabel::PB5: case PinLabel::PB6: case PinLabel::PB7:
        case PinLabel::PC0: case PinLabel::PC1: case PinLabel::PC2: case PinLabel::PC3:
        case PinLabel::PC4: case PinLabel::PC5: case PinLabel::PC6: case PinLabel::PC7:
        case PinLabel::PD0: case PinLabel::PD1: case PinLabel::PD2: case PinLabel::PD3:
        case PinLabel::PD4: case PinLabel::PD5: case PinLabel::PD6: case PinLabel::PD7:
        case PinLabel::UART_TX: case PinLabel::UART_RX:
        case PinLabel::SPI_CLK: case PinLabel::SPI_MOSI: case PinLabel::SPI_MISO: case PinLabel::SPI_CS:
            return PinType::SERIAL;
            
        case PinLabel::PWM0: case PinLabel::PWM1: case PinLabel::PWM2: case PinLabel::PWM3:
            return PinType::IO_PORT;
            
        case PinLabel::LUMA: case PinLabel::CHROMA: case PinLabel::HSYNC: case PinLabel::VSYNC:
        case PinLabel::CSYNC: case PinLabel::DOT_CLK: case PinLabel::COLOR_CLK: case PinLabel::LIGHT_PEN:
            return PinType::VIDEO;
            
        case PinLabel::AUDIO_OUT: case PinLabel::AUDIO_IN: case PinLabel::FILTER_OUT: case PinLabel::FILTER_IN:
        case PinLabel::OSC1: case PinLabel::OSC2: case PinLabel::OSC3: case PinLabel::NOISE:
            return PinType::AUDIO;
            
        case PinLabel::CAS: case PinLabel::RAS: case PinLabel::MUX:
        case PinLabel::DQ0: case PinLabel::DQ1: case PinLabel::DQ2: case PinLabel::DQ3:
        case PinLabel::DQ4: case PinLabel::DQ5: case PinLabel::DQ6: case PinLabel::DQ7:
        case PinLabel::MA0: case PinLabel::MA1: case PinLabel::MA2: case PinLabel::MA3:
        case PinLabel::MA4: case PinLabel::MA5: case PinLabel::MA6: case PinLabel::MA7:
        case PinLabel::MA8: case PinLabel::MA9: case PinLabel::MA10: case PinLabel::MA11:
        case PinLabel::MA12: case PinLabel::MA13: case PinLabel::MA14: case PinLabel::MA15:
            return PinType::MEMORY;
            
        case PinLabel::Q0: case PinLabel::Q1: case PinLabel::Q2: case PinLabel::Q3:
        case PinLabel::Q4: case PinLabel::Q5: case PinLabel::Q6: case PinLabel::Q7:
        case PinLabel::I0: case PinLabel::I1: case PinLabel::I2: case PinLabel::I3:
        case PinLabel::I4: case PinLabel::I5: case PinLabel::I6: case PinLabel::I7:
        case PinLabel::Y0: case PinLabel::Y1: case PinLabel::Y2: case PinLabel::Y3:
        case PinLabel::Y4: case PinLabel::Y5: case PinLabel::Y6: case PinLabel::Y7:
        case PinLabel::S0: case PinLabel::S1: case PinLabel::S2: case PinLabel::S3:
        case PinLabel::G:
            return PinType::LOGIC;
            
        case PinLabel::CNT: case PinLabel::SP: case PinLabel::TOD: case PinLabel::FLAG:
            return PinType::TIMER;
            
        case PinLabel::VREF:
        case PinLabel::AIN0: case PinLabel::AIN1: case PinLabel::AIN2: case PinLabel::AIN3:
        case PinLabel::AIN4: case PinLabel::AIN5: case PinLabel::AIN6: case PinLabel::AIN7:
        case PinLabel::AOUT0: case PinLabel::AOUT1:
            return PinType::ANALOG;
            
        case PinLabel::NC:
            return PinType::NO_CONNECT;
            
        default:
            return PinType::SPECIAL;
    }
}

// ============================================================================
// HELPER FUNCTIONS FOR STANDARD PACKAGE LAYOUTS
// ============================================================================

// Pin maker helper functions
ChipPin make_power_pin(uint8_t num, PinLabel label) {
    return {num, label, 0, false, "POWER", nullptr, false, false};
}

ChipPin make_ground_pin(uint8_t num, PinLabel label) {
    return {num, label, 0, false, "POWER", nullptr, false, false};
}

ChipPin make_address_pin(uint8_t num, PinLabel label, uint8_t bit) {
    return {num, label, bit, false, "ADDR", nullptr, false, false};
}

ChipPin make_data_pin(uint8_t num, PinLabel label, uint8_t bit) {
    return {num, label, bit, false, "DATA", nullptr, false, false};
}

ChipPin make_control_pin(uint8_t num, PinLabel label, bool active_low) {
    return {num, label, 0, active_low, "CONTROL", nullptr, false, false};
}

ChipPin make_clock_pin(uint8_t num, PinLabel label) {
    return {num, label, 0, false, "CLOCK", nullptr, false, false};
}

ChipPin make_interrupt_pin(uint8_t num, PinLabel label, bool active_low) {
    return {num, label, 0, active_low, "INTERRUPT", nullptr, false, false};
}

ChipPin make_gpio_pin(uint8_t num, PinLabel label, const char* port) {
    return {num, label, 0, false, port ? port : "GPIO", nullptr, false, false};
}

ChipPin make_analog_pin(uint8_t num, PinLabel label) {
    return {num, label, 0, false, "ANALOG", nullptr, false, false};
}

ChipPin make_differential_pin(uint8_t num, PinLabel label, bool positive) {
    return {num, label, 0, false, "DIFF", nullptr, positive, !positive};
}

ChipPin make_nc_pin(uint8_t num) {
    return {num, PinLabel::NC, 0, false, "NC", nullptr, false, false};
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
        make_power_pin(1, PinLabel::VCC),
        make_gpio_pin(2, PinLabel::PA0, nullptr),
        make_gpio_pin(3, PinLabel::PA1, nullptr),
        make_ground_pin(4, PinLabel::GND)
    };
    
    layout.right_pins = {
        make_power_pin(8, PinLabel::VDD),
        make_gpio_pin(7, PinLabel::PB0, nullptr),
        make_gpio_pin(6, PinLabel::PB1, nullptr),
        make_ground_pin(5, PinLabel::VSS)
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
            layout.left_pins.push_back(make_ground_pin(7, PinLabel::GND));
        } else {
            layout.left_pins.push_back(make_gpio_pin(i, PinLabel::PA0, "PORT"));
        }
    }
    
    // Right side (pins 8-14)
    for (uint8_t i = 14; i >= 8; i--) {
        char label[8];
        if (i == 14) {
            layout.right_pins.push_back(make_power_pin(14, PinLabel::VCC));
        } else {
            layout.right_pins.push_back(make_gpio_pin(i, PinLabel::PB0, "PORT"));
        }
    }
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
        layout.left_pins.push_back(make_gpio_pin(i, PinLabel::PA0, "PORTA"));
    }
    
    for (uint8_t i = 16; i >= 9; i--) {
        layout.right_pins.push_back(make_gpio_pin(i, PinLabel::PB0, "PORTB"));
    }
    
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
    
    return layout;
}

PinLayout create_dip18_layout() {
    PinLayout layout = {};
    layout.package = {
        300.0f,                      // width (mil) - DIP18 narrow body (7.62mm)
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
        layout.left_pins.push_back(make_gpio_pin(i, PinLabel::MA0, "ADDR"));
    }
    
    for (uint8_t i = 18; i >= 10; i--) {
        layout.right_pins.push_back(make_gpio_pin(i, PinLabel::DQ0, "DATA"));
    }
    
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
        make_gpio_pin(1, PinLabel::PA0, nullptr),
        make_gpio_pin(2, PinLabel::PA1, nullptr),
        make_gpio_pin(3, PinLabel::PA2, nullptr),
        make_ground_pin(4, PinLabel::GND)
    };
    
    layout.right_pins = {
        make_power_pin(8, PinLabel::VCC),
        make_gpio_pin(7, PinLabel::PB0, nullptr),
        make_gpio_pin(6, PinLabel::PB1, nullptr),
        make_gpio_pin(5, PinLabel::PB2, nullptr)
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
        layout.left_pins.push_back(make_gpio_pin(i, PinLabel::PA0, "LEFT"));
    }
    
    for (int i = 8; i <= 14; i++) {
        layout.bottom_pins.push_back(make_gpio_pin(i, PinLabel::PA1, "BOTTOM"));
    }
    
    for (int i = 15; i <= 21; i++) {
        layout.right_pins.push_back(make_gpio_pin(i, PinLabel::PA2, "RIGHT"));
    }
    
    for (int i = 22; i <= 28; i++) {
        layout.top_pins.push_back(make_gpio_pin(i, PinLabel::PA3, "TOP"));
    }
    
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
        make_gpio_pin(1, PinLabel::PA0, "INPUT"),
        make_ground_pin(2, PinLabel::GND),
        make_gpio_pin(3, PinLabel::PA1, "OUTPUT")
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
        make_gpio_pin(1, PinLabel::PA0, "EMITTER"),
        make_gpio_pin(2, PinLabel::PA1, "BASE"),
        make_gpio_pin(3, PinLabel::PA2, "COLLECTOR")
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
        make_gpio_pin(1, PinLabel::PA0, "BASE"),
        make_gpio_pin(2, PinLabel::PA1, "EMITTER")
    };
    
    layout.right_pins = {
        make_gpio_pin(3, PinLabel::PA2, "COLLECTOR")
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
        make_gpio_pin(1, PinLabel::PA0, "INPUT"),
        make_ground_pin(2, PinLabel::GND),
        make_gpio_pin(3, PinLabel::PA1, "OUTPUT")
    };
    
    // Pin 4 is the large thermal tab (typically connected to OUT)
    layout.top_pins = {
        make_gpio_pin(4, PinLabel::PA2, "THERMAL")
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
        layout.bottom_pins.push_back(make_gpio_pin(i, PinLabel::PA0, nullptr));
    }
    
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
    layout.bottom_pins.push_back(make_gpio_pin(1, PinLabel::PA0, "COMMON"));
    for (uint8_t i = 2; i <= 9; i++) {
        layout.bottom_pins.push_back(make_gpio_pin(i, PinLabel::PA1, "RESISTOR"));
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
        layout.left_pins.push_back(make_gpio_pin(i, PinLabel::PA0, nullptr));
    }
    
    for (uint8_t i = total_pins; i > pins_per_side; i--) {
        layout.right_pins.push_back(make_gpio_pin(i, PinLabel::PB0, nullptr));
    }
    
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
        layout.left_pins.push_back(make_gpio_pin(i, PinLabel::PA0, nullptr));
    }
    
    for (uint8_t i = pins_per_side + 1; i <= 2 * pins_per_side; i++) {
        layout.top_pins.push_back(make_gpio_pin(i, PinLabel::PA1, nullptr));
    }
    
    for (uint8_t i = 2 * pins_per_side + 1; i <= 3 * pins_per_side; i++) {
        layout.right_pins.push_back(make_gpio_pin(i, PinLabel::PA2, nullptr));
    }
    
    for (uint8_t i = 3 * pins_per_side + 1; i <= total_pins; i++) {
        layout.bottom_pins.push_back(make_gpio_pin(i, PinLabel::PA3, nullptr));
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
            uint8_t pin_number = row * cols + col + 1;
            layout.grid_pins.push_back(make_gpio_pin(pin_number, PinLabel::PA0, nullptr));
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

// ============================================================================
// ENUM-TO-STRING CONVERSION FUNCTIONS
// ============================================================================

const char* pin_label_to_string(PinLabel label) {
    switch (label) {
        // Power pins
        case PinLabel::VDD: return "VDD";
        case PinLabel::VSS: return "VSS";
        case PinLabel::VCC: return "VCC";
        case PinLabel::GND: return "GND";
        
        // Clock pins
        case PinLabel::PHI0: return "φ0";
        case PinLabel::PHI1: return "φ1";
        case PinLabel::PHI2: return "φ2";
        
        // Address bus pins
        case PinLabel::A0: return "A0";
        case PinLabel::A1: return "A1";
        case PinLabel::A2: return "A2";
        case PinLabel::A3: return "A3";
        case PinLabel::A4: return "A4";
        case PinLabel::A5: return "A5";
        case PinLabel::A6: return "A6";
        case PinLabel::A7: return "A7";
        case PinLabel::A8: return "A8";
        case PinLabel::A9: return "A9";
        case PinLabel::A10: return "A10";
        case PinLabel::A11: return "A11";
        case PinLabel::A12: return "A12";
        case PinLabel::A13: return "A13";
        case PinLabel::A14: return "A14";
        case PinLabel::A15: return "A15";
        case PinLabel::A16: return "A16";
        case PinLabel::A17: return "A17";
        case PinLabel::A18: return "A18";
        case PinLabel::A19: return "A19";
        case PinLabel::A20: return "A20";
        case PinLabel::A21: return "A21";
        case PinLabel::A22: return "A22";
        case PinLabel::A23: return "A23";
        
        // Data bus pins
        case PinLabel::D0: return "D0";
        case PinLabel::D1: return "D1";
        case PinLabel::D2: return "D2";
        case PinLabel::D3: return "D3";
        case PinLabel::D4: return "D4";
        case PinLabel::D5: return "D5";
        case PinLabel::D6: return "D6";
        case PinLabel::D7: return "D7";
        case PinLabel::D8: return "D8";
        case PinLabel::D9: return "D9";
        case PinLabel::D10: return "D10";
        case PinLabel::D11: return "D11";
        case PinLabel::D12: return "D12";
        case PinLabel::D13: return "D13";
        case PinLabel::D14: return "D14";
        case PinLabel::D15: return "D15";
        
        // Control signals
        case PinLabel::RW: return "R/W";
        case PinLabel::SYNC: return "SYNC";
        case PinLabel::RDY: return "RDY";
        case PinLabel::AEC: return "AEC";
        case PinLabel::BE: return "BE";
        case PinLabel::BA: return "BA";
        
        // Interrupt pins
        case PinLabel::IRQ: return "IRQ";
        case PinLabel::NMI: return "NMI";
        case PinLabel::RES: return "RES";
        case PinLabel::ABORT: return "ABORT";
        
        // Special pins
        case PinLabel::SO: return "SO";
        case PinLabel::VP: return "VP";
        case PinLabel::VPB: return "VPB";
        case PinLabel::VPA: return "VPA";
        case PinLabel::VDA: return "VDA";
        case PinLabel::ML: return "ML";
        case PinLabel::E: return "E";
        case PinLabel::MX: return "MX";
        
        // I/O Port pins
        case PinLabel::P0: return "P0";
        case PinLabel::P1: return "P1";
        case PinLabel::P2: return "P2";
        case PinLabel::P3: return "P3";
        case PinLabel::P4: return "P4";
        case PinLabel::P5: return "P5";
        case PinLabel::P6: return "P6";
        case PinLabel::P7: return "P7";
        
        // GPIO pins
        case PinLabel::PA0: return "PA0";
        case PinLabel::PA1: return "PA1";
        case PinLabel::PA2: return "PA2";
        case PinLabel::PA3: return "PA3";
        case PinLabel::PA4: return "PA4";
        case PinLabel::PA5: return "PA5";
        case PinLabel::PA6: return "PA6";
        case PinLabel::PA7: return "PA7";
        case PinLabel::PB0: return "PB0";
        case PinLabel::PB1: return "PB1";
        case PinLabel::PB2: return "PB2";
        case PinLabel::PB3: return "PB3";
        case PinLabel::PB4: return "PB4";
        case PinLabel::PB5: return "PB5";
        case PinLabel::PB6: return "PB6";
        case PinLabel::PB7: return "PB7";
        case PinLabel::PC0: return "PC0";
        case PinLabel::PC1: return "PC1";
        case PinLabel::PC2: return "PC2";
        case PinLabel::PC3: return "PC3";
        case PinLabel::PC4: return "PC4";
        case PinLabel::PC5: return "PC5";
        case PinLabel::PC6: return "PC6";
        case PinLabel::PC7: return "PC7";
        case PinLabel::PD0: return "PD0";
        case PinLabel::PD1: return "PD1";
        case PinLabel::PD2: return "PD2";
        case PinLabel::PD3: return "PD3";
        case PinLabel::PD4: return "PD4";
        case PinLabel::PD5: return "PD5";
        case PinLabel::PD6: return "PD6";
        case PinLabel::PD7: return "PD7";
        
        // Peripheral pins
        case PinLabel::UART_TX: return "UART_TX";
        case PinLabel::UART_RX: return "UART_RX";
        case PinLabel::SPI_CLK: return "SPI_CLK";
        case PinLabel::SPI_MOSI: return "SPI_MOSI";
        case PinLabel::SPI_MISO: return "SPI_MISO";
        case PinLabel::SPI_CS: return "SPI_CS";
        case PinLabel::PWM0: return "PWM0";
        case PinLabel::PWM1: return "PWM1";
        case PinLabel::PWM2: return "PWM2";
        case PinLabel::PWM3: return "PWM3";
        
        // Common control pins
        case PinLabel::CS: return "CS";
        case PinLabel::CS0: return "CS0";
        case PinLabel::CS1: return "CS1";
        case PinLabel::CS2: return "CS2";
        case PinLabel::OE: return "OE";
        case PinLabel::WE: return "WE";
        
        // Video chip pins
        case PinLabel::LUMA: return "LUMA";
        case PinLabel::CHROMA: return "CHROMA";
        case PinLabel::HSYNC: return "HSYNC";
        case PinLabel::VSYNC: return "VSYNC";
        case PinLabel::CSYNC: return "CSYNC";
        case PinLabel::DOT_CLK: return "DOT_CLK";
        case PinLabel::COLOR_CLK: return "COLOR_CLK";
        case PinLabel::LIGHT_PEN: return "LIGHT_PEN";
        case PinLabel::CAS: return "CAS";
        case PinLabel::RAS: return "RAS";
        case PinLabel::MUX: return "MUX";
        
        // Audio chip pins
        case PinLabel::AUDIO_OUT: return "AUDIO_OUT";
        case PinLabel::AUDIO_IN: return "AUDIO_IN";
        case PinLabel::FILTER_OUT: return "FILTER_OUT";
        case PinLabel::FILTER_IN: return "FILTER_IN";
        case PinLabel::OSC1: return "OSC1";
        case PinLabel::OSC2: return "OSC2";
        case PinLabel::OSC3: return "OSC3";
        case PinLabel::NOISE: return "NOISE";
        
        // CIA/Timer chip pins
        case PinLabel::CNT: return "CNT";
        case PinLabel::SP: return "SP";
        case PinLabel::TOD: return "TOD";
        case PinLabel::FLAG: return "FLAG";
        
        // Memory chip pins
        case PinLabel::DQ0: return "DQ0";
        case PinLabel::DQ1: return "DQ1";
        case PinLabel::DQ2: return "DQ2";
        case PinLabel::DQ3: return "DQ3";
        case PinLabel::DQ4: return "DQ4";
        case PinLabel::DQ5: return "DQ5";
        case PinLabel::DQ6: return "DQ6";
        case PinLabel::DQ7: return "DQ7";
        case PinLabel::MA0: return "MA0";
        case PinLabel::MA1: return "MA1";
        case PinLabel::MA2: return "MA2";
        case PinLabel::MA3: return "MA3";
        case PinLabel::MA4: return "MA4";
        case PinLabel::MA5: return "MA5";
        case PinLabel::MA6: return "MA6";
        case PinLabel::MA7: return "MA7";
        case PinLabel::MA8: return "MA8";
        case PinLabel::MA9: return "MA9";
        case PinLabel::MA10: return "MA10";
        case PinLabel::MA11: return "MA11";
        case PinLabel::MA12: return "MA12";
        case PinLabel::MA13: return "MA13";
        case PinLabel::MA14: return "MA14";
        case PinLabel::MA15: return "MA15";
        
        // Logic chip pins
        case PinLabel::Q0: return "Q0";
        case PinLabel::Q1: return "Q1";
        case PinLabel::Q2: return "Q2";
        case PinLabel::Q3: return "Q3";
        case PinLabel::Q4: return "Q4";
        case PinLabel::Q5: return "Q5";
        case PinLabel::Q6: return "Q6";
        case PinLabel::Q7: return "Q7";
        case PinLabel::I0: return "I0";
        case PinLabel::I1: return "I1";
        case PinLabel::I2: return "I2";
        case PinLabel::I3: return "I3";
        case PinLabel::I4: return "I4";
        case PinLabel::I5: return "I5";
        case PinLabel::I6: return "I6";
        case PinLabel::I7: return "I7";
        case PinLabel::Y0: return "Y0";
        case PinLabel::Y1: return "Y1";
        case PinLabel::Y2: return "Y2";
        case PinLabel::Y3: return "Y3";
        case PinLabel::Y4: return "Y4";
        case PinLabel::Y5: return "Y5";
        case PinLabel::Y6: return "Y6";
        case PinLabel::Y7: return "Y7";
        case PinLabel::S0: return "S0";
        case PinLabel::S1: return "S1";
        case PinLabel::S2: return "S2";
        case PinLabel::S3: return "S3";
        case PinLabel::G: return "G";
        
        // Test and configuration pins
        case PinLabel::TEST: return "TEST";
        case PinLabel::NC: return "NC";
        
        // Analog pins
        case PinLabel::VREF: return "VREF";
        case PinLabel::AIN0: return "AIN0";
        case PinLabel::AIN1: return "AIN1";
        case PinLabel::AIN2: return "AIN2";
        case PinLabel::AIN3: return "AIN3";
        case PinLabel::AIN4: return "AIN4";
        case PinLabel::AIN5: return "AIN5";
        case PinLabel::AIN6: return "AIN6";
        case PinLabel::AIN7: return "AIN7";
        case PinLabel::AOUT0: return "AOUT0";
        case PinLabel::AOUT1: return "AOUT1";
        
        // Crystal/oscillator pins
        case PinLabel::XTAL1: return "XTAL1";
        case PinLabel::XTAL2: return "XTAL2";
        case PinLabel::OSC_IN: return "OSC_IN";
        case PinLabel::OSC_OUT: return "OSC_OUT";
        
        // Unknown/custom pin
        case PinLabel::UNKNOWN:
        default:
            return "UNKNOWN";
    }
}

std::string pin_label_to_display_string(PinLabel label) {
    // Convert enum to basic string first
    std::string base_str = pin_label_to_string(label);
    
    // Add Unicode symbols for special pins
    if (label == PinLabel::PHI0) return "Φ0";
    if (label == PinLabel::PHI1) return "Φ1";
    if (label == PinLabel::PHI2) return "Φ2";
    
    return base_str;
}

PinLabel string_to_pin_label(const char* label_str) {
    if (!label_str) return PinLabel::UNKNOWN;
    
    // Power pins
    if (strcmp(label_str, "VDD") == 0) return PinLabel::VDD;
    if (strcmp(label_str, "VSS") == 0) return PinLabel::VSS;
    if (strcmp(label_str, "VCC") == 0) return PinLabel::VCC;
    if (strcmp(label_str, "GND") == 0) return PinLabel::GND;
    
    // Clock pins
    if (strcmp(label_str, "φ0") == 0 || strcmp(label_str, "PHI0") == 0) return PinLabel::PHI0;
    if (strcmp(label_str, "φ1") == 0 || strcmp(label_str, "PHI1") == 0) return PinLabel::PHI1;
    if (strcmp(label_str, "φ2") == 0 || strcmp(label_str, "PHI2") == 0) return PinLabel::PHI2;
    
    // Add more conversions as needed...
    
    return PinLabel::UNKNOWN;
}

PinType pin_label_to_pin_type(PinLabel label) {
    // This function is redundant since ChipPin::get_pin_type() already does this
    // But we keep it for API compatibility
    ChipPin temp_pin = {0, label, 0, false, nullptr, nullptr, false, false};
    return temp_pin.get_pin_type();
}