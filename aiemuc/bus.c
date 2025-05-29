#include "bus.h"
#include "c64.h"
#include <string.h>

// Global bus state
bus_state_t bus;

// Global C64 state pointer for cycle counting
c64_state_t* c64_system = NULL;

// Current active map, updated by switch_cpu_mode()
device_callbacks_t* chip_select_map;

// Callback for each bus cycle (can be set by test harness)
void (*bus_cycle_callback)(void) = NULL;

// ============================================================================
// OPTIMIZED BUS CYCLE - Safe device lifecycle management and callback dispatch
// ============================================================================

void bus_cycle(void) {
    if (c64_system) {
        c64_system->total_cycles++;
        
        // All chips always run for cycle accuracy - using safe device callers
        device_cycle((struct device_s*)&c64_system->vic);
        device_cycle((struct device_s*)&c64_system->cia1);
        device_cycle((struct device_s*)&c64_system->cia2);
        device_cycle((struct device_s*)&c64_system->sid);
    }

    // Update RDY line based on BA (hardware accurate)
    if (bus.control_lines & BA_LINE) {
        bus.control_lines |= RDY_LINE;
    } else {
        bus.control_lines &= ~RDY_LINE;
    }
    // Call the callback if set
    if (bus_cycle_callback) bus_cycle_callback();
}

// Optimized read cycle implementation - direct callback dispatch
void cpu_read_cycle(uint16_t addr) {
    bus.address = addr;
    bus_cycle();
    device_callbacks_t* cb = &chip_select_map[addr >> 8];
    bus.data = cb->read(cb->read_device, addr);
}

// Optimized write cycle implementation - direct callback dispatch
void cpu_write_cycle(uint16_t addr, uint8_t value) {
    bus.address = addr;
    bus.data = value;
    device_callbacks_t* cb = &chip_select_map[addr >> 8];
    cb->write(cb->write_device, addr, value);
    bus_cycle();
}

void bus_init(bus_state_t* bus) {
    // Initialize bus state
    bus->address = 0;
    bus->data = 0;
    bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
}