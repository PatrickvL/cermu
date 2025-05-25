#ifndef MEMORY_H
#define MEMORY_H

#include "c64.h"

// ============================================================================
// MEMORY - 64K RAM + ROM images
// ============================================================================
extern uint8_t ram[65536];
extern uint8_t kernal_rom[8192];
extern uint8_t basic_rom[8192];  
extern uint8_t char_rom[4096];

// ============================================================================
// PLA EMULATION - Pre-computed chip select maps for each memory mode
// ============================================================================

// 32 possible PLA modes, 256 memory blocks each (256-byte granularity for CIA compatibility)
extern uint8_t chip_select_maps[32][256];
extern uint8_t* chip_select_map;

extern uintptr_t mode_read_map[32][256];
extern uintptr_t* read_map;

// PLA functions
void switch_cpu_mode(uint8_t mode);
void generate_pla_maps(void);

// Memory access functions
void cpu_read_cycle(uint16_t addr);
void cpu_write_cycle(uint16_t addr, uint8_t value);

#endif // MEMORY_H