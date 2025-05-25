#include "cpu6510_core.h"
#include <stdio.h>

// ============================================================================
// SIMPLE CPU TEST PROGRAM - Similar to C# aiemu
// ============================================================================

// Test ROM - Simple program to test CPU functionality
static uint8_t test_rom[] = {
    // Reset vector at $FFFC/$FFFD points to $0200
    [0x3FFC] = 0x00, [0x3FFD] = 0x02,  // Reset vector -> $0200
    
    // Test program at $0200
    [0x0200] = 0xA9, 0x42,  // LDA #$42
    [0x0202] = 0x8D, 0x00, 0x04,  // STA $0400
    [0x0205] = 0xA2, 0x05,  // LDX #$05
    [0x0207] = 0xCA,        // DEX
    [0x0208] = 0xD0, 0xFD,  // BNE $0207
    [0x020A] = 0x00,        // BRK
};

void load_test_rom(void) {
    // Copy test ROM to appropriate memory locations
    for (int i = 0; i < sizeof(test_rom); i++) {
        if (test_rom[i] != 0) {
            if (i >= 0x3FFC) {
                // Reset vector in Kernal ROM area
                kernal_rom[i - 0x2000] = test_rom[i];
            } else {
                // Program in RAM
                ram[i] = test_rom[i];
            }
        }
    }
}

void print_cpu_state(void) {
    printf("CPU State:\n");
    printf("  PC: $%04X  A: $%02X  X: $%02X  Y: $%02X  SP: $%02X\n",
           cpu.pc, cpu.a, cpu.x, cpu.y, cpu.sp);
    printf("  Flags: %c%c%c%c%c%c%c%c\n",
           (cpu.p & FLAG_N) ? 'N' : 'n',
           (cpu.p & FLAG_V) ? 'V' : 'v',
           (cpu.p & FLAG_U) ? 'U' : 'u',
           (cpu.p & FLAG_B) ? 'B' : 'b',
           (cpu.p & FLAG_D) ? 'D' : 'd',
           (cpu.p & FLAG_I) ? 'I' : 'i',
           (cpu.p & FLAG_Z) ? 'Z' : 'z',
           (cpu.p & FLAG_C) ? 'C' : 'c');
    printf("  Cycles: %llu\n", cpu.total_cycles);
    printf("  Memory $0400: $%02X\n", ram[0x0400]);
}

int main(void) {
    printf("=== MOS 6510 CPU Test Program ===\n\n");
    
    // Initialize emulator
    printf("Initializing C64 system...\n");
    
    // Initialize chip states
    memset(&vic, 0, sizeof(vic));
    memset(&cia1, 0, sizeof(cia1));
    memset(&cia2, 0, sizeof(cia2));
    memset(&sid, 0, sizeof(sid));
    memset(ram, 0, sizeof(ram));
    
    // Generate PLA maps
    generate_pla_maps();
    switch_cpu_mode(0x07); // All RAM/ROM enabled
    
    // Initialize bus
    bus_state.raw = 0;
    bus_state.bus_control = BA_LINE | AEC_LINE | RDY_LINE;
    
    // Load test program
    load_test_rom();
    
    // Initialize and reset CPU
    cpu6510_init();
    cpu6510_reset();
    
    printf("Initial state:\n");
    print_cpu_state();
    printf("\nExecuting test program...\n");
    
    // Execute for a limited number of cycles
    for (int i = 0; i < 100 && cpu.pc != 0x020B; i++) {
        cpu6510_step();
        
        // Print state every 10 cycles
        if (i % 10 == 9) {
            printf("\nAfter %d cycles:\n", i + 1);
            print_cpu_state();
        }
    }
    
    printf("\nFinal state:\n");
    print_cpu_state();
    
    // Verify test results
    if (ram[0x0400] == 0x42) {
        printf("\n✅ TEST PASSED: Value $42 stored at $0400\n");
    } else {
        printf("\n❌ TEST FAILED: Expected $42 at $0400, got $%02X\n", ram[0x0400]);
    }
    
    if (cpu.x == 0x00) {
        printf("✅ TEST PASSED: X register decremented to 0\n");
    } else {
        printf("❌ TEST FAILED: Expected X=0, got X=$%02X\n", cpu.x);
    }
    
    return 0;
}