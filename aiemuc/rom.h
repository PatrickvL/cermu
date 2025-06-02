#ifndef ROM_H
#define ROM_H

#include "device.h"

typedef struct {
    device_descriptor_t* desc;
    uint8_t* memory;
    uint16_t size;
    uint16_t base_address;
} rom_t;

#endif // ROM_H