#include "c64_config.h"
#include <string.h>

const rom_config_t* system_config_get_default_roms(void) {
    static const rom_config_t default_roms = {
        /* basic_rom_filenames */ {
            "C64 - 901226-01 - Commodore (F833D117) Basic.rom",
            "basic.901226-01.bin",
            "901226-01.bin",
            "basic.rom",
            NULL
        },
        /* kernal_rom_filenames */ {
            "C64 - 901227-03 - Commodore (DBE3E7C7) Kernal.rom",
            "kernal.901227-03.bin",
            "901227-03.bin",
            "kernal.rom",
            NULL
        },
        /* chargen_rom_filenames */ {
            "C64 - 901225-01 - Commodore (EC4272EE) Characters.rom",
            "characters.901225-01.bin",
            "901225-01.bin"
            "chargen.rom",
            "char.rom"
        }
    };
    return &default_roms;
}

void c64_config_init_defaults(c64_config_t* config) {
    if (!config) return;
    
    // System configuration defaults
    config->vicii_standard = VIC_PAL;     // Default to PAL timing
    config->rom_config = NULL;            // Use default ROM paths
    
    // Test mode defaults - normal boot (like real C64)
    config->test_mode = C64_TEST_MODE_NORMAL;
    config->test_binary_config = NULL;
    
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
    
    // Validate test mode configuration
    if (config->test_mode == C64_TEST_MODE_PRG_FILE || config->test_mode == C64_TEST_MODE_BIN_FILE) {
        // These modes require test binary configuration
        if (!config->test_binary_config || !config->test_binary_config->filename) {
            return false;
        }
    }
    
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
