#include <string.h>
#include <stdlib.h>

#include "../../core/aiemuc.h"
#include "../../core/chip.h"
#include "../../core/system.h"
#include "c64.h"
#include "c64_bus.h"
#include "system_config.h"
#include "../../chip/cpu/mos6510/mos6510.h" // cpu
#include "../../chip/io/mos6526.h" // cia
#include "../../chip/sound/mos6581.h" // sid
#include "../../chip/video/mos6569.h" // vicii PAL
#include "../../chip/video/mos6567.h" // vicii NTSC stub
#include "../../chip/video/vicii_common.h"
#include "../../chip/memory/ram.h"
#include "../../chip/memory/rom.h"
#include "../../chip/memory/mos2114.h" // colorram
#include "../../chip/logic/pla.h" // PLA for memory mapping

static uint8_t initial_ram[65536] = {0};

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
    // Do nothing - detached chips do not write
}

void c64_callbacks_init(c64_t* c64) {
    // Initialize ACID allocation system
    c64_bus_t* bus = c64->bus;
    c64_bus_initialize_acids(bus);
    // Allocate ACIDs for all registered chips based on their capabilities
    // ACIDs 1-7: Write-capable chips (3-bit encoding)
    // ACIDs 8+: Read-only chips (4-bit encoding)
    // ACID 0: Reserved for unmapped operations
    
    for (int i = 0; i < c64->system.chip_count; i++) {
        chip_entry_t* dev = &c64->system.chips[i];
        chip_descriptor_t* desc = dev->desc;
          // Allocate ACID based on chip capabilities
        uint8_t allocated_acid = c64_bus_allocate_acid(bus, (uint8_t)i, 
                                                       desc->read, desc->write, 
                                                       dev->rwcb_context);
        
        // Store the allocated ACID in the chip_id_to_acid mapping
        // (This is already done in c64_bus_allocate_acid, but shown for clarity)
        if (allocated_acid != ACID_UNMAPPED) {
            bus->chip_id_to_acid[i] = allocated_acid;
        }
    }
}

void c64_pla_maps_generate(c64_t* c64) {
    system_8bit_t* system = &c64->system;
    c64_bus_t* bus = c64->bus;
    // Find chip IDs for different memory types
    uint8_t ram_chip_id = 0, basic_chip_id = 0, kernal_chip_id = 0;
    uint8_t cartridge_roml_chip_id = 0, cartridge_romh_chip_id = 0, charrom_chip_id = 0;
      for (uint8_t i = 0; i < system->chip_count; i++) {        
        chip_entry_t* dev = &system->chips[i];
        if (dev->desc == &ram_descriptor) {
            ram_chip_id = i;
        }
        // TODO : Generalize this to allow for any chip id
        else if (dev->base_address == 0xA000 && dev->size == 8192) {
            basic_chip_id = i;  // BASIC ROM at $A000-$BFFF
        }
        else if (dev->base_address == 0xE000) {
            kernal_chip_id = i;
        }
        else if (dev->base_address == 0x8000) {
            cartridge_roml_chip_id = i;  // Cartridge ROM Low at $8000-$9FFF
        } 
        else if (dev->base_address == 0xC000) {
            cartridge_romh_chip_id = i;  // Cartridge ROM High at $C000-$DFFF
        }
        else if (dev->base_address == 0xD000 && dev->size == 4096) {
            charrom_chip_id = i;  // Character ROM at $D000-$DFFF
        }
    }

    // Convert chip IDs to ACIDs using the mapping
    uint8_t ram_acid = bus->chip_id_to_acid[ram_chip_id];
    uint8_t basic_acid = bus->chip_id_to_acid[basic_chip_id];
    uint8_t kernal_acid = bus->chip_id_to_acid[kernal_chip_id];
    uint8_t cartridge_roml_acid = bus->chip_id_to_acid[cartridge_roml_chip_id];
    uint8_t cartridge_romh_acid = bus->chip_id_to_acid[cartridge_romh_chip_id];
    uint8_t charrom_acid = bus->chip_id_to_acid[charrom_chip_id];
    
    // Create a temporary PLA instance for generating memory maps
    pla_906114_01_t* pla = pla_906114_01_create();
    if (!pla) {
        // Fallback to simple mapping if PLA creation fails
        for (int mode = 0; mode < 32; mode++) {
            for (int bankidx = 0; bankidx < 32; bankidx++) {
                uint8_t acid = ACIDS_RW_ENCODE(ram_acid, ram_acid);
                bus->acid_per_bankidx_per_mode[mode][bankidx] = acid;
            }
        }
        return;
    }    
    // Use proper ACIDs based on allocated values
    // Additional I/O and memory mapping constants
    uint8_t io_acid = ACID_VIC;           // Default I/O to VIC for unmapped I/O space
    uint8_t colorram_acid = ACID_COLORRAM; // Color RAM
    
    // Generate all 32 memory modes using PLA
    c64_bus_generate_all_pla_modes(bus, (struct pla_906114_01_s*)pla,
                                  ram_acid, basic_acid, kernal_acid,
                                  charrom_acid, io_acid, cartridge_roml_acid,
                                  cartridge_romh_acid, colorram_acid);
    
    // Clean up PLA instance
    pla_906114_01_destroy(pla);
    // Set initial bank mapping to mode 0 (all signals high)
    for (int bankidx = 0; bankidx < 32; bankidx++) {
        bus->acid_per_bankidx[bankidx] = bus->acid_per_bankidx_per_mode[0][bankidx];
    }
}

// ============================================================================
// OPTIMIZED BUS CYCLE - Safe chip lifecycle management and callback dispatch
// ============================================================================

// Callback for each bus cycle (can be set by test harness)
// Usually NULL during normal emulation, only set for testing/debugging
void (*bus_cycle_callback)(void) = NULL;

void c64_non_cpu_cycle(void* c64_ptr) {
    c64_t* c64 = (c64_t*)c64_ptr;  // Cast from opaque pointer
    c64->total_cycles++;
    
    // All chips always run for cycle accuracy - using safe chip callers
    mos6581_cycle(c64->sid);
    mos6526_cycle(c64->cia1);
    mos6526_cycle(c64->cia2);
    vicii_common_cycle(c64->vicii);
    
    // Update RDY line based on BA (hardware accurate)
    if (c64->bus->control_lines & BA_LINE) {
        c64->bus->control_lines |= RDY_LINE;
    } else {
        c64->bus->control_lines &= ~RDY_LINE;
    }
    // Optimized null check with unlikely hint - callback rarely set during normal emulation
    if (unlikely(bus_cycle_callback != NULL)) {
        bus_cycle_callback();
    }
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
    
    // Now having a registry of all chips, the PLA maps can be generated
    c64_pla_maps_generate(c64);
    // Attach RAM directly to MOS6510 for zero page access to avoid circular dependency
    access_callback_t ram_access = {
        .read_func = ram_descriptor.read,
        .write_func = ram_descriptor.write,
        .context = ram_descriptor.get_rwcb_context(c64->ram)
    };
    mos6510_attach_ram(c64->mos6510, &ram_access);

    // Set default memory contents TODO : Read from file?
    c64_memory_init(&c64->system);

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