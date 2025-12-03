#include "vic20.h"
#include "vic20_bus.h"
#include "vic20_config.h"
#include <stdlib.h>
#include <string.h>

// VIC-20 system creation
vic20_t* vic20_system_create(const vic20_config_t* config) {
    vic20_t* vic20 = (vic20_t*)calloc(1, sizeof(vic20_t));
    if (!vic20) return NULL;

    // Initialize system
    system_8bit_init(&vic20->system);

    // Create bus
    vic20->bus.desc = &vic20_bus_descriptor;
    vic20->bus.vic20 = vic20;
    vic20_bus_init_adapters(&vic20->bus);

    // Create chips
    vic20->ram = (ram_t*)ram_create(&ram_descriptor);
    vic20->basic = (rom_t*)rom_system_create_with_size(&rom_descriptor, 8192);
    vic20->charrom = (rom_t*)rom_system_create_with_size(&rom_descriptor, 4096);
    vic20->kernal = (rom_t*)rom_system_create_with_size(&rom_descriptor, 8192);

    // Create VIC-II chip based on configuration
    if (config->video_standard == VIC20_PAL) {
        vic20->vicii = (vicii_t*)mos6561_create(&mos6561_descriptor);
    } else {
        vic20->vicii = (vicii_t*)mos6560_create(&mos6560_descriptor);
    }

    // Create sound chip
    vic20->sid = (mos6581_t*)mos6581_create(&mos6581_descriptor);

    // Create color RAM
    vic20->colorram = (mos2114_t*)mos2114_create(&mos2114_descriptor);

    // Create CIA
    vic20->cia1 = (mos6526_t*)mos6526_system_create(&mos6526_descriptor);
    vic20->cia1->interrupt_line = BUS_MASK_IRQ;

    // Create VIA (MOS6522)
    vic20->via1 = (mos6522_t*)mos6522_create(&mos6522_descriptor);
    vic20->via1->interrupt_line = BUS_MASK_IRQ;

    // Initialize system
    vic20->total_cycles = 0;

    return vic20;
}

// VIC-20 system destruction
void vic20_system_destroy(vic20_t* vic20) {
    if (!vic20) return;

    // Destroy chips
    if (vic20->ram) ram_system_destroy(vic20->ram);
    if (vic20->basic) rom_system_destroy(vic20->basic);
    if (vic20->charrom) rom_system_destroy(vic20->charrom);
    if (vic20->kernal) rom_system_destroy(vic20->kernal);
    if (vic20->vicii) {
        if (vic20->vicii->desc == &mos6561_descriptor) {
            mos6561_destroy(vic20->vicii);
        } else {
            mos6560_destroy(vic20->vicii);
        }
    }
    if (vic20->sid) mos6581_destroy(vic20->sid);
    if (vic20->colorram) mos2114_destroy(vic20->colorram);
    if (vic20->cia1) mos6526_system_destroy(vic20->cia1);
    if (vic20->via1) mos6522_destroy(vic20->via1);

    // Destroy system
    system_chips_destroy(&vic20->system);

    free(vic20);
}

// VIC-20 CPU cycle
void vic20_cpu_cycle(vic20_t* vic20) {
    if (!vic20) return;

    // TODO: Implement CPU cycle with proper timing
    vic20->total_cycles++;
}

// VIC-20 non-CPU cycle
void vic20_non_cpu_cycle(void* vic20_ptr) {
    vic20_t* vic20 = (vic20_t*)vic20_ptr;
    if (!vic20) return;

    // Tick all non-CPU chips
    if (vic20->vicii) {
        vic20->vicii->desc->tick(vic20->vicii, 0);
    }
    if (vic20->sid) {
        vic20->sid->desc->tick(vic20->sid, 0);
    }
    if (vic20->cia1) {
        vic20->cia1->desc->tick(vic20->cia1, 0);
    }
    if (vic20->via1) {
        vic20->via1->desc->tick(vic20->via1, 0);
    }
}

// VIC-20 memory initialization
void vic20_memory_init(system_8bit_t* system, const rom_config_t* rom_config) {
    // TODO: Implement memory initialization
}

// VIC-20 ROM reloading
bool vic20_reload_roms(vic20_t* vic20, const rom_config_t* rom_config) {
    // TODO: Implement ROM reloading
    return false;
}

// VIC-20 framebuffer setup
void vic20_set_framebuffer(vic20_t* vic20, uint32_t* framebuffer, int width, int height) {
    if (!vic20 || !vic20->vicii) return;

    if (vic20->vicii->desc == &mos6561_descriptor) {
        mos6561_set_framebuffer((mos6561_t*)vic20->vicii, framebuffer, width, height);
    } else {
        mos6560_set_framebuffer((mos6560_t*)vic20->vicii, framebuffer, width, height);
    }
}

// VIC-20 PLA memory mapping generation
bool vic20_pla_maps_generate(vic20_t* vic20) {
    // TODO: Implement PLA memory mapping
    return false;
}