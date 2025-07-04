#include "src/systems/c64/c64.h"
#include "src/systems/c64/c64_bus.h"
#include "src/chip/logic/pla.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Simple C64 Bus Debug Test...\n");
    
    // Create a minimal C64 system for testing
    system_config_t config = {0}; // Default config
    c64_t* c64 = c64_system_create(&config);
    if (!c64) {
        printf("Failed to create C64 system\n");
        return 1;
    }
    
    // Generate PLA memory maps
    if(!c64_pla_maps_generate(c64)) {
        printf("Failed to create PLA maps\n");
        return 1;
    }
    
    c64_bus_t* bus = c64->bus;
    
    printf("\nRaw encoded values for first few modes:\n");
    
    // Show raw encoded values for modes 0, 7, 15, 31
    int test_modes[] = {0, 7, 15, 31};
    for (int m = 0; m < 4; m++) {
        int mode = test_modes[m];
        printf("Mode %d: ", mode);
        for (int bank = 0; bank < 16; bank++) {
            uint8_t encoded = bus->encoded_rwid_per_bank_per_mode[mode][bank];
            printf("%02X ", encoded);
        }
        printf("\n");
    }
    
    printf("\nDemonstrating debug dump for first 3 modes only:\n");
    
    // Temporarily modify the debug function to show only first 3 modes
    printf("=== C64 Bus Bank Layout Debug Dump (Limited) ===\n");
    
    for (int mode = 0; mode < 3; mode++) {
        printf("PLA Mode %d (0x%02X):\n", mode, mode);
        printf("  Bank | Read Chip      | Write Chip     | R-ACID | W-ACID | Read/Write Offset\n");
        printf("  -----|----------------|----------------|--------|--------|------------------\n");
        
        for (int bank = 0; bank < 16; bank++) {
            uint8_t encoded = bus->encoded_rwid_per_bank_per_mode[mode][bank];
            printf("  %2d   | encoded=0x%02X   | encoded=0x%02X   |   --   |   --   | (raw)\n", 
                   bank, encoded, encoded);
        }
        printf("\n");
    }
    
    // Clean up
    c64_system_destroy(c64);
    
    printf("Test completed.\n");
    return 0;
}