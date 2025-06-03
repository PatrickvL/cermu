#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stdbool.h>
#include "aiemuc.h"
#include "system.h"
#include "c64_bus.h"

// Forward declarations to avoid circular dependencies
typedef struct mos6510_s mos6510_t;
typedef struct ram_s ram_t;
typedef struct rom_s rom_t;
typedef struct vic_ii_s vic_ii_t;
typedef struct cia_s cia_t;
typedef struct sid_s sid_t;

// TODO : Move to custom.h or delete if not needed
typedef struct {
    device_descriptor_t* desc;
    uint8_t registers[256];
    c64_bus_t* bus;
} custom_t;

typedef struct c64_s {
    system_8bit_t system;
    c64_bus_t* bus;
    mos6510_t* mos6510;
    ram_t* ram;
    rom_t* basic;
    vic_ii_t* vic_ii;
    cia_t* cia1;
    cia_t* cia2;
    custom_t* custom;
    sid_t* sid;
    rom_t* cartridge;
    rom_t* kernal;
    uint64_t total_cycles;  // Total cycles executed by the system
} c64_t;

#define ID_TUPLE(read_id, write_id) (((read_id) << 4) | (write_id))
#define READ_ID(id) (((id) >> 4) & 0xF)
#define WRITE_ID(id) ((id) & 0xF)

// Function declarations
c64_t* c64_system_create(void);
void c64_system_destroy(c64_t* c64);
void c64_non_cpu_cycle(c64_t* c64);
void c64_init(c64_t* c64);
void c64_emulate_frame(c64_t* c64);

// Global variables
extern c64_t* c64;

#endif // C64_H