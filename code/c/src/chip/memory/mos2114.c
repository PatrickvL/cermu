#include "mos2114.h"
#include <stdlib.h>
#include <string.h>

void* mos2114_create(chip_descriptor_t* desc) {
    mos2114_t* mos2114 = (mos2114_t*)calloc(1, sizeof(mos2114_t));
    if (!mos2114) return NULL;
    mos2114->desc = desc;
    // Note: memory pointer will be set later to point into unified buffer
    mos2114->memory = NULL;
    return mos2114;
}

void mos2114_destroy(void* chip) {
    free(chip);
}

uint8_t mos2114_read(void* chip, uint16_t address) {
    mos2114_t* mos2114 = (mos2114_t*)chip;
    // Color RAM is mapped at $D800-$DBFF (1024 bytes)
    // Mask to 10 bits for 1K addressing
    uint16_t offset = address & 0x3FF;  // 0x3FF = 1023, ensures we stay within bounds
    return mos2114->memory[offset];
}

void mos2114_write(void* chip, uint16_t address, uint8_t value) {
    mos2114_t* mos2114 = (mos2114_t*)chip;
    // MOS2114 is 4-bit wide, so only store lower 4 bits
    uint16_t offset = address & 0x3FF;  // Mask to 1K boundary
    mos2114->memory[offset] = value & 0x0F;
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