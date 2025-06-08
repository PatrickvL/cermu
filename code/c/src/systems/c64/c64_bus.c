#include "c64_bus.h"
#include "c64.h"
#include <stdlib.h>
#include <string.h>

// Inline function to calculate condensed bank index from address.
// Bank number is derived from the upper 4 bits of the address, whereby the lower 4 
// IO range (bank 13), returns 16 + the page number (from the 2nd address nybble).
static inline int c64_bus_address_to_bankidx(uint16_t address) {
    int page = (address >> 8) & 0x0F;
    int bank = address >> 12; // bank 0 to 15 (13 is unused)
    int bank13_delta = 3 + page; // 3 to 18, when added to 13 gives 16 to 31
    int is_bank13_mask = - (int)(bank == 13); // 0x00000000 or 0xFFFFFFFF
    return bank + (is_bank13_mask & bank13_delta); // 0 to 31: 0 to 15 bank numbers (13 unused), 16 and up for bank 13 pages
}

uint8_t c64_bus_memory_read(c64_bus_t* c64_bus, uint16_t address) {
    int bankidx = c64_bus_address_to_bankidx(address);
    uint8_t acid = ACID_READ_DECODE(c64_bus->acid_per_bankidx[bankidx]);
    access_callback_t* cb = &c64_bus->access_callback_per_acid[acid];
    return cb->read_func(cb->context, address);
}

void c64_bus_memory_write(c64_bus_t* c64_bus, uint16_t address, uint8_t value) {
    int bankidx = c64_bus_address_to_bankidx(address);
    uint8_t acid = ACID_WRITE_DECODE(c64_bus->acid_per_bankidx[bankidx]);
    access_callback_t* cb = &c64_bus->access_callback_per_acid[acid];
    cb->write_func(cb->context, address, value);
    // TODO : Move below signalling of VIC-II bank change to somewhere else with less impact on performance
    if (address == 0xDD00) {
        c64_t* c64 = c64_bus->c64;
        uint8_t bank = 3 - (value & 0x3);
        if (c64->sid->desc->bank_change) {
            c64->sid->desc->bank_change(c64->sid, bank);
        }
    }
}

void c64_bus_system_destroy(void* chip) {
    free(chip);
}

void* c64_bus_system_create(chip_descriptor_t* desc) {
    c64_bus_t* c64_bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!c64_bus) return NULL;
    c64_bus->desc = desc;
    // Initialize bus state
    c64_bus->address = 0;
    c64_bus->data = 0;
    c64_bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
    
    // Initialize ACID allocation tracking
    c64_bus_initialize_acids(c64_bus);
    
    // Initialize the integrated adapter interfaces
    c64_bus_init_adapters(c64_bus);
    
    return c64_bus;
}

void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64) {
    c64_bus->c64 = c64;  // Store as opaque pointer
}

chip_descriptor_t c64_bus_descriptor = {
    .create = c64_bus_system_create,
    .destroy = c64_bus_system_destroy,
    .bus_attach = NULL,
    .read = NULL,
    .write = NULL,
    .bank_change = NULL
};

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode) {
    // Update the ACID mapping for the current mode
    memcpy(c64_bus->acid_per_bankidx, c64_bus->acid_per_bankidx_per_mode[mode], sizeof(c64_bus->acid_per_bankidx));
}

uint8_t c64_bus_read_cycle(c64_bus_t *c64_bus, uint16_t addr) {
    c64_bus->address = addr; // Perhaps this is no longer needed
    uint8_t data = c64_bus_memory_read(c64_bus, addr);
    c64_bus->data = data; // Perhaps this is no longer needed
    c64_non_cpu_cycle(c64_bus->c64);
    return data;
}

void c64_bus_write_cycle(c64_bus_t* c64_bus, uint16_t addr, uint8_t value) {
    c64_bus->address = addr; // Perhaps this is no longer needed
    c64_bus->data = value; // Perhaps this is no longer needed
    c64_bus_memory_write(c64_bus, addr, value);
    c64_non_cpu_cycle(c64_bus->c64);
}

// PLA integration functions
#include "../../chip/logic/pla.h"

