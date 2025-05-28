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

// VIC functions
void vic_init(void);
void vic_cycle(void);

// Optimized I/O handlers (called via callback table)
uint8_t vic_r8(void);
void vic_w8(void);

#endif // VIC_H