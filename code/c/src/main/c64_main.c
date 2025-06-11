#include "../systems/c64/c64.h"
#include "../systems/c64/system_config.h"
#include <stdio.h>

// ============================================================================
// MAIN FUNCTION - Simple test harness
// ============================================================================
int main(void) {
    // Create system configuration (PAL by default)
    system_config_t config = {
        .vic_standard = VIC_PAL,
        .rom_config = NULL  // Use default ROM paths
    };
    
    // Initialize the C64 system and get the instance
    c64_t* c64 = c64_system_create(&config);
    if (!c64) {
        printf("Failed to initialize C64 system!\n");
        return 1;
    }
    
    // Note: ROM files are now loaded automatically from data/c64/roms/
    printf("C64 emulator initialized successfully with ROM loading!\n");
    
    // Clean up
    c64_system_destroy(c64);
    return 0;
}