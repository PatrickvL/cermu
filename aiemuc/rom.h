#ifndef ROM_H
#define ROM_H

#include "c64.h"

// ============================================================================
// ROM EMULATION - KERNAL, BASIC, and Character ROM
// ============================================================================

// ROM images
extern uint8_t kernal_rom[8192];
extern uint8_t basic_rom[8192];
extern uint8_t char_rom[4096];

// Optimized I/O handlers (called via callback table)
void basic_read_handler(void);
void char_rom_read_handler(void);
void kernel_read_handler(void);

#endif // ROM_H