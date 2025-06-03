#include "vic.h" // TODO : Rename to mos6581.c or mos????.[hc]
#include "c64_bus.h"
#include <string.h>
#include <stdlib.h>

// Forward declarations
void vicii_bank_change(void* context, uint8_t bank);

void vicii_system_destroy(void* device) {
    free(device);
}

void* vicii_system_create(device_descriptor_t* desc) {
    mos6581_t* vic = (mos6581_t*)calloc(1, sizeof(mos6581_t));
    if (!vic) return NULL;
    vic->desc = desc;
    vic->bank_change = vicii_bank_change;
    return vic;
}

void vicii_bus_attach(void* device, c64_bus_t* bus) {
    mos6581_t* vic = (mos6581_t*)device;
    vic->bus = bus;
}

uint8_t vicii_registers_read(void* device, uint16_t address) {
    mos6581_t* vic = (mos6581_t*)device;
    uint8_t reg = address & 0x3F;
    if (reg <= 0x2E) {
        if (reg == 0x12) return vic->raster_line;
        if (reg == 0x1E) {
            uint8_t val = vic->collision_sprite;
            vic->collision_sprite = 0;
            return val;
        }
        if (reg == 0x1F) {
            uint8_t val = vic->collision_bg;
            vic->collision_bg = 0;
            return val;
        }
        return vic->registers[reg];
    }
    return 0;
}

void vicii_registers_write(void* context, uint16_t address, uint8_t value) {
    mos6581_t* vic = (mos6581_t*)context;
    uint8_t reg = address & 0x3F;
    if (reg <= 0x2E) vic->registers[reg] = value;
}

void vicii_bank_change(void* context, uint8_t bank) {
    mos6581_t* vic = (mos6581_t*)context;
    vic->bank = bank;
}

device_descriptor_t mos6581_descriptor = {
    .create = vicii_system_create,
    .destroy = vicii_system_destroy,
    .bus_attach = vicii_bus_attach,
    .read = vicii_registers_read,
    .write = vicii_registers_write,
    .bank_change = vicii_bank_change
};

static void mos6581_cycle(mos6581_t* mos6581) {
    // Always increment raster timing
    mos6581->raster_cycle++;
    if (mos6581->raster_cycle >= 63) {
        mos6581->raster_cycle = 0;
        mos6581->raster_line++;
        if (mos6581->raster_line >= 312) mos6581->raster_line = 0;
    }
    
    // Check for badline condition (hardware accurate)
    mos6581->badline_condition = (mos6581->raster_line >= 0x30 && mos6581->raster_line <= 0xF7) &&
                                ((mos6581->raster_line & 7) == (mos6581->registers[0x11] & 7));
    
    // Generate BA signal for badline
    if (mos6581->badline_condition && mos6581->raster_cycle >= 15 && mos6581->raster_cycle <= 54) {
        mos6581->bus->control_lines &= ~BA_LINE; // Pull BA low - CPU will stall
    } else {
        mos6581->bus->control_lines |= BA_LINE;  // Release BA
    }
    
    // AEC follows BA with one cycle delay (hardware accurate)
    if (mos6581->prev_ba && !(mos6581->bus->control_lines & BA_LINE)) {
        mos6581->bus->control_lines &= ~AEC_LINE;
    } else if (!mos6581->prev_ba && (mos6581->bus->control_lines & BA_LINE)) {
        mos6581->bus->control_lines |= AEC_LINE;
    }
    mos6581->prev_ba = (mos6581->bus->control_lines & BA_LINE) != 0;
}
