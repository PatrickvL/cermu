#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>

typedef enum {
    VIC_PAL,   // PAL timing standard
    VIC_NTSC   // NTSC timing standard
} vic_standard_t;

/**
 * ROM file configuration structure.
 * Holds paths to required ROM files with multiple alternatives per ROM type.
 */
typedef struct {
    const char* basic_rom_paths[4];    // BASIC ROM alternatives (null-terminated array)
    const char* kernal_rom_paths[4];   // KERNAL ROM alternatives  
    const char* chargen_rom_paths[5];  // Character generator ROM alternatives
} rom_config_t;

/**
 * System configuration options.
 * Pass this to c64_system_create() to configure the C64 system.
 */
typedef struct {
    vic_standard_t vic_standard;
    rom_config_t* rom_config;  // Optional ROM configuration (NULL = use defaults)
} system_config_t;

/**
 * Get default ROM configuration with common file paths.
 * Returns a static configuration with typical ROM file locations.
 */
const rom_config_t* system_config_get_default_roms(void);

#endif // SYSTEM_CONFIG_H