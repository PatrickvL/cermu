#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "aiemuc.h"
#include "bus.h"
#include "device.h"
#include "system.h"
#include "c64_bus.h"
#include "mos6510.h"
#include "cia.h"
#include "vic.h"
#include "sid.h"
#include "ram.h"
#include "rom.h"
#include "c64.h"

/*/ Types
typedef struct c64_state c64_state_t;
typedef struct system_8bit system_8bit_t;
typedef struct c64_bus c64_bus_t;
typedef struct mos6510 mos6510_t;
typedef struct cia cia_t;
typedef struct vic_ii vic_ii_t;
typedef struct sid sid_t;
typedef struct custom custom_t;
typedef struct ram ram_t;
typedef struct rom rom_t;
*/
// TODO : Move to c64_custom.c

void* custom_system_create(void* bus) {
    custom_t* custom = (custom_t*)calloc(1, sizeof(custom_t));
    if (!custom) return NULL;
    custom->desc = &custom_descriptor;
    custom->bus = (bus_interface_t*)bus;
    return custom;
}

void custom_system_destroy(void* context) {
    free(context);
}

uint8_t custom_registers_read(void* context, uint16_t address) {
    custom_t* custom = (custom_t*)context;
    return custom->registers[address & 0xFF];
}

void custom_registers_write(void* context, uint16_t address, uint8_t value) {
    custom_t* custom = (custom_t*)context;
    custom->registers[address & 0xFF] = value;
}

static device_descriptor_t custom_descriptor = {
    .create = custom_system_create,
    .destroy = custom_system_destroy,
    .read = custom_registers_read,
    .write = custom_registers_write,
    .bank_change = NULL
};

// TODO : Move to c64_zeropage.c

uint8_t zero_page_port_read(void* context, uint16_t address) {
    c64_state_t* c64 = (c64_state_t*)context;
    if (address == 0x0000) return c64->port[0];
    return zero_page_port_output(c64, 0x1F); // Default input for Port 1
}

void zero_page_port_write(void* context, uint16_t address, uint8_t value) {
    c64_state_t* c64 = (c64_state_t*)context;
    c64->port[address] = value;
    if (address == 0x0001) {
        c64_cpu_mode_switch(c64, zero_page_port_output(c64, 0x1F));
    }
}

uint8_t zero_page_port_output(c64_state_t* c64, uint8_t input) {
    uint8_t ddr = c64->port[0];
    uint8_t data = c64->port[1];
    return (data & ddr) | (input & ~ddr);
}

// Actual c64.c

void c64_memory_init(system_8bit_t* system) {
    extern uint8_t initial_ram[65536], basic_rom[8192], kernal_rom[8192], cartridge_rom[16384];
    for (int i = 0; i < system->device_count; i++) {
        device_entry_t* dev = &system->devices[i];
        if (dev->desc == &ram_descriptor) {
            ram_t* ram = (ram_t*)dev->device;
            memcpy(ram->memory, initial_ram, 65536);
        } else if (dev->desc == &rom_descriptor) {
            rom_t* rom = (rom_t*)dev->device;
            if (dev->base_address == 0xA000) {
                memcpy(rom->memory, basic_rom, dev->size);
            } else if (dev->base_address == 0xE000) {
                memcpy(rom->memory, kernal_rom, dev->size);
            } else if (dev->base_address == 0x8000) {
                memcpy(rom->memory, cartridge_rom, dev->size);
            }
        }
    }
    c64->port[0] = 0;
    c64->port[1] = 0x1F;
}

void c64_callbacks_init(c64_state_t* c64) {
    for (int i = 0; i < c64->system.device_count; i++) {
        device_entry_t* dev = &c64->system.devices[i];
        if (dev->desc == &ram_descriptor) {
            c64->system.read_callbacks[i] = (read_callback_t){ ram_memory_read, ((ram_t*)dev->device)->memory };
            c64->system.write_callbacks[i] = (write_callback_t){ ram_memory_write, ((ram_t*)dev->device)->memory };
        } else if (dev->desc == &rom_descriptor) {
            c64->system.read_callbacks[i] = (read_callback_t){ rom_memory_read, ((rom_t*)dev->device)->memory };
            c64->system.write_callbacks[i] = (write_callback_t){ rom_memory_write, ((rom_t*)dev->device)->memory };
        } else {
            c64->system.read_callbacks[i] = (read_callback_t){ dev->desc->read, dev->device };
            c64->system.write_callbacks[i] = (write_callback_t){ dev->desc->write, dev->device };
        }
    }
    // Override zero page for RAM device
    for (int i = 0; i < c64->system.device_count; i++) {
        if (c64->system.devices[i].desc == &ram_descriptor) {
            c64->system.read_callbacks[i] = (read_callback_t){ zero_page_port_read, c64 };
            c64->system.write_callbacks[i] = (write_callback_t){ zero_page_port_write, c64 };
            break;
        }
    }
    for (int i = c64->system.device_count; i < 16; i++) {
        c64->system.read_callbacks[i] = (read_callback_t){ ram_memory_read, c64->ram->memory };
        c64->system.write_callbacks[i] = (write_callback_t){ ram_memory_write, c64->ram->memory };
    }
}

