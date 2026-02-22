#pragma once
/*
 * rockwell65c02.h - C API Wrapper for Rockwell 65C02 CPU
 *
 * This file provides C-compatible wrapper functions for the Rockwell 65C02 CPU
 * which includes CMOS enhancements and bit manipulation instructions.
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

typedef struct rockwell65c02_t rockwell65c02_t;

// ============================================================================
// ROCKWELL 65C02 API
// ============================================================================

// Create/destroy CPU instance
rockwell65c02_t *rockwell65c02_create(void);
void rockwell65c02_destroy(rockwell65c02_t *cpu);

// Basic API functions
bus_state_t rockwell65c02_init(rockwell65c02_t *cpu);
bus_state_t rockwell65c02_reset(rockwell65c02_t *cpu, bus_state_t pins);
bus_state_t rockwell65c02_tick(rockwell65c02_t *cpu, bus_state_t pins);
bool rockwell65c02_opdone(rockwell65c02_t *cpu);

// Register access
uint8_t rockwell65c02_get_a(rockwell65c02_t *cpu);
uint8_t rockwell65c02_get_x(rockwell65c02_t *cpu);
uint8_t rockwell65c02_get_y(rockwell65c02_t *cpu);
uint8_t rockwell65c02_get_s(rockwell65c02_t *cpu);
uint8_t rockwell65c02_get_p(rockwell65c02_t *cpu);
uint16_t rockwell65c02_get_pc(rockwell65c02_t *cpu);

void rockwell65c02_set_a(rockwell65c02_t *cpu, uint8_t value);
void rockwell65c02_set_x(rockwell65c02_t *cpu, uint8_t value);
void rockwell65c02_set_y(rockwell65c02_t *cpu, uint8_t value);
void rockwell65c02_set_s(rockwell65c02_t *cpu, uint8_t value);
void rockwell65c02_set_p(rockwell65c02_t *cpu, uint8_t value);
void rockwell65c02_set_pc(rockwell65c02_t *cpu, uint16_t value);
