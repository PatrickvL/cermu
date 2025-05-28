#ifndef VIC_H
#define VIC_H

#include "c64.h"
#include "bus.h"

// ============================================================================
// VIC-II EMULATION - Video chip with cycle-accurate badline generation
// ============================================================================

typedef struct {
    device_t device;  // Generic device interface (must be first)
    
    uint8_t registers[64];
    uint16_t raster_line;
    uint8_t raster_cycle;
    bool badline_condition;
    bool prev_ba;
} vic_state_t;

extern vic_state_t vic;

// Forward declaration
struct device_s;

// VIC functions
void vic_init(vic_state_t* vic_dev);
void vic_cycle(vic_state_t* vic_dev);

// Optimized I/O handlers (called via callback table)
uint8_t vic_r8(struct device_s* dev);
void vic_w8(struct device_s* dev);

#endif // VIC_H