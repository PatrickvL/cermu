#include "c64_config.h"
#include <string.h>

const rom_config_t* system_config_get_default_roms(void) {
    static const rom_config_t default_roms = {
        .basic_rom_filenames = {
            "basic.901226-01.bin",
            "basic.rom", 
            "901226-01.bin",
            NULL,
            NULL
        },
        .kernal_rom_filenames = {
            "kernal.901227-03.bin",
            "kernal.rom",
            "901227-03.bin", 
            NULL,
            NULL
        },
        .chargen_rom_filenames = {
            "characters.901225-01.bin",
            "char.rom",
            "chargen.rom",
            "901225-01.bin",
            NULL,
            NULL
        }
    };
    return &default_roms;
}

void c64_config_init_defaults(c64_config_t* config) {
    if (!config) return;
    
    // System configuration defaults
    config->vicii_standard = VIC_PAL;     // Default to PAL timing
    config->rom_config = NULL;            // Use default ROM paths
    
    // Initialize with no cartridge ROMs by default (saves memory)
    config->roml_present = false;
    config->romh_present = false;
    config->roml_filename = NULL;
    config->romh_filename = NULL;
    
    // Default cartridge signals (no cartridge)
    config->initial_exrom_state = true;   // EXROM high = no cartridge ROM
    config->initial_game_state = true;    // GAME high = no cartridge ROM
}

bool c64_config_validate(const c64_config_t* config) {
    if (!config) return false;
    
    // If ROM is present, filename must be provided
    if (config->roml_present && !config->roml_filename) {
        return false;
    }
    
    if (config->romh_present && !config->romh_filename) {
        return false;
    }
    
    // If ROM is not present, filename should be NULL
    if (!config->roml_present && config->roml_filename) {
        return false;
    }
    
    if (!config->romh_present && config->romh_filename) {
        return false;
    }
    
    return true;
}
