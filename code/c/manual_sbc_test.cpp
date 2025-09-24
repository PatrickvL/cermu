
#include <iostream>
int main() {
    // Test the exact SBC calculation
    int a = 156;      // 0x9C
    int data = 196;   // 0xC4
    int initial_carry = 1;  // From P=109 = 0x6D
    int borrow = (initial_carry) ? 0 : 1;  // 0 since carry=1
    
    std::cout << "SBC Calculation:" << std::endl;
    std::cout << "A = " << a << " (0x" << std::hex << a << ")" << std::endl;
    std::cout << "Data = " << std::dec << data << " (0x" << std::hex << data << ")" << std::endl;
    std::cout << "Initial carry = " << std::dec << initial_carry << std::endl;
    std::cout << "Borrow = " << borrow << std::endl;
    
    int temp = a - data - borrow;
    std::cout << "temp = " << std::dec << temp << std::endl;
    std::cout << "Result = " << (temp & 0xFF) << " (0x" << std::hex << (temp & 0xFF) << ")" << std::endl;
    std::cout << "Carry should be: " << (temp >= 0 ? 1 : 0) << std::endl;
    
    return 0;
}

