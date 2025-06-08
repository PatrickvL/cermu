#include "mos2114.h"
#include <stdlib.h>
#include <string.h>

void* mos2114_create(chip_descriptor_t* desc) {
    mos2114_t* mos2114 = (mos2114_t*)calloc(1, sizeof(mos2114_t));
    if (!mos2114) return NULL;
    mos2114->desc = desc;
    // Initialize all memory to 0 (which is also the default color)
    memset(mos2114->memory, 0, sizeof(mos2114->memory));
    return mos2114;
}

void mos2114_destroy(void* chip) {
    free(chip);
}

// Initialize MOS2114 with chip entry information
void mos2114_memory_init(void* chip, chip_entry_t* chip_entry) {
    mos2114_t* mos2114 = (mos2114_t*)chip;
    if (!mos2114 || !chip_entry) return;
    
    // Set rwcb_context to memory buffer for direct memory access
    chip_entry->rwcb_context = mos2114->memory;
}

uint8_t mos2114_read(void* context, uint16_t address) {
    uint8_t* memory = (uint8_t*)context;
    // Address routing handled by bus system, no mask needed
    return memory[address];
}

void mos2114_write(void* context, uint16_t address, uint8_t value) {
    uint8_t* memory = (uint8_t*)context;
    // MOS2114 is 4-bit wide, so only store lower 4 bits
    // Address routing handled by bus system, no mask needed
    memory[address] = value & 0x0F;
}

chip_descriptor_t mos2114_descriptor = {
    .create = mos2114_create,
    .destroy = mos2114_destroy,
    .bus_attach = NULL,
    .read = mos2114_read,
    .write = mos2114_write,
    .bank_change = NULL
};