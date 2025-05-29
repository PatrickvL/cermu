#include "bus.h"

void bus_init(bus_state_t* bus) {
    // Initialize bus state
    bus->address = 0;
    bus->data = 0;
    bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
}