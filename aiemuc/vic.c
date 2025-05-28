#include "vic.h"
#include "bus.h"

vic_state_t vic;

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
void vic_cycle(void) {
    // Always increment raster timing
    vic.raster_cycle++;
    if (vic.raster_cycle >= 63) {
        vic.raster_cycle = 0;
        vic.raster_line++;
        if (vic.raster_line >= 312) vic.raster_line = 0;
    }
    
    // Check for badline condition (hardware accurate)
    vic.badline_condition = (vic.raster_line >= 0x30 && vic.raster_line <= 0xF7) &&
                           ((vic.raster_line & 7) == (vic.registers[0x11] & 7));
    
    // Generate BA signal for badline
    if (vic.badline_condition && vic.raster_cycle >= 15 && vic.raster_cycle <= 54) {
        bus_state.control_lines &= ~BA_LINE; // Pull BA low - CPU will stall
    } else {
        bus_state.control_lines |= BA_LINE;  // Release BA
    }
    
    // AEC follows BA with one cycle delay (hardware accurate)
    if (vic.prev_ba && !(bus_state.control_lines & BA_LINE)) {
        bus_state.control_lines &= ~AEC_LINE;
    } else if (!vic.prev_ba && (bus_state.control_lines & BA_LINE)) {
        bus_state.control_lines |= AEC_LINE;
    }
    vic.prev_ba = (bus_state.control_lines & BA_LINE) != 0;
}

// New optimized I/O handlers - called directly via callback table (no chip select checks!)
void vic_handle_read(void) {
    uint8_t reg = bus_state.address & 0x3F;
    bus_state.data = vic.registers[reg];
}

void vic_handle_write(void) {
    uint8_t reg = bus_state.address & 0x3F;
    vic.registers[reg] = bus_state.data & vic_write_masks[reg];
}