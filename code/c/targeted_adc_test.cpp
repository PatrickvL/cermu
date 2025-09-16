#include <iostream>
#include <iomanip>

// Targeted test to find conditions that produce 0xad or 0x6d
int main() {
    std::cout << "=== Targeted BCD ADC Test - Looking for 0xAD vs 0x6D ===" << std::endl;
    
    uint8_t data = 0xd5;
    
    // Test all possible A values and carry states
    for (uint16_t a = 0x00; a <= 0xFF; a++) {
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
            
            // Construct status register (D flag = 1 since we're in decimal mode)
            uint8_t status = (n_binary ? 0x80 : 0) | (v_flag ? 0x40 : 0) | 
                           (0x20) | // Unused bit always 1
                           (0x08) | // Decimal flag = 1
                           (z_binary ? 0x02 : 0) | (c_bcd ? 0x01 : 0);
            
            // Check if we found our target values
            if (status == 0xad || status == 0x6d) {
                std::cout << "A=0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << a
                          << " C=" << (int)carry_in
                          << " -> BCD=0x" << std::setw(2) << std::setfill('0') << (int)bcd_result
                          << " Binary=0x" << std::setw(2) << std::setfill('0') << (int)binary_result
                          << " P=0x" << std::setw(2) << std::setfill('0') << (int)status;
                
                if (status == 0xad) {
                    std::cout << " *** EXPECTED! ***";
                } else if (status == 0x6d) {
                    std::cout << " *** GOT (WRONG)! ***";
                }
                
                std::cout << " [N=" << (int)n_binary << " V=" << (int)v_flag 
                          << " Z=" << (int)z_binary << " C=" << (int)c_bcd << "]" << std::endl;
            }
        }
    }
    
    return 0;
}