#ifndef SID_H
#define SID_H

#include "c64.h"

// ============================================================================
// SID EMULATION - Sound chip with envelope generators
// ============================================================================

typedef struct {
    uint8_t registers[32];

    uint16_t envelope_counter[3];
    uint8_t envelope_state[3];
} sid_state_t;

extern sid_state_t sid;

// SID functions
void sid_cycle(void);

#endif // SID_H