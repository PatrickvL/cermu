#include "src/systems/c64/c64.h"
#include "src/systems/c64/c64_bus.h"
#include "src/chip/logic/pla.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("=== C64 Bus Bank Layout Debug Demonstration ===\n");
    printf("This demonstrates the PLA memory mapping for different modes.\n\n");
    
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
    
    printf("Raw encoded values for selected modes:\n");
    int test_modes[] = {0, 7, 15, 31};
    for (int m = 0; m < 4; m++) {
        int mode = test_modes[m];
        printf("Mode %2d (0x%02X): ", mode, mode);
        for (int bank = 0; bank < 16; bank++) {
            uint8_t encoded = bus->encoded_rwid_per_bank_per_mode[mode][bank];
            printf("%02X ", encoded);
        }
        printf("\n");
    }
    printf("\n");
    
    // Show specific modes in detail
    printf("=== Detailed Bank Layout for Key Modes ===\n\n");
    
    int detail_modes[] = {0, 7, 31};  // Show just a few key modes
    for (int m = 0; m < 3; m++) {
        int mode = detail_modes[m];
        
        printf("PLA Mode %d (0x%02X):\n", mode, mode);
        printf("  Bank | Address Range | Read Chip      | Write Chip     | R-ACID | W-ACID | Read/Write Offset\n");
        printf("  -----|---------------|----------------|----------------|--------|--------|------------------\n");
        
        for (int bank = 0; bank < 16; bank++) {
            uint8_t encoded = bus->encoded_rwid_per_bank_per_mode[mode][bank];
            uint8_t read_acid, write_acid;
            uint16_t bank_address = bank * 0x1000;
            
            // Decode the ACIDs with enhanced I/O handling
            if (encoded == 0) {
                // I/O region
                uint8_t io_page = (bank_address >> 8) & 0x0F;
                read_acid = io_page;
                write_acid = io_page;
            } else {
                // Non-I/O region
                uint8_t read_code = encoded & 0x0F;
                uint8_t write_code = (encoded >> 5) & 0x07;
                read_acid = (read_code == 0) ? 0 : read_code + 15;
                write_acid = (write_code == 0) ? 0 : write_code + 15;
            }
            
            // Get chip names
            const char* read_chip;
            const char* write_chip;
            
            if (read_acid <= 15) {
                // I/O chips
                switch (read_acid) {
                    case 0: case 1: case 2: case 3: read_chip = "VIC-II"; break;
                    case 4: case 5: case 6: case 7: read_chip = "SID"; break;
                    case 8: case 9: case 10: case 11: read_chip = "Color-RAM"; break;
                    case 12: read_chip = "CIA1"; break;
                    case 13: read_chip = "CIA2"; break;
                    case 14: read_chip = "IO1"; break;
                    case 15: read_chip = "IO2"; break;
                    default: read_chip = "I/O"; break;
                }
            } else {
                switch (read_acid) {
                    case 16: read_chip = "CPU-ZeroBank"; break;
                    case 17: read_chip = "RAM"; break;
                    case 18: read_chip = "Cart-ROML"; break;
                    case 19: read_chip = "Cart-ROMH"; break;
                    case 20: read_chip = "UNMAPPED"; break;
                    case 21: read_chip = "BASIC-ROM"; break;
                    case 22: read_chip = "CHAR-ROM"; break;
                    case 23: read_chip = "KERNAL-ROM"; break;
                    default: read_chip = "UNKNOWN"; break;
                }
            }
            
            if (write_acid <= 15) {
                switch (write_acid) {
                    case 0: case 1: case 2: case 3: write_chip = "VIC-II"; break;
                    case 4: case 5: case 6: case 7: write_chip = "SID"; break;
                    case 8: case 9: case 10: case 11: write_chip = "Color-RAM"; break;
                    case 12: write_chip = "CIA1"; break;
                    case 13: write_chip = "CIA2"; break;
                    case 14: write_chip = "IO1"; break;
                    case 15: write_chip = "IO2"; break;
                    default: write_chip = "I/O"; break;
                }
            } else {
                switch (write_acid) {
                    case 16: write_chip = "CPU-ZeroBank"; break;
                    case 17: write_chip = "RAM"; break;
                    case 18: write_chip = "Cart-ROML"; break;
                    case 19: write_chip = "Cart-ROMH"; break;
                    case 20: write_chip = "UNMAPPED"; break;
                    case 21: write_chip = "BASIC-ROM"; break;
                    case 22: write_chip = "CHAR-ROM"; break;
                    case 23: write_chip = "KERNAL-ROM"; break;
                    default: write_chip = "UNKNOWN"; break;
                }
            }
            
            char offset_str[32];
            if (read_acid <= 15) {
                snprintf(offset_str, sizeof(offset_str), "I/O-page/I/O-page");
            } else {
                snprintf(offset_str, sizeof(offset_str), "$%04X/$%04X", 
                         (unsigned int)(bank_address & 0x1FFF), 
                         (unsigned int)(bank_address & 0x1FFF));
            }
            
            printf("  %2d   | $%04X-$%04X | %-14s | %-14s | %6d | %6d | %s\n", 
                   bank, 
                   (unsigned int)bank_address, 
                   (unsigned int)(bank_address + 0x0FFF),
                   read_chip, 
                   write_chip, 
                   read_acid, 
                   write_acid, 
                   offset_str);
        }
        printf("\n");
    }
    
    printf("=== Mode Summary ===\n");
    printf("Mode 0: Typical configuration with BASIC and KERNAL ROMs visible\n");
    printf("Mode 7: Different memory configuration\n");
    printf("Mode 31: All-RAM mode or different cartridge configuration\n");
    printf("\nNote: ROM loading failed, so some mappings may default to I/O regions\n");
    
    // Clean up
    c64_system_destroy(c64);
    
    printf("\nDebug dump demonstration completed successfully!\n");
    return 0;
}