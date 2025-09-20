#include <cstdio>
#include <fstream>
#include <json/json.h>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

// Mock memory for ProcessorTests
class ProcessorTestMemory {
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
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    ProcessorTestMemory mem;
    
    printf("=== RTI PROCESSOR TESTS DEBUG ===\n");
    
    // Load ProcessorTests RTI test file
    std::ifstream file("tests/processor_tests/6502/v1/40.json");
    if (!file.is_open()) {
        printf("ERROR: Could not open RTI ProcessorTests file\n");
        return 1;
    }
    
    Json::Value root;
    file >> root;
    
    printf("Loaded %d RTI test cases\n", (int)root.size());
    
    int passed = 0;
    int failed = 0;
    int analyzed = 0;
    
    // Test first few cases to analyze failure patterns
    for (int i = 0; i < std::min(10, (int)root.size()); i++) {
        const Json::Value& test = root[i];
        printf("\n=== TEST CASE %d ===\n", i);
        
        // Extract initial state
        const Json::Value& initial = test["initial"];
        uint16_t pc = initial["pc"].asUInt();
        uint8_t s = initial["s"].asUInt();
        uint8_t a = initial["a"].asUInt();
        uint8_t x = initial["x"].asUInt();
        uint8_t y = initial["y"].asUInt();
        uint8_t p = initial["p"].asUInt();
        
        // Set up memory
        const Json::Value& ram = initial["ram"];
        for (const auto& addr_data : ram) {
            uint16_t addr = addr_data[0].asUInt();
            uint8_t data = addr_data[1].asUInt();
            mem.write(addr, data);
        }
        
        printf("Initial: PC=0x%04X, S=0x%02X, A=0x%02X, X=0x%02X, Y=0x%02X, P=0x%02X\n",
               pc, s, a, x, y, p);
        
        // Initialize CPU
        cpu.init_for_test();
        cpu.set_pc(pc);
        cpu.set_s(s);
        cpu.set_a(a);
        cpu.set_x(x);
        cpu.set_y(y);
        cpu.set_p(p);
        
        // Execute RTI instruction
        for (int cycle = 0; cycle < 10; cycle++) {
            uint16_t addr = cpu.get_address();
            bool rw = cpu.get_rw();
            uint8_t write_data = rw ? 0 : cpu.get_write_data();
            
            // Create bus state
            bus_state_t bus_state = 0;
            BUS_SET_ADDR(bus_state, addr);
            bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY = 1 (ready)
            
            if (rw) {
                uint8_t read_data = mem.read(addr);
                BUS_SET_DATA(bus_state, read_data);
                printf("Cycle %d: READ  addr=0x%04X data=0x%02X\n", cycle, addr, read_data);
            } else {
                mem.write(addr, write_data);
                BUS_SET_DATA(bus_state, write_data);
                printf("Cycle %d: WRITE addr=0x%04X data=0x%02X\n", cycle, addr, write_data);
            }
            
            // Execute cycle
            bus_state = cpu.cycle_tick(bus_state);
            
            // Check if instruction completed
            if (cpu.get_cycle_step() == 0) {
                printf("RTI completed after %d cycles\n", cycle + 1);
                break;
            }
        }
        
        // Compare final state
        const Json::Value& final = test["final"];
        uint16_t expected_pc = final["pc"].asUInt();
        uint8_t expected_s = final["s"].asUInt();
        uint8_t expected_a = final["a"].asUInt();
        uint8_t expected_x = final["x"].asUInt();
        uint8_t expected_y = final["y"].asUInt();
        uint8_t expected_p = final["p"].asUInt();
        
        uint16_t actual_pc = cpu.get_pc();
        uint8_t actual_s = cpu.get_s();
        uint8_t actual_a = cpu.get_a();
        uint8_t actual_x = cpu.get_x();
        uint8_t actual_y = cpu.get_y();
        uint8_t actual_p = cpu.get_p();
        
        printf("Expected: PC=0x%04X, S=0x%02X, A=0x%02X, X=0x%02X, Y=0x%02X, P=0x%02X\n",
               expected_pc, expected_s, expected_a, expected_x, expected_y, expected_p);
        printf("Actual:   PC=0x%04X, S=0x%02X, A=0x%02X, X=0x%02X, Y=0x%02X, P=0x%02X\n",
               actual_pc, actual_s, actual_a, actual_x, actual_y, actual_p);
        
        bool test_passed = (actual_pc == expected_pc && actual_s == expected_s &&
                           actual_a == expected_a && actual_x == expected_x &&
                           actual_y == expected_y && actual_p == expected_p);
        
        if (test_passed) {
            printf("PASS\n");
            passed++;
        } else {
            printf("FAIL\n");
            failed++;
            
            // Detailed failure analysis
            if (actual_pc != expected_pc) {
                printf("  PC MISMATCH: expected 0x%04X, got 0x%04X (diff: %+d)\n", 
                       expected_pc, actual_pc, (int)actual_pc - (int)expected_pc);
            }
            if (actual_p != expected_p) {
                printf("  P MISMATCH: expected 0x%02X, got 0x%02X\n", expected_p, actual_p);
                printf("    B flag: expected %d, got %d\n", 
                       (expected_p & 0x10) ? 1 : 0, (actual_p & 0x10) ? 1 : 0);
                printf("    U flag: expected %d, got %d\n", 
                       (expected_p & 0x20) ? 1 : 0, (actual_p & 0x20) ? 1 : 0);
            }
            if (actual_s != expected_s) {
                printf("  S MISMATCH: expected 0x%02X, got 0x%02X (diff: %+d)\n", 
                       expected_s, actual_s, (int)actual_s - (int)expected_s);
            }
        }
        
        analyzed++;
        if (analyzed >= 10) break;  // Analyze first 10 cases
    }
    
    printf("\n=== RTI ANALYSIS SUMMARY ===\n");
    printf("Analyzed: %d test cases\n", analyzed);
    printf("Passed: %d (%.1f%%)\n", passed, (100.0 * passed) / analyzed);
    printf("Failed: %d (%.1f%%)\n", failed, (100.0 * failed) / analyzed);
    
    return 0;
}