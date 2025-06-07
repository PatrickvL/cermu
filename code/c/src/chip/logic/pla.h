#ifndef PLA_H
#define PLA_H

#include "../../core/device.h"
#include <stdint.h>
#include <stdbool.h>

// Programmable Logic Array (PLA) - C64 Memory Management Unit
// The PLA chip controls memory banking and I/O mapping in the C64
// TODO: Implement PLA functionality for memory management

typedef struct pla_s {
    device_descriptor_t* desc;
    // TODO: Add PLA state variables
    uint8_t control_register;
    bool basic_rom_enabled;
    bool kernal_rom_enabled;
    bool io_enabled;
    bool char_rom_enabled;
} pla_t;

// Function declarations (to be implemented)
void* pla_create(device_descriptor_t* desc);
void pla_destroy(void* device);
uint8_t pla_read(void* context, uint16_t address);
void pla_write(void* context, uint16_t address, uint8_t value);

extern device_descriptor_t pla_descriptor;

#endif // PLA_H
