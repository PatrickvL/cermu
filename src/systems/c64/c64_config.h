#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../core/config/path_discovery.h"

typedef enum {
    VIC_PAL,   // PAL timing standard
    VIC_NTSC   // NTSC timing standard
} vicii_standard_t;

/**
 * C64 test initialization modes.
 * Determines how RAM is initialized at system startup.
 */
typedef enum {
    C64_TEST_MODE_NORMAL,          // Normal boot with Kernal ROM (standard operation)
    C64_TEST_MODE_DEBUG_PATTERNS,  // Fill RAM with test patterns (current behavior)
    C64_TEST_MODE_PRG_FILE,        // Load a PRG file (2-byte load address + data)
    C64_TEST_MODE_BIN_FILE         // Load a BIN file at specific address
} c64_test_mode_t;

/**
 * Test binary configuration.
 * Used for PRG and BIN file loading modes.
 */
typedef struct {
    const char* filename;          // Path to test binary file
    uint16_t load_address;         // Load address (only used for BIN files, PRG has embedded address)
    bool auto_start;               // Whether to jump to loaded code (vs normal Kernal boot)
    uint16_t start_address;        // Address to jump to if auto_start is true (0 = use PRG's SYS address)
} c64_test_binary_config_t;

/**
 * ROM file configuration structure.
 * Holds paths to required ROM files with multiple alternatives per ROM type.
 */
typedef struct {
    const char* basic_rom_filenames[5];    // BASIC ROM filename alternatives (null-terminated array)
    const char* kernal_rom_filenames[5];   // KERNAL ROM filename alternatives  
    const char* chargen_rom_filenames[6];  // Character generator ROM filename alternatives
} rom_config_t;

/**
 * C64 system configuration structure.
 * This centralizes all C64 system settings to eliminate parameter proliferation.
 * Pass this to c64_system_create() to configure the C64 system.
 */
typedef struct {
    // System configuration
    vicii_standard_t vicii_standard;
    rom_config_t* rom_config;    // Optional ROM configuration (NULL = use defaults)
    
    // Test/initialization mode
    c64_test_mode_t test_mode;   // How to initialize RAM at startup
    c64_test_binary_config_t* test_binary_config;  // Test binary config (NULL if not used)
    
    // Cartridge ROM configuration
    bool roml_present;           // Whether ROML ROM should be included ($8000-$9FFF)
    bool romh_present;           // Whether ROMH ROM should be included ($A000-$BFFF/$E000-$FFFF)
    const char* roml_filename;   // Path to ROML ROM file (NULL if not present)
    const char* romh_filename;   // Path to ROMH ROM file (NULL if not present)
    
    // Initial cartridge control signal states
    bool initial_exrom_state;    // Initial EXROM signal state
    bool initial_game_state;     // Initial GAME signal state
} c64_config_t;

/**
 * Get default ROM configuration with common file paths.
 * Returns a static configuration with typical ROM file locations.
 */
const rom_config_t* system_config_get_default_roms(void);

// Initialize C64 configuration with default values
void c64_config_init_defaults(c64_config_t* config);

// Validate C64 configuration settings
bool c64_config_validate(const c64_config_t* config);

