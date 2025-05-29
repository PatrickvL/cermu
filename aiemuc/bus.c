#include "bus.h"

// Global bus state
bus_state_t bus_tmp; // TODO : Remove this once bus is moved to c64_state_t
bus_state_t* bus = &bus_tmp; // TODO : Move this towards c64_state_t

void bus_init(bus_state_t* bus) {
    // Initialize bus state
    bus->address = 0;
    bus->data = 0;
    bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
}