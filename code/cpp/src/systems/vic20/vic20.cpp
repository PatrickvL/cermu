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

    // Create VIC-II chip (MOS6561 enhanced version)
    vic20->vicii = (vicii_t*)mos6561_create(&mos6561_descriptor);

    // Create VIA (MOS6522) - this is the main integration point
    vic20->via1 = (mos6522_t*)mos6522_create(&mos6522_descriptor);
    vic20->via1->interrupt_line = BUS_MASK_IRQ;

    // Initialize system
    vic20->total_cycles = 0;

    return vic20;
}

// VIC-20 system destruction
void vic20_system_destroy(vic20_t* vic20) {
    if (!vic20) return;

    // Destroy VIC-II
    if (vic20->vicii) mos6561_destroy(vic20->vicii);

    // Destroy VIA
    if (vic20->via1) mos6522_destroy(vic20->via1);

    // Destroy system
    system_chips_destroy(&vic20->system);

    free(vic20);
}

// VIC-20 CPU cycle
void vic20_cpu_cycle(vic20_t* vic20) {
    if (!vic20) return;

    // Tick the VIA
    if (vic20->via1) {
        mos6522_tick(vic20->via1, 0);
    }

    vic20->total_cycles++;
}

// VIC-20 non-CPU cycle
void vic20_non_cpu_cycle(void* vic20_ptr) {
    vic20_t* vic20 = (vic20_t*)vic20_ptr;
    if (!vic20) return;

    // Tick the VIC-II
    if (vic20->vicii) {
        mos6561_tick(vic20->vicii, 0);
    }

    // Tick the VIA
    if (vic20->via1) {
        mos6522_tick(vic20->via1, 0);
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
    mos6561_set_framebuffer((mos6561_t*)vic20->vicii, framebuffer, width, height);
}

// VIC-20 PLA memory mapping generation
bool vic20_pla_maps_generate(vic20_t* vic20) {
    // TODO: Implement PLA memory mapping
    return false;
}