#ifndef C64_BUS_H
#define C64_BUS_H

#include "device.h"
#include "bus.h"
#include "c64.h"

typedef struct {
    device_descriptor_t* desc;
    bus_interface_t interface;
    c64_state_t* c64;
} c64_bus_t;

#endif C64_BUS_H
