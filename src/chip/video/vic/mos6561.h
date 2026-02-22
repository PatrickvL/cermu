#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "vic_common.h"

// VIC-6561 chip structure (inherits from vic_base_t via C++ inheritance)
typedef struct mos6561_s : public vic_base_t {
    void init();  // Initialize PAL-specific configuration
} mos6561_t;