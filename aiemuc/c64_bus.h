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

// Device ID definitions (unified index space, ordered by base address)
// The first 7 IDs are for devices supporting both read and write callbacks.
// The rest (CARTRIDGE, BASIC, KERNAL) are read-only.
#define DEVID_UNMAPPED    0   // Unmapped (PLA hole)
#define DEVID_ZERO_PAGE   1   // Zero page (special handling for $0000/$0001)
#define DEVID_RAM         2   // RAM (main memory)
#define DEVID_VIC         3   // VIC-II ($D000)
#define DEVID_SID         4   // SID ($D400)
#define DEVID_COLORRAM    5   // Color RAM ($D800)
#define DEVID_CIA         6   // CIA1 ($DC00) and CIA2 ($DD00), same device type, different context
// Devices below only support read (ROM/Cartridge)
#define DEVID_CARTRIDGE   7   // Cartridge ROM ($8000)
#define DEVID_BASIC_ROM   8   // BASIC ROM ($A000)
#define DEVID_KERNAL_ROM  9   // KERNAL ROM ($E000)
// Add more as needed, keeping IDs unique and ordered by base address

// Encoding macros ([7:5] write ID, [4]:spare bit, [3:0] read ID)
#define DEVIDS_RW_ENCODE(read_id, write_id) \
    (((read_id) & 0x0F) | (((write_id) & 0x07) << 5)) // TODO : Encode spare bit once needed
#define DEVID_READ_DECODE(entry) ((entry) & 0x0F)
#define DEVID_WRITE_DECODE(entry) ((entry) >> 5)
#define DECODE_SPARE(entry) (((entry) >> 4) & 0x01)

typedef struct c64_s c64_t; // external, avoid circular dependency (via c64.h)

typedef struct c64_bus_s {
    device_descriptor_t* desc;
    c64_t* c64;
    uint8_t  control_lines; // R/W, IRQ, NMI, BA, AEC, RDY
    uint8_t  data;          // D0-D7
    uint16_t address;       // A0-A15
    alignas(64) access_callback_t access_callback_per_devid[16]; // indexed by device ID - unified read/write/context
    alignas(64) uint8_t devid_per_bankidx[32]; // Maps each condensed index (32 entries) to a device ID
    alignas(64) uint8_t devid_per_bankidx_per_mode[32][32]; // Condensed from 256 to 32 entries per mode
} c64_bus_t;

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode);

// Bus cycle functions
uint8_t c64_bus_read_cycle(c64_bus_t *bus, uint16_t addr);
void c64_bus_write_cycle(c64_bus_t* bus, uint16_t addr, uint8_t value);

// Memory functions
uint8_t c64_bus_memory_read(c64_bus_t *bus, uint16_t address);
void c64_bus_memory_write(c64_bus_t *bus, uint16_t address, uint8_t value);

// System functions  
void c64_bus_system_attach(c64_bus_t* c64_bus, c64_t* c64);

#endif // C64_BUS_H