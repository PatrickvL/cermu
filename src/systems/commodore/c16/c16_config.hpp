#pragma once

#include <cstdint>

// Stub C16 configuration structure
// TODO: Implement proper C16 config when MOS7501 CPU and TED 7360 chips are available
struct c16_config_t {
    uint8_t placeholder;  // Placeholder to make struct valid
};

// Stub ROM configuration structure
// TODO: Use proper ROM loader when available
struct rom_config_t {
    const char* basic_rom_path;
    const char* kernal_rom_path;
};