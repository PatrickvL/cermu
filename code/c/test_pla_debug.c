#include "src/systems/c64/c64.h"
#include "src/systems/c64/c64_bus.h"
#include "src/chip/logic/pla.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Testing PLA-based memory mapping generation...\n");
    
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
    
    
    // Test a few memory modes to see if they have different mappings
    c64_bus_t* bus = c64->bus;
    
    printf("Testing different memory modes:\n");
    
    // Mode 0: All signals high (should be mostly RAM)
    printf("Mode 0 (all signals high):\n");
    for (int i = 0; i < 8; i++) {
        uint8_t acid = bus->encoded_rwid_per_bank_per_mode[0][i];
        printf("  Bank %d: acid = 0x%02X\n", i, acid);
    }
    
    // Mode 31: All signals low (different configuration)
    printf("Mode 31 (all signals low):\n");
    for (int i = 0; i < 8; i++) {
        uint8_t acid = bus->encoded_rwid_per_bank_per_mode[31][i];
        printf("  Bank %d: acid = 0x%02X\n", i, acid);
    }
    
    // Check if modes are different (indicating PLA is actually working)
    bool modes_differ = false;
    for (int i = 0; i < 32; i++) {
        if (bus->encoded_rwid_per_bank_per_mode[0][i] != bus->encoded_rwid_per_bank_per_mode[31][i]) {
            modes_differ = true;
            break;
        }
    }
    
    if (modes_differ) {
        printf("SUCCESS: Different memory modes have different mappings!\n");
    } else {
        printf("WARNING: All memory modes have identical mappings\n");
    }
    
    printf("\n=== Demonstrating Bank Layout Debug Dump ===\n");
    
    // Show debug dump for first few modes as demonstration
    printf("Calling c64_bus_debug_dump_bank_layout()...\n\n");
    c64_bus_debug_dump_bank_layout(c64);
    
    // Clean up
    c64_system_destroy(c64);
    
    printf("PLA mapping test completed.\n");
    return 0;
}
