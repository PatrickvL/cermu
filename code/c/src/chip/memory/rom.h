#ifndef ROM_H
#define ROM_H

#include "../../core/chip.h"

typedef struct rom_s {
    chip_descriptor_t* desc;
    uint8_t* memory;
} rom_t;

// Specialized ROM creation function that takes size parameter  
void* rom_system_create_with_size(chip_descriptor_t* desc, unsigned int size);

extern chip_descriptor_t rom_descriptor;

#endif // ROM_H