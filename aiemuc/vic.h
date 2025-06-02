#ifndef VIC_H
#define VIC_H

#include "device.h"
#include "bus.h"

typedef void (*vic_bank_change_func_t)(void* context, uint8_t bank);

typedef struct {
    device_descriptor_t* desc;
    uint8_t registers[47];
    uint8_t raster_line;
    uint8_t collision_sprite, collision_bg;
    uint8_t bank;
    bus_interface_t* bus;
    vic_bank_change_func_t bank_change;
} vic_ii_t;

// old
// ============================================================================
// VIC-II EMULATION - Video chip with cycle-accurate badline generation
// ============================================================================

typedef struct {
    const device_t* device;  // Pointer to device descriptor (must be first)
    
    uint8_t registers[64];
    uint16_t raster_line;
    uint8_t raster_cycle;
    bool badline_condition;
    bool prev_ba;
    
    // Bus attachment
    bus_state_t* bus;
} vic_state_t;

// Device attachment
void vic_attach_bus(vic_state_t* vic_dev, bus_state_t* bus_state);

#endif // VIC_H