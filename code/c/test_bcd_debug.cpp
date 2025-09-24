#include <iostream>
#include <iomanip>
#include "src/chip/cpu/fam65xx_cpp/unified_helpers.hpp"

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== BCD Addition Debug ===" << std::endl;
    
    uint8_t a = 0x40;
    uint8_t data = 0xD5;
    uint8_t carry = 0;
    
    std::cout << "Testing BCD addition: $" << std::hex << std::setfill('0') << std::setw(2) 
              << (int)a << " + $" << std::setw(2) << (int)data << " + " << (int)carry << std::endl;
    
    uint8_t bcd_result = bcd_add_unified(a, data, carry);
    
    std::cout << "BCD result: $" << std::setw(2) << (int)bcd_result << std::endl;
    
    // Binary for comparison
    uint16_t binary_result = a + data + carry;
    std::cout << "Binary result: $" << std::setw(2) << (int)(binary_result & 0xFF) 
              << " (carry=" << (binary_result > 0xFF ? 1 : 0) << ")" << std::endl;
    
    // Let's trace the BCD logic step by step
    std::cout << "\n--- BCD Logic Trace ---" << std::endl;
    uint16_t lo_nibble = (a & 0x0F) + (data & 0x0F) + carry;
    uint16_t hi_nibble = (a >> 4) + (data >> 4);
    
    std::cout << "Low nibble: " << std::hex << (a & 0x0F) << " + " << (data & 0x0F) 
              << " + " << carry << " = " << lo_nibble << std::endl;
    std::cout << "High nibble: " << (a >> 4) << " + " << (data >> 4) << " = " << hi_nibble << std::endl;
    
    if (lo_nibble > 9) {
        std::cout << "Low nibble > 9, adding 6: " << lo_nibble << " + 6 = " << (lo_nibble + 6) << std::endl;
        lo_nibble += 6;
        hi_nibble += 1;
        std::cout << "High nibble incremented: " << hi_nibble << std::endl;
    }
    if (hi_nibble > 9) {
        std::cout << "High nibble > 9, adding 6: " << hi_nibble << " + 6 = " << (hi_nibble + 6) << std::endl;
        hi_nibble += 6;
    }
    
    uint8_t final_result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
    std::cout << "Final BCD result: $" << std::setw(2) << (int)final_result << std::endl;
    
    return 0;
}