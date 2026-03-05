/**
 * C64 Banking Mode Verification Utility
 *
 * Generates all 32 CPU banking modes for comparison against the
 * PLA dissection document (c64_pla_dissected_a4ss.pdf, Appendix A)
 */

#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdio>
#include "c64_bus.h"
#include "c64_chips.h"
#include "../../chip/logic/pla.h"

// Chip type names for readability
static const char* chip_name(uint8_t chip) {
    switch (chip) {
        case CHIP_RAM: return "ram";
        case CHIP_BASIC: return "BASIC";
        case CHIP_KERNAL: return "KERNAL";
        case CHIP_CHARROM: return "CHARROM";
        case CHIP_IO: return "I/O";
        case CHIP_ROML: return "ROML";
        case CHIP_ROMH: return "ROMH";
        case CHIP_UNMAPPED: return "---";
        default: return "???";
    }
}

// Convert mode to control line states
static void decode_mode(uint8_t mode, bool& loram, bool& hiram, bool& charen, 
                       bool& game, bool& exrom) {
    loram = (mode & 0x01) != 0;
    hiram = (mode & 0x02) != 0;
    charen = (mode & 0x04) != 0;
    exrom = (mode & 0x08) != 0;
    game = (mode & 0x10) != 0;
}

// Generate mode description matching PLA document format
static std::string mode_description(uint8_t mode) {
    bool loram, hiram, charen, game, exrom;
    decode_mode(mode, loram, hiram, charen, game, exrom);
    
    // Format: LHGX where L=LORAM, H=HIRAM, G=GAME, X=EXROM
    char buf[32];
    snprintf(buf, sizeof(buf), "LHGX=%d%d%d%d", loram?1:0, hiram?1:0, game?1:0, exrom?1:0);
    return buf;
}

// Get cartridge type description
static std::string cart_description(bool game, bool exrom) {
    if (game && exrom) return "none";
    if (game && !exrom) return "8k";
    if (!game && !exrom) return "16k";
    if (!game && exrom) return "Ultimax";
    return "???";
}

// Print mode header
static void print_mode_header(uint8_t mode) {
    bool loram, hiram, charen, game, exrom;
    decode_mode(mode, loram, hiram, charen, game, exrom);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  MODE $%02X: %s  (CHAREN=%d)\n", mode, mode_description(mode).c_str(), charen?1:0);
    printf("  Cartridge: %s\n", cart_description(game, exrom).c_str());
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  Address   CPU Read  CPU Write  Notes\n");
    printf("  ───────   ────────  ─────────  ─────\n");
}

