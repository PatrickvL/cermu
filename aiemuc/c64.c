#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "device.h"
#include "system.h"
#include "c64.h"
#include "c64_bus.h"
#include "mos6510.h" // cpu
#include "mos6526.h" // cia
#include "mos6581.h" // sid
#include "mos6569.h" // vicii
#include "ram.h"
#include "rom.h"

static uint8_t initial_ram[65536] = {0};
static uint8_t basic_rom[8192] = {0};
static uint8_t kernal_rom[8192] = {0};
static uint8_t cartridge_rom[16384] = {0};

// Device descriptor declarations (defined in respective .c files)
extern device_descriptor_t c64_bus_descriptor;
extern device_descriptor_t mos6510_descriptor;
extern device_descriptor_t mos6526_descriptor;
extern device_descriptor_t mos6581_descriptor;
extern device_descriptor_t mos6569_descriptor;
extern device_descriptor_t ram_descriptor;
extern device_descriptor_t rom_descriptor;

//typedef struct custom custom_t;

// TODO : Move to c64_custom.c
void* custom_system_create(device_descriptor_t* desc) {
    custom_t* custom = (custom_t*)calloc(1, sizeof(custom_t));
    if (!custom) return NULL;
    custom->desc = desc;
    return custom;
}

void custom_system_destroy(void* device) {
    free(device);
}

uint8_t custom_registers_read(void* context, uint16_t address) {
    custom_t* custom = (custom_t*)context;
    return custom->registers[address & 0xFF];
}

void custom_registers_write(void* context, uint16_t address, uint8_t value) {
    custom_t* custom = (custom_t*)context;
    custom->registers[address & 0xFF] = value;
}

device_descriptor_t custom_descriptor = {
    .create = custom_system_create,
    .destroy = custom_system_destroy,
    .bus_attach = NULL,
    .read = custom_registers_read,
    .write = custom_registers_write,
    .bank_change = NULL
};

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
}

uint8_t c64_detached_read(c64_t* c64, uint16_t address) {
    (void)c64;
    (void)address;
    return 0xFF; // TODO : Return the current bus.data?
}

void c64_detached_write(c64_t* c64, uint16_t address, uint8_t value) {
    (void)c64;
    (void)address;
    (void)value;
    // Do nothing - detached devices do not write
}

static read_callback_t c64_detached_read_callback = {
    .func = (device_read_func_t)c64_detached_read,
    .context = NULL
};

static write_callback_t c64_detached_write_callback = {
    .func = (device_write_func_t)c64_detached_write,
    .context = NULL
};

void c64_callbacks_init(c64_t* c64) {
    c64_bus_t* bus = c64->bus;     
    for (int i = 0; i < c64->system.device_count; i++) {
        device_entry_t* dev = &c64->system.devices[i];
        device_descriptor_t* desc = dev->desc;
        if (desc->read)
            bus->read_callbacks[i] = (read_callback_t){ desc->read, dev->rwcb_context };
        else
            bus->read_callbacks[i] = c64_detached_read_callback;
        if (desc->write)
            bus->write_callbacks[i] = (write_callback_t){ desc->write, dev->rwcb_context };
        else
            bus->write_callbacks[i] = c64_detached_write_callback;
    }
}

void c64_pla_maps_generate(c64_t* c64) {
    system_8bit_t* system = &c64->system;
    c64_bus_t* bus = c64->bus;    uint8_t ram_id = 0, basic_id = 0, kernal_id = 0, cartridge_id = 0;
    for (int i = 0; i < system->device_count; i++) {
        device_entry_t* dev = &system->devices[i];
        if (dev->desc == &ram_descriptor) ram_id = (uint8_t)i;
        else if (dev->base_address == 0xA000) basic_id = (uint8_t)i;
        else if (dev->base_address == 0xE000) kernal_id = (uint8_t)i;
        else if (dev->base_address == 0x8000) cartridge_id = (uint8_t)i;
    }
    for (int mode = 0; mode < 32; mode++) {
        bool loram = mode & 1, hiram = mode & 2, charen = !(mode & 4), game = mode & 8;
        
        // Initialize condensed table with 32 entries instead of 256
        for (int index = 0; index < 32; index++) {
            uint8_t id = ram_id;  // Default to RAM
            
            if (index < 16) {
                // Banks 0-15: Use bank number directly as index
                // Each bank represents 4KB (0x1000 bytes)
                uint16_t bank_start = (uint16_t)(index * 0x1000);
                
                if (index == 0) {
                    id = ram_id; // Zero page bank is always RAM
                } else if (hiram && bank_start >= 0xE000) {
                    id = kernal_id;
                } else if (loram && bank_start >= 0xA000 && bank_start <= 0xBFFF) {
                    id = basic_id;
                } else if (game && bank_start >= 0x8000 && bank_start <= 0xBFFF) {
                    id = cartridge_id;
                } else if (charen && bank_start >= 0xD000 && bank_start <= 0xDFFF) {
                    // For IO bank (0xD000-0xDFFF), find the appropriate device
                    for (int i = 0; i < system->device_count; i++) {
                        device_entry_t* dev = &system->devices[i];
                        if (dev->size && bank_start >= dev->base_address && bank_start < dev->base_address + dev->size) {
                            id = (uint8_t)i;
                            break;
                        }
                    }
                }
            } else {
                // Indices 16-31: IO range (bank 13) broken down by pages
                // index 16 = page 0 of bank 13 (0xD000-0xD0FF)
                // index 17 = page 1 of bank 13 (0xD100-0xD1FF)
                // ...
                // index 31 = page 15 of bank 13 (0xDF00-0xDFFF)
                int page_in_bank = index - 16;
                uint16_t addr = (uint16_t)(0xD000 + (page_in_bank * 256));
                
                if (charen) {
                    // Find the appropriate device for this specific page in IO range
                    for (int i = 0; i < system->device_count; i++) {
                        device_entry_t* dev = &system->devices[i];
                        if (dev->size && addr >= dev->base_address && addr < dev->base_address + dev->size) {
                            id = (uint8_t)i;
                            break;
                        }
                    }
                } else {
                    id = ram_id; // If CHAREN is off, IO range maps to RAM
                }
            }
            
            bus->device_id_per_page_per_mode[mode][index] = DEVIDS_RW_ENCODE(id, id);
        }
    }
}

