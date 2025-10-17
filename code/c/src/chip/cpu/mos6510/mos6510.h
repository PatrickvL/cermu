#pragma once
/*
 * mos6510.h - MOS 6510 CPU C Compatibility Header
 * 
 * This is a compatibility wrapper that redirects to the unified
 * MOS 65xx family implementation.
 */

// Redirect to the unified implementation
#include "../fam65xx/mos6510.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// Re-export the chip functions for compatibility
#define mos6510_chip_create mos6510_chip_create_impl

#ifdef __cplusplus
}
#endif