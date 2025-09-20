#include <iostream>
#include <iomanip>
#include <cstdint>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/bus_cycle_interface.h"

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

class SimpleRTITest {
private:
    fam65xx<cpu_6502_config> cpu;
    uint8_t memory[65536];

public:
    SimpleRTITest() {
        // Clear memory
        for (int i = 0; i < 65536; i++) {
            memory[i] = 0x00;
        }
    }

    // Test RTI instruction cycle count and cycle table
    void test_rti_comprehensive() {
        std::cout << "\n=== RTI CYCLE TABLE VERIFICATION ===\n";
        
        // First, verify the cycle table definitions
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
            std::cout << "✗ Step 1 incorrectly uses MemOp " << (int)step1_cycle.mem_op << " instead of READ_PC (" << (uint8_t)MemOp::READ_PC << ")\n";
        }
        
        // Verify step 6 has SYNC bit
        auto step6_cycle = cpu.GET_CYCLE(0x40, 6);
        if (step6_cycle.is_sync()) {
            std::cout << "✓ Step 6 correctly has SYNC bit set\n";
        } else {
            std::cout << "✗ Step 6 missing SYNC bit\n";
        }
        
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
            
            // Show register state before execution
            if (cycle_count > 0 && current_step > 0) {
                std::cout << "  Before: PC=0x" << std::hex << cpu.get_pc() 
                         << ", P=0x" << (int)cpu.get_p() 
                         << ", S=0x" << (int)cpu.get_s() << std::dec << "\n";
            }
            
            // Execute cycle
            bus_state = cpu.cycle_tick(bus_state);
            cycle_count++;
            
            // Show register state after execution
            if (cycle_count > 1) {
                std::cout << "  After:  PC=0x" << std::hex << cpu.get_pc() 
                         << ", P=0x" << (int)cpu.get_p() 
                         << ", S=0x" << (int)cpu.get_s() << std::dec << "\n";
            }
            
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
                     
            // Analyze the issue
            if (cycle_count != 6) {
                std::cout << "CYCLE COUNT ISSUE: RTI should execute in exactly 6 cycles\n";
                if (cycle_count == 7) {
                    std::cout << "Suspect: Cycle 1 might be using READ_PC_INC instead of READ_PC\n";
                }
            }
            
            if (cpu.get_p() != 0x30) {
                std::cout << "FLAG RESTORATION ISSUE: P register not correctly restored\n";
                std::cout << "Expected P=0x30 (flags: --, U-, --, --, --, --, --, --)\n";
                std::cout << "Got P=0x" << std::hex << (int)cpu.get_p() << std::dec << "\n";
                
                uint8_t expected_p = 0x30;
                uint8_t actual_p = cpu.get_p();
                uint8_t diff = expected_p ^ actual_p;
                
                if (diff & 0x20) {
                    std::cout << "U flag (bit 5) issue: expected " << ((expected_p & 0x20) ? "1" : "0") 
                             << ", got " << ((actual_p & 0x20) ? "1" : "0") << "\n";
                }
                if (diff & 0x10) {
                    std::cout << "B flag (bit 4) issue: expected " << ((expected_p & 0x10) ? "1" : "0") 
                             << ", got " << ((actual_p & 0x10) ? "1" : "0") << "\n";
                }
            }
        }
    }

    // Test with different flag combinations
    void test_rti_flag_combinations() {
        std::cout << "\n=== RTI FLAG RESTORATION TEST ===\n";
        
        // Test different status values to verify flag handling
        uint8_t test_flags[] = {0x00, 0x20, 0x30, 0xFF, 0xAC, 0x8C};
        
        for (uint8_t test_flag : test_flags) {
            std::cout << "Testing with status=0x" << std::hex << (int)test_flag << std::dec << "\n";
            
            // Initialize CPU
            cpu.init_for_test();
            
            // Set up RTI instruction
            memory[0x1000] = 0x40;  // RTI opcode
            cpu.set_pc(0x1000);
            
            // Set up stack
            cpu.set_s(0xFC);
            memory[0x01FD] = test_flag;  // Test status
            memory[0x01FE] = 0x34;       // PCL
            memory[0x01FF] = 0x12;       // PCH
            
            // Execute RTI
            bus_state_t bus_state = 0;
            bus_state |= BUS_BIT(BUS_RDY_BIT);
            
            int cycles = 0;
            while (cycles < 10) {
                uint16_t addr = cpu.get_address();
                bus_state = BUS_SET_ADDR(bus_state, addr);
                bus_state = BUS_SET_DATA(bus_state, memory[addr]);
                
                bus_state = cpu.cycle_tick(bus_state);
                cycles++;
                
                if (cpu.get_cycle_step() == 0 && cycles > 1) {
                    break;
                }
            }
            
            uint8_t result_p = cpu.get_p();
            
            // RTI should clear B flag and preserve U flag
            uint8_t expected_p = (test_flag & ~0x10) | 0x20;  // Clear B (bit 4), set U (bit 5)
            
            if (result_p == expected_p) {
                std::cout << "  ✓ PASS: P=0x" << std::hex << (int)result_p << std::dec << "\n";
            } else {
                std::cout << "  ✗ FAIL: Expected P=0x" << std::hex << (int)expected_p 
                         << ", got P=0x" << (int)result_p << std::dec << "\n";
            }
        }
    }
};

int main() {
    std::cout << "=== SIMPLE RTI INSTRUCTION TEST ===\n";
    
    SimpleRTITest test;
    
    // Run tests
    test.test_rti_comprehensive();
    test.test_rti_flag_combinations();
    
    std::cout << "\n=== RTI SIMPLE TEST COMPLETE ===\n";
    return 0;
}