#include "mos6510_test_harness.h"
#include <stdio.h>

int main() {
    printf("Simple Klaus test debug...\n");
    
    test_harness_t* harness = test_harness_create();
    if (!harness) {
        printf("Failed to create harness\n");
        return 1;
    }
    
    // Load Klaus test
    if (!test_harness_load_binary(harness, "6502_65C02_functional_tests/bin_files/6502_functional_test.bin")) {
        printf("Failed to load test binary\n");
        test_harness_destroy(harness);
        return 1;
    }
    
    printf("Test binary loaded successfully\n");
    
    mos6510_t* cpu = harness->c64->mos6510;
    
    // Set all-RAM mode
    mos6510_write_cycle(cpu, 0x0001, 0x04);
    cpu->pc = 0x0400;  // Klaus test start
    
    printf("Starting CPU at PC=$%04X\n", cpu->pc);
    
    // Try a few single steps with debug
    for (int i = 0; i < 5; i++) {
        uint16_t old_pc = cpu->pc;
        uint8_t opcode = mos6510_read_cycle(cpu, cpu->pc);
        printf("Step %d: PC=$%04X, opcode=$%02X\n", i, old_pc, opcode);
        
        bool result = mos6510_step(cpu);
        printf("Step result: %s, new PC=$%04X\n", result ? "true" : "false", cpu->pc);
        
        if (cpu->pc == old_pc) {
            printf("CPU stuck at same PC!\n");
            break;
        }
    }
    
    test_harness_destroy(harness);
    return 0;
}
