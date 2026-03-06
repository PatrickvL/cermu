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
        // Active-low pins — base name without "/" prefix.
        // The display layer adds "/" via get_invert_logic().
        case PinLabel::_ABORT: return "ABORT";
        case PinLabel::_AEC: return "AEC";
        case PinLabel::_BASIC: return "BASIC";
        case PinLabel::_CAS: return "CAS";
        case PinLabel::_CASRAM_PLA: return "CASRAM";
        case PinLabel::_CHAREN: return "CHAREN";
        case PinLabel::_CHAROM: return "CHAROM";
        case PinLabel::_CS: return "CS";
        case PinLabel::_CS0: return "CS0";
        case PinLabel::_CS1: return "CS1";
        case PinLabel::_CS2: return "CS2";
        case PinLabel::_EXROM: return "EXROM";
        case PinLabel::_GAME: return "GAME";
        case PinLabel::_HIRAM: return "HIRAM";
        case PinLabel::_IO: return "I/O";
        case PinLabel::_IRQ: return "IRQ";
        case PinLabel::_IRQA: return "IRQA";
        case PinLabel::_IRQB: return "IRQB";
        case PinLabel::_KERNAL: return "KERNAL";
        case PinLabel::_LORAM: return "LORAM";
        case PinLabel::_ML: return "ML";
        case PinLabel::_NMI: return "NMI";
        case PinLabel::_OE: return "OE";
        case PinLabel::_RAS: return "RAS";
        case PinLabel::_RD: return "RD";
        case PinLabel::_RES: return "RES";
        case PinLabel::_ROMH: return "ROMH";
        case PinLabel::_ROML: return "ROML";
        case PinLabel::_SO: return "SO";
        case PinLabel::_VA14: return "VA14";
        case PinLabel::_VP: return "VP";
        case PinLabel::_VPB: return "VPB";
        case PinLabel::_WE: return "WE";

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
        case PinLabel::CS3: return "CS3";
        case PinLabel::CURSOR: return "CURSOR";
        case PinLabel::DE: return "DE";
        case PinLabel::DUMP: return "DUMP";
        case PinLabel::ENABLE: return "E";
        case PinLabel::LPSTB: return "LPSTB";
        case PinLabel::OE: return "OE";
        case PinLabel::WE: return "WE";
        case PinLabel::RD: return "RD";
        case PinLabel::RS: return "RS";
        case PinLabel::RS0: return "RS0";
        case PinLabel::RS1: return "RS1";
        case PinLabel::ALE: return "ALE";
        
        // Video chip pins
        case PinLabel::LUMA: return "LUMA";
        case PinLabel::COLU: return "COLU";
        case PinLabel::CHROMA: return "CHROMA";
        case PinLabel::HSYNC: return "HSYNC";
        case PinLabel::VSYNC: return "VSYNC";
        case PinLabel::CSYNC: return "CSYNC";
        case PinLabel::COMP_BLK: return "BLK";
        case PinLabel::DOT_CLK: return "DOT CLK";
        case PinLabel::COLOR_CLK: return "COLOR CLK";
        case PinLabel::LIGHT_PEN: return "LP";
        case PinLabel::CAS: return "CAS";
        case PinLabel::RAS: return "RAS";
        case PinLabel::MUX: return "MUX";
        case PinLabel::VOUT: return "VOUT";
        case PinLabel::RA0: return "RA0";
        case PinLabel::RA1: return "RA1";
        case PinLabel::RA2: return "RA2";
        case PinLabel::RA3: return "RA3";
        case PinLabel::RA4: return "RA4";
        
        // Audio chip pins
        case PinLabel::AUD0: return "AUD0";
        case PinLabel::AUD1: return "AUD1";
        case PinLabel::AUDIO_OUT: return "AUDIO OUT";
        case PinLabel::AUDIO_IN: return "AUDIO IN";
        case PinLabel::FILTER_OUT: return "FILT OUT";
        case PinLabel::FILTER_IN: return "FILT IN";
        case PinLabel::OSC1: return "OSC1";
        case PinLabel::OSC2: return "OSC2";
        case PinLabel::OSC3: return "OSC3";
        case PinLabel::NOISE: return "NOISE";
        
        // CIA/Timer chip pins
        case PinLabel::CNT: return "CNT";
        case PinLabel::SP: return "SP";
        case PinLabel::TOD: return "TOD";
        case PinLabel::FLAG: return "FLAG";
        case PinLabel::PC: return "PC";
        case PinLabel::SDR: return "SDR";
        
        // VIA handshake pins
        case PinLabel::CA1: return "CA1";
        case PinLabel::CA2: return "CA2";
        case PinLabel::CB1: return "CB1";
        case PinLabel::CB2: return "CB2";
        
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
        case PinLabel::CASRAM: return "CASRAM";
        
        // SID chip pins
        case PinLabel::CAP1A: return "CAP1A";
        case PinLabel::CAP1B: return "CAP1B";
        case PinLabel::CAP2A: return "CAP2A";
        case PinLabel::CAP2B: return "CAP2B";
        case PinLabel::POTX: return "POTX";
        case PinLabel::POTY: return "POTY";
        case PinLabel::EXT_IN: return "EXT IN";

        // TIA-specific input pins
        case PinLabel::INPT0: return "INPT0";
        case PinLabel::INPT1: return "INPT1";
        case PinLabel::INPT2: return "INPT2";
        case PinLabel::INPT3: return "INPT3";
        case PinLabel::INPT4: return "INPT4";
        case PinLabel::INPT5: return "INPT5";

        // TIA power
        case PinLabel::VTIA: return "Vtia";
        
        // VIC-II specific
        case PinLabel::COLOR: return "COLOR";
        case PinLabel::SOUND: return "SOUND";
        
        // PLA specific pins
        case PinLabel::BASIC: return "BASIC";
        case PinLabel::KERNAL: return "KERNAL";
        case PinLabel::CHAROM: return "CHAROM";
        case PinLabel::CASRAM_PLA: return "CASRAM";
        case PinLabel::GRW: return "GRW";
        case PinLabel::IO: return "I/O";
        case PinLabel::ROML: return "ROML";
        case PinLabel::ROMH: return "ROMH";
        case PinLabel::GAME: return "GAME";
        case PinLabel::EXROM: return "EXROM";
        case PinLabel::CHAREN: return "CHAREN";
        case PinLabel::LORAM: return "LORAM";
        case PinLabel::HIRAM: return "HIRAM";
        case PinLabel::VA12: return "VA12";
        case PinLabel::VA13: return "VA13";
        case PinLabel::VA14: return "VA14";
        
        // Keyboard matrix pins
        case PinLabel::K0: return "K0";
        case PinLabel::K1: return "K1";
        case PinLabel::K2: return "K2";
        case PinLabel::K3: return "K3";
        case PinLabel::K4: return "K4";
        case PinLabel::K5: return "K5";
        case PinLabel::K6: return "K6";
        case PinLabel::K7: return "K7";
        
        // NES-specific pins
        case PinLabel::AD1: return "AD1";
        case PinLabel::AD2: return "AD2";
        case PinLabel::IN0: return "IN0";
        case PinLabel::IN1: return "IN1";
        case PinLabel::OUT0: return "OUT0";
        case PinLabel::OUT1: return "OUT1";
        case PinLabel::OUT2: return "OUT2";
        case PinLabel::EXT0: return "EXT0";
        case PinLabel::EXT1: return "EXT1";
        case PinLabel::EXT2: return "EXT2";
        case PinLabel::EXT3: return "EXT3";
        
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
        
        // Clock pins
        case PinLabel::CLK: return "CLK";
        case PinLabel::M2: return "M2";
        
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
        // Active-low interrupt pins
        case PinLabel::_ABORT:
        case PinLabel::_IRQ:
        case PinLabel::_IRQA:
        case PinLabel::_IRQB:
        case PinLabel::_NMI:
        case PinLabel::_RES:
            return PinType::INTERRUPT;

        // Active-low control pins
        case PinLabel::_AEC:
        case PinLabel::_BASIC:
        case PinLabel::_CAS:
        case PinLabel::_CASRAM_PLA:
        case PinLabel::_CHAREN:
        case PinLabel::_CHAROM:
        case PinLabel::_CS:
        case PinLabel::_CS0:
        case PinLabel::_CS1:
        case PinLabel::_CS2:
        case PinLabel::_EXROM:
        case PinLabel::_GAME:
        case PinLabel::_HIRAM:
        case PinLabel::_IO:
        case PinLabel::_KERNAL:
        case PinLabel::_LORAM:
        case PinLabel::_OE:
        case PinLabel::_RAS:
        case PinLabel::_RD:
        case PinLabel::_ROMH:
        case PinLabel::_ROML:
        case PinLabel::_WE:
            return PinType::CONTROL;

        // Active-low special pins
        case PinLabel::_ML:
        case PinLabel::_SO:
        case PinLabel::_VP:
        case PinLabel::_VPB:
            return PinType::SPECIAL;

        // Active-low address pin
        case PinLabel::_VA14:
            return PinType::ADDRESS;

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
        case PinLabel::CLK:
        case PinLabel::M2:
        case PinLabel::DOT_CLK:
        case PinLabel::COLOR_CLK:
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
        case PinLabel::MA0: case PinLabel::MA1: case PinLabel::MA2: case PinLabel::MA3:
        case PinLabel::MA4: case PinLabel::MA5: case PinLabel::MA6: case PinLabel::MA7:
        case PinLabel::MA8: case PinLabel::MA9: case PinLabel::MA10: case PinLabel::MA11:
        case PinLabel::MA12: case PinLabel::MA13: case PinLabel::MA14: case PinLabel::MA15:
        case PinLabel::RA0: case PinLabel::RA1: case PinLabel::RA2: case PinLabel::RA3:
        case PinLabel::RA4:
            return PinType::ADDRESS;
            
        // Data bus pins
        case PinLabel::D0: case PinLabel::D1: case PinLabel::D2: case PinLabel::D3:
        case PinLabel::D4: case PinLabel::D5: case PinLabel::D6: case PinLabel::D7:
        case PinLabel::D8: case PinLabel::D9: case PinLabel::D10: case PinLabel::D11:
        case PinLabel::D12: case PinLabel::D13: case PinLabel::D14: case PinLabel::D15:
        case PinLabel::DQ0: case PinLabel::DQ1: case PinLabel::DQ2: case PinLabel::DQ3:
        case PinLabel::DQ4: case PinLabel::DQ5: case PinLabel::DQ6: case PinLabel::DQ7:
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
        case PinLabel::CS3:
        case PinLabel::CURSOR:
        case PinLabel::DE:
        case PinLabel::DUMP:
        case PinLabel::ENABLE:
        case PinLabel::LPSTB:
        case PinLabel::OE:
        case PinLabel::WE:
        case PinLabel::RD:
        case PinLabel::RS:
        case PinLabel::RS0:
        case PinLabel::RS1:
        case PinLabel::ALE:
        case PinLabel::CAS:
        case PinLabel::RAS:
        case PinLabel::MUX:
        case PinLabel::CASRAM:
        case PinLabel::CASRAM_PLA:
        case PinLabel::GRW:
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
        case PinLabel::CA1: case PinLabel::CA2: case PinLabel::CB1: case PinLabel::CB2:
        case PinLabel::CNT: case PinLabel::SP: case PinLabel::TOD: case PinLabel::FLAG:
        case PinLabel::PC: case PinLabel::SDR:
        case PinLabel::K0: case PinLabel::K1: case PinLabel::K2: case PinLabel::K3:
        case PinLabel::K4: case PinLabel::K5: case PinLabel::K6: case PinLabel::K7:
        case PinLabel::AD1: case PinLabel::AD2:
        case PinLabel::IN0: case PinLabel::IN1:
        case PinLabel::OUT0: case PinLabel::OUT1: case PinLabel::OUT2:
        case PinLabel::EXT0: case PinLabel::EXT1: case PinLabel::EXT2: case PinLabel::EXT3:
        case PinLabel::POTX: case PinLabel::POTY:
        case PinLabel::LIGHT_PEN:
        case PinLabel::INPT0: case PinLabel::INPT1: case PinLabel::INPT2:
        case PinLabel::INPT3: case PinLabel::INPT4: case PinLabel::INPT5:
            return PinType::IO_PORT;
            
        // Analog pins
        case PinLabel::VREF:
        case PinLabel::AIN0: case PinLabel::AIN1: case PinLabel::AIN2: case PinLabel::AIN3:
        case PinLabel::AIN4: case PinLabel::AIN5: case PinLabel::AIN6: case PinLabel::AIN7:
        case PinLabel::AOUT0: case PinLabel::AOUT1:
        case PinLabel::CAP1A: case PinLabel::CAP1B: case PinLabel::CAP2A: case PinLabel::CAP2B:
        case PinLabel::FILTER_OUT: case PinLabel::FILTER_IN:
        case PinLabel::EXT_IN:
        case PinLabel::VTIA:
            return PinType::ANALOG;
            
        // Video output pins
        case PinLabel::LUMA:
        case PinLabel::COLU:
        case PinLabel::CHROMA:
        case PinLabel::HSYNC:
        case PinLabel::VSYNC:
        case PinLabel::CSYNC:
        case PinLabel::COMP_BLK:
        case PinLabel::COLOR:
        case PinLabel::VOUT:
            return PinType::VIDEO;
            
        // Audio output pins
        case PinLabel::AUD0: case PinLabel::AUD1:
        case PinLabel::AUDIO_OUT:
        case PinLabel::AUDIO_IN:
        case PinLabel::SOUND:
        case PinLabel::OSC1: case PinLabel::OSC2: case PinLabel::OSC3:
        case PinLabel::NOISE:
            return PinType::AUDIO;
            
        // PLA/ROM select pins
        case PinLabel::BASIC: case PinLabel::KERNAL: case PinLabel::CHAROM:
        case PinLabel::IO: case PinLabel::ROML: case PinLabel::ROMH:
        case PinLabel::GAME: case PinLabel::EXROM:
        case PinLabel::CHAREN: case PinLabel::LORAM: case PinLabel::HIRAM:
            return PinType::CONTROL;
            
        // Video address lines
        case PinLabel::VA12: case PinLabel::VA13: case PinLabel::VA14:
            return PinType::ADDRESS;
            
        // Logic chip pins
        case PinLabel::Q0: case PinLabel::Q1: case PinLabel::Q2: case PinLabel::Q3:
        case PinLabel::Q4: case PinLabel::Q5: case PinLabel::Q6: case PinLabel::Q7:
        case PinLabel::I0: case PinLabel::I1: case PinLabel::I2: case PinLabel::I3:
        case PinLabel::I4: case PinLabel::I5: case PinLabel::I6: case PinLabel::I7:
        case PinLabel::Y0: case PinLabel::Y1: case PinLabel::Y2: case PinLabel::Y3:
        case PinLabel::Y4: case PinLabel::Y5: case PinLabel::Y6: case PinLabel::Y7:
        case PinLabel::S0: case PinLabel::S1: case PinLabel::S2: case PinLabel::S3:
        case PinLabel::G:
            return PinType::DATA;
            
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