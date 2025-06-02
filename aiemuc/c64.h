#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu6510.h"
#include "aiemuc.h"
#include "system.h"
#include "mos6510.h"
#include "cia.h"
#include "ram.h"
#include "rom.h"
#include "vic.h"
#include "sid.h"
#include "c64_bus.h"

// TODO : Move to custom.h or delete if not needed
typedef struct {
    device_descriptor_t* desc;
    uint8_t registers[256];
    bus_interface_t* bus;
} custom_t;

typedef struct c64_state_s {
    system_8bit_t system;
    c64_bus_t* bus; // was bus_state_t
    mos6510_t* mos6510; // was cpu6510_state_t
    ram_t* ram; // was ram_state_t
    rom_t* basic; // was rom_state_t
    vic_ii_t* vic_ii; // was vic_state_t
    cia_t* cia1; // was cia_state_t
    cia_t* cia2; // was cia_state_t
    custom_t* custom;
    sid_t* sid; // was sid_state_t
    rom_t* cartridge; // was rom_state_t
    rom_t* kernal; // was rom_state_t
    uint64_t total_cycles;  // Total cycles executed by the system
    uint8_t port[2];
} c64_state_t;

#define MAKE_MASK(id) (((id) << 4) | (id))

// Global variables
c64_state_t* c64 = NULL;
extern void* mos6510_opcode_map[256];

void c64_init(c64_state_t* c64);
void c64_emulate_frame(c64_state_t* c64);

#endif // C64_H