#ifndef C64_BUS_H
#define C64_BUS_H

#include "aiemuc.h"
#include "device.h"
#include "system.h"
#include <stdint.h>
#include <stdbool.h>

// Bus control line definitions
#define IRQ_LINE    (1 << 0)
#define NMI_LINE    (1 << 1)
#define BA_LINE     (1 << 2)
#define AEC_LINE    (1 << 3)
#define RDY_LINE    (1 << 4)

// Macro definitions for device ID extraction
#define ID_TUPLE(read_id, write_id) (((read_id) << 4) | (write_id))
#define READ_ID(id) (((id) >> 4) & 0xF)
#define WRITE_ID(id) ((id) & 0xF)

typedef struct c64_s c64_t; // external, avoid circular dependency (via c64.h)

typedef struct c64_bus_s {
    device_descriptor_t* desc;
    c64_t* c64;
    uint8_t* device_id_per_page; // Maps each page (covering 256 bytes each, 256 pages) to a device ID
    alignas(64) read_callback_t read_callbacks[16]; // indexed by device ID
    alignas(64) write_callback_t write_callbacks[16]; // indexed by device ID
    alignas(64) uint8_t device_id_per_page_per_mode[32][256]; 

    uint16_t address;       // A0-A15
    uint8_t  data;          // D0-D7
    uint8_t  control_lines; // R/W, IRQ, NMI, BA, AEC, RDY
} c64_bus_t;

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode);

// Bus cycle functions
uint8_t c64_bus_read_cycle(c64_bus_t *bus, uint16_t addr);
void c64_bus_write_cycle(c64_bus_t* bus, uint16_t addr, uint8_t value);

// Memory functions
uint8_t c64_bus_memory_read(void* device, uint16_t address);
void c64_bus_memory_write(void* context, uint16_t address, uint8_t value);

// System functions  
void c64_bus_system_attach(c64_bus_t* c64_bus, c64_t* c64);

#endif // C64_BUS_H