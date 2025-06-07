#include "../systems/c64/c64.h"
#include <stdio.h>

// ============================================================================
// MAIN FUNCTION - Simple test harness
// ============================================================================
int main(void) {
    // Initialize the C64 system and get the instance
    c64_t* c64 = c64_system_create();
    if (!c64) {
        printf("Failed to initialize C64 system!\n");
        return 1;
    }
    
    // Note: ROM is now properly implemented, reset can be enabled when needed
    // mos6510_reset();
    
    printf("C64 emulator initialized successfully!\n");
    
    // Clean up
    c64_system_destroy(c64);
    return 0;
}