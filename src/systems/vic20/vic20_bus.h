#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/bus_cycle_interface.h"
#include "../../core/system_lines.h"

// VIC-20 default bus state with pull-up resistors.
// Data bus: 0xFF (pull-ups), BA/AEC/RDY/RW HIGH, active-low IRQ/NMI/RES HIGH (inactive).
#define VIC20_BUS_DEFAULT_STATE \
    (BUS_STATE(0, 0xFF, BUS_MASK_BA | BUS_MASK_AEC | BUS_MASK_RDY | BUS_MASK_RW) | \
     BUS_BIT(BUS_RES_BIT) | BUS_BIT(BUS_IRQ_BIT) | BUS_BIT(BUS_NMI_BIT))

// VIC-20 bus structure
typedef struct {
    void* vic20; // Pointer to VIC-20 system
    bus_state_t state;
    bus_state_t default_state;  // Pull-up resistor state (control lines HIGH)
    // Memory mapping state
    uint16_t bank_base;
    // I/O state
    uint16_t io_address;
    bool io_access_pending;
    uint8_t system_lines;
} vic20_bus_t;

// Function prototypes
void* vic20_bus_create();
void vic20_bus_destroy(void* chip);
bus_state_t vic20_bus_tick(void* chip, bus_state_t bus_state);
void vic20_bus_attach(void* chip, void* bus);
void vic20_bus_init_adapters(vic20_bus_t* bus);
void vic20_bus_system_attach(vic20_bus_t* bus, void* system);