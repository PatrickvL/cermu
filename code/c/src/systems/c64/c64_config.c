#include "c64_config.h"

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
