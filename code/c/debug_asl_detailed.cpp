#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"
#include <iostream>
#include <iomanip>

using namespace fam65xx_cpp;

int main() {
    // Create CPU instance
    fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Set up test state similar to ProcessorTests
    cpu.set_pc(0xd91a);  // Start PC
    cpu.set_a(0x39);     // Initial A value
    cpu.set_p(0x2a);     // Initial P value
    
    std::cout << "=== ASL A Detailed Execution Trace ===" << std::endl;
    std::cout << "Initial state:" << std::endl;
    std::cout << "  PC: 0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "  A:  0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << std::endl;
    std::cout << "  P:  0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    std::cout << "  Cycle: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Create a simple memory that returns ASL A opcode (0x0A) at 0xd91a
    auto create_bus_state = [](uint16_t addr, uint8_t data) -> bus_state_t {
        bus_state_t bus = 0;
        BUS_SET_ADDR(bus, addr);
        BUS_SET_DATA(bus, data);
        bus |= BUS_BIT(BUS_RDY_BIT);  // RDY high
        return bus;
    };
    
    // Cycle 0: Opcode fetch
    std::cout << "=== CYCLE 0: Opcode Fetch ===" << std::endl;
    uint16_t pc_before = cpu.get_pc();
    std::cout << "PC before: 0x" << std::hex << std::setw(4) << std::setfill('0') << pc_before << std::endl;
    
    bus_state_t bus_state = create_bus_state(pc_before, 0x0A);  // ASL A opcode
    std::cout << "Bus address: 0x" << std::hex << std::setw(4) << std::setfill('0') << BUS_GET_ADDR(bus_state) << std::endl;
    std::cout << "Bus data: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)BUS_GET_DATA(bus_state) << std::endl;
    
    bus_state = cpu.cycle_tick(bus_state);
    
    uint16_t pc_after = cpu.get_pc();
    std::cout << "PC after: 0x" << std::hex << std::setw(4) << std::setfill('0') << pc_after << std::endl;
    std::cout << "PC increment: " << (pc_after - pc_before) << std::endl;
    std::cout << "Opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << cpu.get_opcode() << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Cycle 1: Execute ASL A
    std::cout << "=== CYCLE 1: Execute ASL A ===" << std::endl;
    pc_before = cpu.get_pc();
    uint8_t a_before = cpu.get_a();
    uint8_t p_before = cpu.get_p();
    
    std::cout << "PC before: 0x" << std::hex << std::setw(4) << std::setfill('0') << pc_before << std::endl;
    std::cout << "A before: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)a_before << std::endl;
    std::cout << "P before: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)p_before << std::endl;
    
    // For ASL A, no memory access should happen, so bus address doesn't matter
    bus_state = create_bus_state(0x0000, 0x00);  // Dummy data
    
    bus_state = cpu.cycle_tick(bus_state);
    
    pc_after = cpu.get_pc();
    uint8_t a_after = cpu.get_a();
    uint8_t p_after = cpu.get_p();
    
    std::cout << "PC after: 0x" << std::hex << std::setw(4) << std::setfill('0') << pc_after << std::endl;
    std::cout << "A after: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)a_after << std::endl;
    std::cout << "P after: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)p_after << std::endl;
    std::cout << "PC increment: " << (pc_after - pc_before) << std::endl;
    std::cout << "Cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    std::cout << std::endl;
    
    // Check if instruction completed
    if (cpu.get_cycle_step() == 0) {
        std::cout << "=== INSTRUCTION COMPLETED ===" << std::endl;
    } else {
        std::cout << "=== INSTRUCTION NOT YET COMPLETED ===" << std::endl;
        std::cout << "Current cycle step: " << (int)cpu.get_cycle_step() << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== FINAL RESULTS ===" << std::endl;
    std::cout << "Expected PC: 0xd91b (0xd91a + 1)" << std::endl;
    std::cout << "Actual PC:   0x" << std::hex << std::setw(4) << std::setfill('0') << cpu.get_pc() << std::endl;
    std::cout << "Expected A:  0x72 (0x39 << 1)" << std::endl;
    std::cout << "Actual A:    0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_a() << std::endl;
    std::cout << "Expected P:  0x28" << std::endl;
    std::cout << "Actual P:    0x" << std::hex << std::setw(2) << std::setfill('0') << (int)cpu.get_p() << std::endl;
    
    bool pc_correct = (cpu.get_pc() == 0xd91b);
    bool a_correct = (cpu.get_a() == 0x72);
    bool p_correct = (cpu.get_p() == 0x28);
    
    std::cout << std::endl;
    std::cout << "PC correct: " << (pc_correct ? "YES" : "NO") << std::endl;
    std::cout << "A correct:  " << (a_correct ? "YES" : "NO") << std::endl;
    std::cout << "P correct:  " << (p_correct ? "YES" : "NO") << std::endl;
    
    if (pc_correct && a_correct && p_correct) {
        std::cout << "*** TEST PASSED ***" << std::endl;
    } else {
        std::cout << "*** TEST FAILED ***" << std::endl;
    }
    
    return 0;
}