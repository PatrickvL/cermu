#pragma once

#include <cstdint>

#include "vic_common.h"

// VIC-6560 chip structure (inherits from vic_base_t via C++ inheritance)
struct mos6560_t : public vic_base_t {
    // VIC-6560 specific fields (none currently, all in base)

    void init();  // Initialize NTSC-specific configuration
};