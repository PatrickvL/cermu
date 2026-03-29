#pragma once

#include <cstdint>
#include <cstddef>

enum vicii_standard_t {
    VIC_PAL,   // PAL timing standard
    VIC_NTSC   // NTSC timing standard
};

/**
 * C64 test initialization modes.
 * Determines how RAM is initialized at system startup.
 */
enum c64_test_mode_t {
    C64_TEST_MODE_NORMAL,          // Normal boot with Kernal ROM (standard operation)
    C64_TEST_MODE_DEBUG_PATTERNS,  // Fill RAM with test patterns (current behavior)
    C64_TEST_MODE_PRG_FILE,        // Load a PRG file (2-byte load address + data)
    C64_TEST_MODE_BIN_FILE         // Load a BIN file at specific address
};

/**
 * Test binary configuration.
 * Used for PRG and BIN file loading modes.
 */
struct c64_test_binary_config_t {
    const char* filename;          // Path to test binary file
    uint16_t load_address;         // Load address (only used for BIN files, PRG has embedded address)
    bool auto_start;               // Whether to jump to loaded code (vs normal Kernal boot)
    uint16_t start_address;        // Address to jump to if auto_start is true (0 = use PRG's SYS address)
};

/**
 * C64 system configuration structure.
 * Centralizes C64-specific settings (video standard, ROM config, test mode).
 */
struct c64_config_t {
    // System configuration
    vicii_standard_t vicii_standard;
    
    // Test/initialization mode
    c64_test_mode_t test_mode;   // How to initialize RAM at startup
    c64_test_binary_config_t* test_binary_config;  // Test binary config (NULL if not used)
    
    // Cartridge ROM configuration
    bool roml_present;           // Whether ROML ROM should be included ($8000-$9FFF)
    bool romh_present;           // Whether ROMH ROM should be included ($A000-$BFFF/$E000-$FFFF)
    
    // Methods
    void init_defaults();
};

