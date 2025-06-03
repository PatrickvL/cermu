#ifndef MOS6569_H
#define MOS6569_H

#include "device.h"
#include "c64_bus.h"
#include <stdint.h>
#include <stdbool.h>

// MOS 6569, also known as VIC-II (Video Interface Chip II),
// is the video chip used in the Commodore 64.
// It provides video output, sprite handling, and collision detection.

typedef void (*mos6569_bank_change_func_t)(void* context, uint8_t bank);

typedef struct mos6569_s {
    device_descriptor_t* desc;
    uint8_t registers[47];
    uint8_t raster_line;
    uint8_t raster_cycle;
    bool badline_condition;
    bool prev_ba;
    uint8_t collision_sprite, collision_bg;
    uint8_t bank;
    c64_bus_t* bus;
    mos6569_bank_change_func_t bank_change;
} mos6569_t;

// Function declarations
void mos6581_cycle(mos6569_t* vic);

#endif // MOS6569_H