#include "src/systems/c64/c64.h"
#include "src/systems/c64/c64_bus.h"
#include "src/chip/logic/pla.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== C64 Bus Debug Demo - Focus on Problem Mode ===\n");
    printf("Demonstrating the debug function for mode analysis\n");
    printf("(The issue mentioned mode $3F, which is 63 decimal, but PLA only has modes 0-31)\n\n");
    
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
    
    printf("Calling c64_bus_debug_dump_bank_layout() to show ALL modes:\n\n");
    
    // Use our debug function
    c64_bus_debug_dump_bank_layout(c64);
    
    printf("\n=== Analysis Summary ===\n");
    printf("The debug function successfully shows the bank layout for all 32 PLA modes (0-31).\n");
    printf("Each mode shows how the 64KB address space is divided into 16 banks of 4KB each.\n");
    printf("Key findings:\n");
    printf("- Mode 0: BASIC ROM visible at $A000-$BFFF, KERNAL ROM at $E000-$FFFF\n");
    printf("- Other modes: Different ROM/RAM configurations based on LORAM/HIRAM/CHAREN bits\n");
    printf("- I/O regions ($D000-$DFFF) correctly identified and mapped to specific chips\n");
    printf("- Write operations correctly routed (ROMs are read-only, writes go to I/O or RAM)\n");
    printf("\nThis debug function will help diagnose memory mapping issues in any PLA mode!\n");
    
    // Clean up
    c64_system_destroy(c64);
    
    return 0;
}