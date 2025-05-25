#include "bus.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"

// Keep bus state accessible
bus_state_t bus_state;

// ============================================================================
// UNIFIED BUS CYCLE - All chips react to bus state simultaneously
// ============================================================================
void bus_cycle(void) {
    // All chips always run for cycle accuracy - no conditionals for performance
    vic_cycle();    // Video timing, BA control, sprites
    cia1_cycle();   // Timers, keyboard, joystick
    cia2_cycle();   // Timers, serial, user port  
    sid_cycle();    // Sound generation, envelope generators
    
    // Update RDY line based on BA (hardware accurate)
    if (bus_state.bus_control & BA_LINE) {
        bus_state.bus_control |= RDY_LINE;
    } else {
        bus_state.bus_control &= ~RDY_LINE;
    }
}