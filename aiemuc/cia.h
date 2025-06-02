#ifndef CIA_H
#define CIA_H

#include "device.h"
#include "c64_bus.h"

typedef struct {
    device_descriptor_t* desc;
    uint8_t pra, prb, ddra, ddrb;
    uint16_t timer_a, timer_b;
    uint8_t tod_10ths, tod_sec, tod_min, tod_hr;
    uint8_t sdr, icr, cra, crb;
    bool tod_latched;
    uint8_t tod_latch[4];
    c64_bus_t* bus;
} cia_t;

#endif // CIA_H