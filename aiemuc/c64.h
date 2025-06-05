#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stdbool.h>
#include "aiemuc.h"
#include "system.h"
#include "mos6510.h" // cpu
#include "mos6526.h" // cia
#include "mos6581.h" // sid
#include "mos6569.h" // vicii

// Forward declaration to avoid circular dependency with c64_bus.h
typedef struct c64_bus_s c64_bus_t;

// Forward declarations to avoid circular dependencies
// Note: mos6510_t is defined in mos6510.h as anonymous struct, so no forward declaration needed
typedef struct ram_s ram_t;
typedef struct rom_s rom_t;
typedef struct mos6581_s mos6581_t;
typedef struct mos6526_s mos6526_t;
typedef struct mos6569_s mos6569_t;

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
    mos6581_t* sid;
    mos6526_t* cia1;
    mos6526_t* cia2;
    custom_t* custom;
    mos6569_t* vicii;
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
void c64_emulate_frame(c64_t* c64);

#endif // C64_H