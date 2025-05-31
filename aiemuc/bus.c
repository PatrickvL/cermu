#include "bus.h"

// Current active map, updated by switch_cpu_mode()
extern device_callbacks_t* chip_select_map;

void bus_init(bus_state_t* bus) {
    // Initialize bus state
    bus->address = 0;
    bus->data = 0;
    bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
}

uint8_t bus_read_cycle(bus_state_t *bus, uint16_t addr) {
    // For all other addresses, use chip select map
    bus->address = addr;
    c64_non_cpu_cycles();
    device_callbacks_t* cb = &chip_select_map[addr >> 8];
    uint8_t data = cb->read(cb->read_device, addr);
    bus->data = data;
    return data;
}

void bus_write_cycle(bus_state_t* bus, uint16_t addr, uint8_t value) {
    // For all other addresses, use chip select map
    bus->address = addr;
    bus->data = value;
    device_callbacks_t* cb = &chip_select_map[addr >> 8];
    cb->write(cb->write_device, addr, value);
    c64_non_cpu_cycles();
}