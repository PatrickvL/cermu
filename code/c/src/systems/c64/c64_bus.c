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
    c64_bus->desc = desc;    // Initialize bus state
    c64_bus->address = 0;
    c64_bus->data = 0;
    c64_bus->control_lines = BA_LINE | AEC_LINE | RDY_LINE;
      // Initialize system lines with default cartridge signals (no cartridge)
    c64_bus->system_lines = SYS_MASK_EXROM | SYS_MASK_GAME;  // Both high = no cartridge
    
    // Initialize CPU port state to default (LORAM=1, HIRAM=1, CHAREN=1)
    c64_bus->cpu_port_state = 0x07;  // Default CPU port state
    
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
                                 uint8_t cartridge_romh_id, uint8_t colorram_id) {
    // Clear current mapping
    for (int i = 0; i < 32; i++) {
        bus->acid_per_bankidx[i] = ACIDS_RW_ENCODE(ACID_UNMAPPED, ACID_UNMAPPED);
    }    // Map memory regions based on PLA outputs
    for (uint32_t addr = 0; addr < 0x10000; addr += 0x100) {
        uint8_t read_acid = ACID_UNMAPPED;
        uint8_t write_acid = ACID_UNMAPPED;
        
        // Set address in PLA
        pla_906114_01_set_address_high((pla_906114_01_t*)pla, (addr >> 8) & 0x0F);
        
        // Get bank index for this address (same for both read and write)
        int bank_idx = c64_bus_address_to_bankidx(addr);
        
        // Configure PLA for READ mode
        ((pla_906114_01_t*)pla)->inputs.r_w = true;  // Read mode
        pla_906114_01_update_outputs((pla_906114_01_t*)pla);
        
        // Determine read ACID based on PLA outputs for read mode
        if (!pla->outputs.n_casram) {
            // RAM is selected
            if (addr < 0x0002) {
                // Special handling for CPU I/O ports (read/write)
                read_acid = ACID_ZEROPAGE;
            } else {
                // Main RAM (read/write)
                read_acid = ram_id;
            }
        } else if (!pla->outputs.n_basic) {
            // BASIC ROM (read-only)
            read_acid = basic_id;
        } else if (!pla->outputs.n_kernal) {
            // KERNAL ROM (read-only)
            read_acid = kernal_id;
        } else if (!pla->outputs.n_charrom) {
            // Character ROM (read-only)
            read_acid = charrom_id;
        } else if (!pla->outputs.n_io) {
            // I/O region - determine specific chip
            if (addr >= 0xD000 && addr < 0xD400) {
                // VIC-II (read/write)
                read_acid = ACID_VIC;
            } else if (addr >= 0xD400 && addr < 0xD800) {
                // SID (read/write)
                read_acid = ACID_SID;
            } else if (addr >= 0xD800 && addr < 0xDC00) {
                // Color RAM (read/write)
                read_acid = colorram_id;
            } else if (addr >= 0xDC00 && addr < 0xE000) {
                // CIA1/CIA2 (read/write)
                read_acid = ACID_CIA;
            } else {
                // Other I/O (assume read/write for flexibility)
                read_acid = io_id;
            }
        } else if (!pla->outputs.n_roml) {
            // Cartridge ROM Low (read-only)
            read_acid = cartridge_roml_id;
        } else if (!pla->outputs.n_romh) {
            // Cartridge ROM High (read-only)
            read_acid = cartridge_romh_id;
        }        
        // Configure PLA for WRITE mode and get write bank index
        ((pla_906114_01_t*)pla)->inputs.r_w = false;  // Write mode
        pla_906114_01_update_outputs((pla_906114_01_t*)pla);
        
        // Determine write ACID based on PLA outputs for write mode
        if (!pla->outputs.n_casram) {
            // RAM is selected
            if (addr < 0x0002) {
                // Special handling for CPU I/O ports (read/write)
                write_acid = ACID_ZEROPAGE;
            } else {
                // Main RAM (read/write)
                write_acid = ram_id;
            }
        } else if (!pla->outputs.n_io) {
            // I/O region - determine specific chip (only writable devices in write mode)
            if (addr >= 0xD000 && addr < 0xD400) {
                // VIC-II (read/write)
                write_acid = ACID_VIC;
            } else if (addr >= 0xD400 && addr < 0xD800) {
                // SID (read/write)
                write_acid = ACID_SID;
            } else if (addr >= 0xD800 && addr < 0xDC00) {
                // Color RAM (read/write)
                write_acid = colorram_id;
            } else if (addr >= 0xDC00 && addr < 0xE000) {
                // CIA1/CIA2 (read/write)
                write_acid = ACID_CIA;
            } else {
                // Other I/O (assume read/write for flexibility)
                write_acid = io_id;
            }
        }
        // Note: ROM areas (BASIC, KERNAL, Character ROM, Cartridge) are not writable, 
        // so write_acid remains ACID_UNMAPPED for those regions
        
        // Use bank index for the mapping
        // Encode both read and write ACIDs into the mapping
        bus->acid_per_bankidx[bank_idx] = ACIDS_RW_ENCODE(read_acid, write_acid);
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
// PLA MODE GENERATION - Convert CPU port bits + cartridge signals to 5-bit PLA mode
// ============================================================================

uint8_t c64_bus_generate_pla_mode(c64_bus_t* c64_bus, uint8_t cpu_port_bits) {
    // The PLA expects a 5-bit mode value with the following bit mapping:
    // Bit 0: LORAM (from CPU port bit 0)
    // Bit 1: HIRAM (from CPU port bit 1) 
    // Bit 2: CHAREN (from CPU port bit 2)
    // Bit 3: EXROM (from cartridge signal)
    // Bit 4: GAME (from cartridge signal)
    
    uint8_t pla_mode = 0;
    
    // Extract CPU I/O port control bits (bits 0-2 of $0001)
    pla_mode |= (cpu_port_bits & 0x01);      // LORAM (bit 0)
    pla_mode |= (cpu_port_bits & 0x02);      // HIRAM (bit 1)
    pla_mode |= (cpu_port_bits & 0x04);      // CHAREN (bit 2)
    
    // Add cartridge control signals from system lines
    pla_mode |= ((c64_bus->system_lines & SYS_MASK_EXROM) ? 0x08 : 0); // EXROM (bit 3)
    pla_mode |= ((c64_bus->system_lines & SYS_MASK_GAME) ? 0x10 : 0);  // GAME (bit 4)
    
    return pla_mode;
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
    // Store the current CPU port state for use by cartridge functions
    c64_bus->cpu_port_state = effective_output;
    // Generate proper 5-bit PLA mode from CPU port bits and cartridge signals
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, effective_output);
    c64_bus_mode_switch(c64_bus, pla_mode);
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
        // Chip supports write operations - allocate from write-capable range (1-7)
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
        // Chip only supports read operations - allocate from read-only range (8+)
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

// ============================================================================
// CARTRIDGE INTERFACE FUNCTIONS - Control EXROM and GAME signals
// ============================================================================

/**
 * Set the EXROM signal state for cartridge control.
 * The EXROM signal controls cartridge ROM visibility and memory mapping.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @param active true = EXROM active (signal low), false = EXROM inactive (signal high)
 */
void c64_bus_set_exrom_signal(c64_bus_t* c64_bus, bool active) {
    if (!c64_bus) return;
    
    if (active) {
        c64_bus->system_lines &= ~SYS_MASK_EXROM;  // Clear bit (signal low)
    } else {
        c64_bus->system_lines |= SYS_MASK_EXROM;   // Set bit (signal high)
    }
    
    // Regenerate PLA mode with updated cartridge signals
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, c64_bus->cpu_port_state);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

/**
 * Set the GAME signal state for cartridge control.
 * The GAME signal controls cartridge ROM banking and Ultimax mode.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @param active true = GAME active (signal low), false = GAME inactive (signal high)
 */
void c64_bus_set_game_signal(c64_bus_t* c64_bus, bool active) {
    if (!c64_bus) return;
    
    if (active) {
        c64_bus->system_lines &= ~SYS_MASK_GAME;   // Clear bit (signal low)
    } else {
        c64_bus->system_lines |= SYS_MASK_GAME;    // Set bit (signal high)
    }
    
    // Regenerate PLA mode with updated cartridge signals
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, c64_bus->cpu_port_state);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

/**
 * Set both EXROM and GAME signals simultaneously for cartridge control.
 * This is more efficient than calling the individual functions separately.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @param exrom_active true = EXROM active (signal low), false = EXROM inactive (signal high)
 * @param game_active true = GAME active (signal low), false = GAME inactive (signal high)
 */
void c64_bus_set_cartridge_signals(c64_bus_t* c64_bus, bool exrom_active, bool game_active) {
    if (!c64_bus) return;
    
    // Update EXROM signal
    if (exrom_active) {
        c64_bus->system_lines &= ~SYS_MASK_EXROM;  // Clear bit (signal low)
    } else {
        c64_bus->system_lines |= SYS_MASK_EXROM;   // Set bit (signal high)
    }
      // Update GAME signal
    if (game_active) {
        c64_bus->system_lines &= ~SYS_MASK_GAME;   // Clear bit (signal low)
    } else {
        c64_bus->system_lines |= SYS_MASK_GAME;    // Set bit (signal high)
    }
    
    // Regenerate PLA mode with updated cartridge signals (once for both signals)
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, c64_bus->cpu_port_state);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

/**
 * Get the current EXROM signal state.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @return true = EXROM active (signal low), false = EXROM inactive (signal high)
 */
bool c64_bus_get_exrom_signal(c64_bus_t* c64_bus) {
    if (!c64_bus) return false;
    
    // Return inverted state (bit set = signal high = inactive)
    return (c64_bus->system_lines & SYS_MASK_EXROM) == 0;
}

/**
 * Get the current GAME signal state.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @return true = GAME active (signal low), false = GAME inactive (signal high)
 */
bool c64_bus_get_game_signal(c64_bus_t* c64_bus) {
    if (!c64_bus) return false;
    
    // Return inverted state (bit set = signal high = inactive)
    return (c64_bus->system_lines & SYS_MASK_GAME) == 0;
}
