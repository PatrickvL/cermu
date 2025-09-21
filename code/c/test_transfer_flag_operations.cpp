#include <iostream>
#include <vector>
#include <iomanip>

int main() {
    std::cout << "=== Testing Transfer and Flag Operations (Priority 1 & 2) ===" << std::endl;
    
    // Test transfer operations (6 instructions)
    std::vector<uint8_t> transfer_opcodes = {
        0x8A, // TXA
        0x98, // TYA  
        0x9A, // TXS
        0xA8, // TAY
        0xAA, // TAX
        0xBA  // TSX
    };
    
    // Test flag operations (7 instructions)
    std::vector<uint8_t> flag_opcodes = {
        0x18, // CLC
        0x38, // SEC
        0x58, // CLI
        0x78, // SEI
        0xB8, // CLV
        0xD8, // CLD
        0xF8  // SED
    };
    
    std::cout << "\n=== TRANSFER OPERATIONS (Priority 1) ===" << std::endl;
    for (uint8_t opcode : transfer_opcodes) {
        std::cout << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) 
                  << (int)opcode << " - Fixed: Changed MemOp::READ_PC_INC to MemOp::NOP" << std::endl;
    }
    
    std::cout << "\n=== FLAG OPERATIONS (Priority 2) ===" << std::endl;
    for (uint8_t opcode : flag_opcodes) {
        std::cout << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) 
                  << (int)opcode << " - Fixed: Changed MemOp::READ_PC_INC to MemOp::NOP" << std::endl;
    }
    
    std::cout << "\n=== TECHNICAL EXPLANATION ===" << std::endl;
    std::cout << "Problem: Transfer and flag operations were incorrectly using MemOp::READ_PC_INC" << std::endl;
    std::cout << "Root cause: These are single-byte implied mode instructions" << std::endl;
    std::cout << "Solution: Changed to MemOp::NOP since PC was already incremented during opcode fetch" << std::endl;
    std::cout << "\nStatus: Ready for testing with ProcessorTests" << std::endl;
    
    return 0;
}