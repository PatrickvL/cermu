#include <iostream>
#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
using namespace fam65xx_cpp;
int main() {
    fam65xx_with_cycle_count<config_6502> cpu;
    uint8_t memory[0x10000];
    std::fill(memory, memory + sizeof(memory), 0xEA);
    memory[0x0000] = 0xA9; // LDA immediate
    memory[0x0001] = 0x42; // Load 0x42
    cpu.init_for_test();
    cpu.set_a(0x00);
    cpu.set_pc(0x0000);
    bus_state_t bus_state = 0;
    // Cycle 1: Opcode fetch
    bus_state = BUS_SET_DATA(bus_state, memory[cpu.get_address()]);
    bus_state = cpu.cycle_tick(bus_state);
    // Cycle 2: Read immediate value
    bus_state = BUS_SET_DATA(bus_state, memory[cpu.get_address()]);
    bus_state = cpu.cycle_tick(bus_state);
    bool passed = (cpu.get_a() == 0x42) && ((cpu.get_p() & 0x82) == 0x00);
    std::cout << "LDA #$42 regression test: " << (passed ? "PASS" : "FAIL") << std::endl;
    return passed ? 0 : 1;
}
