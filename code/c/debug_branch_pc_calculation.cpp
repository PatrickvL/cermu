#include "src/chip/cpu/fam65xx_cpp/fam65xx.hpp"
#include "src/chip/cpu/fam65xx_cpp/cpu_config.hpp"

int main() {
    printf("=== Branch PC Calculation Debug ===\n");
    
    // Create CPU instance
    fam65xx_cpp::fam65xx<config_6502> cpu;
    cpu.init_for_test();
    
    // Test case from the ProcessorTests failure:
    // PC at 0x90ca, opcode BPL (0x10), offset 0xb3 (-77)
    // Expected: 0x907f, Got: 0x90cc
    
    // Set up initial state
    cpu.set_pc(0x90ca);
    cpu.set_p(0x00); // N=0, so BPL should branch
    
    printf("Initial state:\n");
    printf("  PC: 0x%04X\n", cpu.get_pc());
    printf("  P:  0x%02X (N=%d)\n", cpu.get_p(), (cpu.get_p() & 0x80) ? 1 : 0);
    
    // Simulate memory containing BPL instruction and offset
    // Address 0x90ca: 0x10 (BPL)
    // Address 0x90cb: 0xb3 (offset = -77)
    
    bus_state_t bus_state = 0;
    
    // Cycle 0: Fetch opcode (BPL = 0x10)
    bus_state = BUS_SET_DATA(bus_state, 0x10);
    printf("\nCycle 0 (opcode fetch):\n");
    printf("  Address: 0x%04X, Data: 0x%02X\n", cpu.get_address(), BUS_GET_DATA(bus_state));
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After cycle: PC=0x%04X, step=%d\n", cpu.get_pc(), cpu.get_cycle_step());
    
    // Cycle 1: Fetch offset
    bus_state = BUS_SET_DATA(bus_state, 0xb3);
    printf("\nCycle 1 (offset fetch):\n");
    printf("  Address: 0x%04X, Data: 0x%02X (signed: %d)\n", 
           cpu.get_address(), BUS_GET_DATA(bus_state), (int8_t)BUS_GET_DATA(bus_state));
    bus_state = cpu.cycle_tick(bus_state);
    printf("  After cycle: PC=0x%04X, step=%d\n", cpu.get_pc(), cpu.get_cycle_step());
    
    // Calculate expected result manually
    // When PC is 0x90cb (after reading offset), branch calculation should be:
    // target = 0x90cb + (-77) = 0x90cb - 77 = 0x90cb - 0x4d = 0x907e
    // But ProcessorTests expects 0x907f...
    
    printf("\nManual calculation:\n");
    uint16_t pc_after_offset = 0x90cb; // PC after reading offset byte
    int8_t signed_offset = (int8_t)0xb3; // -77
    uint16_t calculated_target = pc_after_offset + signed_offset;
    printf("  PC after offset read: 0x%04X\n", pc_after_offset);
    printf("  Signed offset: %d (0x%02X)\n", signed_offset, 0xb3);
    printf("  Calculated target: 0x%04X\n", calculated_target);
    printf("  ProcessorTests expects: 0x907f\n");
    printf("  Our result: 0x%04X\n", cpu.get_pc());
    
    // Check if there's a difference in expected vs actual
    if (calculated_target != 0x907f) {
        printf("\nDISCREPANCY: Our calculation (0x%04X) != ProcessorTests expected (0x907f)\n", calculated_target);
        printf("This suggests ProcessorTests uses a different PC base for branch calculation.\n");
        
        // Try different PC bases
        uint16_t test_base1 = 0x90ca + 2; // PC + 2 (after instruction)
        uint16_t test_result1 = test_base1 + signed_offset;
        printf("  If base is PC+2 (0x%04X): target = 0x%04X\n", test_base1, test_result1);
        
        uint16_t test_base2 = 0x90ca + 1; // PC + 1 
        uint16_t test_result2 = test_base2 + signed_offset;
        printf("  If base is PC+1 (0x%04X): target = 0x%04X\n", test_base2, test_result2);
    }
    
    return 0;
}