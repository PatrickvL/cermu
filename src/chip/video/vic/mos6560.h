#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "vic_common.h"

// VIC-6560 chip structure (inherits from vic_base_t via C++ inheritance)
typedef struct mos6560_s : public vic_base_t {
    // VIC-6560 specific fields (none currently, all in base)

    void init();  // Initialize NTSC-specific configuration
} mos6560_t;