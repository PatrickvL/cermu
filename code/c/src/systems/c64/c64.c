#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../core/aiemuc.h"
#include "../../core/chip.h"
#include "../../core/system.h"
#include "c64.h"
#include "c64_bus.h"
#include "c64_config.h"
#include "../../core/storage/rom_loader.h"
#include "../../core/config/path_discovery.h"
#include "../../chip/cpu/mos6510/mos6510.h" // cpu
#include "../../chip/io/mos6526.h" // cia
#include "../../chip/sound/mos6581.h" // sid
#include "../../chip/video/vic_ii/mos6569.h" // vicii PAL
#include "../../chip/video/vic_ii/mos6567.h" // vicii NTSC stub
#include "../../chip/video/vic_ii/vicii_common.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/logic/pla.h" // PLA for memory mapping

void c64_memory_init(system_8bit_t* system, const rom_config_t* rom_config) {
    // Use default ROM configuration if none provided
    if (!rom_config) {
        rom_config = system_config_get_default_roms();
    }
    
    // Discover ROM root path for C64 system
    char rom_root_path[1024];
    bool rom_root_found = system_config_discover_rom_root("c64", rom_root_path, sizeof(rom_root_path));
    
    for (int i = 0; i < system->chip_count; i++) {
        chip_entry_t* dev = &system->chips[i];
        
        // Initialize RAM
        if (dev->desc == &ram_descriptor) {
            ram_t* ram = (ram_t*)dev->chip;
            // Initialize RAM to zero - no need for separate initial_ram array
            memset(ram->memory, 0, 65536);
        }
        
        // Initialize ROM chips by loading from files
        else if (dev->desc == &rom_descriptor) {
            rom_t* rom = (rom_t*)dev->chip;
            if (!rom->memory) {
                printf("Warning: ROM chip has no allocated memory\n");
                continue;
            }
            
            bool rom_loaded = false;
              // Only attempt to load ROMs if we found the ROM root directory
            if (rom_root_found) {
                // Determine ROM type based on memory address and size
                if (dev->base_address == 0xA000 && dev->size == 8192) {
                    // BASIC ROM
                    rom_loaded = rom_loader_load_from_root(rom_root_path, (const char**)rom_config->basic_rom_filenames, 8192, 
                                                         rom->memory, dev->size);
                    if (!rom_loaded) {
                        printf("Warning: Failed to load BASIC ROM\n");
                    }
                }
                else if (dev->base_address == 0xE000 && dev->size == 8192) {
                    // KERNAL ROM  
                    rom_loaded = rom_loader_load_from_root(rom_root_path, (const char**)rom_config->kernal_rom_filenames, 8192,
                                                         rom->memory, dev->size);
                    if (!rom_loaded) {
                        printf("Warning: Failed to load KERNAL ROM\n");
                    }
                }
                else if (dev->base_address == 0xD000 && dev->size == 4096) {
                    // Character ROM
                    rom_loaded = rom_loader_load_from_root(rom_root_path, (const char**)rom_config->chargen_rom_filenames, 4096,
                                                         rom->memory, dev->size);
                    if (!rom_loaded) {
                        printf("Warning: Failed to load Character ROM\n");
                    }
                }
            } else {
                printf("Warning: ROM root not found, skipping ROM loading\n");
            }
            
            // If ROM loading failed, fill with default pattern (0xFF for unloaded ROM)
            if (!rom_loaded) {
                memset(rom->memory, 0xFF, dev->size);
            }
        }
    }
}

uint8_t c64_detached_read(void* context, uint16_t address) {
    (void)address;
    return c64_bus_adapter_detached_read(context);
}

void c64_detached_write(void* context, uint16_t address, uint8_t value) {
    (void)context;
    (void)address;
    (void)value;
    // Do nothing - detached chips do not write
}

