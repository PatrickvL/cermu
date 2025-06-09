#include "vicii_common.h"
#include "../../systems/c64/c64_bus.h"
#include <stdlib.h>
#include <string.h>

void* vicii_common_system_create(chip_descriptor_t* desc, void (*bank_change)(void*, uint8_t)) {
    vicii_common_t* vicii = (vicii_common_t*)calloc(1, sizeof(vicii_common_t));
    if (!vicii) return NULL;
    vicii->desc = desc;
    vicii->bank_change = bank_change;
    return vicii;
}

void vicii_common_system_destroy(void* chip) {
    free(chip);
}

void vicii_common_bus_attach(void* chip, void* bus) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    vicii->bus = bus;
}

uint8_t vicii_common_registers_read(void* chip, uint16_t address) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    uint8_t reg = address & 0x3F;
    if (reg <= 0x2E) {
        if (reg == 0x12) return (uint8_t)(vicii->raster_line & 0xFF);
        if (reg == 0x1E) {
            uint8_t val = vicii->collision_sprite;
            vicii->collision_sprite = 0;
            return val;
        }
        if (reg == 0x1F) {
            uint8_t val = vicii->collision_bg;
            vicii->collision_bg = 0;
            return val;
        }
        return vicii->registers[reg];
    }
    return 0;
}

void vicii_common_registers_write(void* chip, uint16_t address, uint8_t value) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    uint8_t reg = address & 0x3F;
    if (reg <= 0x2E) vicii->registers[reg] = value;
}

void vicii_common_bank_change(void* chip, uint8_t bank) {
    vicii_common_t* vicii = (vicii_common_t*)chip;
    vicii->bank = bank;
}

void vicii_common_cycle(vicii_common_t* vicii, uint8_t cycles_per_line, uint16_t total_lines) {
    // Raster timing advance
    vicii->raster_cycle++;
    if (vicii->raster_cycle >= cycles_per_line) {
        vicii->raster_cycle = 0;
        vicii->raster_line++;
        if (vicii->raster_line >= total_lines) vicii->raster_line = 0;
    }

    // Badline detection
    vicii->badline_condition = (vicii->raster_line >= 0x30 && vicii->raster_line <= 0xF7) &&
                               ((vicii->raster_line & 7) == (vicii->registers[0x11] & 7));

    // BA signal for CPU stall
    if (vicii->badline_condition && vicii->raster_cycle >= 15 && vicii->raster_cycle <= 54) {
        ((c64_bus_t*)vicii->bus)->control_lines &= ~BA_LINE;
    } else {
        ((c64_bus_t*)vicii->bus)->control_lines |= BA_LINE;
    }

    // AEC follows BA with one-cycle delay
    if (vicii->prev_ba && !(((c64_bus_t*)vicii->bus)->control_lines & BA_LINE)) {
        ((c64_bus_t*)vicii->bus)->control_lines &= ~AEC_LINE;
    } else if (!vicii->prev_ba && (((c64_bus_t*)vicii->bus)->control_lines & BA_LINE)) {
        ((c64_bus_t*)vicii->bus)->control_lines |= AEC_LINE;
    }
    vicii->prev_ba = (((c64_bus_t*)vicii->bus)->control_lines & BA_LINE) != 0;
}