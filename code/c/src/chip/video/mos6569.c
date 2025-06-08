#include "mos6569.h" // vicii
#include "../../systems/c64/c64_bus.h"
#include <string.h>
#include <stdlib.h>

// Forward declarations
void mos6569_bank_change(void* context, uint8_t bank);

void mos6569_system_destroy(void* chip) {
    free(chip);
}

void* mos6569_system_create(chip_descriptor_t* desc) {
    mos6569_t* vicii = (mos6569_t*)calloc(1, sizeof(mos6569_t));
    if (!vicii) return NULL;
    vicii->desc = desc;
    vicii->bank_change = mos6569_bank_change;
    return vicii;
}

void mos6569_bus_attach(void* chip, void* bus) {
    mos6569_t* vicii = (mos6569_t*)chip;
    c64_bus_t* c64_bus = (c64_bus_t*)bus;
    vicii->bus = c64_bus;
}

uint8_t mos6569_registers_read(void* chip, uint16_t address) {
    mos6569_t* vicii = (mos6569_t*)chip;
    // VIC-II has 64 registers that mirror throughout its address space
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

void mos6569_registers_write(void* context, uint16_t address, uint8_t value) {
    mos6569_t* vicii = (mos6569_t*)context;
    // VIC-II has 64 registers that mirror throughout its address space
    uint8_t reg = address & 0x3F;
    if (reg <= 0x2E) vicii->registers[reg] = value;
}

void mos6569_bank_change(void* context, uint8_t bank) {
    mos6569_t* vicii = (mos6569_t*)context;
    vicii->bank = bank;
}

chip_descriptor_t mos6569_descriptor = {
    .create = mos6569_system_create,
    .destroy = mos6569_system_destroy,
    .bus_attach = mos6569_bus_attach,
    .read = mos6569_registers_read,
    .write = mos6569_registers_write,
    .bank_change = mos6569_bank_change
};

void mos6569_cycle(mos6569_t* vicii) {
    // Always increment raster timing
    vicii->raster_cycle++;
    if (vicii->raster_cycle >= 63) {
        vicii->raster_cycle = 0;
        vicii->raster_line++;
        if (vicii->raster_line >= 312) vicii->raster_line = 0;
    }
    
    // Check for badline condition (hardware accurate)
    vicii->badline_condition = (vicii->raster_line >= 0x30 && vicii->raster_line <= 0xF7) &&
                                ((vicii->raster_line & 7) == (vicii->registers[0x11] & 7));
    
    // Generate BA signal for badline
    if (vicii->badline_condition && vicii->raster_cycle >= 15 && vicii->raster_cycle <= 54) {
        ((c64_bus_t*)vicii->bus)->control_lines &= ~BA_LINE; // Pull BA low - CPU will stall
    } else {
        ((c64_bus_t*)vicii->bus)->control_lines |= BA_LINE;  // Release BA
    }
    
    // AEC follows BA with one cycle delay (hardware accurate)
    if (vicii->prev_ba && !(((c64_bus_t*)vicii->bus)->control_lines & BA_LINE)) {
        ((c64_bus_t*)vicii->bus)->control_lines &= ~AEC_LINE;
    } else if (!vicii->prev_ba && (((c64_bus_t*)vicii->bus)->control_lines & BA_LINE)) {
        ((c64_bus_t*)vicii->bus)->control_lines |= AEC_LINE;
    }
    vicii->prev_ba = (((c64_bus_t*)vicii->bus)->control_lines & BA_LINE) != 0;
}
