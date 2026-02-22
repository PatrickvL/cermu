#pragma once
/*
 * wdc65c02.h - C API Wrapper for WDC 65C02 CPU
 *
 * This file provides C-compatible wrapper functions for the WDC 65C02 CPU
 * which includes CMOS enhancements and bug fixes.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * - Pure C interface for maximum compatibility
 * - Opaque CPU handle (void pointer)
 * - Minimal API surface - only basic functions
 */

#include <stdbool.h>
#include <stdint.h>

#include "../../core/chip.h"
#include "../../core/system_lines.h"

// ============================================================================
// OPAQUE CPU HANDLE
// ============================================================================

typedef struct wdc65c02_t wdc65c02_t;

// ============================================================================
// WDC 65C02 API
// ============================================================================

// Create/destroy CPU instance
wdc65c02_t *wdc65c02_create(void);
void wdc65c02_destroy(wdc65c02_t *cpu);

// Basic API functions
bus_state_t wdc65c02_init(wdc65c02_t *cpu);
bus_state_t wdc65c02_reset(wdc65c02_t *cpu, bus_state_t pins);
bus_state_t wdc65c02_tick(wdc65c02_t *cpu, bus_state_t pins);
bool wdc65c02_opdone(wdc65c02_t *cpu);

// Register access
uint8_t wdc65c02_get_a(wdc65c02_t *cpu);
uint8_t wdc65c02_get_x(wdc65c02_t *cpu);
uint8_t wdc65c02_get_y(wdc65c02_t *cpu);
uint8_t wdc65c02_get_s(wdc65c02_t *cpu);
uint8_t wdc65c02_get_p(wdc65c02_t *cpu);
uint16_t wdc65c02_get_pc(wdc65c02_t *cpu);

void wdc65c02_set_a(wdc65c02_t *cpu, uint8_t value);
void wdc65c02_set_x(wdc65c02_t *cpu, uint8_t value);
void wdc65c02_set_y(wdc65c02_t *cpu, uint8_t value);
void wdc65c02_set_s(wdc65c02_t *cpu, uint8_t value);
void wdc65c02_set_p(wdc65c02_t *cpu, uint8_t value);
void wdc65c02_set_pc(wdc65c02_t *cpu, uint16_t value);
