#include "c64.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// MAIN FUNCTION - Simple test harness
// ============================================================================
int main(void) {
    c64_init();
    // Skip reset for now to avoid reading from ROM
    // cpu6510_reset();
    
    printf("C64 emulator initialized successfully!\n");
    return 0;
}