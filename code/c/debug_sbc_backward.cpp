
#include <iostream>
int main() {
    // Work backwards from expected result
    int expected_a = 120;  // 0x78
    int initial_a = 156;   // 0x9C  
    int data = 196;        // 0xC4
    
    std::cout << "Expected A = " << expected_a << " (0x" << std::hex << expected_a << ")" << std::endl;
    
    // What would the calculation need to be?
    // A - Data - Borrow = Result
    // 156 - 196 - Borrow = 120
    // -40 - Borrow = 120
    // Borrow = -40 - 120 = -160
    
    std::cout << "If A=156, Data=196, and Result=120:" << std::endl;
    int needed_borrow = initial_a - data - expected_a;
    std::cout << "Needed borrow = " << needed_borrow << std::endl;
    
    // Try different borrow values
    for (int borrow = 0; borrow <= 1; borrow++) {
        int temp = initial_a - data - borrow;
        int result = temp & 0xFF;
        std::cout << "Borrow=" << borrow << " → temp=" << temp << " → result=" << result << " (0x" << std::hex << result << ")" << std::endl;
    }
    
    // Check with unsigned arithmetic
    std::cout << "\nUsing uint8_t arithmetic:" << std::endl;
    for (int borrow = 0; borrow <= 1; borrow++) {
        uint8_t a_u8 = 156;
        uint8_t data_u8 = 196;
        uint8_t result = a_u8 - data_u8 - borrow;
        std::cout << "Borrow=" << borrow << " → result=" << (int)result << " (0x" << std::hex << (int)result << ")" << std::endl;
    }
    
    return 0;
}