void c64_bus_populate_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla,
                                 uint8_t ram_id, uint8_t basic_id, uint8_t kernal_id, 
                                 uint8_t charrom_id, uint8_t io_id, uint8_t cartridge_roml_id, 
                                 uint8_t cartridge_romh_id, uint8_t colorram_id) {    // Clear current mapping
    for (int i = 0; i < 32; i++) {
        bus->acid_per_bankidx[i] = ACID_UNMAPPED;
    }
    // Map memory regions based on PLA outputs
    for (uint32_t addr = 0; addr < 0x10000; addr += 0x100) {
        int bank_idx = c64_bus_address_to_bankidx(addr);
        uint8_t device_id = ACID_UNMAPPED;
        
        // Set address in PLA
        pla_906114_01_set_address_high((pla_906114_01_t*)pla, (addr >> 8) & 0x0F);
          // Determine device based on PLA outputs
        if (!pla->outputs.n_casram) {
            // RAM is selected
            if (addr < 0x0002) {
                device_id = ACID_ZEROPAGE;  // Special handling for CPU I/O ports
            } else {
                device_id = ram_id;
            }
        } else if (!pla->outputs.n_basic) {
            device_id = basic_id;
        } else if (!pla->outputs.n_kernal) {
            device_id = kernal_id;
        } else if (!pla->outputs.n_charrom) {
            device_id = charrom_id;
        } else if (!pla->outputs.n_io) {
            // I/O region - determine specific device
            if (addr >= 0xD000 && addr < 0xD400) {
                device_id = ACID_VIC;
            } else if (addr >= 0xD400 && addr < 0xD800) {
                device_id = ACID_SID;
            } else if (addr >= 0xD800 && addr < 0xDC00) {
                device_id = colorram_id;
            } else if (addr >= 0xDC00 && addr < 0xE000) {
                device_id = ACID_CIA;
            } else {
                device_id = io_id;
            }
        } else if (!pla->outputs.n_roml) {
            device_id = cartridge_roml_id;
        } else if (!pla->outputs.n_romh) {
            device_id = cartridge_romh_id;
        }
        
        bus->acid_per_bankidx[bank_idx] = device_id;
    }
}

void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla,
                                   uint8_t ram_id, uint8_t basic_id, uint8_t kernal_id,
                                   uint8_t charrom_id, uint8_t io_id, uint8_t cartridge_roml_id,
                                   uint8_t cartridge_romh_id, uint8_t colorram_id) {
    pla_906114_01_t* pla_impl = (pla_906114_01_t*)pla;
    
    // Generate all 32 memory modes (5-bit combinations of LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (int mode = 0; mode < 32; mode++) {
        // Set PLA inputs based on mode
        pla_impl->inputs.n_loram = (mode & 0x01) == 0;    // LORAM (inverted)
        pla_impl->inputs.n_hiram = (mode & 0x02) == 0;    // HIRAM (inverted)
        pla_impl->inputs.n_charen = (mode & 0x04) == 0;   // CHAREN (inverted)
        pla_impl->inputs.n_exrom = (mode & 0x08) == 0;    // EXROM (inverted)
        pla_impl->inputs.n_game = (mode & 0x10) == 0;     // GAME (inverted)
        
        // Set other inputs for normal CPU operation
        pla_impl->inputs.aec = true;     // CPU has bus control
        pla_impl->inputs.ba = true;      // Bus available
        pla_impl->inputs.r_w = true;     // Read mode
        pla_impl->inputs.n_cas = true;   // No CAS
        pla_impl->inputs.n_va14 = true;  // VA14 high
        pla_impl->inputs.va13 = false;   // VA13 low
        pla_impl->inputs.va12 = false;   // VA12 low
        pla_impl->inputs.n_ce = false;   // Chip enabled
          // Populate mapping for this mode
        c64_bus_populate_pla_mapping(bus, pla, ram_id, basic_id, kernal_id, 
                                   charrom_id, io_id, cartridge_roml_id, 
                                   cartridge_romh_id, colorram_id);        // Copy the mapping to the mode-specific array
        memcpy(bus->acid_per_bankidx_per_mode[mode], bus->acid_per_bankidx, sizeof(bus->acid_per_bankidx));
    }
}

// ============================================================================
// ADAPTER INTERFACES - Integrated adapter initialization
// ============================================================================

// Adapter function implementations for C64 bus
static uint8_t c64_bus_adapter_bus_read(void* context, uint16_t address) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    return c64_bus_memory_read(c64_bus, address);
}

static void c64_bus_adapter_bus_write(void* context, uint16_t address, uint8_t value) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    c64_bus_memory_write(c64_bus, address, value);
}

