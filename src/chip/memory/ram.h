#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/system_lines.h"

typedef struct ram_s {
    uint8_t* memory = nullptr;
    bool owns_memory = false;

    ~ram_s();
} ram_t;