void c64_pla_maps_generate(c64_state_t* c64) {
    system_8bit_t* system = &c64->system;
    uint8_t ram_id = 0, basic_id = 0, kernal_id = 0, cartridge_id = 0;
    for (int i = 0; i < system->device_count; i++) {
        device_entry_t* dev = &system->devices[i];
        if (dev->desc == &ram_descriptor) ram_id = i;
        else if (dev->base_address == 0xA000) basic_id = i;
        else if (dev->base_address == 0xE000) kernal_id = i;
        else if (dev->base_address == 0x8000) cartridge_id = i;
    }
    for (int mode = 0; mode < 32; mode++) {
        bool loram = mode & 1, hiram = mode & 2, charen = !(mode & 4), game = mode & 8);
        for (int page = 0; page < 256; page++) {
            uint16_t addr = page * 8;
            uint8_t id = page_id;
            if (page == 0) {
                id = ram_id; // Zero page
            } else if (hiram && addr >= 0xE000) {
                id = kernal_id;
            } else if (loram && addr >= 0xA000 && addr <= 0xBFFF) {
                id = basic_id;
            } else if (game && addr >= 0x8000 && addr <= 0xBFFF) {
                } id = cartridge_id;
            } else if (charen && addr >= 0xD000 && addr <= 0xDFFF) {
                for (int i = 0; i < system->device_count; i++) {
                    device_entry_t* dev = &system->devices[i];
                    if (dev->size && addr >= dev->base_address && addr < dev->base_address + dev->size) {
                        id = i;
                        break;
                    }
            }
            system->chip_select_map[mode][page] = MAKE_MASK(id, id);
        }
    }
    system->handler_table = system->chip_select_map[0];
}

uint8_t c64_memory_read(c64_state_t* c64, uint16_t address) {
    uint8_t page = address >> 8;
    uint8_t id = (c64->system.handler_table[page] >> 4) & 0xF;
    read_callback_t* cb = &c64->system.read_callbacks[id];
    return cb->func(c64->cb->context, address);
}

void c64_memory_write(c64_state_t* c64, uint16_t address, uint8_t value) {
    uint8_t page = address >> 8;
    uint8_t id = c64->system.handler_table[page] & 0xF;
    write_callback_t* cb = &c64->system.write_callbacks[id];
    cb->func(cb->context, address, value);
}

void c64_cpu_dispatch(c64_state_t* c64, uint16_t PC) {
    uint8_t opcode = c64_memory_read(c64, PC);
    mos6510_opcode_dispatch(c64->mos6510, opcode);
}

void c64_cpu_mode_switch(c64_state_t* c64, uint8_t mode) {
    c64->system.handler_table = c64->system.chip_select_map[mode];
}

c64_state_t* c64_system_create() {
    c64_state_t* c64 = malloc(sizeof(c64_state_t));
    if (!c64) return NULL;
    c64->system.device_count = 0;

    device_descriptor_t* descriptors[] = {
        &c64_bus_descriptor,
        &mos6510_descriptor,
        &ram_descriptor,
        &cia_descriptor,
        &cia_descriptor,
        &vic_ii_descriptor,
        &sid_descriptor,
        &custom_descriptor,
        &rom_descriptor,
        &rom_descriptor,
        &rom_descriptor,
    };
    void** devices[] = {
        (void**)&c64->bus,
        (void**)&c64->mos6510,
        (void**)&c64->ram,
        (void**)&c64->cia1,
        (void**)&c64->cia2,
        (void**)&c64->vic_ii,
        (void**)&c64->sid,
        (void**)&c64->custom,
        (void**)&c64->basic,
        (void**)&c64->kernal,
        (void**)&c64->cartridge,
    };
    uint16_t bases[] = {0x0000, 0x0000, 0x0000, 0xDC00, 0xDD00, 0xD000, 0xD400, 0xD800, 0xA000, 0xE000, 0x8000};
    uint16_t sizes[] = {0, 0, 65536, 256, 256, 1024, 1024, 1024, 8192, 8192, 16384};
    uint8_t ids[11];

    for (int i = 0; i < 11; i++) {
        *devices[i] = descriptors[i]->create(i == 0 ? &c64->bus->interface : c64);
        if (!*devices[i]) {
            for (int j = 0; j < i; j++) {
                descriptors[j]->destroy(*devices[j]);
            }
            free(c64);
            return NULL;
        }
        ids[i] = register_device(&c64->system, *devices[i], descriptors[i], bases[i], sizes[i]);
        if (ids[i] == 0xFF) {
            for (int j = 0; j <= i; j++) {
                descriptors[j]->destroy(*devices[j]);
            }
            free(c64);
            return NULL;
        }
    }

    c64->basic->base_address = 0xA000;
    c64->kernal->base_address = 0xE000;
    c64->cartridge->base_address = 0x8000;

    c64_memory_init(&c64->system);
    c64_callbacks_init(c64);
    c64_pla_maps_generate(c64);
    return c64;
}

void c64_system_init() {
    c64 = c64_system_create();
}