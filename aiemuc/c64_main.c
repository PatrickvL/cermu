#include "c64.h"
#include <stdio.h>

// ============================================================================
// MAIN FUNCTION - Simple test harness
// ============================================================================
int main(void) {
    // Create C64 state instance
    c64_state_t c64;
    
    // Initialize the C64
    c64_init(&c64);
    // Note: ROM is now properly implemented, reset can be enabled when needed
    // cpu6510_reset();
    
    printf("C64 emulator initialized successfully!\n");
    return 0;
}