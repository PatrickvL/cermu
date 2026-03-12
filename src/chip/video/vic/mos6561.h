#pragma once

#include <cstdint>

#include "chip/video/vic/vic_common.h"

// VIC-6561 chip structure (inherits from vic_base_t via C++ inheritance)
struct mos6561_t : public vic_base_t {
    void init();  // Initialize PAL-specific configuration
};