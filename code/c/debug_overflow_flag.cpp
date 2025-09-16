#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

int main() {
    std::cout << "=== Debugging Overflow Flag in ADC ===\n";
    
    // Create CPU instance
    fam65xx_cpp::fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[65536] = {0};
    
    // Test case: "69 8f b3" - expected P=0x6d, got P=0xad
    // This suggests we need to find the initial state for this test
    
    std::cout << "Let's examine overflow flag calculation:\n";
    std::cout << "V flag should be set when:\n";
    std::cout << "- Adding two positive numbers gives negative result\n";
    std::cout << "- Adding two negative numbers gives positive result\n\n";
    
    // Test case 1: positive + positive = negative (should set V)
    uint8_t a1 = 0x50;  // +80
    uint8_t operand1 = 0x50;  // +80
    uint16_t result1 = a1 + operand1;  // +160 = 0xA0 = -96 in signed
    bool should_set_v1 = ((a1 & 0x80) == (operand1 & 0x80)) && ((a1 & 0x80) != (result1 & 0x80));
    std::cout << "Test 1: 0x" << std::hex << (int)a1 << " + 0x" << (int)operand1 
              << " = 0x" << (int)(result1 & 0xFF) 
              << " (should set V: " << (should_set_v1 ? "YES" : "NO") << ")\n";
    
    // Test case 2: negative + negative = positive (should set V)  
    uint8_t a2 = 0x80;  // -128
    uint8_t operand2 = 0x80;  // -128
    uint16_t result2 = a2 + operand2;  // -256 = 0x100 = 0x00 = 0 (positive)
    bool should_set_v2 = ((a2 & 0x80) == (operand2 & 0x80)) && ((a2 & 0x80) != (result2 & 0x80));
    std::cout << "Test 2: 0x" << std::hex << (int)a2 << " + 0x" << (int)operand2 
              << " = 0x" << (int)(result2 & 0xFF) 
              << " (should set V: " << (should_set_v2 ? "YES" : "NO") << ")\n";
              
    // Test case 3: positive + negative = should NOT set V typically
    uint8_t a3 = 0x50;  // +80
    uint8_t operand3 = 0x80;  // -128
    uint16_t result3 = a3 + operand3;  // -48 = 0xD0
    bool should_set_v3 = ((a3 & 0x80) == (operand3 & 0x80)) && ((a3 & 0x80) != (result3 & 0x80));
    std::cout << "Test 3: 0x" << std::hex << (int)a3 << " + 0x" << (int)operand3 
              << " = 0x" << (int)(result3 & 0xFF) 
              << " (should set V: " << (should_set_v3 ? "YES" : "NO") << ")\n";
    
    std::cout << "\nOverflow flag formula:\n";
    std::cout << "V = (A_sign == operand_sign) && (A_sign != result_sign)\n";
    std::cout << "where sign = bit 7\n";
    
    return 0;
}