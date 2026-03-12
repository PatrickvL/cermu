#include "systems/commodore/c64/c64_config.hpp"
#include <cstring>

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

void c64_config_t::init_defaults() {
    // System configuration defaults
    vicii_standard = VIC_PAL;     // Default to PAL timing
    rom_config = NULL;            // Use default ROM paths
    
    // Test mode defaults - normal boot (like real C64)
    test_mode = C64_TEST_MODE_NORMAL;
    test_binary_config = NULL;
    
    // Initialize with no cartridge ROMs by default (saves memory)
    roml_present = false;
    romh_present = false;
}
