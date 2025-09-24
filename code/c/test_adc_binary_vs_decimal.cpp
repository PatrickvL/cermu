#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/unified_helpers.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_defs.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== ADC Binary vs Decimal Mode Comparison ===" << std::endl;
    
    // Test the exact same case in both binary and decimal mode
    type_safe_array<uint8_t, 14> reg_binary, reg_decimal;
    
    // Setup identical initial conditions
    reg_binary[CpuReg::A] = 0x40;
    reg_binary[CpuReg::P] = 0x24;  // D=0 (binary mode), I=1
    
    reg_decimal[CpuReg::A] = 0x40;
    reg_decimal[CpuReg::P] = 0x2C; // D=1 (decimal mode), I=1
    
    uint8_t data = 0xD5;
    
    std::cout << "Initial: A=$40, data=$D5, carry=0" << std::endl;
    
    // Test binary mode
    using BusConfig = config_6502;
    alu_adc_unified<BusConfig>(reg_binary, data);
    
    std::cout << "\nBinary Mode (D=0):" << std::endl;
    std::cout << "A = $" << std::hex << std::setfill('0') << std::setw(2) << (int)reg_binary[CpuReg::A] << std::endl;
    std::cout << "P = $" << std::setw(2) << (int)reg_binary[CpuReg::P] << std::endl;
    std::cout << "C=" << ((reg_binary[CpuReg::P] & P_CARRY) ? 1 : 0);
    std::cout << " V=" << ((reg_binary[CpuReg::P] & P_OVERFLOW) ? 1 : 0);
    std::cout << " N=" << ((reg_binary[CpuReg::P] & P_NEGATIVE) ? 1 : 0);
    std::cout << " Z=" << ((reg_binary[CpuReg::P] & P_ZERO) ? 1 : 0) << std::endl;
    
    // Test decimal mode  
    alu_adc_unified<BusConfig>(reg_decimal, data);
    
    std::cout << "\nDecimal Mode (D=1):" << std::endl;
    std::cout << "A = $" << std::setw(2) << (int)reg_decimal[CpuReg::A] << std::endl;
    std::cout << "P = $" << std::setw(2) << (int)reg_decimal[CpuReg::P] << std::endl;
    std::cout << "C=" << ((reg_decimal[CpuReg::P] & P_CARRY) ? 1 : 0);
    std::cout << " V=" << ((reg_decimal[CpuReg::P] & P_OVERFLOW) ? 1 : 0);
    std::cout << " N=" << ((reg_decimal[CpuReg::P] & P_NEGATIVE) ? 1 : 0);
    std::cout << " Z=" << ((reg_decimal[CpuReg::P] & P_ZERO) ? 1 : 0) << std::endl;
    
    // Manual binary calculation for reference
    uint16_t binary_calc = 0x40 + 0xD5;
    std::cout << "\nReference Binary: $40 + $D5 = $" << std::setw(4) << binary_calc 
              << " → A=$" << std::setw(2) << (binary_calc & 0xFF) 
              << ", C=" << (binary_calc > 0xFF ? 1 : 0) << std::endl;
    
    return 0;
}