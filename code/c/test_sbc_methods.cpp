
#include <iostream>
int main() {
    // Test SBC with different interpretations
    uint8_t a = 156;     // 0x9C
    uint8_t data = 196;  // 0xC4  
    uint8_t carry_flag = 1;  // From P=109
    
    std::cout << "Testing different SBC interpretations:" << std::endl;
    std::cout << "A = " << (int)a << ", Data = " << (int)data << ", Carry = " << (int)carry_flag << std::endl;
    
    // Method 1: A - Data - (1 - Carry)  -- Standard interpretation
    uint8_t borrow1 = 1 - carry_flag;
    uint8_t result1 = a - data - borrow1;
    std::cout << "Method 1 (A - Data - (1-C)): " << (int)result1 << " (0x" << std::hex << (int)result1 << ")" << std::endl;
    
    // Method 2: A - Data - !Carry  -- Alternative interpretation  
    uint8_t borrow2 = !carry_flag;
    uint8_t result2 = a - data - borrow2;
    std::cout << "Method 2 (A - Data - !C): " << (int)result2 << " (0x" << std::hex << (int)result2 << ")" << std::endl;
    
    // Method 3: A + ~Data + Carry  -- ADC with inverted data
    uint16_t temp3 = a + (~data) + carry_flag;
    uint8_t result3 = temp3 & 0xFF;
    std::cout << "Method 3 (A + ~Data + C): " << (int)result3 << " (0x" << std::hex << (int)result3 << ")" << std::endl;
    
    std::cout << "Expected result: 120 (0x78)" << std::endl;
    
    return 0;
}

