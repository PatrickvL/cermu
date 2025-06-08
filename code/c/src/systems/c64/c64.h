#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stdbool.h>
#include "../../core/aiemuc.h"
#include "../../core/system.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/memory/mos2114.h"
#include "../../chip/sound/mos6581.h"
#include "../../chip/io/mos6526.h"
#include "../../chip/video/mos6569.h"
#include "c64_bus.h"  // Include the bus header to get c64_bus_t definition

// No forward declarations needed - all types are defined in included headers


typedef struct c64_s {
    system_8bit_t system;
    c64_bus_t* bus;
    void* mos6510;  // mos6510_t* - opaque pointer to avoid circular dependency
    ram_t* ram;
    rom_t* basic;
    mos6581_t* sid;
    mos6526_t* cia1;
    mos6526_t* cia2;
    mos2114_t* colorram;
    mos6569_t* vicii;
    rom_t* cartridge;
    rom_t* kernal;
    uint64_t total_cycles;  // Total cycles executed by the system
} c64_t;

// Function declarations
c64_t* c64_system_create(void);
void c64_system_destroy(c64_t* c64);
void c64_non_cpu_cycle(void* c64);  // c64_t* - using void* for consistency
void c64_pla_maps_generate(c64_t* c64);  // PLA memory mapping generation

// ACID detached callback functions for unmapped chips
uint8_t c64_detached_read(void* context, uint16_t address);
void c64_detached_write(void* context, uint16_t address, uint8_t value);

#endif // C64_H