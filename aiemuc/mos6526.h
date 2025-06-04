#ifndef MOS6526_H
#define MOS6526_H

#include "device.h"
#include "c64_bus.h"
#include <stdint.h>
#include <stdbool.h>

// MOS 6526, also known as CIA (Complex Interface Adapter),
// is a peripheral chip used in the Commodore 64.
// It provides timers, I/O ports, and a real-time clock (RTC) functionality.

typedef struct mos6526_s {
    device_descriptor_t* desc;
    uint8_t pra, prb, ddra, ddrb;
    uint16_t timer_a, timer_b;
    uint8_t tod_10ths, tod_sec, tod_min, tod_hr;
    uint8_t sdr, icr, cra, crb;
    bool tod_latched;
    uint8_t tod_latch[4];
    c64_bus_t* bus;
} mos6526_t;

// Function declarations
void mos6526_cycle(mos6526_t* cia);

#endif // CIA_H