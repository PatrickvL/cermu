#ifndef VIC_H
#define VIC_H

#include "device.h"
#include "c64_bus.h"

typedef void (*vic_bank_change_func_t)(void* context, uint8_t bank);

typedef struct {
    device_descriptor_t* desc;
    uint8_t registers[47];
    uint8_t raster_line;
    uint8_t raster_cycle;
    bool badline_condition;
    bool prev_ba;
    uint8_t collision_sprite, collision_bg;
    uint8_t bank;
    c64_bus_t* bus;
    vic_bank_change_func_t bank_change;
} vic_ii_t;

#endif // VIC_H