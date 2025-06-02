#ifndef RAM_H
#define RAM_H

#include "device.h"

typedef struct {
    device_descriptor_t* desc;
    uint8_t memory[65536];
} ram_t;

#endif // RAM_H