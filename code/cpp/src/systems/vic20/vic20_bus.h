#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/bus_cycle_interface.h"
#include "../../core/chip.h"

// VIC-20 bus structure
typedef struct {
    chip_descriptor_t* desc;
    void* vic20; // Pointer to VIC-20 system
    bus_state_t state;
    // Memory mapping state
    uint16_t bank_base;
    // I/O state
    uint16_t io_address;
    bool io_access_pending;
    uint8_t system_lines;
} vic20_bus_t;

// Chip descriptor for VIC-20 bus
extern chip_descriptor_t vic20_bus_descriptor;

// Function prototypes
void* vic20_bus_create(chip_descriptor_t* desc);
void vic20_bus_destroy(void* chip);
bus_state_t vic20_bus_tick(void* chip, bus_state_t bus_state);
void vic20_bus_attach(void* chip, void* bus);
void vic20_bus_init_adapters(vic20_bus_t* bus);
void vic20_bus_system_attach(vic20_bus_t* bus, void* system);