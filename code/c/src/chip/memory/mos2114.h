#ifndef MOS2114_H
#define MOS2114_H

#include <stdint.h>
#include "../../core/system.h"

// MOS Technology 2114 Static RAM - 1K x 4-bit
// Used as ColorRAM in C64 at 0xD800-0xDBFF
// Only the lower 4 bits are used (color information)
// https://www.amiga-stuff.com/hardware/1kx4-sram.html
//
// HARDWARE CONNECTION: PLA _GRW Signal Control
// ============================================
// In C64 hardware, this Color RAM chip's #WE (Write Enable) pin is connected
// to the PLA's _GRW output signal. The PLA controls when Color RAM writes are
// allowed based on memory configuration and address decoding.
//
// _GRW Signal gates Color RAM writes when:
// - I/O region is disabled (Character ROM visible instead)
// - Address is outside Color RAM range ($D800-$DBFF)
// - CPU is reading (not writing)
// - Memory banking prevents I/O access
//
// This prevents Color RAM corruption during memory bank switching and ensures
// hardware-accurate behavior matching real C64 systems.
typedef struct mos2114_s {
    chip_descriptor_t* desc;
    uint8_t* memory;  // Pointer to allocated 1KB Color RAM memory
} mos2114_t;

// Chip descriptor
extern chip_descriptor_t mos2114_descriptor;

// Creation and destruction functions
void* mos2114_create(chip_descriptor_t* desc);
void mos2114_destroy(void* chip);

// Read/Write functions - bus state interface
bus_state_t mos2114_read(void* context, bus_state_t bus_state);
bus_state_t mos2114_write(void* context, bus_state_t bus_state);

#endif // MOS2114_H