// Verify all 32 banking modes
void verify_c64_banking_modes() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║         C64 CPU Banking Mode Verification                         ║\n");
    printf("║  Compare against: c64_pla_dissected_a4ss.pdf, Appendix A         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    
    // Create bus and PLA for testing
    c64_bus_t* bus = new c64_bus_t();
    
    pla_906114_01_t* pla = pla_906114_01_create();
    if (!pla) {
        printf("ERROR: Failed to create PLA\n");
        delete bus;
        return;
    }
    
    // Generate all PLA modes
    bus->generate_all_pla_modes(pla);
    
    // Test all 32 modes
    for (uint8_t mode = 0; mode < 32; mode++) {
        bool loram, hiram, charen, game, exrom;
        decode_mode(mode, loram, hiram, charen, game, exrom);
        
        print_mode_header(mode);
        
        // Test all 16 banks ($0000-$FFFF in $1000 increments)
        for (uint16_t bank = 0; bank < 16; bank++) {
            uint16_t addr = bank << 12;
            
            // Get read and write chips from pre-generated mode table
            uint8_t encoded = bus->cpu_encoded_chip_per_bank_per_mode[mode][bank];
            uint8_t read_chip = decode_read_chip(encoded);
            uint8_t write_chip = decode_write_chip(encoded);
            
            // Check if CHAREN affects this bank
            bool charen_affects = false;
            if (bank == 13) { // $D000-$DFFF
                // Test with CHAREN toggled
                uint8_t alt_mode = mode ^ 0x04; // Toggle CHAREN bit
                uint8_t alt_encoded = bus->cpu_encoded_chip_per_bank_per_mode[alt_mode][bank];
                uint8_t alt_read = decode_read_chip(alt_encoded);
                charen_affects = (read_chip != alt_read);
            }
            
            printf("  $%04X    %-8s  %-8s", addr, chip_name(read_chip), chip_name(write_chip));
            if (charen_affects) {
                printf("  *");
            }
            printf("\n");
        }
        
        // Add summary for special configurations
        bool is_ultimax = (!game && exrom);
        bool has_16k_cart = (!game && !exrom);
        bool has_8k_cart = (game && !exrom);
        
        if (is_ultimax) {
            printf("\n  NOTE: Ultimax mode - only 4K RAM + ROML + I/O + ROMH visible\n");
            printf("        Unmapped areas return floating bus values\n");
        } else if (has_16k_cart) {
            printf("\n  NOTE: 16K cartridge mode\n");
        } else if (has_8k_cart) {
            printf("\n  NOTE: 8K cartridge mode\n");
        }
    }
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  Legend:\n");
    printf("    *  = CHAREN bit affects this bank\n");
    printf("    RAM     = Internal DRAM\n");
    printf("    BASIC   = BASIC ROM ($A000-$BFFF)\n");
    printf("    KERNAL  = KERNAL ROM ($E000-$FFFF)\n");
    printf("    CHARROM = Character ROM ($D000-$DFFF)\n");
    printf("    I/O     = I/O chips ($D000-$DFFF)\n");
    printf("    ROML    = Cartridge ROM Low ($8000-$9FFF)\n");
    printf("    ROMH    = Cartridge ROM High ($A000-$BFFF or $E000-$FFFF)\n");
    printf("    ---     = Unmapped (floating bus, Ultimax only)\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("\n");
    
    // Cleanup
    pla_906114_01_destroy(pla);
    delete bus;
}

// Parse LHGX pattern string and generate matching mode numbers
// Pattern format: "1111", "011x", "xx01", etc.
// L=LORAM(bit0), H=HIRAM(bit1), G=GAME(bit4), X=EXROM(bit3)
// 'x' means don't care, '0'/'1' means must match
static void parse_lhgx_pattern(const char* pattern, uint8_t modes_out[], int& count_out) {
    count_out = 0;
    
    // Parse the 4-character LHGX pattern
    if (strlen(pattern) != 4) return;
    
    char l = pattern[0];  // LORAM bit
    char h = pattern[1];  // HIRAM bit
    char g = pattern[2];  // GAME bit
    char x = pattern[3];  // EXROM bit
    
    // Enumerate all 32 possible modes (5 bits: LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (uint8_t mode = 0; mode < 32; mode++) {
        bool loram = (mode & 0x01) != 0;
        bool hiram = (mode & 0x02) != 0;
        bool charen = (mode & 0x04) != 0;  // CHAREN is always 'x' in LHGX patterns
        bool exrom = (mode & 0x08) != 0;
        bool game = (mode & 0x10) != 0;
        
        // Check if this mode matches the pattern
        bool matches = true;
        if (l != 'x' && ((l == '1') != loram)) matches = false;
        if (h != 'x' && ((h == '1') != hiram)) matches = false;
        if (g != 'x' && ((g == '1') != game)) matches = false;
        if (x != 'x' && ((x == '1') != exrom)) matches = false;
        
        if (matches) {
            modes_out[count_out++] = mode;
        }
    }
}

