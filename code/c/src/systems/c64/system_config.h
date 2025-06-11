#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    VIC_PAL,   // PAL timing standard
    VIC_NTSC   // NTSC timing standard
} vic_standard_t;

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

/**
 * Discover the ROM root folder by searching upwards from executable location.
 * Searches for 'data' folder containing the system ROM files.
 * 
 * @param system_name System folder name (e.g., "c64", "vic20")
 * @param out_path Buffer to store the discovered ROM root path
 * @param path_size Size of the output buffer
 * @return true if ROM root folder was found, false otherwise
 */
bool system_config_discover_rom_root(const char* system_name, char* out_path, size_t path_size);

#endif // SYSTEM_CONFIG_H