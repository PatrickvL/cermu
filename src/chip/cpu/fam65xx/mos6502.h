#pragma once
/*
 * mos6502.h - C API Wrapper for MOS 6502 CPU
 *
 * This file provides C-compatible wrapper functions for the C++ template-based
 * CPU emulator implementation. Each supported processor gets its own set of
 * wrapper functions.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * - Pure C interface for maximum compatibility
 * - Opaque CPU handles (void pointers)
 * - Processor-specific function names
 * - Minimal API surface - only basic functions
 */

#include <stdbool.h>
#include <stdint.h>

#include "../../../core/chip.h"
#include "../../../core/system_lines.h"
#include "fam65xx_types.h"

// ============================================================================
// OPAQUE CPU HANDLE
// ============================================================================

typedef struct mos6502_t mos6502_t;

// ============================================================================
// MOS 6502 (Original NMOS) API
// ============================================================================

// Create/destroy CPU instance
mos6502_t *mos6502_create(void);
void mos6502_destroy(mos6502_t *cpu);

// Basic API functions
bus_state_t mos6502_init(mos6502_t *cpu);
bus_state_t mos6502_bootstrap(mos6502_t *cpu, bus_state_t pins);
bus_state_t mos6502_reset(mos6502_t *cpu, bus_state_t pins);
bus_state_t mos6502_tick(mos6502_t *cpu, bus_state_t pins);
bus_state_t mos6502_tick_phi2(mos6502_t *cpu, bus_state_t pins);
bus_state_t mos6502_tick_phi1(mos6502_t *cpu, bus_state_t pins);
bool mos6502_opdone(mos6502_t *cpu);

// Register access
uint8_t mos6502_get_a(mos6502_t *cpu);
uint8_t mos6502_get_x(mos6502_t *cpu);
uint8_t mos6502_get_y(mos6502_t *cpu);
uint8_t mos6502_get_s(mos6502_t *cpu);
uint8_t mos6502_get_p(mos6502_t *cpu);
uint16_t mos6502_get_pc(mos6502_t *cpu);

void mos6502_set_a(mos6502_t *cpu, uint8_t value);
void mos6502_set_x(mos6502_t *cpu, uint8_t value);
void mos6502_set_y(mos6502_t *cpu, uint8_t value);
void mos6502_set_s(mos6502_t *cpu, uint8_t value);
void mos6502_set_p(mos6502_t *cpu, uint8_t value);
void mos6502_set_pc(mos6502_t *cpu, uint16_t value);

#ifdef __cplusplus
class ChipBase;
ChipBase* mos6502_as_chip_base(mos6502_t *cpu);
#endif