// Group modes by table for easier comparison with PLA document
void verify_c64_banking_by_table() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════╗\n");
    printf("║         C64 Banking Verification - Grouped by PLA Tables         ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════╝\n");
    
    // Create bus and PLA for testing
    c64_bus_t* bus = new c64_bus_t();
    
    pla_906114_01_t* pla = pla_906114_01_create();
    if (!pla) {
        printf("ERROR: Failed to create PLA\n");
        delete bus;
        return;
    }
    
    // Generate all PLA modes
    bus->generate_all_pla_modes(pla);
    
    // Define table groups matching PLA document Appendix A
    // Using actual LHGX strings from the document
    struct TableGroup {
        const char* name;
        const char* doc_ref;
        const char* lhgx_pattern;  // Pattern like "1111", "011x", "xx01"
        uint8_t modes[32];  // Generated from pattern
        int mode_count;
    };
    
    // Tables from c64_pla_dissected_a4ss.pdf, Appendix A
    // Pattern format: LHGX where L=LORAM, H=HIRAM, G=GAME, X=EXROM
    // 'x' means don't care (both 0 and 1 match)
    TableGroup tables[] = {
        {"Standard (all ROMs)", "Table A.1", "1111", {}, 0},
        {"BASIC out", "Table A.2", "011x", {}, 0},
        {"16K cart out", "Table A.3", "1000", {}, 0},
        {"KERNAL out", "Table A.4", "101x", {}, 0},
        {"All out", "Table A.5 (001x or 00x0)", "00xx", {}, 0},  // Special: excludes Ultimax
        {"16K cartridge", "Table A.6", "1100", {}, 0},
        {"16K ROML out", "Table A.7", "0100", {}, 0},
        {"8K cartridge", "Table A.8", "1110", {}, 0},
        {"Ultimax", "Table A.9", "xx01", {}, 0}
    };
    
    // Parse patterns and generate mode lists
    for (auto& table : tables) {
        parse_lhgx_pattern(table.lhgx_pattern, table.modes, table.mode_count);
        
        // Special handling for Table A.5: exclude Ultimax modes (xx01)
        if (strcmp(table.name, "All out") == 0) {
            // Filter out modes where G=0 and X=1 (Ultimax pattern xx01)
            int filtered_count = 0;
            uint8_t filtered_modes[32];
            for (int i = 0; i < table.mode_count; i++) {
                uint8_t mode = table.modes[i];
                bool game = (mode & 0x10) != 0;
                bool exrom = (mode & 0x08) != 0;
                // Exclude Ultimax: !game && exrom
                if (!((!game) && exrom)) {
                    filtered_modes[filtered_count++] = mode;
                }
            }
            memcpy(table.modes, filtered_modes, filtered_count);
            table.mode_count = filtered_count;
        }
    }
    
    for (auto& table : tables) {
        printf("\n");
        printf("─────────────────────────────────────────────────────────────────\n");
        printf("  %s (%s: LHGX=%s)\n", table.name, table.doc_ref, table.lhgx_pattern);
        printf("─────────────────────────────────────────────────────────────────\n");
        
        // Determine if this table should show CHAREN columns
        // Tables where LHGX pattern ends with 'x' and has multiple modes differing only in CHAREN
        bool show_charen_cols = false;
        
        // Check if we have modes that differ only in CHAREN (bit 2) and possibly EXROM
        // Pattern "011x" means EXROM can vary, but we want CHAREN columns
        if (table.mode_count >= 2) {
            // Group modes by their LHGX bits (ignoring CHAREN)
            // If we find pairs that differ only in CHAREN, show columns
            for (int i = 0; i < table.mode_count; i++) {
                uint8_t mode_i = table.modes[i];
                uint8_t lhgx_i = (mode_i & 0x1B);  // Mask out CHAREN bit (0x04)
                
                for (int j = i + 1; j < table.mode_count; j++) {
                    uint8_t mode_j = table.modes[j];
                    uint8_t lhgx_j = (mode_j & 0x1B);  // Mask out CHAREN bit
                    
                    // If LHGX bits match but modes differ only in CHAREN
                    if (lhgx_i == lhgx_j && (mode_i ^ mode_j) == 0x04) {
                        show_charen_cols = true;
                        break;
                    }
                }
                if (show_charen_cols) break;
            }
        }
        
        if (show_charen_cols) {
            // Find a representative mode and use it for both CHAREN variants
            // Pick the first mode and toggle CHAREN bit
            uint8_t base_mode = table.modes[0];
            uint8_t mode_charen0 = base_mode & ~0x04;
            uint8_t mode_charen1 = base_mode | 0x04;
            
            printf("             #CHAREN=1              #CHAREN=0\n");
            printf("  Address   CPU R   CPU W     |   CPU R   CPU W\n");
            printf("  ───────  ──────  ──────     |  ──────  ──────\n");
            
            // Print all 16 banks once, showing both CHAREN variants
            for (uint16_t bank = 0; bank < 16; bank++) {
                uint16_t addr = bank << 12;
                
                // CHAREN=1 side
                uint8_t enc1 = bus->cpu_encoded_chip_per_bank_per_mode[mode_charen1][bank];
                uint8_t r1 = decode_read_chip(enc1);
                uint8_t w1 = decode_write_chip(enc1);
                
                // CHAREN=0 side
                uint8_t enc0 = bus->cpu_encoded_chip_per_bank_per_mode[mode_charen0][bank];
                uint8_t r0 = decode_read_chip(enc0);
                uint8_t w0 = decode_write_chip(enc0);
                
                printf("  $%04X   %-6s  %-6s     |  %-6s  %-6s\n",
                       addr, chip_name(r1), chip_name(w1), chip_name(r0), chip_name(w0));
            }
            
            // Add footer showing mode configuration (show LHGX pattern)
            bool loram, hiram, charen, game, exrom;
            decode_mode(mode_charen1, loram, hiram, charen, game, exrom);
            printf("─────────────────────────────────────────────────────────────────\n");
            printf("  #LORAM=%d #HIRAM=%d #GAME=%d #EXROM=x (CHAREN varies)\n",
                   loram?1:0, hiram?1:0, game?1:0);
        } else {
            // Single column format for tables with single mode or complex multi-mode tables
            printf("  Address    CPU R    CPU W\n");
            printf("  ───────   ───────  ───────\n");
            
            // For tables with multiple unrelated modes, just show the first mode
            // (These are placeholder tables like "All out" that group many modes)
            uint8_t mode = table.modes[0];
            
            for (uint16_t bank = 0; bank < 16; bank++) {
                uint16_t addr = bank << 12;
                uint8_t enc = bus->cpu_encoded_chip_per_bank_per_mode[mode][bank];
                uint8_t r = decode_read_chip(enc);
                uint8_t w = decode_write_chip(enc);
                printf("  $%04X    %-7s  %-7s\n", addr, chip_name(r), chip_name(w));
            }
            
            // Add footer showing mode configuration
            bool loram, hiram, charen, game, exrom;
            decode_mode(mode, loram, hiram, charen, game, exrom);
            printf("─────────────────────────────────────────────────────────────────\n");
            printf("  #LORAM=%d #HIRAM=%d #CHAREN=%d #GAME=%d #EXROM=%d\n",
                   loram?1:0, hiram?1:0, charen?1:0, game?1:0, exrom?1:0);
        }
    }
    
    printf("\n");
    
    // Cleanup
    pla_906114_01_destroy(pla);
    delete bus;
}