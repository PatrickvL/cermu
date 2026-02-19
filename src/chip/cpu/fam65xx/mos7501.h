#pragma once
/*
 * mos7501.h - C API Wrapper for MOS 7501/8501 CPU
 *
 * The MOS 7501 (also known as CSG 7501) is the CPU used in the Commodore 16
 * and Plus/4 computers. It is essentially the same NMOS 6502 core as the
 * MOS 6510 but with key differences:
 *
 *   - NO NMI line (directly connected to VCC on the C16/Plus4 hardware)
 *   - Different I/O port pin mask: 0x5F (pins 0-4 and 6; pin 5 absent)
 *     Port bit 0: Cassette motor control
 *     Port bit 1: Serial bus SRQ IN
 *     Port bit 2: Serial bus data
 *     Port bit 3: Serial bus clock
 *     Port bit 4: Serial bus ATN
 *     Port bit 6: Cassette sense
 *
 * The 8501 is the same die in a later package revision (functionally identical).
 *
 * API follows the same pattern as mos6510.h.
 */

#include <stdbool.h>
#include <stdint.h>

#include "../../core/chip.h"
#include "../../core/system_lines.h"

typedef struct mos7501_t mos7501_t;

// ============================================================================
// MOS 7501-SPECIFIC DESCRIPTOR (extends chip_descriptor_t)
// ============================================================================

typedef struct {
  chip_descriptor_t base; // Base chip descriptor

  // 7501-specific I/O port callbacks (same structure as 6510)
  uint8_t (*m7501_in_cb)(void *user_data);   // Read from external I/O pins
  void (*m7501_out_cb)(uint8_t data,
                       void *user_data);      // Write to external I/O pins
  uint8_t m7501_io_pullup;                    // Pull-up resistor configuration
  uint8_t m7501_io_floating;                  // Floating pin configuration
  void *m7501_user_data;                      // User data for I/O callbacks
} mos7501_desc_t;

// ============================================================================
// MOS 7501 (C16/Plus4) API
// ============================================================================

mos7501_t *mos7501_create(void);
void mos7501_destroy(mos7501_t *cpu);

bus_state_t mos7501_init(mos7501_t *cpu, const mos7501_desc_t *desc);
bus_state_t mos7501_reset(mos7501_t *cpu, bus_state_t pins);
bus_state_t mos7501_bootstrap(mos7501_t *cpu, bus_state_t pins);
bool mos7501_opdone(mos7501_t *cpu);

// Register access
uint8_t mos7501_get_a(mos7501_t *cpu);
uint8_t mos7501_get_x(mos7501_t *cpu);
uint8_t mos7501_get_y(mos7501_t *cpu);
uint8_t mos7501_get_s(mos7501_t *cpu);
uint8_t mos7501_get_p(mos7501_t *cpu);
uint16_t mos7501_get_pc(mos7501_t *cpu);
uint8_t mos7501_get_ir(mos7501_t *cpu);

// Get current opcode entry (for disassembly)
#ifdef __cplusplus
#include "fam65xx_types.h"
opcode_info_t mos7501_get_opcode_entry(mos7501_t *cpu);
opcode_info_t mos7501_lookup_opcode(uint8_t opcode);
#endif

void mos7501_set_a(mos7501_t *cpu, uint8_t value);
void mos7501_set_x(mos7501_t *cpu, uint8_t value);
void mos7501_set_y(mos7501_t *cpu, uint8_t value);
void mos7501_set_s(mos7501_t *cpu, uint8_t value);
void mos7501_set_p(mos7501_t *cpu, uint8_t value);
void mos7501_set_pc(mos7501_t *cpu, uint16_t value);
void mos7501_set_ab(mos7501_t *cpu, uint16_t value);

// Reset CPU instruction pipeline to fetch state
void mos7501_transition_to_fetch(mos7501_t *cpu);

// I/O Port access (7501-specific, same as 6510 but different pin mask)
uint8_t mos7501_get_io_ddr(mos7501_t *cpu);
uint8_t mos7501_get_io_data(mos7501_t *cpu);
uint8_t mos7501_get_io_input(mos7501_t *cpu);
void mos7501_set_io_input(mos7501_t *cpu, uint8_t value);

// Set the context pointer passed to the bank_change callback.
void mos7501_set_bank_change_context(mos7501_t *cpu, void* context);

// Global descriptor for chip registration
extern chip_descriptor_t mos7501_descriptor;

// Chip-compatible tick function
bus_state_t mos7501_tick_phi2(void *cpu, bus_state_t pins);
bus_state_t mos7501_tick_phi1(void *cpu, bus_state_t pins);
