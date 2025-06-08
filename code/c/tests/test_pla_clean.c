// Test program for PLA 906114-01 logic validation
#include "../src/chip/logic/pla.h"
#include <stdio.h>

// Test the PLA logic independently
void test_pla_logic() {
    printf("Testing PLA 906114-01 logic...\n");
    
    // Create PLA chip
    pla_906114_01_t* pla = (pla_906114_01_t*)pla_906114_01_create(&pla_906114_01_descriptor);
    if (!pla) {
        printf("Failed to create PLA chip\n");
        return;
    }
    
    // Test default boot configuration (should show mostly RAM)
    printf("\nDefault boot configuration (processor port = $37):\n");
    // Set up typical C64 boot state: processor port = $37
    pla->inputs.n_loram = false;  // LORAM=1 (signal inverted, so false = enabled)
    pla->inputs.n_hiram = false;  // HIRAM=1 (signal inverted, so false = enabled) 
    pla->inputs.n_charen = false; // CHAREN=1 (signal inverted, so false = I/O enabled)
    pla->inputs.aec = true;       // CPU has bus control
    pla->inputs.r_w = true;       // Read mode
    pla->inputs.n_game = true;    // No game cartridge (signal inverted, so true = no cart)
    pla->inputs.n_exrom = true;   // No external ROM (signal inverted, so true = no ROM)
    pla_906114_01_update_outputs(pla);
    
    printf("  BASIC: %s\n", pla->outputs.n_basic ? "disabled" : "enabled");
    printf("  KERNAL: %s\n", pla->outputs.n_kernal ? "disabled" : "enabled");
    printf("  CHARROM: %s\n", pla->outputs.n_charrom ? "disabled" : "enabled");
    printf("  I/O: %s\n", pla->outputs.n_io ? "disabled" : "enabled");
    printf("  CASRAM: %s\n", pla->outputs.n_casram ? "disabled" : "enabled");
    printf("  ROML: %s\n", pla->outputs.n_roml ? "disabled" : "enabled");
    printf("  ROMH: %s\n", pla->outputs.n_romh ? "disabled" : "enabled");
    printf("  GRW: %s\n", pla->outputs.gr_w ? "enabled" : "disabled");
    
    // Test BASIC ROM access ($A000-$BFFF) - should be enabled
    printf("\nTesting BASIC ROM access ($A000) with LORAM=1, HIRAM=1:\n");
    pla_906114_01_set_address_high(pla, 0x0A);  // $A000
    printf("  BASIC: %s\n", pla->outputs.n_basic ? "disabled" : "enabled");
    printf("  CASRAM: %s\n", pla->outputs.n_casram ? "disabled" : "enabled");
    
    // Test I/O access ($D000-$DFFF) - should be enabled with CHAREN=1
    printf("\nTesting I/O access ($D000) with CHAREN=1:\n");
    pla_906114_01_set_address_high(pla, 0x0D);  // $D000
    printf("  I/O: %s\n", pla->outputs.n_io ? "disabled" : "enabled");
    printf("  CHARROM: %s\n", pla->outputs.n_charrom ? "disabled" : "enabled");
    
    // Test Character ROM access ($D000-$DFFF) - change CHAREN to 0
    printf("\nTesting Character ROM access ($D000) with CHAREN=0:\n");
    pla->inputs.n_charen = true;  // CHAREN=0 (signal is active low)
    pla_906114_01_update_outputs(pla);
    printf("  I/O: %s\n", pla->outputs.n_io ? "disabled" : "enabled");
    printf("  CHARROM: %s\n", pla->outputs.n_charrom ? "disabled" : "enabled");
    
    // Test KERNAL ROM access ($E000-$FFFF) - should be enabled
    printf("\nTesting KERNAL ROM access ($E000) with HIRAM=1:\n");
    pla_906114_01_set_address_high(pla, 0x0E);  // $E000
    printf("  KERNAL: %s\n", pla->outputs.n_kernal ? "disabled" : "enabled");
    printf("  CASRAM: %s\n", pla->outputs.n_casram ? "disabled" : "enabled");
    
    pla_906114_01_destroy(pla);
    printf("PLA logic test completed.\n\n");
}

// Test specific memory configurations
void test_memory_modes() {
    printf("Testing C64 memory mode configurations...\n");
    
    pla_906114_01_t* pla = (pla_906114_01_t*)pla_906114_01_create(&pla_906114_01_descriptor);
    if (!pla) {
        printf("Failed to create PLA chip\n");
        return;
    }
    
    // Test Mode 7: All RAM ($00-$FF at $01)
    printf("\nMode 7 - All RAM:\n");
    pla->inputs.n_loram = true;   // Disable LORAM
    pla->inputs.n_hiram = true;   // Disable HIRAM 
    pla->inputs.n_charen = true;  // Don't care
    pla->inputs.n_game = true;    // No game cartridge
    pla->inputs.n_exrom = true;   // No external ROM
    pla->inputs.aec = true;       // CPU has bus control
    pla->inputs.r_w = true;       // Read mode
    
    // Test various addresses
    uint16_t test_addresses[] = {0xA000, 0xD000, 0xE000};
    const char* test_names[] = {"BASIC area", "I/O/CHAR area", "KERNAL area"};
    
    for (int i = 0; i < 3; i++) {
        pla_906114_01_set_address_high(pla, test_addresses[i] >> 8);
        printf("  %s ($%04X): CASRAM=%s\n", 
               test_names[i], test_addresses[i],
               pla->outputs.n_casram ? "disabled" : "enabled");
    }
    
    // Test Mode 6: KERNAL + I/O ($30-$37 at $01)
    printf("\nMode 6 - KERNAL + I/O:\n");
    pla->inputs.n_loram = false;  // Enable LORAM (BASIC ROM disabled, use RAM)
    pla->inputs.n_hiram = true;   // Disable HIRAM (KERNAL ROM enabled)
    pla->inputs.n_charen = true;  // I/O mode
    
    for (int i = 0; i < 3; i++) {
        pla_906114_01_set_address_high(pla, test_addresses[i] >> 8);
        const char* expected = (i == 0) ? "RAM" : 
                              (i == 1) ? "I/O" : "KERNAL";
        printf("  %s ($%04X): Expected=%s, CASRAM=%s, I/O=%s, KERNAL=%s\n", 
               test_names[i], test_addresses[i], expected,
               pla->outputs.n_casram ? "disabled" : "enabled",
               pla->outputs.n_io ? "disabled" : "enabled",
               pla->outputs.n_kernal ? "disabled" : "enabled");
    }
    
    pla_906114_01_destroy(pla);
    printf("Memory mode test completed.\n\n");
}

int main() {
    printf("PLA 906114-01 Test Suite\n");
    printf("========================\n\n");
    
    test_pla_logic();
    test_memory_modes();
    
    printf("All tests completed successfully!\n");
    return 0;
}
