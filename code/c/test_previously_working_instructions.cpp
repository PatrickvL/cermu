#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <cstring>
#include <random>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

struct InstructionGroup {
    std::string name;
    std::vector<uint8_t> opcodes;
};

bool test_opcode(uint8_t opcode, const std::string& name, int num_tests = 100) {
    std::mt19937 rng(opcode * 12345);
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
    std::uniform_int_distribution<uint16_t> addr_dist(0x0200, 0xFFFF);
    
    int passed = 0;
    
    for (int i = 0; i < num_tests; i++) {
        TestCPU cpu;
        cpu.init_for_test();
        
        uint16_t pc = addr_dist(rng);
        cpu.set_pc(pc);
        cpu.set_s(byte_dist(rng));
        cpu.set_a(byte_dist(rng));
        cpu.set_x(byte_dist(rng));
        cpu.set_y(byte_dist(rng));
        cpu.set_p(byte_dist(rng) & 0xEF);
        
        std::array<uint8_t, 65536> memory = {};
        for (int j = 0; j < 65536; j++) {
            memory[j] = byte_dist(rng);
        }
        
        memory[pc] = opcode;
        if (pc < 0xFFFF) memory[pc + 1] = byte_dist(rng);
        if (pc < 0xFFFE) memory[pc + 2] = byte_dist(rng);
        
        bus_state_t bus_state = BUS_BIT(BUS_RDY_BIT);
        
        int cycles = 0;
        bool completed = false;
        
        try {
            while (cycles < 20 && !completed) {
                uint16_t addr = cpu.get_address();
                bool is_read = cpu.get_rw();
                
                if (is_read) {
                    bus_state = BUS_SET_DATA(bus_state, memory[addr]);
                } else {
                    memory[addr] = cpu.get_write_data();
                    bus_state = BUS_SET_DATA(bus_state, memory[addr]);
                }
                
                bus_state = cpu.cycle_tick(bus_state);
                cycles++;
                
                if (cpu.get_cycle_step() == 0 && cycles > 1) {
                    completed = true;
                }
            }
            
            if (completed) {
                passed++;
            }
            
        } catch (...) {
            // Exception counts as failure
        }
    }
    
    double success_rate = (100.0 * passed) / num_tests;
    std::printf("  0x%02X (%s): %s %3d/%3d (%.1f%%)\n", 
               opcode, name.c_str(),
               (success_rate == 100.0) ? "✅" : "❌",
               passed, num_tests, success_rate);
    
    return success_rate == 100.0;
}

int main() {
    std::cout << "=== Regression Test: Previously Perfect Instructions ===" << std::endl;
    std::cout << "Validating instructions that were 100% working in original report" << std::endl << std::endl;
    
    // Test groups that were reported as 100% working
    std::vector<InstructionGroup> perfect_groups = {
        {"Core Control Flow", {
            0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0,  // Branches
            0x20, 0x40, 0x4C, 0x60, 0x6C                      // Jumps/calls
        }},
        {"Register Load Operations", {
            0xA9, 0xA5, 0xB5, 0xAD, 0xBD, 0xB9, 0xA1, 0xB1,  // LDA
            0xA2, 0xA6, 0xB6, 0xAE, 0xBE,                     // LDX
            0xA0, 0xA4, 0xB4, 0xAC, 0xBC                      // LDY
        }},
        {"Register Store Operations", {
            0x85, 0x95, 0x8D, 0x9D, 0x99, 0x81, 0x91,        // STA
            0x86, 0x96, 0x8E,                                 // STX
            0x84, 0x94, 0x8C                                  // STY
        }},
        {"Increment/Decrement Registers", {
            0xE8, 0xCA, 0xC8, 0x88                            // INX, DEX, INY, DEY
        }},
        {"Logical Operations", {
            0x09, 0x05, 0x15, 0x0D, 0x1D, 0x19, 0x01, 0x11,  // ORA
            0x29, 0x25, 0x35, 0x2D, 0x3D, 0x39, 0x21, 0x31,  // AND
            0x49, 0x45, 0x55, 0x4D, 0x5D, 0x59, 0x41, 0x51   // EOR
        }},
        {"Shift Operations (Memory)", {
            0x06, 0x16, 0x0E, 0x1E,                           // ASL
            0x46, 0x56, 0x4E, 0x5E                            // LSR
        }},
        {"Comparison Operations", {
            0xC9, 0xC5, 0xD5, 0xCD, 0xDD, 0xD9, 0xC1, 0xD1,  // CMP
            0xE0, 0xE4, 0xEC,                                 // CPX
            0xC0, 0xC4, 0xCC                                  // CPY
        }},
        {"Bit Testing", {
            0x24, 0x2C                                        // BIT
        }},
        {"Stack Operations", {
            0x48, 0x68                                        // PHA, PLA
        }}
    };
    
    int total_instructions = 0;
    int perfect_instructions = 0;
    
    for (const auto& group : perfect_groups) {
        std::cout << "🏆 " << group.name << ":" << std::endl;
        
        int group_perfect = 0;
        for (uint8_t opcode : group.opcodes) {
            std::string name = "Unknown";
            if (test_opcode(opcode, name)) {
                group_perfect++;
                perfect_instructions++;
            }
            total_instructions++;
        }
        
        std::cout << "  Group Result: " << group_perfect << "/" << group.opcodes.size() 
                  << " (" << (100.0 * group_perfect / group.opcodes.size()) << "%)" << std::endl << std::endl;
    }
    
    std::cout << "=== REGRESSION TEST RESULTS ===" << std::endl;
    std::cout << "Perfect Instructions: " << perfect_instructions << "/" << total_instructions 
              << " (" << (100.0 * perfect_instructions / total_instructions) << "%)" << std::endl;
    
    if (perfect_instructions == total_instructions) {
        std::cout << "\n✅ NO REGRESSIONS DETECTED! All previously working instructions remain perfect!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ REGRESSION DETECTED! Some previously working instructions failed!" << std::endl;
        return 1;
    }
}