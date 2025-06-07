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

// Encoding macros (Option 1: [7:4] write ID, [3:0] read ID)
#define DEVIDS_RW_ENCODE(read_id, write_id) \
    (((read_id) & 0x0F) | (((write_id) & 0x0F) << 4))
#define DEVID_READ_DECODE(entry) ((entry) & 0x0F)
#define DEVID_WRITE_DECODE(entry) ((entry) >> 4) // For now. Once write IDs are reduced to 2 bits use shift 6 and enable below spare bit
//#define DECODE_SPARE(entry) (((entry) >> 4) & 0x03)
// Encoding macros (Option 2: [7:6] write ID, [5:4] spare, [3:0] read ID)
//    (((read_id) & 0x0F) | (((flags) & 0x03) << 4) | (((write_id) & 0x03) << 6))

typedef struct c64_s c64_t; // external, avoid circular dependency (via c64.h)

// Inline function to calculate condensed device index from address
// IO range (bank 13) maps to index 16-31, other banks use bank number directly
static inline int c64_bus_get_device_index(uint16_t address) {
    int bank = address >> 12;
    int page = (address >> 8) & 0x0F;
    return bank + ((bank == 13) * (page + 3));
}

typedef struct c64_bus_s {
    device_descriptor_t* desc;
    c64_t* c64;
    uint8_t* device_id_per_page; // Maps each condensed index (32 entries) to a device ID
    alignas(64) device_access_callback_t device_access_callbacks[16]; // indexed by device ID - unified read/write/context
    alignas(64) uint8_t device_id_per_index_per_mode[32][32]; // Condensed from 256 to 32 entries per mode

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