uint8_t c64_dev_descriptor_to_acid(chip_entry_t* dev) {
    chip_descriptor_t* desc = dev->desc;
    
    if (desc == &mos2114_descriptor) return ACID_COLORRAM_D8; // Color RAM
    if (desc == &mos6510_descriptor) return ACID_ZEROBANK; // CPU handles zero bank
    if (desc == &mos6526_descriptor) { // CIA chips get slots 12-13 (DC00-DDFF)
        if (dev->base_address == 0xDC00) return ACID_CIA1_DC; // 12
        if (dev->base_address == 0xDD00) return ACID_CIA2_DD; // 13
    }
    if (desc == &mos6567_descriptor || desc == &mos6569_descriptor) return ACID_VIC_D0; // VIC-II
    if (desc == &mos6581_descriptor) return ACID_SID_D4; // SID
    if (desc == &ram_descriptor) return ACID_RAM; // RAM
    if (desc == &rom_descriptor) { // ROMs are handled by base address
        if (dev->base_address == 0x8000) return ACID_ROML;
        if (dev->base_address == 0xA000) return ACID_BASIC;
        if (dev->base_address == 0xC000) return ACID_ROMH;
        if (dev->base_address == 0xD000) return ACID_CHARROM;
        if (dev->base_address == 0xE000) return ACID_KERNAL;
    }
    // Note : ACID_IO1_DE and ACID_IO2_DF are not yet supported, but reserved for future expansion

    return ACID_UNMAPPED; // Default unmapped access
}

void c64_callbacks_init(c64_t* c64) {
    // Initialize optimized callback system
    c64_bus_t* bus = c64->bus;

    // Initialize all callbacks to stub functions first
    for (int acid = 0;  acid < ACID_MAX; acid++) {
        // Register read callback
        bus->read_callbacks[acid].read = c64_detached_read;
        bus->read_callbacks[acid].context = bus;
        // Register write callback (only for writable chips 0-19)
        if (acid < 20)
            bus->write_funcs[acid] = c64_detached_write;
    }
    
    // Register chip callbacks in optimized arrays based on chip types
    for (int i = 0; i < c64->system.chip_count; i++) {
        chip_entry_t* dev = &c64->system.chips[i];
        // Map chips to optimized callback slots based on their type and address
        uint8_t acid = c64_dev_descriptor_to_acid(dev);

        switch (acid) {
            case ACID_VIC_D0:
                // VIC-II (NTSC or PAL) gets I/O slots 0-3 (D000-D3FF)
                c64_bus_register_chip_callbacks(bus, ACID_VIC_D0, dev);
                c64_bus_register_chip_callbacks(bus, ACID_VIC_D1, dev);
                c64_bus_register_chip_callbacks(bus, ACID_VIC_D2, dev);
                c64_bus_register_chip_callbacks(bus, ACID_VIC_D3, dev);
                break;
            case ACID_SID_D4:
                // SID gets I/O slots 4-7 (D400-D7FF)
                c64_bus_register_chip_callbacks(bus, ACID_SID_D4, dev);
                c64_bus_register_chip_callbacks(bus, ACID_SID_D5, dev);
                c64_bus_register_chip_callbacks(bus, ACID_SID_D6, dev);
                c64_bus_register_chip_callbacks(bus, ACID_SID_D7, dev);
                break;
            case ACID_COLORRAM_D8:
                // Color RAM gets I/O slots 8-11 (D800-DBFF)
                c64_bus_register_chip_callbacks(bus, ACID_COLORRAM_D8, dev);
                c64_bus_register_chip_callbacks(bus, ACID_COLORRAM_D9, dev);
                c64_bus_register_chip_callbacks(bus, ACID_COLORRAM_DA, dev);
                c64_bus_register_chip_callbacks(bus, ACID_COLORRAM_DB, dev);
                break;
            default:
                // Register the chip callback
                c64_bus_register_chip_callbacks(bus, acid, dev);
                break;
        }       
    }
}

bool c64_pla_maps_generate(c64_t* c64) {
    c64_bus_t* bus = c64->bus;
    
    // Create a temporary PLA instance for generating memory maps
    pla_906114_01_t* pla = pla_906114_01_create();
    if (!pla)
        return false;
    
    // Generate all 32 memory modes using PLA
    c64_bus_generate_all_pla_modes(bus, (struct pla_906114_01_s*)pla);
    
    // Clean up PLA instance
    pla_906114_01_destroy(pla);
    
    // Set initial bank mapping to mode 1 (known to enable Kernal ROM) using proper mode switch
    c64_bus_mode_switch(bus, 0x01);
    return true;
}

