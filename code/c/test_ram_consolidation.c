#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "src/chip/cpu/mos6510/mos6510.h"
#include "src/core/system.h"

// Simple RAM implementation for testing
static uint8_t test_ram[65536];

uint8_t test_ram_read(void* context, uint16_t address) {
    (void)context;
    return test_ram[address];
}

void test_ram_write(void* context, uint16_t address, uint8_t value) {
    (void)context;
    test_ram[address] = value;
}

int main() {
    printf("Testing RAM access consolidation...\n");
    
    // Create a CPU instance
    mos6510_t cpu;
    memset(&cpu, 0, sizeof(cpu));
    
    // Test that RAM access is initially NULL (after memset)
    if (cpu.ram_access.context != NULL || 
        cpu.ram_access.read_func != NULL || 
        cpu.ram_access.write_func != NULL) {
        printf("FAIL: RAM access should be NULL after memset\n");
        return 1;
    }
    printf("PASS: RAM access properly initialized to NULL\n");
    
    // Attach RAM using the consolidated interface (use test_ram as context)
    mos6510_attach_ram(&cpu, test_ram, test_ram_read, test_ram_write);
    
    // Debug: Print what was actually set
    printf("DEBUG: context=%p, read_func=%p, write_func=%p\n", 
           cpu.ram_access.context, (void*)cpu.ram_access.read_func, (void*)cpu.ram_access.write_func);
    printf("DEBUG: expected context=%p, read_func=%p, write_func=%p\n", 
           (void*)test_ram, (void*)test_ram_read, (void*)test_ram_write);
    
    // Test that RAM access is properly set
    if (cpu.ram_access.context != test_ram || 
        cpu.ram_access.read_func != test_ram_read || 
        cpu.ram_access.write_func != test_ram_write) {
        printf("FAIL: RAM access not properly attached\n");
        return 1;
    }
    printf("PASS: RAM access properly attached\n");
    
    // Test reading/writing through the consolidated interface
    test_ram[0x50] = 0xAB;
    uint8_t read_value = cpu.ram_access.read_func(cpu.ram_access.context, 0x50);
    if (read_value != 0xAB) {
        printf("FAIL: RAM read failed, expected 0xAB, got 0x%02X\n", read_value);
        return 1;
    }
    printf("PASS: RAM read works correctly\n");
    
    cpu.ram_access.write_func(cpu.ram_access.context, 0x60, 0xCD);
    if (test_ram[0x60] != 0xCD) {
        printf("FAIL: RAM write failed, expected 0xCD, got 0x%02X\n", test_ram[0x60]);
        return 1;
    }
    printf("PASS: RAM write works correctly\n");
    
    printf("All tests passed! RAM access consolidation working correctly.\n");
    return 0;
}
