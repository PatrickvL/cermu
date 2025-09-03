#include "../systems/c64/c64.h"
#include "../systems/c64/c64_config.h"
#include <stdio.h>

// ============================================================================
// MAIN FUNCTION - Simple test harness
// ============================================================================
int main(void) {
    // Create system configuration (PAL by default)
    c64_config_t config = {
        .vicii_standard = VIC_PAL,
        .rom_config = NULL  // Use default ROM paths
    };
    
    // Initialize the C64 system and get the instance
    c64_t* c64 = c64_system_create(&config);
    if (!c64) {
        printf("Failed to initialize C64 system!\n");
        return 1;
    }

    // Simple run loop on the C++ 6510 core
    printf("C64 emulator initialized. Running 5000 cycles...\n");
    const uint64_t cycles = 5000;
    for (uint64_t i = 0; i < cycles; i++) {
        c64_cpu_cycle(c64);
    }
    printf("Completed %llu cycles.\n", (unsigned long long)cycles);

    // Clean up
    c64_system_destroy(c64);
    return 0;
}