static void c64_bus_adapter_non_cpu_cycle(void* context) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    c64_non_cpu_cycle(c64_bus->c64);
}

// Control lines adapter functions
static uint32_t c64_control_lines_get(void* context) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert the C64's uint8_t control_lines to the new uint32_t format
    // For now, just extend it to 32 bits
    return (uint32_t)c64_bus->control_lines;
}

static void c64_control_lines_set(void* context, uint32_t lines) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert back to the C64's uint8_t format
    // For now, just truncate (assumes lower 8 bits contain the relevant data)
    c64_bus->control_lines = (uint8_t)(lines & 0xFF);
}

// I/O port adapter functions
static void c64_io_port_output_changed(void* context, uint8_t ddr, uint8_t port_data, uint8_t effective_output) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Use the effective output for mode switching
    c64_bus_mode_switch(c64_bus, effective_output);
}

static uint8_t c64_io_port_input_read(void* context) {
    // For C64, the I/O port typically reads the current port state
    // This can be extended to read actual external signals if needed
    (void)context; // Unused for now
    return 0xFF; // Default to all inputs high
}

void c64_bus_init_adapters(c64_bus_t* c64_bus) {
    // Initialize bus cycle adapter
    c64_bus->bus_adapter.bus_read = c64_bus_adapter_bus_read;
    c64_bus->bus_adapter.bus_write = c64_bus_adapter_bus_write;
    c64_bus->bus_adapter.cycle_tick = c64_bus_adapter_non_cpu_cycle;
    c64_bus->bus_adapter.context = c64_bus;
    
    // Initialize control lines adapter
    c64_bus->control_lines_adapter.get_lines = c64_control_lines_get;
    c64_bus->control_lines_adapter.set_lines = c64_control_lines_set;
    c64_bus->control_lines_adapter.context = c64_bus;
    
    // Initialize I/O port adapter
    c64_bus->io_port_adapter.output_pins_changed = c64_io_port_output_changed;
    c64_bus->io_port_adapter.read_external_pins = c64_io_port_input_read;
    c64_bus->io_port_adapter.context = c64_bus;
}

// ============================================================================
// ACID ALLOCATION AND MANAGEMENT
// ============================================================================

void c64_bus_initialize_acids(c64_bus_t* bus) {
    // Initialize ACID allocation counters
    bus->next_write_acid = 1;  // Start after ACID_UNMAPPED (0)
    bus->next_read_acid = 8;   // Start at first read-only ACID
    
    // Initialize chip ID to ACID mapping
    for (int i = 0; i < 16; i++) {
        bus->chip_id_to_acid[i] = ACID_UNMAPPED;
    }
    
    // Initialize all access callbacks to detached/unmapped defaults
    for (int i = 0; i < 16; i++) {
        bus->access_callback_per_acid[i].read_func = c64_detached_read;
        bus->access_callback_per_acid[i].write_func = c64_detached_write;
        bus->access_callback_per_acid[i].context = NULL;
    }
}

uint8_t c64_bus_allocate_acid(c64_bus_t* bus, uint8_t chip_id, 
                              chip_read_func_t read_func, chip_write_func_t write_func, 
                              void* context) {
    uint8_t allocated_acid = ACID_UNMAPPED;
    
    // Determine allocation strategy based on callback availability
    if (write_func != NULL) {
        // Device supports write operations - allocate from write-capable range (1-7)
        if (bus->next_write_acid <= 7) {
            allocated_acid = bus->next_write_acid++;
        } else {
            // Fall back to read-only range if write range is exhausted
            // This shouldn't happen in normal C64 configuration
            if (bus->next_read_acid < 16) {
                allocated_acid = bus->next_read_acid++;
            }
        }
    } else if (read_func != NULL) {
        // Device only supports read operations - allocate from read-only range (8+)
        if (bus->next_read_acid < 16) {
            allocated_acid = bus->next_read_acid++;
        }
    }
    
    // If allocation succeeded, register the callbacks and context
    if (allocated_acid != ACID_UNMAPPED && chip_id < 16) {
        bus->chip_id_to_acid[chip_id] = allocated_acid;
        
        // Set up the access callbacks
        if (read_func) {
            bus->access_callback_per_acid[allocated_acid].read_func = read_func;
        }
        if (write_func) {
            bus->access_callback_per_acid[allocated_acid].write_func = write_func;
        }
        bus->access_callback_per_acid[allocated_acid].context = context;
    }
      return allocated_acid;
}
