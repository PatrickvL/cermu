#include "vic.h" // TODO : Rename to vic_ii.c or mos????.[hc]
#include "c64_bus.h"
#include <string.h>

void vicii_system_destroy(void* device) {
    free(device);
}

void* vicii_system_create(device_descriptor_t* desc) {
    vic_ii_t* vic = (vic_ii_t*)calloc(1, sizeof(vic_ii_t));
    if (!vic) return NULL;
    vic->desc = desc;
    vic->bank_change = vicii_bank_change;
    return vic;
}

void* vicii_bus_attach(void* device, c64_bus_t* bus) {
    vic_ii_t* vic = (vic_ii_t*)device;
    vic->bus = bus;
}

uint8_t vicii_registers_read(void* device, uint16_t address) {
    vic_ii_t* vic = (vic_ii_t*)device;
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
    vic_ii_t* vic = (vic_ii_t*)context;
    uint8_t reg = address & 0x3F;
    if (reg <= 0x2E) vic->registers[reg] = value;
}

void vicii_bank_change(void* context, uint8_t bank) {
    vic_ii_t* vic = (vic_ii_t*)context;
    vic->bank = bank;
}

static device_descriptor_t vic_ii_descriptor = {
    .create = vicii_system_create,
    .destroy = vicii_system_destroy,
    .bus_attach = vicii_bus_attach,
    .read = vicii_registers_read,
    .write = vicii_registers_write,
    .bank_change = vicii_bank_change
};

static void vic_ii_cycle(vic_ii_t* vic_ii) {
    // Always increment raster timing
    vic_ii->raster_cycle++;
    if (vic_ii->raster_cycle >= 63) {
        vic_ii->raster_cycle = 0;
        vic_ii->raster_line++;
        if (vic_ii->raster_line >= 312) vic_ii->raster_line = 0;
    }
    
    // Check for badline condition (hardware accurate)
    vic_ii->badline_condition = (vic_ii->raster_line >= 0x30 && vic_ii->raster_line <= 0xF7) &&
                                ((vic_ii->raster_line & 7) == (vic_ii->registers[0x11] & 7));
    
    // Generate BA signal for badline
    if (vic_ii->badline_condition && vic_ii->raster_cycle >= 15 && vic_ii->raster_cycle <= 54) {
        vic_ii->bus->control_lines &= ~BA_LINE; // Pull BA low - CPU will stall
    } else {
        vic_ii->bus->control_lines |= BA_LINE;  // Release BA
    }
    
    // AEC follows BA with one cycle delay (hardware accurate)
    if (vic_ii->prev_ba && !(vic_ii->bus->control_lines & BA_LINE)) {
        vic_ii->bus->control_lines &= ~AEC_LINE;
    } else if (!vic_ii->prev_ba && (vic_ii->bus->control_lines & BA_LINE)) {
        vic_ii->bus->control_lines |= AEC_LINE;
    }
    vic_ii->prev_ba = (vic_ii->bus->control_lines & BA_LINE) != 0;
}
