#ifndef RAM_H
#define RAM_H

#include "c64.h"

// ============================================================================
// RAM EMULATION - 64K system RAM with CPU port handling
// ============================================================================

// 64K RAM
extern uint8_t ram[65536];

// Optimized I/O handlers (called via callback table)
void ram_read_handler(void);
void ram_write_handler(void);

#endif // RAM_H