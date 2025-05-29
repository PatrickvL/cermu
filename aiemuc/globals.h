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

#include "cpu6510.h"

extern cpu6510_state_t cpu;

#endif // GLOBALS_H