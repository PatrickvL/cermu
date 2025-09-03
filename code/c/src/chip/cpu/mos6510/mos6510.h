#ifndef MOS6510_H
#define MOS6510_H

#include "../../../core/system_lines.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// MOS6510 CPU chip - zero overhead C interface
typedef struct mos6510_chip mos6510_chip_t;

// Create/destroy CPU instance
mos6510_chip_t* mos6510_create(void);
void mos6510_destroy(mos6510_chip_t* cpu);

// Initialize CPU with optional IO callback
void mos6510_init(mos6510_chip_t* cpu, 
                  void (*io_callback)(uint16_t addr, uint8_t data, bool write));

// Main CPU tick - direct bus interface
bus_state_t mos6510_tick(mos6510_chip_t* cpu, bus_state_t bus_state);

// Debug interface
uint16_t mos6510_get_pc(mos6510_chip_t* cpu);
uint8_t mos6510_get_a(mos6510_chip_t* cpu);
uint8_t mos6510_get_x(mos6510_chip_t* cpu);
uint8_t mos6510_get_y(mos6510_chip_t* cpu);
uint8_t mos6510_get_s(mos6510_chip_t* cpu);
uint8_t mos6510_get_p(mos6510_chip_t* cpu);

// System integration functions
bus_state_t mos6510_tick_chip(void* chip, bus_state_t bus_state);

// Forward declare chip descriptor structure
struct chip_descriptor_s;

// Chip descriptor for system registration
extern struct chip_descriptor_s mos6510_descriptor;

#ifdef __cplusplus
}
#endif

#endif // MOS6510_H