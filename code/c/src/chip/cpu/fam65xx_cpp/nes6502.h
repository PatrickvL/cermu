#pragma once
/*
 * nes6502.h - C API Wrapper for NES 6502 CPU
 *
 * This file provides C-compatible wrapper functions for the NES 6502 CPU
 * variant which lacks BCD support and some illegal opcodes.
 *
 * DESIGN PRINCIPLES:
 * ==================
 * - Pure C interface for maximum compatibility
 * - Opaque CPU handle (void pointer)
 * - Minimal API surface - only basic functions
 */

#include <stdint.h>
#include <stdbool.h>

#include "../../core/chip.h"
#include "../../core/system_lines.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// OPAQUE CPU HANDLE
// ============================================================================

typedef struct nes6502_t nes6502_t;

// ============================================================================
// NES 6502 API
// ============================================================================

// Create/destroy CPU instance
nes6502_t* nes6502_create(void);
void nes6502_destroy(nes6502_t* cpu);

// Basic API functions
bus_state_t nes6502_init(nes6502_t* cpu, const chip_descriptor_t* desc);
bus_state_t nes6502_reset(nes6502_t* cpu, bus_state_t pins);
bus_state_t nes6502_tick(nes6502_t* cpu, bus_state_t pins);
bool nes6502_opdone(nes6502_t* cpu);

// Register access
uint8_t nes6502_get_a(nes6502_t* cpu);
uint8_t nes6502_get_x(nes6502_t* cpu);
uint8_t nes6502_get_y(nes6502_t* cpu);
uint8_t nes6502_get_s(nes6502_t* cpu);
uint8_t nes6502_get_p(nes6502_t* cpu);
uint16_t nes6502_get_pc(nes6502_t* cpu);

void nes6502_set_a(nes6502_t* cpu, uint8_t value);
void nes6502_set_x(nes6502_t* cpu, uint8_t value);
void nes6502_set_y(nes6502_t* cpu, uint8_t value);
void nes6502_set_s(nes6502_t* cpu, uint8_t value);
void nes6502_set_p(nes6502_t* cpu, uint8_t value);
void nes6502_set_pc(nes6502_t* cpu, uint16_t value);

// APU functions (only available when APU is enabled)
float nes6502_generate_audio_sample(nes6502_t* cpu);
bool nes6502_apu_needs_dma(nes6502_t* cpu);
uint16_t nes6502_apu_dma_address(nes6502_t* cpu);
void nes6502_apu_load_dma_sample(nes6502_t* cpu, uint8_t data);
bool nes6502_apu_irq(nes6502_t* cpu);
void nes6502_set_apu_region(nes6502_t* cpu, bool is_pal);

#ifdef __cplusplus
}
#endif