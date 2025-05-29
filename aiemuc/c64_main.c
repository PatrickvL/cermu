#include "c64.h"
#include <stdio.h>

// ============================================================================
// MAIN FUNCTION - Simple test harness
// ============================================================================
int main(void) {
    c64_init();
    // Note: ROM is now properly implemented, reset can be enabled when needed
    // cpu6510_reset();
    
    printf("C64 emulator initialized successfully!\n");
    return 0;
}