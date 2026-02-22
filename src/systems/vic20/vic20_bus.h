#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/bus_cycle_interface.h"
#include "../../core/system_lines.h"

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