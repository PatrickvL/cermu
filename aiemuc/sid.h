#ifndef SID_H
#define SID_H

#include "device.h"

typedef struct sid_s {
    device_descriptor_t* desc;
    uint8_t registers[29];
    uint16_t envelope_counter[3];
    uint8_t envelope_state[3];
    uint8_t pot_x, pot_y;
    uint8_t osc3, env3;
} sid_t;

// Function declarations
void sid_cycle(sid_t* sid);

#endif // SID_H