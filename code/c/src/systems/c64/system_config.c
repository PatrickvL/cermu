#include "system_config.h"

const rom_config_t* system_config_get_default_roms(void) {
    static const rom_config_t default_roms = {
        .basic_rom_paths = {
            "../../data/c64/roms/basic.901226-01.bin",
            "data/c64/roms/basic.rom", 
            "data/c64/roms/901226-01.bin",
            NULL
        },
        .kernal_rom_paths = {
            "../../data/c64/roms/kernal.901227-03.bin",
            "data/c64/roms/kernal.rom",
            "data/c64/roms/901227-03.bin", 
            NULL
        },
        .chargen_rom_paths = {
            "../../data/c64/roms/characters.901225-01.bin",
            "data/c64/roms/char.rom",
            "data/c64/roms/chargen.rom",
            "data/c64/roms/901225-01.bin",
            NULL
        }
    };
    return &default_roms;
}
