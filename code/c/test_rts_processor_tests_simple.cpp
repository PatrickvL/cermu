#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace fam65xx_cpp;

int main() {
    std::cout << "=== RTS ProcessorTests Status Check ===\n";
    
    // Check if 60.json exists
    const std::string test_file = "tests/processor_tests/6502/v1/60.json";
    std::ifstream file(test_file);
    if (!file.is_open()) {
        std::cout << "ERROR: Cannot find " << test_file << std::endl;
        std::cout << "ProcessorTests for RTS (0x60) not available." << std::endl;
        return 1;
    }
    
    std::cout << "Found RTS ProcessorTests file: " << test_file << std::endl;
    
    // Read first few lines to validate JSON format
    std::string line;
    std::getline(file, line);
    std::cout << "First line: " << line.substr(0, 50) << "..." << std::endl;
    
    // Count total lines/tests by reading the entire file
    file.clear();
    file.seekg(0, std::ios::beg);
    
    int test_count = 0;
    while (std::getline(file, line)) {
        if (line.find("\"initial\"") != std::string::npos) {
            test_count++;
        }
    }
    
    std::cout << "Estimated RTS test count: " << test_count << std::endl;
    
    // Test a single RTS instruction manually
    std::cout << "\n=== Manual RTS Test ===\n";
    
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up a simple RTS scenario
    cpu.set_pc(0x4000);        // PC at RTS instruction
    cpu.set_s(0xFC);           // Stack pointer at 0xFC
    cpu.set_a(0x00);
    cpu.set_x(0x00);
    cpu.set_y(0x00);
    cpu.set_p(0x20);           // Normal flags
    
    std::cout << "Initial state:\n";
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << "\n";
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_s()) << "\n";
    
    // Simulate memory system
    auto get_memory = [](uint16_t addr) -> uint8_t {
        switch (addr) {
            case 0x4000: return 0x60;  // RTS opcode
            case 0x01FD: return 0x55;  // Low byte of return address-1
            case 0x01FE: return 0x34;  // High byte of return address-1
            default: return 0x00;
        }
    };
    
    // Execute cycles manually
    int cycle = 0;
    while (cycle < 10) {
        uint16_t addr = (cycle == 0) ? cpu.get_pc() : cpu.get_address();
        uint8_t data = get_memory(addr);
        
        auto create_bus_state = [](uint16_t address, uint8_t memory_data) -> bus_state_t {
            bus_state_t state = 0;
            BUS_SET_ADDR(state, address);
            BUS_SET_DATA(state, memory_data);
            state |= BUS_BIT(BUS_RDY_BIT);
            return state;
        };
        
        bus_state_t bus_state = create_bus_state(addr, data);
        
        std::cout << "Cycle " << cycle << ": ";
        std::cout << "Step=" << static_cast<int>(cpu.get_cycle_step());
        std::cout << ", Addr=0x" << std::hex << std::setw(4) << std::setfill('0') << addr;
        std::cout << ", Data=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data);
        std::cout << ", PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc();
        std::cout << ", SP=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_s());
        std::cout << std::dec << "\n";
        
        // Store old values to detect changes
        uint16_t old_pc = cpu.get_pc();
        uint8_t old_step = cpu.get_cycle_step();
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction completed
        if (cpu.get_cycle_step() == 0 && cycle > 0) {
            std::cout << "Instruction completed! PC changed from 0x" << std::hex << old_pc 
                      << " to 0x" << cpu.get_pc() << std::dec << "\n";
            break;
        }
        
        cycle++;
    }
    
    std::cout << "\nFinal state:\n";
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << "\n";
    std::cout << "  SP: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(cpu.get_s()) << "\n";
    std::cout << "  Expected PC: 0x3456\n";
    std::cout << "  Expected SP: 0xFE\n";
    
    bool pc_correct = (cpu.get_pc() == 0x3456);
    bool sp_correct = (cpu.get_s() == 0xFE);
    
    std::cout << "\nTest result: " << (pc_correct && sp_correct ? "PASS" : "FAIL") << "\n";
    std::cout << "PC correct: " << (pc_correct ? "YES" : "NO") << "\n";
    std::cout << "SP correct: " << (sp_correct ? "YES" : "NO") << "\n";
    
    return (pc_correct && sp_correct) ? 0 : 1;
}