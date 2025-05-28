#ifndef GLOBALS_H
#define GLOBALS_H

// Include device headers to get type definitions
#include "ram.h"
#include "rom.h"
#include "vic.h"
#include "cia.h"
#include "sid.h"

// ============================================================================
// GLOBAL DEVICE INSTANCES - All declared in c64.c
// This header centralizes all device extern declarations to avoid circular dependencies
// ============================================================================

extern ram_state_t ram;
extern rom_state_t basic_rom;
extern rom_state_t kernal_rom;
extern rom_state_t char_rom;
extern vic_state_t vic;
extern cia_state_t cia1, cia2;
extern sid_state_t sid;

#endif // GLOBALS_H