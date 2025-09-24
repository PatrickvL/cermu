#include <iostream>
#include <cstdint>

// Simplified version of our SBC logic to test
void debug_sbc_unified(uint8_t& a_reg, uint8_t& p_reg, uint8_t data) {
    const uint8_t a = a_reg;
    uint8_t result;
    uint8_t flags;
    
    // Binary mode (decimal disabled for ProcessorTests compatibility)
    const uint8_t borrow = (p_reg & 0x01) ? 0 : 1;  // P_CARRY = 0x01
    const int16_t temp = (int16_t)a - (int16_t)data - (int16_t)borrow;
    result = temp & 0xFF;
    flags = (temp >= 0 ? 0x01 : 0) |  // P_CARRY
            ((a ^ data) & (a ^ result) & 0x80 ? 0x40 : 0);  // P_OVERFLOW
    
    // SBC only modifies C and V flags - preserve all others  
    p_reg = (p_reg & ~(0x01 | 0x40)) | flags;
    a_reg = result;
}

int main() {
    // Test the exact failing case
    uint8_t a = 156;     // Initial A = 156 (0x9C)
    uint8_t p = 109;     // Initial P = 109 (0x6D) 
    uint8_t data = 196;  // Data = 196 (0xC4)
    
    std::cout << "=== SBC Debug Test ===" << std::endl;
    std::cout << "Initial: A=0x" << std::hex << (int)a << ", P=0x" << (int)p << std::endl;
    std::cout << "Data: 0x" << (int)data << std::endl;
    
    // Show carry flag
    std::cout << "Initial carry: " << (p & 0x01 ? 1 : 0) << std::endl;
    std::cout << "Borrow: " << (p & 0x01 ? 0 : 1) << std::endl;
    
    debug_sbc_unified(a, p, data);
    
    std::cout << "Result: A=0x" << (int)a << ", P=0x" << (int)p << std::endl;
    std::cout << "Final carry: " << (p & 0x01 ? 1 : 0) << std::endl;
    
    std::cout << "\nExpected: A=0x78, P=0xAC" << std::endl;
    std::cout << "Expected carry: " << (0xAC & 0x01 ? 1 : 0) << std::endl;
    
    return 0;
}