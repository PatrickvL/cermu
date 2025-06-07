#include "c64_bus_adapter.h"
#include "c64.h"
#include "../../core/control_lines_interface.h"

// Adapter function implementations for C64 bus
static uint8_t c64_bus_adapter_bus_read(void* context, uint16_t address) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    return c64_bus_memory_read(c64_bus, address);
}

static void c64_bus_adapter_bus_write(void* context, uint16_t address, uint8_t value) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    c64_bus_memory_write(c64_bus, address, value);
}

static void c64_bus_adapter_non_cpu_cycle(void* context) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    c64_non_cpu_cycle(c64_bus->c64);
}

// Control lines adapter functions
static uint32_t c64_control_lines_get(void* context) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert the C64's uint8_t control_lines to the new uint32_t format
    // For now, just extend it to 32 bits
    return (uint32_t)c64_bus->control_lines;
}

static void c64_control_lines_set(void* context, uint32_t lines) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert back to the C64's uint8_t format
    // For now, just truncate (assumes lower 8 bits contain the relevant data)
    c64_bus->control_lines = (uint8_t)(lines & 0xFF);
}

// I/O port adapter functions
static void c64_io_port_output_changed(void* context, uint8_t ddr, uint8_t port_data, uint8_t effective_output) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Use the effective output for mode switching
    c64_bus_mode_switch(c64_bus, effective_output);
}

static uint8_t c64_io_port_input_read(void* context) {
    // For C64, the I/O port typically reads the current port state
    // This can be extended to read actual external signals if needed
    (void)context; // Unused for now
    return 0xFF; // Default to all inputs high
}

bus_cycle_ops_t c64_bus_create_adapter(c64_bus_t* c64_bus) {
    bus_cycle_ops_t interface = {
        .bus_read = c64_bus_adapter_bus_read,
        .bus_write = c64_bus_adapter_bus_write,
        .cycle_tick = c64_bus_adapter_non_cpu_cycle,
        .context = c64_bus
    };
    return interface;
}

control_lines_interface_t c64_control_lines_create_adapter(c64_bus_t* c64_bus) {
    control_lines_interface_t interface = {
        .get_lines = c64_control_lines_get,
        .set_lines = c64_control_lines_set,
        .context = c64_bus
    };
    return interface;
}

mos6510_io_port_interface_t c64_io_port_create_adapter(c64_bus_t* c64_bus) {
    mos6510_io_port_interface_t interface = {
        .output_pins_changed = c64_io_port_output_changed,
        .read_external_pins = c64_io_port_input_read,
        .context = c64_bus
    };
    return interface;
}
