#include "vic.h"
#include "bus.h"
#include <string.h>

// VIC initialization
void vic_init(vic_state_t* vic_dev) {
    // Initialize VIC state
    memset(vic_dev, 0, sizeof(*vic_dev));
    
    // Set up device callbacks
    vic_dev->device.r8 = vic_r8;
    vic_dev->device.w8 = vic_w8;
}

// VIC write masks for each register (defines which bits are writable)
const uint8_t vic_write_masks[64] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $00-$07
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $08-$0F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $10-$17
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $18-$1F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $20-$27
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $28-$2F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // $30-$37
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF   // $38-$3F
};

// Optimized VIC cycle function - no I/O handling, just timing and logic
void vic_cycle(vic_state_t* vic_dev) {
    // Always increment raster timing
    vic_dev->raster_cycle++;
    if (vic_dev->raster_cycle >= 63) {
        vic_dev->raster_cycle = 0;
        vic_dev->raster_line++;
        if (vic_dev->raster_line >= 312) vic_dev->raster_line = 0;
    }
    
    // Check for badline condition (hardware accurate)
    vic_dev->badline_condition = (vic_dev->raster_line >= 0x30 && vic_dev->raster_line <= 0xF7) &&
                                ((vic_dev->raster_line & 7) == (vic_dev->registers[0x11] & 7));
    
    // Generate BA signal for badline
    if (vic_dev->badline_condition && vic_dev->raster_cycle >= 15 && vic_dev->raster_cycle <= 54) {
        bus.control_lines &= ~BA_LINE; // Pull BA low - CPU will stall
    } else {
        bus.control_lines |= BA_LINE;  // Release BA
    }
    
    // AEC follows BA with one cycle delay (hardware accurate)
    if (vic_dev->prev_ba && !(bus.control_lines & BA_LINE)) {
        bus.control_lines &= ~AEC_LINE;
    } else if (!vic_dev->prev_ba && (bus.control_lines & BA_LINE)) {
        bus.control_lines |= AEC_LINE;
    }
    vic_dev->prev_ba = (bus.control_lines & BA_LINE) != 0;
}

// New optimized I/O handlers - called directly via callback table (no chip select checks!)
uint8_t vic_r8(struct device_s* dev) {
    vic_state_t* vic_dev = (vic_state_t*)dev;
    uint8_t reg = bus.address & 0x3F;
    return vic_dev->registers[reg];
}

void vic_w8(struct device_s* dev) {
    vic_state_t* vic_dev = (vic_state_t*)dev;
    uint8_t reg = bus.address & 0x3F;
    vic_dev->registers[reg] = bus.data & vic_write_masks[reg];
}