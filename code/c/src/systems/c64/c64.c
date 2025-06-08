#include <string.h>
#include <stdlib.h>

#include "../../core/chip.h"
#include "../../core/system.h"
#include "c64.h"
#include "c64_bus.h"
#include "../../chip/cpu/mos6510/mos6510.h" // cpu
#include "../../chip/io/mos6526.h" // cia
#include "../../chip/sound/mos6581.h" // sid
#include "../../chip/video/mos6569.h" // vicii
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/memory/mos2114.h" // colorram
#include "../../chip/logic/pla.h" // PLA for memory mapping

static uint8_t initial_ram[65536] = {0};

// Device descriptor declarations for non-CPU devices (defined in respective .c files)
extern chip_descriptor_t c64_bus_descriptor;
extern chip_descriptor_t mos6526_descriptor;
extern chip_descriptor_t mos6581_descriptor;
extern chip_descriptor_t mos6569_descriptor;
extern chip_descriptor_t ram_descriptor;
extern chip_descriptor_t rom_descriptor;

// Actual c64.c

void c64_memory_init(system_8bit_t* system) {
    extern uint8_t initial_ram[65536];
    for (int i = 0; i < system->chip_count; i++) {
        chip_entry_t* dev = &system->chips[i];
        if (dev->desc == &ram_descriptor) {
            ram_t* ram = (ram_t*)dev->chip;
            memcpy(ram->memory, initial_ram, 65536);
        }
        // ROM initialization removed - ROMs should be loaded from files when needed
    }
}

uint8_t c64_detached_read(void* context, uint16_t address) {
    (void)context;
    (void)address;
    return 0xFF; // Return default value for detached reads
}

void c64_detached_write(void* context, uint16_t address, uint8_t value) {
    (void)context;
    (void)address;
    (void)value;
    // Do nothing - detached devices do not write
}

void c64_callbacks_init(c64_t* c64) {
    // Simplified callback initialization for current API
    c64_bus_t* bus = c64->bus;
    
    // Initialize all access callbacks to detached defaults
    for (int i = 0; i < 16; i++) {
        bus->access_callback_per_devid[i].read_func = c64_detached_read;
        bus->access_callback_per_devid[i].write_func = c64_detached_write;
        bus->access_callback_per_devid[i].context = NULL;
    }
      // Set up device-specific callbacks based on device IDs
    for (int i = 0; i < c64->system.chip_count; i++) {
        chip_entry_t* dev = &c64->system.chips[i];
        chip_descriptor_t* desc = dev->desc;
        if (i < 16) { // Safety check for device ID bounds
            if (desc->read) {
                bus->access_callback_per_devid[i].read_func = desc->read;
                bus->access_callback_per_devid[i].context = dev->rwcb_context;
            }
            if (desc->write) {
                bus->access_callback_per_devid[i].write_func = desc->write;
                bus->access_callback_per_devid[i].context = dev->rwcb_context;
            }
        }
    }
}

