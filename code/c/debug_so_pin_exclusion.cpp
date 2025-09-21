#include <iostream>
#include <iomanip>

// Test which accumulator opcodes need SO pin exclusion
int main() {
    std::cout << "=== Accumulator Operations Requiring SO Pin Exclusion ===" << std::endl;
    
    // The failing accumulator operations from Priority 4
    uint8_t accumulator_opcodes[] = {
        0x0A,  // ASL A
        0x4A,  // LSR A  
        0x2A,  // ROL A
        0x6A   // ROR A
    };
    
    std::cout << "Current SO pin exclusion list:" << std::endl;
    std::cout << "0x48, 0x08, 0x68, 0x28 (stack operations)" << std::endl;
    std::cout << "0x20, 0x60, 0x40 (control flow)" << std::endl;
    std::cout << "0x4C, 0x6C (JMP instructions)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Missing accumulator operations:" << std::endl;
    for (uint8_t opcode : accumulator_opcodes) {
        std::cout << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) 
                  << (int)opcode;
        
        switch (opcode) {
            case 0x0A: std::cout << " // ASL A"; break;
            case 0x4A: std::cout << " // LSR A"; break;
            case 0x2A: std::cout << " // ROL A"; break;
            case 0x6A: std::cout << " // ROR A"; break;
        }
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Updated exclusion condition should be:" << std::endl;
    std::cout << "opcode != 0x48 && opcode != 0x08 && opcode != 0x68 && opcode != 0x28 &&" << std::endl;
    std::cout << "opcode != 0x20 && opcode != 0x60 && opcode != 0x40 &&" << std::endl;
    std::cout << "opcode != 0x4C && opcode != 0x6C &&" << std::endl;
    std::cout << "opcode != 0x0A && opcode != 0x4A && opcode != 0x2A && opcode != 0x6A" << std::endl;
    
    return 0;
}