#pragma once
/*
 * wdc65c816.h - C API Wrapper for WDC 65C816 CPU
 *
 * This file provides C-compatible wrapper functions for the WDC 65C816 CPU
 * which includes 16-bit enhancements and expanded address space.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * - Pure C interface for maximum compatibility
 * - Opaque CPU handle (void pointer)
 * - Minimal API surface - only basic functions
 * - 16-bit register access for extended features
 */

#include <stdbool.h>
#include <stdint.h>

#include "../../core/chip.h"
#include "../../core/system_lines.h"

// ============================================================================
// OPAQUE CPU HANDLE
// ============================================================================

typedef struct wdc65c816_t wdc65c816_t;

// ============================================================================
// WDC 65C816 API
// ============================================================================

// Create/destroy CPU instance
wdc65c816_t *wdc65c816_create(void);
void wdc65c816_destroy(wdc65c816_t *cpu);

// Basic API functions
bus_state_t wdc65c816_init(wdc65c816_t *cpu);
bus_state_t wdc65c816_reset(wdc65c816_t *cpu, bus_state_t pins);
bus_state_t wdc65c816_tick(wdc65c816_t *cpu, bus_state_t pins);
bool wdc65c816_opdone(wdc65c816_t *cpu);

// 8-bit register access (compatible mode)
uint8_t wdc65c816_get_a(wdc65c816_t *cpu);
uint8_t wdc65c816_get_x(wdc65c816_t *cpu);
uint8_t wdc65c816_get_y(wdc65c816_t *cpu);
uint8_t wdc65c816_get_s(wdc65c816_t *cpu);
uint8_t wdc65c816_get_p(wdc65c816_t *cpu);
uint16_t wdc65c816_get_pc(wdc65c816_t *cpu);

void wdc65c816_set_a(wdc65c816_t *cpu, uint8_t value);
void wdc65c816_set_x(wdc65c816_t *cpu, uint8_t value);
void wdc65c816_set_y(wdc65c816_t *cpu, uint8_t value);
void wdc65c816_set_s(wdc65c816_t *cpu, uint8_t value);
void wdc65c816_set_p(wdc65c816_t *cpu, uint8_t value);
void wdc65c816_set_pc(wdc65c816_t *cpu, uint16_t value);

// 16-bit register access (65C816-specific)
uint16_t wdc65c816_get_a_full(wdc65c816_t *cpu);
uint16_t wdc65c816_get_x_full(wdc65c816_t *cpu);
uint16_t wdc65c816_get_y_full(wdc65c816_t *cpu);
uint16_t wdc65c816_get_d(wdc65c816_t *cpu);
uint8_t wdc65c816_get_dbr(wdc65c816_t *cpu);
uint8_t wdc65c816_get_pbr(wdc65c816_t *cpu);
bool wdc65c816_get_emulation_mode(wdc65c816_t *cpu);

void wdc65c816_set_a_full(wdc65c816_t *cpu, uint16_t value);
void wdc65c816_set_x_full(wdc65c816_t *cpu, uint16_t value);
void wdc65c816_set_y_full(wdc65c816_t *cpu, uint16_t value);
void wdc65c816_set_d(wdc65c816_t *cpu, uint16_t value);
void wdc65c816_set_dbr(wdc65c816_t *cpu, uint8_t value);
void wdc65c816_set_pbr(wdc65c816_t *cpu, uint8_t value);
void wdc65c816_set_emulation_mode(wdc65c816_t *cpu, bool emulation);
