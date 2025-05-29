#ifndef C64_H
#define C64_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu6510.h"
#include "ram.h"
#include "rom.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"

// Compiler optimization hints
#ifdef _MSC_VER
#define likely(x)   (x)
#define unlikely(x) (x)
#else
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// ============================================================================
// C64 SYSTEM STATE STRUCTURE
// ============================================================================
typedef struct c64_state_s {
    cpu6510_state_t cpu;
    ram_state_t ram;
    rom_state_t basic_rom;
    rom_state_t kernal_rom;
    rom_state_t char_rom;
    vic_state_t vic;
    cia_state_t cia1, cia2;
    sid_state_t sid;
} c64_state_t;

// ============================================================================
// C64 SYSTEM FUNCTIONS
// ============================================================================
void c64_init(c64_state_t* c64);
void c64_emulate_frame(c64_state_t* c64);

#endif // C64_H