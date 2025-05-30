#include "vic.h"
#include "bus.h"
#include <string.h>

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

// New optimized I/O handlers - called directly via callback table (no chip select checks!)
uint8_t vic_r8(struct device_s* dev, uint16_t address) {
    vic_state_t* vic_dev = (vic_state_t*)dev;
    uint8_t reg = address & 0x3F;
    return vic_dev->registers[reg];
}

void vic_w8(struct device_s* dev, uint16_t address, uint8_t data) {
    vic_state_t* vic_dev = (vic_state_t*)dev;
    uint8_t reg = address & 0x3F;
    vic_dev->registers[reg] = data & vic_write_masks[reg];
}
// Forward declaration of device descriptor
static const device_t vic_device_descriptor;

// Direct implementation functions for device lifecycle
static void vic_init(struct device_s* dev) {
    vic_state_t* vic_dev = (vic_state_t*)dev;
    // Initialize VIC state
    memset(vic_dev, 0, sizeof(*vic_dev));
    // Set up device callbacks from descriptor pointer
    vic_dev->device = &vic_device_descriptor;
}

static void vic_cycle(struct device_s* dev) {
    vic_state_t* vic_dev = (vic_state_t*)dev;
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
        vic_dev->bus->control_lines &= ~BA_LINE; // Pull BA low - CPU will stall
    } else {
        vic_dev->bus->control_lines |= BA_LINE;  // Release BA
    }
    
    // AEC follows BA with one cycle delay (hardware accurate)
    if (vic_dev->prev_ba && !(vic_dev->bus->control_lines & BA_LINE)) {
        vic_dev->bus->control_lines &= ~AEC_LINE;
    } else if (!vic_dev->prev_ba && (vic_dev->bus->control_lines & BA_LINE)) {
        vic_dev->bus->control_lines |= AEC_LINE;
    }
    vic_dev->prev_ba = (vic_dev->bus->control_lines & BA_LINE) != 0;
}

// Static device descriptor for VIC
static const device_t vic_device_descriptor = {
    .r8 = vic_r8,
    .w8 = vic_w8,
    .init = vic_init,
    .cycle = vic_cycle,
    .cleanup = NULL
};

// Attach bus to VIC
void vic_attach_bus(vic_state_t* vic_dev, bus_state_t* bus_state) {
    vic_dev->bus = bus_state;
}
