/*
 * pin_labels.cpp - Pin label enum to string conversion functions
 * 
 * This file provides efficient enum-to-string conversion for pin labels,
 * supporting both plain text and Unicode display formats.
 */

#include "chip_layout.h"
#include <unordered_map>
#include <string>

// Fast enum-to-string lookup table for basic pin names
const char* pin_label_to_string(PinLabel label) {
    switch (label) {
        // Power pins
        case PinLabel::VDD: return "VDD";
        case PinLabel::VSS: return "VSS";
        case PinLabel::VCC: return "VCC";
        case PinLabel::GND: return "GND";
        
        // Clock pins
        case PinLabel::PHI0: return "PHI0";
        case PinLabel::PHI1: return "PHI1";
        case PinLabel::PHI2: return "PHI2";
        
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

// Display string with Unicode symbols for enhanced GUI rendering
std::string pin_label_to_display_string(PinLabel label) {
    switch (label) {
        // Clock pins with Greek Phi symbols
        case PinLabel::PHI0: return "Φ0";
        case PinLabel::PHI1: return "Φ1";
        case PinLabel::PHI2: return "Φ2";
        
        // Control signals with proper notation
        case PinLabel::RW: return "R/W";
        
        // All other pins use the basic string representation
        default:
            return std::string(pin_label_to_string(label));
    }
}

// Derive pin type from pin label for GUI color coding and categorization
PinType pin_label_to_pin_type(PinLabel label) {
    switch (label) {
        // Power pins
        case PinLabel::VDD:
        case PinLabel::VSS:
        case PinLabel::VCC:
        case PinLabel::GND:
            return PinType::POWER;
            
        // Clock pins
        case PinLabel::PHI0:
        case PinLabel::PHI1:
        case PinLabel::PHI2:
        case PinLabel::XTAL1:
        case PinLabel::XTAL2:
        case PinLabel::OSC_IN:
        case PinLabel::OSC_OUT:
            return PinType::CLOCK;
            
        // Address bus pins
        case PinLabel::A0: case PinLabel::A1: case PinLabel::A2: case PinLabel::A3:
        case PinLabel::A4: case PinLabel::A5: case PinLabel::A6: case PinLabel::A7:
        case PinLabel::A8: case PinLabel::A9: case PinLabel::A10: case PinLabel::A11:
        case PinLabel::A12: case PinLabel::A13: case PinLabel::A14: case PinLabel::A15:
        case PinLabel::A16: case PinLabel::A17: case PinLabel::A18: case PinLabel::A19:
        case PinLabel::A20: case PinLabel::A21: case PinLabel::A22: case PinLabel::A23:
            return PinType::ADDRESS;
            
        // Data bus pins
        case PinLabel::D0: case PinLabel::D1: case PinLabel::D2: case PinLabel::D3:
        case PinLabel::D4: case PinLabel::D5: case PinLabel::D6: case PinLabel::D7:
        case PinLabel::D8: case PinLabel::D9: case PinLabel::D10: case PinLabel::D11:
        case PinLabel::D12: case PinLabel::D13: case PinLabel::D14: case PinLabel::D15:
            return PinType::DATA;
            
        // Control signals
        case PinLabel::RW:
        case PinLabel::SYNC:
        case PinLabel::RDY:
        case PinLabel::AEC:
        case PinLabel::BE:
        case PinLabel::BA:
        case PinLabel::CS:
        case PinLabel::CS0:
        case PinLabel::CS1:
        case PinLabel::CS2:
        case PinLabel::OE:
        case PinLabel::WE:
            return PinType::CONTROL;
            
        // Interrupt pins
        case PinLabel::IRQ:
        case PinLabel::NMI:
        case PinLabel::RES:
        case PinLabel::ABORT:
            return PinType::INTERRUPT;
            
        // I/O Port pins
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
        case PinLabel::PWM0: case PinLabel::PWM1: case PinLabel::PWM2: case PinLabel::PWM3:
            return PinType::IO_PORT;
            
        // Analog pins
        case PinLabel::VREF:
        case PinLabel::AIN0: case PinLabel::AIN1: case PinLabel::AIN2: case PinLabel::AIN3:
        case PinLabel::AIN4: case PinLabel::AIN5: case PinLabel::AIN6: case PinLabel::AIN7:
        case PinLabel::AOUT0: case PinLabel::AOUT1:
            return PinType::ANALOG;
            
        // Special pins
        case PinLabel::SO:
        case PinLabel::VP:
        case PinLabel::VPB:
        case PinLabel::VPA:
        case PinLabel::VDA:
        case PinLabel::ML:
        case PinLabel::E:
        case PinLabel::MX:
        case PinLabel::TEST:
            return PinType::SPECIAL;
            
        // No connect pins
        case PinLabel::NC:
            return PinType::NO_CONNECT;
            
        // Unknown/custom pin
        case PinLabel::UNKNOWN:
        default:
            return PinType::SPECIAL;
    }
}

// ChipPin member function implementation
PinType ChipPin::get_pin_type() const {
    return pin_label_to_pin_type(label);
}