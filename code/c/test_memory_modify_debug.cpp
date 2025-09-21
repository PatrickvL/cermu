#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

// Test configuration
struct TestBusConfig {
    static constexpr bool has_cmos_fixes = false;
    static constexpr bool has_illegal_opcodes = true;
    static constexpr bool has_abort_pin = false;
    static constexpr bool has_stp_wai_instructions = false;
};

// Debug memory interface that shows what's happening
class DebugMemory {
private:
    uint8_t data[65536];
    
public:
    DebugMemory() {
        std::fill(data, data + 65536, 0x00);
        // Set up test program: ASL $80 at 0x1000
        data[0x1000] = 0x06;  // ASL zp opcode
        data[0x1001] = 0x80;  // Zero page address
        data[0x80] = 0x42;    // Data to shift
    }
    
    uint8_t read(uint16_t addr) {
        std::cout << "  MEM READ:  [0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << addr << "] = 0x" << std::setw(2) << (int)data[addr] << std::endl;
        return data[addr];
    }
    
    void write(uint16_t addr, uint8_t value) {
        std::cout << "  MEM WRITE: [0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << addr << "] = 0x" << std::setw(2) << (int)value << std::endl;
        data[addr] = value;
    }
    
    uint8_t peek(uint16_t addr) const {
        return data[addr];
    }
};

int main() {
    std::cout << "=== Detailed Memory Modify Debug ===\n\n";
    
    DebugMemory mem;
    CPU<TestBusConfig> cpu;
    
    // Initialize CPU
    cpu.reset();
    cpu.set_pc(0x1000);
    
    std::cout << "Initial state:\n";
    std::cout << "PC = 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Memory[0x80] = 0x" << std::hex << (int)mem.peek(0x80) << std::endl;
    std::cout << "\nStarting execution...\n\n";
    
    // Execute cycles one by one with detailed debugging
    for (int cycle = 1; cycle <= 10; cycle++) {
        std::cout << "=== CYCLE " << cycle << " ===\n";
        
        // Show current CPU state before cycle
        std::cout << "Before cycle: PC=0x" << std::hex << cpu.get_pc() 
                  << ", Step=" << (int)cpu.get_cycle_step() << std::endl;
        
        // Get the cycle descriptor
        auto cycle_desc = cpu.get_current_cycle_descriptor();
        std::cout << "Cycle descriptor: MemOp=" << (int)cycle_desc.get_mem_op() 
                  << ", DataOp=" << (int)cycle_desc.get_data_op()
                  << ", AluOp=" << (int)cycle_desc.get_alu_op()
                  << ", Sync=" << (cycle_desc.is_sync() ? "true" : "false") << std::endl;
        
        // Execute one cycle
        bool finished = cpu.cycle(mem);
        
        std::cout << "After cycle:  PC=0x" << std::hex << cpu.get_pc() 
                  << ", Step=" << (int)cpu.get_cycle_step() 
                  << ", Finished=" << (finished ? "true" : "false") << std::endl;
        
        if (finished) {
            std::cout << "\nInstruction completed after " << cycle << " cycles.\n";
            break;
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "\nFinal state:\n";
    std::cout << "PC = 0x" << std::hex << cpu.get_pc() << std::endl;
    std::cout << "Memory[0x80] = 0x" << std::hex << (int)mem.peek(0x80) << std::endl;
    
    return 0;
}