void c64_cpu_dispatch(c64_t* c64, uint16_t PC) {
    uint8_t opcode = c64_bus_memory_read(c64->bus, PC);
    mos6510_opcode_dispatch(c64->mos6510, opcode);
}

// ============================================================================
// OPTIMIZED BUS CYCLE - Safe device lifecycle management and callback dispatch
// ============================================================================

// Callback for each bus cycle (can be set by test harness)
void (*bus_cycle_callback)(void) = NULL;

void c64_non_cpu_cycle(c64_t* c64) {
    c64->total_cycles++;
    
    // All chips always run for cycle accuracy - using safe device callers
    mos6581_cycle(c64->sid);
    mos6526_cycle(c64->cia1);
    mos6526_cycle(c64->cia2);
    mos6569_cycle(c64->vicii);
    
    // Update RDY line based on BA (hardware accurate)
    if (c64->bus->control_lines & BA_LINE) {
        c64->bus->control_lines |= RDY_LINE;
    } else {
        c64->bus->control_lines &= ~RDY_LINE;
    }
    // Call the callback if set
    if (bus_cycle_callback) bus_cycle_callback();
}

//

void c64_system_destroy(c64_t* c64) {
    if (!c64) return;

    system_devices_destroy(&c64->system);
    free(c64);
}

c64_t* c64_system_create() {
    c64_t* c64 = calloc(1, sizeof(c64_t));
    if (!c64) {
        return NULL;
    }

    device_descriptor_t* descriptors[] = {
        &c64_bus_descriptor,
        &mos6510_descriptor,
        &ram_descriptor,
        &mos6526_descriptor,
        &mos6526_descriptor,
        &mos6581_descriptor,
        &mos6569_descriptor,
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
        (void**)&c64->sid,
        (void**)&c64->vicii,
        (void**)&c64->custom,
        (void**)&c64->basic,
        (void**)&c64->kernal,
        (void**)&c64->cartridge,
    };
    uint16_t bases[] = {0x0000, 0x0000, 0x0000, 0xDC00, 0xDD00, 0xD000, 0xD400, 0xD800, 0xA000, 0xE000, 0x8000};
    unsigned int sizes[] = {0, 0, 65536, 256, 256, 1024, 1024, 1024, 8192, 8192, 16384};
    uint8_t ids[11];

    for (int i = 0; i < 11; i++) {
        *devices[i] = descriptors[i]->create(descriptors[i]);
        if (!*devices[i]) {
            c64_system_destroy(c64);
            return NULL;
        }
        ids[i] = system_device_register(&c64->system, *devices[i], descriptors[i], bases[i], sizes[i]);
        if (ids[i] == 0xFF) {
            c64_system_destroy(c64);
            return NULL;
        }
    }

    // Now having a registry of all devices, the PLA maps can be generated
    c64_pla_maps_generate(c64);

    // Initialize memory devices with their device_entry_t to set rwcb_context (no loops)
    ram_memory_init(c64->ram, &c64->system.devices[ids[2]]);
    
    // Initialize ROM devices with their address and size information
    // Pass device_entry_t so ROM can set its own rwcb_context
    rom_memory_init(c64->basic, 0xA000, 8192, &c64->system.devices[ids[8]]);
    rom_memory_init(c64->kernal, 0xE000, 8192, &c64->system.devices[ids[9]]);
    rom_memory_init(c64->cartridge, 0x8000, 16384, &c64->system.devices[ids[10]]);
    
    // Set default memory contents TODO : Read from file?
    c64_memory_init(&c64->system);

    // Now that all devices have their rwcb_context set, we can initialize the callbacks
    c64_callbacks_init(c64);
    
    // Now attach devices to the bus
    for (int i = 0; i < 11; i++) {
        if (i == 0) {
            // Make sure that the c64 bus has access to the c64 instance.
            // This is necessary so the below (indirect, via bus_attach)
            // call to mos6510_bus_attach, which calls mos6510_ioport_write,
            // can call c64_bus_mode_switch with the actual c64 instance.
            c64_bus_system_attach(c64->bus, c64);
        }
        if (descriptors[i]->bus_attach) {
            descriptors[i]->bus_attach(*devices[i], c64->bus);
        }
    }
    return c64;
}

void c64_emulate_frame(c64_t* c64) {
    (void)c64;
}