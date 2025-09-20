#include <cstdio>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <fstream>
#include <string>

// Mock memory for RTI test cases
class TestMemory {
public:
    uint8_t memory[0x10000] = {0};
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

int main() {
    printf("=== RTI PROCESSOR TEST CASE DEBUG ===\n");
    
    // Read first few test cases from RTI ProcessorTests
    std::ifstream file("tests/processor_tests/6502/v1/40.json");
    if (!file.is_open()) {
        printf("Error: Could not open RTI test file\n");
        return 1;
    }
    
    std::string line;
    bool in_test = false;
    int test_count = 0;
    
    printf("\n=== EXAMINING FIRST RTI TEST CASES ===\n");
    while (std::getline(file, line) && test_count < 2) {
        if (line.find("\"name\":") != std::string::npos) {
            in_test = true;
            test_count++;
            printf("\n--- RTI TEST CASE %d ---\n", test_count);
            printf("Name: %s\n", line.c_str());
        }
        else if (in_test && line.find("\"initial\":") != std::string::npos) {
            printf("Initial: %s\n", line.c_str());
        }
        else if (in_test && line.find("\"final\":") != std::string::npos) {
            printf("Expected: %s\n", line.c_str());
        }
        else if (in_test && line.find("\"cycles\":") != std::string::npos) {
            printf("Cycles: %s\n", line.c_str());
        }
        else if (in_test && line.find("}") != std::string::npos && line.find("{") == std::string::npos) {
            in_test = false;
            printf("--- End of test case ---\n");
        }
    }
    file.close();
    
    printf("\n=== CREATING MINIMAL RTI PROCESSOR-STYLE TEST ===\n");
    
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    TestMemory mem;
    
    // Set up minimal RTI test similar to ProcessorTests
    mem.memory[0x0000] = 0x40;  // RTI opcode at address 0x0000
    
    // Set up stack with test values
    mem.memory[0x01FD] = 0x30;  // Status register (P) - bit 4 (B) set, bit 5 (U) set
    mem.memory[0x01FE] = 0x00;  // PC low byte
    mem.memory[0x01FF] = 0x80;  // PC high byte -> PC = 0x8000
    
    // Initialize CPU
    cpu.init_for_test();
    cpu.set_pc(0x0000);
    cpu.set_sp(0xFC);  // SP = 0xFC, so it will pull from 0x01FD, 0x01FE, 0x01FF
    cpu.set_p(0x04);   // Initial processor status (I flag set)
    
    printf("BEFORE RTI:\n");
    printf("  PC=0x%04X, SP=0x%02X, P=0x%02X\n", cpu.get_pc(), cpu.get_sp(), cpu.get_p());
    printf("  Stack: [0x01FD]=0x%02X, [0x01FE]=0x%02X, [0x01FF]=0x%02X\n",
           mem.read(0x01FD), mem.read(0x01FE), mem.read(0x01FF));
    
    // Execute RTI instruction cycle by cycle using proper bus interface
    int cycles = 0;
    bool completed = false;
    
    printf("\nExecuting RTI cycle by cycle:\n");
    
    while (cycles < 10 && !completed) {
        cycles++;
        
        uint16_t addr = cpu.get_address();
        bool rw = cpu.get_rw();
        
        printf("Cycle %d: PC=0x%04X, SP=0x%02X, Step=%d, Addr=0x%04X, R/W=%s\n",
               cycles, cpu.get_pc(), cpu.get_sp(), cpu.get_cycle_step(),
               addr, rw ? "READ" : "WRITE");
        
        // Create bus state
        bus_state_t bus_state = 0;
        BUS_SET_ADDR(bus_state, addr);
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY = 1 (ready)
        
        if (rw) {
            uint8_t read_data = mem.read(addr);
            BUS_SET_DATA(bus_state, read_data);
            printf("  Reading 0x%02X from 0x%04X\n", read_data, addr);
        } else {
            uint8_t write_data = cpu.get_write_data();
            mem.write(addr, write_data);
            BUS_SET_DATA(bus_state, write_data);
            printf("  Writing 0x%02X to 0x%04X\n", write_data, addr);
        }
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction completed
        if (cpu.get_cycle_step() == 0 && cycles > 1) {
            completed = true;
            printf("  -> RTI instruction completed\n");
            break;
        }
    }
    
    // Check results
    uint16_t actual_pc = cpu.get_pc();
    uint8_t actual_p = cpu.get_p();
    uint8_t actual_sp = cpu.get_sp();
    
    printf("\nAFTER RTI:\n");
    printf("  PC=0x%04X, SP=0x%02X, P=0x%02X\n", actual_pc, actual_sp, actual_p);
    printf("  Cycles executed: %d\n", cycles);
    
    // Expected results based on ProcessorTests format
    uint16_t expected_pc = 0x8000;
    uint8_t expected_sp = 0xFF;
    uint8_t expected_p = 0x20;  // 0x30 with B flag (bit 4) cleared
    
    printf("\nEXPECTED (ProcessorTests style):\n");
    printf("  PC=0x%04X, SP=0x%02X, P=0x%02X\n", expected_pc, expected_sp, expected_p);
    
    // Verify flag handling
    bool actual_b = (actual_p & 0x10) != 0;
    bool actual_u = (actual_p & 0x20) != 0;
    bool expected_b = false;  // B flag should be cleared by RTI
    bool expected_u = true;   // U flag should remain set
    
    printf("\nFLAG ANALYSIS:\n");
    printf("  B flag (bit 4): expected %s, actual %s - %s\n",
           expected_b ? "SET" : "CLEAR", actual_b ? "SET" : "CLEAR",
           (actual_b == expected_b) ? "PASS" : "FAIL");
    printf("  U flag (bit 5): expected %s, actual %s - %s\n",
           expected_u ? "SET" : "CLEAR", actual_u ? "SET" : "CLEAR",
           (actual_u == expected_u) ? "PASS" : "FAIL");
    
    bool passed = (actual_pc == expected_pc && actual_p == expected_p && actual_sp == expected_sp);
    
    if (passed) {
        printf("\nRESULT: PASS - RTI executed correctly in ProcessorTests style\n");
    } else {
        printf("\nRESULT: FAIL - RTI execution differs from ProcessorTests expectations\n");
        
        if (!completed) {
            printf("  Issue: Instruction did not complete within expected cycles\n");
        }
        if (actual_pc != expected_pc) {
            printf("  Issue: PC mismatch - expected 0x%04X, got 0x%04X\n", expected_pc, actual_pc);
        }
        if (actual_sp != expected_sp) {
            printf("  Issue: SP mismatch - expected 0x%02X, got 0x%02X\n", expected_sp, actual_sp);
        }
        if (actual_p != expected_p) {
            printf("  Issue: P mismatch - expected 0x%02X, got 0x%02X\n", expected_p, actual_p);
        }
        if (actual_b != expected_b) {
            printf("  Issue: B flag handling - RTI should clear B flag\n");
        }
        if (actual_u != expected_u) {
            printf("  Issue: U flag handling - RTI should preserve U flag\n");
        }
    }
    
    printf("\nThis test helps identify ProcessorTests compatibility issues vs simple test success.\n");
    
    return 0;
}