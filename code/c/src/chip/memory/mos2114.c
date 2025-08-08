#include "mos2114.h"
#include <stdlib.h>
#include <string.h>

void* mos2114_create(chip_descriptor_t* desc) {
    mos2114_t* mos2114 = (mos2114_t*)calloc(1, sizeof(mos2114_t));
    if (!mos2114) return NULL;
    mos2114->desc = desc;
    
    // Allocate 1KB memory for Color RAM (MOS2114 1K x 4-bit)
    mos2114->memory = (uint8_t*)calloc(1024, sizeof(uint8_t));
    if (!mos2114->memory) {
        free(mos2114);
        return NULL;
    }
    return mos2114;
}

void mos2114_destroy(void* chip) {
    if (!chip) return;
    mos2114_t* mos2114 = (mos2114_t*)chip;
    
    // Free the allocated Color RAM memory
    if (mos2114->memory) {
        free(mos2114->memory);
        mos2114->memory = NULL;
    }
    
    free(mos2114);
}

// MOS2114 read function - bus state interface
bus_state_t mos2114_read(void* context, bus_state_t bus_state) {
    mos2114_t* mos2114 = (mos2114_t*)context;
    // Color RAM is mapped at $D800-$DBFF (1024 bytes)
    // Mask to 10 bits for 1K addressing
    uint16_t offset = bus_state.addr & 0x3FF;  // 0x3FF = 1023, ensures we stay within bounds
    
    // MOS2114 is 4-bit wide - only lower 4 bits are valid (and written by mos2114_write)
    // Upper 4 bits return undefined/floating values (use previous bus data)
    uint8_t color_nibble = mos2114->memory[offset]; // No need to mask, already 4 bits
    uint8_t floating_upper_bits = bus_state.data & 0xF0;  // Keep upper bits from bus
    bus_state.data = floating_upper_bits | color_nibble;
    return bus_state;
}

// MOS2114 write function - bus state interface
bus_state_t mos2114_write(void* context, bus_state_t bus_state) {
    mos2114_t* mos2114 = (mos2114_t*)context;
    // MOS2114 is 4-bit wide, so only store lower 4 bits
    uint16_t offset = bus_state.addr & 0x3FF;  // Mask to 1K boundary
    mos2114->memory[offset] = bus_state.data & 0x0F;
    return bus_state;
}

chip_descriptor_t mos2114_descriptor = {
    .description = "MOS2114 Color RAM (1K x 4-bit)",
    .create = mos2114_create,
    .destroy = mos2114_destroy,
    .bus_attach = NULL,
    .read = mos2114_read,
    .write = mos2114_write,
    .bank_change = NULL
};