#include <iostream>
#include <iomanip>

int main() {
    std::cout << "=== Specific Test Case Analysis ===" << std::endl;
    
    // Test case from line 112: "69 d5 74"
    uint8_t a = 0xA7;        // Initial A = 167
    uint8_t data = 0xD5;     // ADC operand = 213
    uint8_t initial_p = 0x2F; // Initial P = 47
    
    std::cout << "Initial: A=0x" << std::hex << std::uppercase << (int)a 
              << " P=0x" << (int)initial_p << std::endl;
    
    // Extract carry from initial P
    uint8_t carry_in = (initial_p & 0x01) ? 1 : 0;
    bool decimal_mode = (initial_p & 0x08) != 0;
    
    std::cout << "Decimal mode: " << decimal_mode << ", Carry in: " << (int)carry_in << std::endl;
    
    if (decimal_mode) {
        // BCD mode calculation
        
        // Binary addition for N/Z/V flags
        uint16_t binary_temp = a + data + carry_in;
        uint8_t binary_result = binary_temp & 0xFF;
        
        std::cout << "Binary addition: 0x" << (int)a << " + 0x" << (int)data 
                  << " + " << (int)carry_in << " = 0x" << (int)binary_temp 
                  << " (result: 0x" << (int)binary_result << ")" << std::endl;
        
        // BCD addition for final result and carry
        uint16_t lo_nibble = (a & 0x0F) + (data & 0x0F) + carry_in;
        uint16_t hi_nibble = (a >> 4) + (data >> 4);
        
        std::cout << "BCD: lo_nibble = " << std::dec << (int)lo_nibble 
                  << ", hi_nibble = " << (int)hi_nibble << std::endl;
        
        if (lo_nibble > 9) {
            lo_nibble += 6;
            hi_nibble += 1;
            std::cout << "Corrected lo_nibble: " << (int)lo_nibble << ", hi_nibble: " << (int)hi_nibble << std::endl;
        }
        if (hi_nibble > 9) {
            hi_nibble += 6;
            std::cout << "Corrected hi_nibble: " << (int)hi_nibble << std::endl;
        }
        
        uint8_t bcd_result = ((hi_nibble & 0x0F) << 4) | (lo_nibble & 0x0F);
        
        // Calculate flags
        bool n_flag = binary_result & 0x80;  // N based on binary result
        bool z_flag = binary_result == 0;    // Z based on binary result  
        bool v_flag = (~(a ^ data) & (a ^ binary_result) & 0x80) != 0; // V based on binary
        bool c_flag = hi_nibble > 0x0F;      // C based on BCD
        
        std::cout << "BCD result: 0x" << std::hex << (int)bcd_result << std::endl;
        std::cout << "Flags: N=" << (int)n_flag << " V=" << (int)v_flag 
                  << " Z=" << (int)z_flag << " C=" << (int)c_flag << std::endl;
        
        // Construct final P register (preserve D and I bits from calculation context)
        uint8_t final_p = (n_flag ? 0x80 : 0) | (v_flag ? 0x40 : 0) |
                         (0x20) |  // Unused bit
                         (0x08) |  // Decimal bit
                         (0x00) |  // IRQ bit (cleared in expected result)
                         (z_flag ? 0x02 : 0) | (c_flag ? 0x01 : 0);
        
        std::cout << "Expected: A=0xE3 P=0xAD" << std::endl;
        std::cout << "Calculated: A=0x" << std::hex << (int)bcd_result 
                  << " P=0x" << (int)final_p << std::endl;
        
        if (bcd_result == 0xE3 && final_p == 0xAD) {
            std::cout << "*** PERFECT MATCH! ***" << std::endl;
        } else {
            std::cout << "*** MISMATCH! ***" << std::endl;
            std::cout << "A difference: " << (int)(bcd_result - 0xE3) << std::endl;
            std::cout << "P difference: 0x" << (int)(final_p ^ 0xAD) << std::endl;
        }
    }
    
    return 0;
}