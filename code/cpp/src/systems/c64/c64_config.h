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

