#include "c64.h"
#include "c64_bus.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// SIMPLE CPU TEST PROGRAM - Similar to C# aiemu
// ============================================================================

// Global C64 system state
static c64_t c64;

// Test ROM - Simple program to test CPU functionality
static uint8_t test_code[] = {
    // Test program at $0200
    [0x0200] = 0xA9, 0x42,  // LDA #$42
    [0x0202] = 0x8D, 0x00, 0x04,  // STA $0400
    [0x0205] = 0xA2, 0x05,  // LDX #$05
    [0x0207] = 0xCA,        // DEX
    [0x0208] = 0xD0, 0xFD,  // BNE $0207
    [0x020A] = 0x00,        // BRK
};

void load_test_rom(void) {
    // Copy test code to appropriate memory locations
    for (int i = 0; i < sizeof(test_code); i++) {
        // Program in RAM
        c64.ram.data[i] = test_code[i];
    }
    // Write reset vector at $FFFC/$FFFD points to $0200
    c64.ram.data[0xFFFC] = 0x00, c64.ram.data[0xFFFD] = 0x02;
}

void print_cpu_state(void) {
    printf("PC:$%04X A:$%02X X:$%02X Y:$%02X SP:$%02X P:$%02X $0400:$%02X Flags:%c%c%c%c%c%c%c%c Cycles:%llu\n",
        c64.cpu.pc, c64.cpu.a, c64.cpu.x, c64.cpu.y, c64.cpu.sp, c64.cpu.p, c64.ram.data[0x0400],
        (c64.cpu.p & FLAG_N) ? 'N' : 'n',
        (c64.cpu.p & FLAG_V) ? 'V' : 'v',
        (c64.cpu.p & FLAG_U) ? 'U' : 'u',
        (c64.cpu.p & FLAG_B) ? 'B' : 'b',
        (c64.cpu.p & FLAG_D) ? 'D' : 'd',
        (c64.cpu.p & FLAG_I) ? 'I' : 'i',
        (c64.cpu.p & FLAG_Z) ? 'Z' : 'z',
        (c64.cpu.p & FLAG_C) ? 'C' : 'c', c64.total_cycles);
}

// Print CPU state on every bus cycle (for debugging)
static int cycle_print_count = 0;
void print_cpu_state_on_cycle(void) {
    // Print every cycle, or limit if desired
    // if (++cycle_print_count <= 1000)
    print_cpu_state();
}

// Extern declaration for bus_cycle_callback
extern void (*bus_cycle_callback)(void);

int main(void) {
    printf("=== MOS 6510 CPU Test Program ===\n\n");
    // Initialize emulator (clear RAM, bus)
    printf("Initializing C64 system...\n");
    c64_init(&c64);
    // Load test program (RAM and kernal_rom)
    load_test_rom();
    // Force PLA mode to all RAM (no ROMs) so $FFFC/$FFFD are read from RAM
    switch_cpu_mode(0x00); // 0x00 = all RAM, disables all ROMs
    // Set bus cycle callback for per-cycle state printing
    bus_cycle_callback = print_cpu_state_on_cycle;
    // Reset CPU (sets up mode and reads reset vector)
    mos6510_reset(&c64.cpu);
    printf("Debug: PC after reset: $%04X\n", c64.cpu.pc);
    printf("Initial state:\n");
    print_cpu_state();
    printf("\nExecuting test program...\n");
    // Execute for a limited number of instructions (not just cycles)
    for (int i = 0; i < 100 && c64.cpu.pc != 0x020B; i++) {
        // Execute one full instruction (which will call bus_cycle many times)
        mos6510_execute(&c64.cpu);
        // Print state every 10 instructions (already printed per cycle if callback is set)
        if (i % 10 == 9) {
            printf("\nAfter %d instructions:\n", i + 1);
            print_cpu_state();
        }
    }
    // Optionally disable callback after test
    bus_cycle_callback = NULL;
    printf("\nFinal state:\n");
    print_cpu_state();
    // Verify test results
    if (c64.ram.data[0x0400] == 0x42) {
        printf("\n✅ TEST PASSED: Value $42 stored at $0400\n");
    } else {
        printf("\n❌ TEST FAILED: Expected $42 at $0400, got $%02X\n", c64.ram.data[0x0400]);
    }
    if (c64.cpu.x == 0x00) {
        printf("✅ TEST PASSED: X register decremented to 0\n");
    } else {
        printf("❌ TEST FAILED: Expected X=0, got X=$%02X\n", c64.cpu.x);
    }
    return 0;
}