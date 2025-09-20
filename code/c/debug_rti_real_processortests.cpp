#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <cstring>
#include <tuple>

struct ProcessorTest {
    std::string name;
    uint16_t initial_pc, initial_s, initial_a, initial_x, initial_y, initial_p;
    uint16_t final_pc, final_s, final_a, final_x, final_y, final_p;
    std::vector<std::tuple<uint16_t, uint8_t>> initial_ram;
    std::vector<std::tuple<uint16_t, uint8_t>> final_ram;
    std::vector<std::tuple<uint16_t, uint8_t, std::string>> cycles;
};

// Test case from ProcessorTests RTI 40.json
ProcessorTest get_test_case_1() {
    ProcessorTest test;
    test.name = "40 9c 2c";
    test.initial_pc = 34673;
    test.initial_s = 110;
    test.initial_a = 162;
    test.initial_x = 129;
    test.initial_y = 126;
    test.initial_p = 99;
    
    test.final_pc = 26026;
    test.final_s = 113;
    test.final_a = 162;
    test.final_x = 129;
    test.final_y = 126;
    test.final_p = 172;
    
    // Initial RAM
    test.initial_ram.push_back({34673, 64});   // RTI opcode at PC
    test.initial_ram.push_back({34674, 156});  // Next bytes
    test.initial_ram.push_back({34675, 44});
    test.initial_ram.push_back({366, 152});    // Stack area
    test.initial_ram.push_back({367, 156});
    test.initial_ram.push_back({368, 170});
    test.initial_ram.push_back({369, 101});
    test.initial_ram.push_back({26026, 14});    // Target address
    
    // Final RAM (should be unchanged)
    test.final_ram.push_back({366, 152});
    test.final_ram.push_back({367, 156});
    test.final_ram.push_back({368, 170});
    test.final_ram.push_back({369, 101});
    test.final_ram.push_back({26026, 14});
    test.final_ram.push_back({34673, 64});
    test.final_ram.push_back({34674, 156});
    test.final_ram.push_back({34675, 44});
    
    // Expected cycles
    test.cycles.push_back({34673, 64, "read"});
    test.cycles.push_back({34674, 156, "read"});
    test.cycles.push_back({366, 152, "read"});
    test.cycles.push_back({367, 156, "read"});
    test.cycles.push_back({368, 170, "read"});
    test.cycles.push_back({369, 101, "read"});
    
    return test;
}

class TestMemory {
public:
    uint8_t memory[65536];
    
    TestMemory() {
        memset(memory, 0, sizeof(memory));
    }
    
    void load_ram(const std::vector<std::tuple<uint16_t, uint8_t>>& ram) {
        for (const auto& entry : ram) {
            memory[std::get<0>(entry)] = std::get<1>(entry);
        }
    }
    
    uint8_t read(uint16_t addr) {
        return memory[addr];
    }
    
    void write(uint16_t addr, uint8_t data) {
        memory[addr] = data;
    }
};

