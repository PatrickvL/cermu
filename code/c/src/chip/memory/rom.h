#ifndef ROM_H
#define ROM_H

#include "../../core/chip.h"

typedef struct rom_s {
    chip_descriptor_t* desc;
    uint8_t* memory;
    uint16_t size;
    uint16_t base_address;
} rom_t;

// Function declarations
void rom_memory_init(void* chip, uint16_t base_address, uint16_t size, chip_entry_t* chip_entry);

#endif // ROM_H