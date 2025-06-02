#ifndef MOS6510_H
#define MOS6510_H

#include "device.h"
#include "bus.h"

typedef struct {
    device_descriptor_t* desc;
    bus_interface_t* bus;
} mos6510_t;

#endif MOS6510_H