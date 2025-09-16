#include <iostream>
#include <iomanip>

// Simple manual BCD ADC test to understand the expected behavior
int main() {
    // Test case: "69 d5 74" - ADC #$d5
    // Expected P: 0xad, Got P: 0x6d
    // Difference: bit 7 (N flag) - expected N=1, got N=0
    
    std::cout << "=== Manual BCD ADC Analysis ===" << std::endl;
    
    // Let's analyze what values could produce this result
    // ADC #$d5 means adding 0xd5 to accumulator
    
    uint8_t data = 0xd5;
    std::cout << "Data: 0x" << std::hex << std::uppercase << (int)data << std::endl;
    
    // Try different accumulator values that might be at PC 0x31e3
    for (uint8_t a = 0x70; a <= 0x80; a++) {
        for (uint8_t carry_in = 0; carry_in <= 1; carry_in++) {
            // Binary addition for flag calculation
            uint16_t binary_temp = a + data + carry_in;
            uint8_t binary_result = binary_temp & 0xFF;
            
            // BCD addition for result calculation
            uint16_t lo_nibble = (a & 0x0F) + (data & 0x0F) + carry_in;
            uint16_t hi_nibble = (a >> 4) + (data >> 4);
            
            if (lo_nibble > 9) {
                lo_nibble += 6;
                hi_nibble += 1;
            }
            if (hi_nibble > 9) {
                hi_nibble += 6;
            }
            
            uint8_t bcd_result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
            
            // Calculate flags
            bool n_binary = binary_result & 0x80;
            bool z_binary = binary_result == 0;
            bool v_flag = (~(a ^ data) & (a ^ binary_result) & 0x80) != 0;
            bool c_bcd = hi_nibble > 0x0F;
            
            uint8_t status = (n_binary ? 0x80 : 0) | (v_flag ? 0x40 : 0) | 
                           (z_binary ? 0x02 : 0) | (c_bcd ? 0x01 : 0);
            
            std::cout << "A=0x" << std::setw(2) << std::setfill('0') << (int)a
                      << " C=" << (int)carry_in
                      << " -> BCD=0x" << std::setw(2) << std::setfill('0') << (int)bcd_result
                      << " Binary=0x" << std::setw(2) << std::setfill('0') << (int)binary_result
                      << " P=0x" << std::setw(2) << std::setfill('0') << (int)status;
            
            if (status == 0xad) {
                std::cout << " *** EXPECTED! ***";
            } else if (status == 0x6d) {
                std::cout << " *** GOT! ***";
            }
            
            std::cout << " [N=" << (int)n_binary << " V=" << (int)v_flag 
                      << " Z=" << (int)z_binary << " C=" << (int)c_bcd << "]" << std::endl;
        }
    }
    
    return 0;
}