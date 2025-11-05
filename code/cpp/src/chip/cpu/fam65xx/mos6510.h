#pragma once
/*
 * mos6510.h - C API Wrapper for MOS 6510 CPU
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

#include <stdint.h>
#include <stdbool.h>

#include "../../core/chip.h"
#include "../../core/system_lines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mos6510_t mos6510_t;

// ============================================================================
// MOS 6510-SPECIFIC DESCRIPTOR (extends chip_descriptor_t)
// ============================================================================

// MOS 6510 descriptor that wraps chip_descriptor_t with I/O port specific fields
typedef struct {
    chip_descriptor_t base;         // Base chip descriptor
    
    // 6510-specific I/O port callbacks
    uint8_t (*m6510_in_cb)(void* user_data);    // Read from external I/O pins
    void (*m6510_out_cb)(uint8_t data, void* user_data);  // Write to external I/O pins
    uint8_t m6510_io_pullup;        // Pull-up resistor configuration
    uint8_t m6510_io_floating;      // Floating pin configuration  
    void* m6510_user_data;          // User data for I/O callbacks
} mos6510_desc_t;

// ============================================================================
// MOS 6510 (C64/C128) API
// ============================================================================

mos6510_t* mos6510_create(void);
void mos6510_destroy(mos6510_t* cpu);

bus_state_t mos6510_init(mos6510_t* cpu, const mos6510_desc_t* desc);
bus_state_t mos6510_reset(mos6510_t* cpu, bus_state_t pins);
bus_state_t mos6510_tick(mos6510_t* cpu, bus_state_t pins);
bool mos6510_opdone(mos6510_t* cpu);

// Register access (same as 6502)
uint8_t mos6510_get_a(mos6510_t* cpu);
uint8_t mos6510_get_x(mos6510_t* cpu);
uint8_t mos6510_get_y(mos6510_t* cpu);
uint8_t mos6510_get_s(mos6510_t* cpu);
uint8_t mos6510_get_p(mos6510_t* cpu);
uint16_t mos6510_get_pc(mos6510_t* cpu);

void mos6510_set_a(mos6510_t* cpu, uint8_t value);
void mos6510_set_x(mos6510_t* cpu, uint8_t value);
void mos6510_set_y(mos6510_t* cpu, uint8_t value);
void mos6510_set_s(mos6510_t* cpu, uint8_t value);
void mos6510_set_p(mos6510_t* cpu, uint8_t value);
void mos6510_set_pc(mos6510_t* cpu, uint16_t value);

// I/O Port access (6510-specific)
uint8_t mos6510_get_io_ddr(mos6510_t* cpu);
uint8_t mos6510_get_io_data(mos6510_t* cpu);
uint8_t mos6510_get_io_input(mos6510_t* cpu);
void mos6510_set_io_input(mos6510_t* cpu, uint8_t value);

// Global descriptor for chip registration
extern chip_descriptor_t mos6510_descriptor;

// Chip-compatible tick function
bus_state_t mos6510_tick_chip(void* cpu, bus_state_t pins);

#ifdef __cplusplus
}
#endif