void c64_pla_maps_generate(c64_t* c64) {
    system_8bit_t* system = &c64->system;
    c64_bus_t* bus = c64->bus;
      // Find device IDs for different memory types
    uint8_t ram_id = 0, basic_id = 0, kernal_id = 0, cartridge_id = 0;
    for (int i = 0; i < system->chip_count; i++) {
        chip_entry_t* dev = &system->chips[i];
        if (dev->desc == &ram_descriptor) ram_id = i;
        else if (dev->base_address == 0xA000) basic_id = i;
        else if (dev->base_address == 0xE000) kernal_id = i;
        else if (dev->base_address == 0x8000) cartridge_id = i;
    }
      // Create a temporary PLA instance for generating memory maps
    pla_906114_01_t* pla = pla_906114_01_create();
    if (!pla) {
        // Fallback to simple mapping if PLA creation fails
        for (int mode = 0; mode < 32; mode++) {
            for (int bankidx = 0; bankidx < 32; bankidx++) {
                uint8_t devid = DEVIDS_RW_ENCODE(ram_id, ram_id);
                bus->devid_per_bankidx_per_mode[mode][bankidx] = devid;
            }
        }
        return;
    }
    
    // Use proper device IDs based on predefined constants
    // Some devices may not be registered yet, use predefined IDs where appropriate
    uint8_t charrom_id = DEVID_UNMAPPED;  // Character ROM might not be a separate device
    uint8_t io_id = DEVID_VIC;           // Default I/O to VIC for unmapped I/O space
    uint8_t cartridge_roml_id = cartridge_id; // Low cartridge ROM
    uint8_t cartridge_romh_id = cartridge_id; // High cartridge ROM  
    uint8_t colorram_id = DEVID_COLORRAM; // Color RAM
    
    // Generate all 32 memory modes using PLA
    c64_bus_generate_all_pla_modes(bus, (struct pla_906114_01_s*)pla,
                                  ram_id, basic_id, kernal_id,
                                  charrom_id, io_id, cartridge_roml_id,
                                  cartridge_romh_id, colorram_id);
    
    // Clean up PLA instance
    pla_906114_01_destroy(pla);
    
    // Set initial bank mapping to mode 0 (all signals high)
    for (int bankidx = 0; bankidx < 32; bankidx++) {
        bus->devid_per_bankidx[bankidx] = bus->devid_per_bankidx_per_mode[0][bankidx];
    }
}

// ============================================================================
// OPTIMIZED BUS CYCLE - Safe device lifecycle management and callback dispatch
// ============================================================================

// Callback for each bus cycle (can be set by test harness)
// Usually NULL during normal emulation, only set for testing/debugging
void (*bus_cycle_callback)(void) = NULL;

void c64_non_cpu_cycle(void* c64_ptr) {
    c64_t* c64 = (c64_t*)c64_ptr;  // Cast from opaque pointer
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
    // Optimized null check with unlikely hint - callback rarely set during normal emulation
    if (__builtin_expect(bus_cycle_callback != NULL, 0)) {
        bus_cycle_callback();
    }
}

//

void c64_system_destroy(c64_t* c64) {
    if (!c64) return;

    system_chips_destroy(&c64->system);
    free(c64);
}

c64_t* c64_system_create() {
    c64_t* c64 = malloc(sizeof(c64_t));
    if (!c64) return NULL;

    chip_descriptor_t* descriptors[] = {
        &c64_bus_descriptor,
        &mos6510_descriptor,
        &ram_descriptor,
        &mos6526_descriptor,
        &mos6526_descriptor,
        &mos6581_descriptor,
        &mos6569_descriptor,
        &mos2114_descriptor,
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
        (void**)&c64->colorram,
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
        ids[i] = system_chip_register(&c64->system, *devices[i], descriptors[i], bases[i], sizes[i]);
        if (ids[i] == 0xFF) {
            c64_system_destroy(c64);
            return NULL;
        }
    }
    
    // Now having a registry of all devices, the PLA maps can be generated
    c64_pla_maps_generate(c64);    // Initialize memory devices with their chip_entry_t to set rwcb_context (no loops)
    ram_memory_init(c64->ram, &c64->system.chips[ids[2]]);
    
    // Initialize ROM devices with their address and size information
    // Pass chip_entry_t so ROM can set its own rwcb_context
    rom_memory_init(c64->basic, 0xA000, 8192, &c64->system.chips[ids[8]]);
    rom_memory_init(c64->kernal, 0xE000, 8192, &c64->system.chips[ids[9]]);
    rom_memory_init(c64->cartridge, 0x8000, 16384, &c64->system.chips[ids[10]]);
    
    // Set default memory contents TODO : Read from file?
    c64_memory_init(&c64->system);

    // Attach RAM directly to MOS6510 for zero page access to avoid circular dependency
    mos6510_attach_ram(c64->mos6510, c64->ram, ram_memory_read, ram_memory_write);

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