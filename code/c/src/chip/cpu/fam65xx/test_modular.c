/*
 * test_modular.c - Simple test to verify modular CPU emulator compilation
 * 
 * This test verifies that the split modular files can be compiled together
 * and that basic CPU functionality works as expected.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Simple memory implementation for testing */
static uint8_t test_memory[65536];

uint8_t memory_read(uint16_t addr) {
    return test_memory[addr];
}

void memory_write(uint16_t addr, uint8_t data) {
    test_memory[addr] = data;
}

/* Include the modular CPU implementation */
#define CHIPS_IMPL
#include "fam65xx.h"

int main(void) {
    fam65xx_t cpu;
    bus_state_t pins = 0;
    
    printf("Testing modular FAM65XX CPU emulator...\n");
    
    /* Initialize CPU */
    printf("1. Initializing CPU... ");
    fam65xx_init(&cpu);
    printf("OK\n");
    
    /* Set up a simple test program: LDA #$42, NOP */
    printf("2. Setting up test program... ");
    memory_write(0xFFFC, 0x00);  /* Reset vector low */
    memory_write(0xFFFD, 0x80);  /* Reset vector high - start at $8000 */
    memory_write(0x8000, 0xA9);  /* LDA #$42 */
    memory_write(0x8001, 0x42);
    memory_write(0x8002, 0xEA);  /* NOP */
    printf("OK\n");
    
    /* Execute reset sequence */
    printf("3. Executing reset sequence... ");
    for (int i = 0; i < 10; i++) {
        pins = fam65xx_tick(&cpu, pins);
    }
    printf("OK (PC = $%04X)\n", CPU_PC(&cpu));
    
    /* Execute LDA #$42 */
    printf("4. Executing LDA #$42... ");
    for (int i = 0; i < 5; i++) {
        pins = fam65xx_tick(&cpu, pins);
    }
    printf("OK (A = $%02X)\n", CPU_A(&cpu));
    
    /* Verify results */
    printf("5. Verifying results... ");
    if (CPU_A(&cpu) == 0x42 && CPU_PC(&cpu) == 0x8002) {
        printf("OK\n");
        printf("\nTest PASSED! Modular CPU emulator is working correctly.\n");
        
        /* Print CPU state */
        printf("\nFinal CPU State:\n");
        printf("  A = $%02X  X = $%02X  Y = $%02X  P = $%02X  S = $%02X\n",
               CPU_A(&cpu), CPU_X(&cpu), CPU_Y(&cpu), CPU_P(&cpu), CPU_S(&cpu));
        printf("  PC = $%04X\n", CPU_PC(&cpu));
        
        return 0;
    } else {
        printf("FAILED\n");
        printf("  Expected: A=$42, PC=$8002\n");
        printf("  Got:      A=$%02X, PC=$%04X\n", CPU_A(&cpu), CPU_PC(&cpu));
        return 1;
    }
}