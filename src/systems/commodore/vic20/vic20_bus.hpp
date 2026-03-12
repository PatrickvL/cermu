#pragma once

#include <cstdint>

#include "core/bus_cycle_interface.hpp"
#include "core/system_lines.hpp"
#include "chip/cpu/fam65xx/mos6502.hpp"

// VIC-20 default bus state — derived from CPU + system extras.
// MOS6502 provides: RW, RDY, IRQ, NMI, RES.  System adds: BA, AEC, data 0xFF.
#define VIC20_BUS_DEFAULT_STATE \
    (MOS6502::default_bus_state() | BUS_BIT(BUS_BA_BIT) | \
     BUS_BIT(BUS_AEC_BIT) | BUS_DATA_MASK)

// VIC-20 bus structure
struct vic20_bus_t {
    void* vic20; // Pointer to VIC-20 system
    bus_state_t state;
    bus_state_t default_state;  // Pull-up resistor state (control lines HIGH)
    // Memory mapping state
    uint16_t bank_base;
    // I/O state
    uint16_t io_address;
    bool io_access_pending;
    uint8_t system_lines;
};

// Function prototypes
void* vic20_bus_create();
void vic20_bus_destroy(void* chip);
bus_state_t vic20_bus_tick(void* chip, bus_state_t bus_state);
void vic20_bus_attach(void* chip, void* bus);
void vic20_bus_init_adapters(vic20_bus_t* bus);
void vic20_bus_system_attach(vic20_bus_t* bus, void* system);