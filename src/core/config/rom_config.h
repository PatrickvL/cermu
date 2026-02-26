#pragma once

struct rom_config_t {
    const char* basic_rom_filenames[4];
    const char* kernal_rom_filenames[4];
    const char* chargen_rom_filenames[4];
};

const rom_config_t* system_config_get_default_roms(void);