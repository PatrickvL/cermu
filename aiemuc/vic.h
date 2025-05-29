#ifndef VIC_H
#define VIC_H

#include "device.h"
#include "bus.h"

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
} vic_state_t;

// Note: Use device_init(), device_cycle(), device_cleanup() instead of wrapper functions

#endif // VIC_H