bool run_processor_test(const ProcessorTest& test) {
    std::cout << "\n=== RUNNING REAL PROCESSOR TEST: " << test.name << " ===\n";
    
    // Initialize CPU core
    using Config = config_6502;
    fam65xx_cpp::fam65xx<Config> cpu;
    cpu.init_for_test();
    
    // Initialize memory
    TestMemory mem;
    mem.load_ram(test.initial_ram);
    
    // Set initial CPU state
    cpu.set_pc(test.initial_pc);
    cpu.set_sp(test.initial_s);
    cpu.set_a(test.initial_a);
    cpu.set_x(test.initial_x);
    cpu.set_y(test.initial_y);
    cpu.set_p(test.initial_p);
    
    std::cout << "INITIAL STATE:\n";
    std::cout << "  PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << "\n";
    std::cout << "  SP=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_sp() << "\n";
    std::cout << "  A=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << "\n";
    std::cout << "  X=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_x() << "\n";
    std::cout << "  Y=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_y() << "\n";
    std::cout << "  P=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << "\n";
    
    // Show stack content
    std::cout << "STACK CONTENT:\n";
    for (int sp = test.initial_s; sp <= test.initial_s + 10; sp++) {
        uint16_t addr = 0x0100 + (sp & 0xFF);
        std::cout << "  [0x" << std::hex << std::setw(4) << std::setfill('0') << addr << "]=0x"
                  << std::hex << std::setw(2) << std::setfill('0') << (int)mem.read(addr) << "\n";
    }
    
    // Execute instruction cycle by cycle following ProcessorTests format
    std::vector<std::tuple<uint16_t, uint8_t, std::string>> actual_cycles;
    
    int cycle_count = 0;
    bool instruction_done = false;
    
    while (!instruction_done && cycle_count < 20) {
        cycle_count++;
        
        uint16_t addr = cpu.get_address();
        bool rw = cpu.get_rw();
        
        std::cout << "Cycle " << std::dec << cycle_count << ": PC=0x"
                  << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc()
                  << ", SP=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_sp()
                  << ", Step=" << std::dec << cpu.get_cycle_step()
                  << ", Addr=0x" << std::hex << std::setw(4) << std::setfill('0') << addr
                  << ", R/W=" << (rw ? "READ" : "WRITE") << "\n";
        
        // Create bus state
        bus_state_t bus_state = 0;
        BUS_SET_ADDR(bus_state, addr);
        bus_state |= BUS_BIT(BUS_RDY_BIT);  // RDY = 1 (ready)
        
        // Determine read/write operation
        std::string operation = rw ? "read" : "write";
        uint8_t data;
        
        if (rw) {
            // Read operation
            data = mem.read(addr);
            BUS_SET_DATA(bus_state, data);
            std::cout << "  Reading 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)data << " from 0x" << std::setw(4) << addr << "\n";
        } else {
            // Write operation
            data = cpu.get_write_data();
            mem.write(addr, data);
            BUS_SET_DATA(bus_state, data);
            std::cout << "  Writing 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)data << " to 0x" << std::setw(4) << addr << "\n";
        }
        
        actual_cycles.push_back(std::make_tuple(addr, data, operation));
        
        // Execute cycle
        bus_state = cpu.cycle_tick(bus_state);
        
        // Check if instruction is complete
        if (cpu.get_cycle_step() == 0 && cycle_count > 1) {
            instruction_done = true;
            std::cout << "  -> RTI instruction completed\n";
        }
    }
    
    std::cout << "\nFINAL STATE:\n";
    std::cout << "  PC=0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << "\n";
    std::cout << "  SP=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_sp() << "\n";
    std::cout << "  A=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << "\n";
    std::cout << "  X=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_x() << "\n";
    std::cout << "  Y=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_y() << "\n";
    std::cout << "  P=0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << "\n";
    
    // Compare with expected final state
    bool state_match = true;
    if (cpu.get_pc() != test.final_pc) {
        std::cout << "ERROR: PC mismatch - expected 0x" << std::hex << test.final_pc << ", got 0x" << cpu.get_pc() << "\n";
        state_match = false;
    }
    if (cpu.get_sp() != test.final_s) {
        std::cout << "ERROR: SP mismatch - expected 0x" << std::hex << test.final_s << ", got 0x" << (int)cpu.get_sp() << "\n";
        state_match = false;
    }
    if (cpu.get_a() != test.final_a) {
        std::cout << "ERROR: A mismatch - expected 0x" << std::hex << test.final_a << ", got 0x" << (int)cpu.get_a() << "\n";
        state_match = false;
    }
    if (cpu.get_x() != test.final_x) {
        std::cout << "ERROR: X mismatch - expected 0x" << std::hex << test.final_x << ", got 0x" << (int)cpu.get_x() << "\n";
        state_match = false;
    }
    if (cpu.get_y() != test.final_y) {
        std::cout << "ERROR: Y mismatch - expected 0x" << std::hex << test.final_y << ", got 0x" << (int)cpu.get_y() << "\n";
        state_match = false;
    }
    if (cpu.get_p() != test.final_p) {
        std::cout << "ERROR: P mismatch - expected 0x" << std::hex << test.final_p << ", got 0x" << (int)cpu.get_p() << "\n";
        state_match = false;
    }
    
    // Compare cycle traces
    std::cout << "\nCYCLE COMPARISON:\n";
    bool cycles_match = true;
    
    size_t min_cycles = std::min(actual_cycles.size(), test.cycles.size());
    for (size_t i = 0; i < min_cycles; i++) {
        auto actual = actual_cycles[i];
        auto expected = test.cycles[i];
        
        if (std::get<0>(actual) != std::get<0>(expected) ||
            std::get<1>(actual) != std::get<1>(expected) ||
            std::get<2>(actual) != std::get<2>(expected)) {
            
            std::cout << "Cycle " << (i+1) << " MISMATCH:\n";
            std::cout << "  Expected: addr=0x" << std::hex << std::get<0>(expected) 
                      << ", data=0x" << std::get<1>(expected) << ", " << std::get<2>(expected) << "\n";
            std::cout << "  Actual:   addr=0x" << std::hex << std::get<0>(actual) 
                      << ", data=0x" << std::get<1>(actual) << ", " << std::get<2>(actual) << "\n";
            cycles_match = false;
        } else {
            std::cout << "Cycle " << (i+1) << " MATCH: addr=0x" << std::hex << std::get<0>(actual) 
                      << ", data=0x" << std::get<1>(actual) << ", " << std::get<2>(actual) << "\n";
        }
    }
    
    if (actual_cycles.size() != test.cycles.size()) {
        std::cout << "ERROR: Cycle count mismatch - expected " << test.cycles.size() 
                  << ", got " << actual_cycles.size() << "\n";
        cycles_match = false;
    }
    
    bool overall_pass = state_match && cycles_match;
    std::cout << "\nOVERALL RESULT: " << (overall_pass ? "PASS" : "FAIL") << "\n";
    
    return overall_pass;
}

int main() {
    std::cout << "=== RTI REAL PROCESSOR TESTS DEBUG ===\n";
    
    // Run a few test cases
    ProcessorTest test1 = get_test_case_1();
    bool result1 = run_processor_test(test1);
    
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Test 1 (" << test1.name << "): " << (result1 ? "PASS" : "FAIL") << "\n";
    
    return 0;
}