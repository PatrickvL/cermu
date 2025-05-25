#ifndef VIC_H
#define VIC_H

#include "c64.h"

// ============================================================================
// VIC-II EMULATION - Video chip with cycle-accurate badline generation
// ============================================================================

typedef struct {
    uint8_t registers[64];

    uint16_t raster_line;
    uint8_t raster_cycle;
    bool badline_condition;
    bool prev_ba;
} vic_state_t;

extern vic_state_t vic;
extern const uint8_t vic_write_masks[64];

// VIC functions
void vic_cycle(void);

#endif // VIC_H