// ============================================================================
// OPTIMIZED BUS CYCLE - Safe chip lifecycle management and callback dispatch
// ============================================================================

// Callback for each bus cycle (can be set by test harness)
// Usually NULL during normal emulation, only set for testing/debugging
void (*bus_cycle_callback)(void) = NULL;

// Ticks all non-CPU chips once to complete a cycle.
void c64_non_cpu_cycle(void* c64_ptr) {
    c64_t* c64 = (c64_t*)c64_ptr;  // Cast from opaque pointer

    // Optimized null check with unlikely hint - callback rarely set during normal emulation
    if (unlikely(bus_cycle_callback != NULL)) {
        bus_cycle_callback();
    }    

    // Safety checks
    if (unlikely(!c64)) {
        printf("ERROR: c64_non_cpu_cycle called with NULL c64_ptr\n");
        fflush(stdout);
        return;
    }
    if (unlikely(!c64->vicii)) {
        printf("ERROR: c64->vicii is NULL at cycle %llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
        return;
    }

    if (unlikely(!c64->cia1)) {
        printf("ERROR: c64->cia1 is NULL at cycle %llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
        return;
    }
    
    if (unlikely(!c64->cia2)) {
        printf("ERROR: c64->cia2 is NULL at cycle %llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
        return;
    }
    
    if (unlikely(!c64->sid)) {
        printf("ERROR: c64->sid is NULL at cycle %llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
        return;
    }
    
    // Debug output every 1000 cycles to track progress
    if (c64->total_cycles % 1000 == 0) {
        printf("c64_non_cpu_cycle: cycle #%llu\n", (unsigned long long)c64->total_cycles);
        fflush(stdout);
    }
    
    c64->total_cycles++;
    
    // VIC tick handles both phi1 and phi2 phases internally
    vicii_common_cycle(c64->vicii);
    // Other chips tick once per complete cycle
    mos6526_cycle(c64->cia1);
    mos6526_cycle(c64->cia2);
    mos6581_cycle(c64->sid);
//        c64_update_interrupt_lines(c64, bus);
//void c64_update_interrupt_lines(c64_t* c64, c64_bus_t* bus) {
/*
    bus->irq_line = false;
    bus->nmi_line = false;
    
    // VIC-II IRQ
    if (c64->vic.irq_status & c64->vic.irq_mask) {
        bus->irq_line = true;
    }
    
    // CIA interrupts
    if (c64->cia1.interrupt_control & c64->cia1.interrupt_mask) {
        bus->irq_line = true;
    }
    if (c64->cia2.interrupt_control & c64->cia2.interrupt_mask) {
        bus->nmi_line = true;
    }
*/
    // Update RDY line based on BA (hardware accurate)
    if (c64->bus->control_lines & BA_LINE) {
        c64->bus->control_lines |= RDY_LINE;
    } else {
        c64->bus->control_lines &= ~RDY_LINE;
    }
//}
}

// Compact chip creation helper - creates and registers a chip (rwcb_context auto-set by system_chip_register)
static inline void* create_and_register_chip(c64_t* c64, chip_descriptor_t* desc, uint16_t addr, unsigned int size) {
    void* chip;
    
    // Special handling for ROM - allocate memory based on requested size before registration
    if (desc == &rom_descriptor) {
        chip = rom_system_create_with_size(desc, size);
    } else {
        chip = desc->create(desc);
    }
    
    if (!chip) {
        c64_system_destroy(c64);
        return NULL;
    }
    uint8_t chip_id = system_chip_register(&c64->system, chip, desc, addr, size);
    if (chip_id == 0xFF) {
        c64_system_destroy(c64);
        return NULL;
    }
    return chip;
}

//

void c64_system_destroy(c64_t* c64) {
    if (!c64) return;

    system_chips_destroy(&c64->system);
    free(c64);
}

c64_t* c64_system_create(const system_config_t* config) {
    c64_t* c64 = calloc(1, sizeof(c64_t));
    if (!c64) return NULL;

    chip_descriptor_t* vicii_descriptor = (config->vic_standard == VIC_PAL ? &mos6569_descriptor : &mos6567_descriptor);

    // One line per chip - create, register, assign memory address/size, assign to C64 field, and initialize rwcb_context
    if (!(c64->bus = create_and_register_chip(c64, &c64_bus_descriptor, 0x0000, 0))) return NULL;
    if (!(c64->ram = create_and_register_chip(c64, &ram_descriptor, 0x0000, 65536))) return NULL;
    if (!(c64->mos6510 = create_and_register_chip(c64, &mos6510_descriptor, 0x0000, 4096))) return NULL;
    if (!(c64->cartridge_roml = create_and_register_chip(c64, &rom_descriptor, 0x8000, 8192))) return NULL;
    if (!(c64->basic = create_and_register_chip(c64, &rom_descriptor, 0xA000, 8192))) return NULL;
    if (!(c64->cartridge_romh = create_and_register_chip(c64, &rom_descriptor, 0xC000, 8192))) return NULL;
    if (!(c64->charrom = create_and_register_chip(c64, &rom_descriptor, 0xD000, 4096))) return NULL;
    if (!(c64->vicii = create_and_register_chip(c64, vicii_descriptor, 0xD000, 1024))) return NULL;
    if (!(c64->sid = create_and_register_chip(c64, &mos6581_descriptor, 0xD400, 1024))) return NULL;
    if (!(c64->colorram = create_and_register_chip(c64, &mos2114_descriptor, 0xD800, 1024))) return NULL;
    if (!(c64->cia1 = create_and_register_chip(c64, &mos6526_descriptor, 0xDC00, 256))) return NULL;
    if (!(c64->cia2 = create_and_register_chip(c64, &mos6526_descriptor, 0xDD00, 256))) return NULL;
    if (!(c64->kernal = create_and_register_chip(c64, &rom_descriptor, 0xE000, 8192))) return NULL;
    
    // Register PLA for GUI debugging (special case - chip is the C64 system itself)
    uint8_t pla_chip_id = system_chip_register(&c64->system, c64, &pla_descriptor, 0x0000, 0);
    if (pla_chip_id == 0xFF) return NULL;
    
    // Now having a registry of all chips, the PLA maps can be generated
    if (!c64_pla_maps_generate(c64)) return NULL;

    // Attach RAM directly to MOS6510 for zero page access to avoid circular dependency
    access_callback_t ram_access = {
        .read_func = ram_descriptor.read,
        .write_func = ram_descriptor.write,
        .context = ram_descriptor.get_rwcb_context(c64->ram)
    };
    mos6510_attach_ram(c64->mos6510, &ram_access);
    // Set default memory contents and load ROMs from configured paths
    const rom_config_t* rom_config = config->rom_config ? config->rom_config : system_config_get_default_roms();
    c64_memory_init(&c64->system, rom_config);

    // Now that all devices have their rwcb_context set, we can initialize the callbacks
    c64_callbacks_init(c64);
    
    // Attach bus to C64 system first, then all other chips with bus_attach callbacks
    c64_bus_system_attach(c64->bus, c64);
    for (int i = 0; i < c64->system.chip_count; i++) {
        chip_entry_t* chip = &c64->system.chips[i];
        if (chip->desc && chip->desc->bus_attach && chip->desc != &c64_bus_descriptor)
            chip->desc->bus_attach(chip->chip, c64->bus);
    }
    
    // Attach CPU interfaces to the MOS6510
    mos6510_attach_bus_interface(c64->mos6510, c64_bus_get_adapter(c64->bus));
    mos6510_attach_control_lines_interface(c64->mos6510, c64_control_lines_get_adapter(c64->bus));
    mos6510_attach_io_interface(c64->mos6510, c64_io_port_get_adapter(c64->bus));
    // Note: system_lines attachment would need proper system_lines_t structure
    // For now, this will be handled through the control_lines_interface
    
    return c64;
}

bool c64_reload_roms(c64_t* c64, const rom_config_t* rom_config) {
    if (!c64 || !rom_config) {
        return false;
    }
    
    // Reload ROMs using the memory initialization function
    c64_memory_init(&c64->system, rom_config);
    
    printf("ROMs reloaded successfully\n");
    return true;
}