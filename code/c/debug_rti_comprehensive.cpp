#include <iostream>
#include <iomanip>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/bus_cycle_interface.h"

using json = nlohmann::json;
using namespace fam65xx_cpp;

// Create 6502 CPU configuration
struct cpu_6502_config {
    static constexpr CpuVariant cpu_variant = CpuVariant::NMOS_6502;
    static constexpr bool has_cmos_fixes = false;
    static constexpr bool has_illegal_opcodes = true;
    static constexpr bool has_abort_pin = false;
    static constexpr bool has_sync_pin = true;
    static constexpr bool has_so_pin = true;
    static constexpr bool has_be_pin = false;
    static constexpr bool has_ml_pin = false;
    static constexpr bool has_vp_pin = false;
    static constexpr bool has_aec_pin = false;
    static constexpr bool rdy_affects_writes = false;
};

class ComprehensiveRTITest {
private:
    fam65xx<cpu_6502_config> cpu;
    uint8_t memory[65536];
    
    // ProcessorTests state structure
    struct ProcessorTestState {
        uint16_t pc;
        uint8_t s;
        uint8_t a;
        uint8_t x;
        uint8_t y;
        uint8_t p;
        uint8_t ram[65536];
    };

public:
    ComprehensiveRTITest() {
        // Clear memory
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0x00;
        }
    }

    // Test RTI instruction cycle count
    void test_rti_cycle_count() {
        std::cout << "\n=== RTI CYCLE COUNT TEST ===\n";
        
        // Initialize CPU
        cpu.init_for_test();
        
        // Set up RTI instruction at address 0x1000
        memory[0x1000] = 0x40;  // RTI opcode
        
        // Set up stack with test data (status, PCL, PCH)
        cpu.set_s(0xFC);  // Stack pointer at 0xFC (will increment to 0xFD, 0xFE, 0xFF for pulls)
        memory[0x01FD] = 0x30;  // Status (P register)
        memory[0x01FE] = 0x34;  // PCL
        memory[0x01FF] = 0x12;  // PCH
        
        // Set PC to RTI instruction
        cpu.set_pc(0x1000);
        
        // Set up bus state
        bus_state_t bus_state = 0;
        bus_state = BUS_SET_ADDR(bus_state, 0x1000);
        bus_state = BUS_SET_DATA(bus_state, 0x40);
        bus_state |= BUS_BIT(BUS_RDY_BIT);
        
        int cycle_count = 0;
        uint8_t prev_step = 0;
        
        std::cout << "RTI Cycle-by-cycle execution:\n";
        
        // Execute RTI instruction cycle by cycle
        while (true) {
            uint8_t current_step = cpu.get_cycle_step();
            uint16_t current_opcode = cpu.get_opcode();
            
            if (cycle_count == 0) {
                std::cout << "Cycle " << cycle_count << ": Opcode fetch at PC=0x" 
                         << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::dec << "\n";
            } else {
                std::cout << "Cycle " << cycle_count << ": Step " << (int)current_step 
                         << ", Opcode=0x" << std::hex << std::setfill('0') << std::setw(2) << current_opcode << std::dec;
                
                // Get cycle description
                if (current_opcode == 0x40 && current_step > 0) {
                    auto cycle_desc = cpu.GET_CYCLE(0x40, current_step);
                    std::cout << ", MemOp=" << (int)cycle_desc.mem_op 
                             << ", DataOp=" << (int)cycle_desc.data_op
                             << ", Sync=" << (cycle_desc.is_sync() ? "YES" : "NO");
                }
                std::cout << "\n";
            }
            
            // Set up bus data based on address
            uint16_t addr = cpu.get_address();
            bus_state = BUS_SET_ADDR(bus_state, addr);
            bus_state = BUS_SET_DATA(bus_state, memory[addr]);
            
            // Execute cycle
            bus_state = cpu.cycle_tick(bus_state);
            cycle_count++;
            
            // Check if instruction is complete (back to step 0)
            uint8_t new_step = cpu.get_cycle_step();
            if (new_step == 0 && cycle_count > 1) {
                std::cout << "RTI instruction complete after " << cycle_count << " cycles\n";
                break;
            }
            
            // Safety check to prevent infinite loop
            if (cycle_count > 10) {
                std::cout << "ERROR: RTI took more than 10 cycles, stopping\n";
                break;
            }
            
            prev_step = current_step;
        }
        
        // Verify final state
        std::cout << "\nFinal CPU state:\n";
        std::cout << "PC=0x" << std::hex << std::setfill('0') << std::setw(4) << cpu.get_pc() << std::dec << "\n";
        std::cout << "P=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_p() << std::dec << "\n";
        std::cout << "S=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)cpu.get_s() << std::dec << "\n";
        
        // Expected: PC=0x1234, P=0x30, S=0xFF
        if (cycle_count == 6 && cpu.get_pc() == 0x1234 && cpu.get_p() == 0x30 && cpu.get_s() == 0xFF) {
            std::cout << "✓ RTI cycle count test PASSED (6 cycles, correct state)\n";
        } else {
            std::cout << "✗ RTI cycle count test FAILED\n";
            std::cout << "Expected: 6 cycles, PC=0x1234, P=0x30, S=0xFF\n";
            std::cout << "Got: " << cycle_count << " cycles, PC=0x" << std::hex << cpu.get_pc() 
                     << ", P=0x" << (int)cpu.get_p() << ", S=0x" << (int)cpu.get_s() << std::dec << "\n";
        }
    }

    // Test RTI with ProcessorTests-style data
    void test_rti_processor_tests() {
        std::cout << "\n=== RTI PROCESSORTESTS COMPATIBILITY TEST ===\n";
        
        // Load a few test cases from ProcessorTests
        const std::string filename = "tests/processor_tests/6502/v1/40.json";
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "Could not open ProcessorTests file: " << filename << "\n";
            return;
        }
        
        json test_data;
        try {
            file >> test_data;
        } catch (const std::exception& e) {
            std::cout << "Error parsing JSON: " << e.what() << "\n";
            return;
        }
        
        int passed = 0;
        int total = 0;
        
        // Test first 10 cases for detailed debugging
        for (int i = 0; i < std::min(10, (int)test_data.size()); i++) {
            auto test_case = test_data[i];
            auto initial = test_case["initial"];
            auto final = test_case["final"];
            auto cycles = test_case["cycles"];
            
            // Initialize CPU with test case initial state
            cpu.init_for_test();
            cpu.set_pc(initial["pc"]);
            cpu.set_s(initial["s"]);
            cpu.set_a(initial["a"]);
            cpu.set_x(initial["x"]);
            cpu.set_y(initial["y"]);
            cpu.set_p(initial["p"]);
            
            // Set up memory
            for (const auto& ram_entry : initial["ram"]) {
                uint16_t addr = ram_entry[0];
                uint8_t value = ram_entry[1];
                memory[addr] = value;
            }
            
            // Execute RTI instruction
            bus_state_t bus_state = 0;
            bus_state |= BUS_BIT(BUS_RDY_BIT);
            
            int executed_cycles = 0;
            
            while (true) {
                uint16_t addr = cpu.get_address();
                bus_state = BUS_SET_ADDR(bus_state, addr);
                bus_state = BUS_SET_DATA(bus_state, memory[addr]);
                
                bus_state = cpu.cycle_tick(bus_state);
                executed_cycles++;
                
                if (cpu.get_cycle_step() == 0 && executed_cycles > 1) {
                    break;
                }
                
                if (executed_cycles > 20) {
                    std::cout << "Test " << i << ": ERROR - too many cycles\n";
                    break;
                }
            }
            
            // Check results
            bool pc_match = cpu.get_pc() == (uint16_t)final["pc"];
            bool s_match = cpu.get_s() == (uint8_t)final["s"];
            bool a_match = cpu.get_a() == (uint8_t)final["a"];
            bool x_match = cpu.get_x() == (uint8_t)final["x"];
            bool y_match = cpu.get_y() == (uint8_t)final["y"];
            bool p_match = cpu.get_p() == (uint8_t)final["p"];
            bool cycles_match = executed_cycles == (int)cycles.size();
            
            bool test_passed = pc_match && s_match && a_match && x_match && y_match && p_match && cycles_match;
            
            if (test_passed) {
                passed++;
            } else {
                std::cout << "Test " << i << " FAILED:\n";
                std::cout << "  Expected cycles: " << cycles.size() << ", got: " << executed_cycles << "\n";
                std::cout << "  Expected PC: 0x" << std::hex << (uint16_t)final["pc"] << ", got: 0x" << cpu.get_pc() << std::dec << "\n";
                std::cout << "  Expected P: 0x" << std::hex << (uint8_t)final["p"] << ", got: 0x" << (int)cpu.get_p() << std::dec << "\n";
                std::cout << "  Expected S: 0x" << std::hex << (uint8_t)final["s"] << ", got: 0x" << (int)cpu.get_s() << std::dec << "\n";
                
                // Debug flag differences
                uint8_t expected_p = final["p"];
                uint8_t actual_p = cpu.get_p();
                if (expected_p != actual_p) {
                    std::cout << "  Flag analysis:\n";
                    std::cout << "    Expected P: 0b" << std::bitset<8>(expected_p) << " (0x" << std::hex << (int)expected_p << ")\n";
                    std::cout << "    Actual P:   0b" << std::bitset<8>(actual_p) << " (0x" << (int)actual_p << ")\n" << std::dec;
                    std::cout << "    Difference: 0b" << std::bitset<8>(expected_p ^ actual_p) << "\n";
                    
                    // Check specific flags
                    if ((expected_p & 0x20) != (actual_p & 0x20)) {
                        std::cout << "    U flag (bit 5) mismatch: expected " << ((expected_p & 0x20) ? "1" : "0") 
                                 << ", got " << ((actual_p & 0x20) ? "1" : "0") << "\n";
                    }
                    if ((expected_p & 0x10) != (actual_p & 0x10)) {
                        std::cout << "    B flag (bit 4) mismatch: expected " << ((expected_p & 0x10) ? "1" : "0") 
                                 << ", got " << ((actual_p & 0x10) ? "1" : "0") << "\n";
                    }
                }
            }
            total++;
        }
        
        std::cout << "\nProcessorTests compatibility: " << passed << "/" << total << " passed (" 
                 << (100.0 * passed / total) << "%)\n";
    }

    // Test cycle table verification
    void test_cycle_table() {
        std::cout << "\n=== RTI CYCLE TABLE VERIFICATION ===\n";
        
        for (int step = 1; step <= 6; step++) {
            auto cycle_desc = cpu.GET_CYCLE(0x40, step);
            std::cout << "Step " << step << ": MemOp=" << (int)cycle_desc.mem_op 
                     << ", DataOp=" << (int)cycle_desc.data_op 
                     << ", AluOp=" << (int)cycle_desc.alu_op
                     << ", Sync=" << (cycle_desc.is_sync() ? "YES" : "NO") << "\n";
        }
        
        // Verify step 1 uses READ_PC (not READ_PC_INC)
        auto step1_cycle = cpu.GET_CYCLE(0x40, 1);
        if (step1_cycle.mem_op == (uint8_t)MemOp::READ_PC) {
            std::cout << "✓ Step 1 correctly uses READ_PC (no increment)\n";
        } else {
            std::cout << "✗ Step 1 incorrectly uses MemOp " << (int)step1_cycle.mem_op << " instead of READ_PC\n";
        }
        
        // Verify step 6 has SYNC bit
        auto step6_cycle = cpu.GET_CYCLE(0x40, 6);
        if (step6_cycle.is_sync()) {
            std::cout << "✓ Step 6 correctly has SYNC bit set\n";
        } else {
            std::cout << "✗ Step 6 missing SYNC bit\n";
        }
    }
};

int main() {
    std::cout << "=== COMPREHENSIVE RTI INSTRUCTION TEST ===\n";
    
    ComprehensiveRTITest test;
    
    // Run all tests
    test.test_cycle_table();
    test.test_rti_cycle_count();
    test.test_rti_processor_tests();
    
    std::cout << "\n=== RTI COMPREHENSIVE TEST COMPLETE ===\n";
    return 0;
}