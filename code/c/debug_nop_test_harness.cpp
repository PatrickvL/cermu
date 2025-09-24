#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include "src/core/system_lines.h"

using TestConfig = config_6502;
using TestCPU = fam65xx_cpp::fam65xx<TestConfig>;

bool test_nop_detailed(uint8_t opcode, const char* name, const std::vector<uint8_t>& instruction_bytes) {
    TestCPU cpu;
    cpu.init_for_test();
    cpu.set_pc(0x1000);
    cpu.set_p(0x24); // Clear all flags except unused
    
    std::array<uint8_t, 65536> memory = {};
    
    // Set up instruction
    for (size_t i = 0; i < instruction_bytes.size(); i++) {
        memory[0x1000 + i] = instruction_bytes[i];
    }
    
    // Store initial register state
    uint16_t initial_pc = cpu.get_pc();
    uint8_t initial_a = cpu.get_a();
    uint8_t initial_x = cpu.get_x();
    uint8_t initial_y = cpu.get_y();
    uint8_t initial_p = cpu.get_p();
    uint8_t initial_s = cpu.get_s();
    
    printf("=== Testing %s (0x%02X) ===\n", name, opcode);
    printf("Initial: A=%02X X=%02X Y=%02X P=%02X S=%02X PC=%04X\n", 
           initial_a, initial_x, initial_y, initial_p, initial_s, initial_pc);
    
    bus_state_t bus_state = 0;
    bus_state |= BUS_BIT(BUS_RDY_BIT);
    int cycles = 0;
    
    do {
        uint16_t test_addr = cpu.get_address();
        bool is_read = cpu.get_rw();
        
        printf("Cycle %d: addr=%04X, rw=%s, step=%d, PC=%04X\n", 
               cycles + 1, test_addr, is_read ? "R" : "W", cpu.get_cycle_step(), cpu.get_pc());
        
        if (is_read) {
            uint8_t data = memory[test_addr];
            bus_state = BUS_SET_DATA(bus_state, data);
            printf("  Read data: %02X\n", data);
        } else {
            uint8_t data = cpu.get_write_data();
            memory[test_addr] = data;
            bus_state = BUS_SET_DATA(bus_state, data);
            printf("  Write data: %02X\n", data);
        }
        
        bus_state = cpu.cycle_tick(bus_state);
        cycles++;
        
        printf("  After cycle: A=%02X X=%02X Y=%02X P=%02X S=%02X PC=%04X step=%d\n",
               cpu.get_a(), cpu.get_x(), cpu.get_y(), cpu.get_p(), cpu.get_s(), cpu.get_pc(), cpu.get_cycle_step());
        
    } while (cpu.get_cycle_step() != 0 && cycles < 10);
    
    // Check final state
    printf("Final: A=%02X X=%02X Y=%02X P=%02X S=%02X PC=%04X\n", 
           cpu.get_a(), cpu.get_x(), cpu.get_y(), cpu.get_p(), cpu.get_s(), cpu.get_pc());
    
    // Check that PC advanced by the correct amount
    uint16_t expected_pc = initial_pc + instruction_bytes.size();
    bool pc_correct = (cpu.get_pc() == expected_pc);
    
    // Check that registers are unchanged
    bool registers_unchanged = (
        cpu.get_a() == initial_a &&
        cpu.get_x() == initial_x &&
        cpu.get_y() == initial_y &&
        cpu.get_p() == initial_p &&
        cpu.get_s() == initial_s
    );
    
    printf("PC check: expected=%04X, got=%04X -> %s\n", expected_pc, cpu.get_pc(), pc_correct ? "PASS" : "FAIL");
    printf("Register check: %s\n", registers_unchanged ? "PASS" : "FAIL");
    if (!registers_unchanged) {
        if (cpu.get_a() != initial_a) printf("  A changed: %02X -> %02X\n", initial_a, cpu.get_a());
        if (cpu.get_x() != initial_x) printf("  X changed: %02X -> %02X\n", initial_x, cpu.get_x());
        if (cpu.get_y() != initial_y) printf("  Y changed: %02X -> %02X\n", initial_y, cpu.get_y());
        if (cpu.get_p() != initial_p) printf("  P changed: %02X -> %02X\n", initial_p, cpu.get_p());
        if (cpu.get_s() != initial_s) printf("  S changed: %02X -> %02X\n", initial_s, cpu.get_s());
    }
    
    printf("\n");
    return pc_correct && registers_unchanged;
}

int main() {
    printf("=== Debug NOP Test Harness ===\n");
    
    // Test basic NOP
    test_nop_detailed(0xEA, "NOP", {0xEA});
    
    // Test immediate mode NOP
    test_nop_detailed(0x80, "NOP #$42", {0x80, 0x42});
    
    return 0;
}