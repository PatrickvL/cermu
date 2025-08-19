#include "c64_bus.h"
#include "c64.h"
#include "../../chip/io/mos6526.h"
#include "../../chip/cpu/mos6510/mos6510.h"
#include "../../core/aiemuc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// ============================================================================
// BANKING CHANGE CALLBACK
// ============================================================================

void c64_bus_on_banking_change(void* bus_ptr, uint8_t banking_state) {
    c64_bus_t* bus = (c64_bus_t*)bus_ptr;
    if (!bus || !bus->c64) return;
    
    // Convert MOS6510 banking state to C64 bus mode
    // banking_state contains LORAM (bit 0), HIRAM (bit 1), CHAREN (bit 2)
    // Need to add EXROM and GAME bits from system lines
    uint8_t exrom = (bus->system_lines & SYS_MASK_EXROM) ? 1 : 0;
    uint8_t game = (bus->system_lines & SYS_MASK_GAME) ? 1 : 0;
    
    // Construct full PLA mode: GAME | EXROM | CHAREN | HIRAM | LORAM
    uint8_t pla_mode = (game << 4) | (exrom << 3) | (banking_state & 0x07);
    
    // Switch to new memory mapping mode
    c64_bus_mode_switch(bus, pla_mode);
    
    printf("Banking change: MOS6510 state=0x%02X, PLA mode=0x%02X\n", 
           banking_state, pla_mode);
}

// Chip accessor functions - these use the chip descriptor's read/write callbacks using bus_state_t pattern

// Cartridge expansion port IO functions removed (io1_read, io1_write, io2_read, io2_write)
// These were unused and producing compiler warnings
// TODO: Re-implement when cartridge support is added

// Simple 4KB bank calculation for optimized system (0-15)
static inline int8_t c64_bus_get_bank(uint16_t address) {
    return address >> 12;  // Extract 4KB bank (0-15)
}

// ============================================================================
// UNIFIED ADDRESS CALCULATION MACRO - Shared address calculation for performance
// ============================================================================

/**
 * Unified address calculation macro for memory access optimization.
 * Generates branchless address calculation for the unified memory buffer.
 * This macro provides optimal instruction scheduling opportunities for the compiler.
 * 
 * Variables that will be created/used by this macro:
 * - chip: uint8_t - The target chip ID
 * - is_ram: uint32_t - 0xFFFFFFFF for RAM, 0 for others
 * - offset_mask: uint32_t - 0xFFFF for RAM, 0x0FFF for others
 * - chip_offset: uint32_t - Masked address offset
 * - base_offset: uint32_t - Chip base offset in unified buffer
 * - ram_adjustment: uint32_t - Additional offset for RAM (0x9000)
 * - unified_addr: uint32_t - Final unified buffer address
 * 
 * Usage: C64_BUS_UNIFIED_ADDRESS_CALC(chip_value, address)
 */
#define C64_BUS_UNIFIED_ADDRESS_CALC(chip, addr) \
    do { \
        uint32_t is_ram = (-(chip == CHIP_RAM)); \
        uint32_t offset_mask = is_ram | 0x1FFF; \
        uint32_t chip_offset = (addr) & offset_mask; \
        uint32_t base_offset = chip << 13; \
        uint32_t ram_adjustment = is_ram & 0x1000; \
        unified_addr = base_offset - ram_adjustment + chip_offset; \
    } while(0)

// ULTRA-OPTIMIZED VIC-II MEMORY READ - Better performance than CPU version
// VIC-II uses pre-selected active array (indexed by CHIP), can only read, never write
// Uses unified memory buffer for branchless access to ROM and RAM
void c64_bus_vic_read(c64_bus_t* c64_bus, uint16_t address) {
    c64_bus->state.addr = address; // Perhaps this is no longer needed
    // Extract 4KB bank from address (0-15 for VIC-II's 64KB addressable space)
    uint8_t vicii_bank = c64_bus_get_bank(address);
    // Get raw CHIP directly from pre-selected active array (no mode indexing)
    uint8_t chip = c64_bus->vicii_chip_per_bank[vicii_bank];
    
    // Early return for unmapped regions - use whatever is on the bus
    if (chip == CHIP_UNMAPPED) {
        return; // Leave c64_bus->state.data unchanged (floating bus state)
    }
    
    // BRANCHLESS unified address calculation using shared macro
    // Chip mapping: 0=ROML, 1=ROMH, 2=KERNAL, 3=BASIC, 4=CHARROM, 5=RAM
    // Unified offsets: 0x0000=ROML, 0x2000=ROMH, 0x4000=KERNAL, 0x6000=BASIC, 0x8000=CHARROM, 0x9000=RAM
    uint32_t unified_addr;
    C64_BUS_UNIFIED_ADDRESS_CALC(chip, address);
    c64_bus->state.data = c64_bus->unified_memory_buffer[unified_addr];
}

/**
 * New cycle-accurate memory tick function for the refactored architecture.
 * This function will be used by the new MOS6510 implementation to handle
 * memory access in a cycle-accurate manner.
 *
 * Optimized for register-based calling convention to avoid host stack accesses.
 * Takes bus state by value and returns updated bus state for efficient register usage.
 *
 * NOTE: I/O port addresses (0-1) are now handled directly by mos6510_tick()
 * early in the CPU tick to prevent memory system from overwriting I/O port data.
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param bus_state Current bus state (passed by value for register optimization)
 * @return Updated bus state (for register-to-register operation)
 */
bus_state_t REGISTER_CALL c64_memory_tick(c64_bus_t* c64_bus, bus_state_t bus_state) {
    if (unlikely(!c64_bus)) return bus_state;
    
    // Determine if this is a read or write operation
    bool is_read = bus_state.lines & BUS_MASK_RW;
    uint16_t address = bus_state.addr;
    uint8_t cpu_bank = c64_bus_get_bank(address);     // Extract 4KB bank (0-15)
    
    // NOTE: I/O port addresses (0-1) are now handled by mos6510_tick() early in the CPU tick
    // This prevents the memory system from overwriting I/O port read data with RAM data
    
    if (is_read) {
        // === READ OPERATION ===
        uint8_t chip = decode_read_chip(c64_bus->cpu_encoded_chip_per_bank[cpu_bank]);
        
        // Fast path for unified buffer access (RAM, ROM, UNMAPPED)
        if (likely(chip <= CHIP_RAM)) {
            // Unified address calculation using shared macro
            uint32_t unified_addr;
            C64_BUS_UNIFIED_ADDRESS_CALC(chip, address);
            bus_state.data = c64_bus->unified_memory_buffer[unified_addr];
        } else if (chip == CHIP_IO) {
            // I/O region: set pending flag for chips to handle in their tick functions
            bus_set_io_pending(&bus_state);
            // Leave bus_state.data unchanged (floating bus behavior)
        }
        // Note: CHIP values above CHIP_IO are handled by setting I/O pending flag
        
    } else {
        // === WRITE OPERATION ===
        uint8_t chip = decode_write_chip(c64_bus->cpu_encoded_chip_per_bank[cpu_bank]);
        
        // FAST PATH: Direct unified buffer write for CHIP_RAM (most common case)
        if (likely(chip == CHIP_RAM)) {
            // Direct unified buffer write
            // Unified RAM offset: 0x9000 + address
            c64_bus->unified_memory_buffer[0x9000 + address] = bus_state.data;
        } else if (chip == CHIP_IO) {
            // I/O region: set pending flag for chips to handle in their tick functions
            bus_set_io_pending(&bus_state);
        }
        // Note: Writes to UNMAPPED and ROM areas are ignored (no action needed)
    }
    
    return bus_state;
}

void c64_bus_system_destroy(void* chip) {
    c64_bus_t* c64_bus = (c64_bus_t*)chip;
    if (c64_bus && c64_bus->allocated_buffer) {
        aiemuc_aligned_free(c64_bus->allocated_buffer);
    }
    free(chip);
}

void* c64_bus_system_create(chip_descriptor_t* desc) {
    c64_bus_t* c64_bus = (c64_bus_t*)calloc(1, sizeof(c64_bus_t));
    if (!c64_bus) return NULL;
    c64_bus->desc = desc;
    // Initialize bus state
    c64_bus->state.addr = 0;
    c64_bus->state.data = 0;
    c64_bus->state.lines = BUS_MASK_BA | BUS_MASK_AEC | BUS_MASK_RDY;
      // Initialize system lines with default cartridge signals (no cartridge)
    c64_bus->system_lines = SYS_MASK_EXROM | SYS_MASK_GAME;  // Both high = no cartridge
    
    // Note: pla_banking_mode will be initialized by c64_bus_mode_switch()
    // after PLA mapping data is set up in c64_pla_maps_generate()
    // Note: calloc already zeroed *chip_per_bank* arrays
    
    // Initialize the integrated adapter interfaces
    c64_bus_init_adapters(c64_bus);
    
    return c64_bus;
}

void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64) {
    c64_bus->c64 = c64;  // Store as opaque pointer
    
    // Initialize ROM/RAM pointers and allocate unified buffer with default configuration
    c64_config_t default_config;
    c64_config_init_defaults(&default_config);
    c64_bus_init_unified_pointers(c64_bus, c64, &default_config);
}

chip_descriptor_t c64_bus_descriptor = {
    .description = "C64 System Bus Controller",
    .create = c64_bus_system_create,
    .destroy = c64_bus_system_destroy,
    .bus_attach = NULL,
    .bank_change = NULL
};

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode) {
    printf("c64_bus_mode_switch: %02X\n", mode);
    // Update the optimized banking for the current mode
    c64_bus->pla_banking_mode = mode & 0x1F;
    
    memcpy(c64_bus->cpu_encoded_chip_per_bank, c64_bus->cpu_encoded_chip_per_bank_per_mode[mode], 16);
    // Also copy VIC-II active array for optimal performance
    memcpy(c64_bus->vicii_chip_per_bank, c64_bus->vicii_chip_per_bank_per_mode[mode], 16);
}

static void c64_bus_update_pla_mode(c64_bus_t* c64_bus) {

    uint8_t cpu_port_bits = c64_bus->pla_banking_mode & 0x07;
    printf("c64_bus_update_pla_mode.cpu_port_bits: %02X\n", cpu_port_bits);
    uint8_t pla_mode = c64_bus_generate_pla_mode(c64_bus, cpu_port_bits);
    c64_bus_mode_switch(c64_bus, pla_mode);
}

// Waits for the bus to be ready, ticking non-CPU chips.
void c64_wait_for_bus_ready(c64_bus_t *c64_bus, bool is_read_cycle) {
    c64_t* c64 = c64_bus->c64;

    if (is_read_cycle) {
        // A CPU read must wait for VIC to release the bus (AEC high) AND
        // for the BA/RDY line to be high.
        while (!(c64_bus->state.lines & BUS_MASK_AEC) || !(c64_bus->state.lines & BUS_MASK_BA)) {
            c64_non_cpu_cycle(c64);
        }
    } else {
        // A CPU write only needs to wait for VIC to release the address bus.
        // It is NOT affected by the BA/RDY line.
        while (!(c64_bus->state.lines & BUS_MASK_AEC)) {
            c64_non_cpu_cycle(c64);
        }
    }
}

uint8_t c64_bus_read_cycle(c64_bus_t *c64_bus, uint16_t addr) {
    // The core cycle function handles all bus contention (waiting) and performs
    // the final tick for all non-CPU chips. This happens concurrently with the
    // CPU's memory access.
    c64_wait_for_bus_ready(c64_bus, true);
    // The bus is now guaranteed to be ready for the CPU.
    bus_state_t bus_state = c64_bus->state;
    bus_state.addr = addr;
    bus_state.lines |= BUS_MASK_RW; // Set read mode
    c64_bus->state = c64_memory_tick(c64_bus, bus_state);
    // Tick system through complete cycle
    c64_non_cpu_cycle(c64_bus->c64);
    return c64_bus->state.data;
}

void c64_bus_write_cycle(c64_bus_t* c64_bus, uint16_t addr, uint8_t value) {
    // The core cycle function handles all bus contention (waiting) and performs
    // the final tick for all non-CPU chips.
    c64_wait_for_bus_ready(c64_bus, false);
    // The bus is now guaranteed to be ready for the CPU.
    bus_state_t bus_state = c64_bus->state;
    bus_state.addr = addr;
    bus_state.data = value;
    bus_state.lines &= ~BUS_MASK_RW; // Clear read/write bit for write mode
    c64_bus->state = c64_memory_tick(c64_bus, bus_state);
    // Advance system
    c64_non_cpu_cycle(c64_bus->c64);
}

// ============================================================================
// PLA integration functions
// ============================================================================
#include "../../chip/logic/pla.h"

uint8_t pla_906114_01_outputs_to_chip(pla_906114_01_t* pla) {
    if (!pla->outputs.n_casram) {
        // Main RAM (read/write)
        return CHIP_RAM;
    } else if (!pla->outputs.n_basic) {
        // BASIC ROM (read-only)
        return CHIP_BASIC;
    } else if (!pla->outputs.n_kernal) {
        // KERNAL ROM (read-only)
        return CHIP_KERNAL;
    } else if (!pla->outputs.n_charrom) {
        // Character ROM (read-only)
        return CHIP_CHARROM;
    } else if (!pla->outputs.n_io) {
        // I/O region - includes Color RAM, VIC-II, SID, CIA, etc. (read-write)
        return CHIP_IO; // c64_memory_tick will handle mapping to full 16 I/O pages
    } else if (!pla->outputs.n_roml) {
        // Cartridge ROM Low (read-only)
        return CHIP_ROML;
    } else if (!pla->outputs.n_romh) {
        // Cartridge ROM High (read-only)
        return CHIP_ROMH;
    }

    // Default to unmapped when no chip is selected
    // This happens when all PLA outputs are inactive (high)
    return CHIP_UNMAPPED;
}

void c64_bus_populate_cpu_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Set other inputs for normal CPU operation (not VIC-II access)
    pla->inputs.n_aec = false;   // CPU has bus control (AEC high (#EAC low) = CPU access)
    pla->inputs.ba = true;       // Bus available (BA high = no DMA)
    pla->inputs.n_cas = false;   // CAS active (CAS low = enable RAM access for CPU)
    // Map memory regions based on PLA outputs
    for (uint32_t bank = 0; bank < 16; bank++) {
        // Configure PLA for READ mode
        pla->inputs.r_w = true;  // Read mode
        // Set address in PLA (will call pla_906114_01_update_outputs)
        pla_906114_01_set_cpu_address_bank((pla_906114_01_t*)pla, (uint8_t)bank);
        // Determine read CHIP based on PLA outputs for read mode
        uint8_t read_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);
        // I/O port handling now done by MOS6510 bus interface callbacks - use regular RAM

        // Configure PLA for WRITE mode
        pla->inputs.r_w = false;  // Write mode
        pla_906114_01_update_outputs((pla_906114_01_t*)pla);
        uint8_t write_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);
        // I/O port handling now done by MOS6510 bus interface callbacks - use regular RAM

        // Note: ROM areas (BASIC, KERNAL, Character ROM, Cartridge) are not writable, 
        // so write_chip remains CHIP_UNMAPPED for those regions

        // Encode both read and write CHIPs into the mapping
        bus->cpu_encoded_chip_per_bank[bank] = encode_chip_rw(read_chip, write_chip);
    }
}

void c64_bus_populate_vicii_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Set other inputs for VIC-II access (not normal CPU operation)
    pla->inputs.n_aec = true;   // VIC-II has bus control (AEC low (#EAC high) = VIC-II access)
    pla->inputs.ba = false;     // Bus available (BA low = DMA)
    pla->inputs.n_cas = false;  // CAS active for VIC-II regular memory access (not refresh)
    // Configure PLA for READ mode (VIC-II can only read, never write)
    pla->inputs.r_w = true;     // Read mode

    // VIC-II can access all 16 banks (full 16-bit address space)
    // VA14 and VA15 are driven by CIA, so VIC-II can reach all 16 banks
    for (uint32_t bank = 0; bank < 16; bank++) {
        // Set address in PLA (will call pla_906114_01_update_outputs)
        pla_906114_01_set_vicii_address_bank((pla_906114_01_t*)pla, (uint8_t)bank);
        // Determine read CHIP based on PLA outputs for read mode
        uint8_t read_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);
        // VIC-II banking stores direct CHIP values, no encoding needed
        bus->vicii_chip_per_bank[bank] = read_chip;        
    }
}

void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla) {
    // Generate all 32 CPU memory modes (5-bit combinations of LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (int mode = 0; mode < 32; mode++) {
        // Set PLA inputs based on mode
        pla->inputs.n_loram = (mode & 0x01) == 0;    // LORAM (inverted)
        pla->inputs.n_hiram = (mode & 0x02) == 0;    // HIRAM (inverted)
        pla->inputs.n_charen = (mode & 0x04) == 0;   // CHAREN (inverted)
        pla->inputs.n_exrom = (mode & 0x08) == 0;    // EXROM (inverted)
        pla->inputs.n_game = (mode & 0x10) == 0;     // GAME (inverted)
        // CPU address bits will be set during populate_pla_mapping for each bank
        // Populate mapping for this mode
        c64_bus_populate_cpu_pla_mapping(bus, pla);
        // Copy the CPU mapping to the mode-specific array
        memcpy(bus->cpu_encoded_chip_per_bank_per_mode[mode], bus->cpu_encoded_chip_per_bank, 16);
    }
    
    // Generate VIC-II memory modes (different steering parameters)
    // VIC-II uses: #GAME, #EXROM, #VA14 = 3 bits, but #VA14 is handled per-bank
    // So we have 4 unique configs based on #GAME and #EXROM
    // We'll repeat these 4 configs across all 32 modes for easy indexing
    for (int cpu_mode = 0; cpu_mode < 32; cpu_mode++) {
        // Extract relevant bits for VIC-II: #GAME, #EXROM from CPU mode
        bool n_game = (cpu_mode & 0x10) == 0;     // GAME (inverted)
        bool n_exrom = (cpu_mode & 0x08) == 0;    // EXROM (inverted)
        
        // Set PLA inputs for VIC-II (only the relevant ones)
        pla->inputs.n_game = n_game;
        pla->inputs.n_exrom = n_exrom;
        // Note: #VA14 will be set during populate_vicii_pla_mapping for each bank
        
        // Populate VIC-II mapping for this mode (stores direct CHIPs)
        c64_bus_populate_vicii_pla_mapping(bus, pla);
        // Copy the VIC-II raw CHIPs to the mode-specific array
        memcpy(bus->vicii_chip_per_bank_per_mode[cpu_mode], bus->vicii_chip_per_bank, 16);
    }
}

// ============================================================================
// PLA MODE GENERATION - Convert CPU port bits + cartridge signals to 5-bit PLA mode
// ============================================================================

uint8_t c64_bus_generate_pla_mode(c64_bus_t* c64_bus, uint8_t cpu_port_bits) {
    printf("c64_bus_generate_pla_mode.cpu_port_bits: %02X\n", cpu_port_bits);
    // The PLA expects a 5-bit mode value with the following bit mapping:
    // Bit 0: !LORAM (from CPU port bit 0)
    // Bit 1: !HIRAM (from CPU port bit 1) 
    // Bit 2: !CHAREN (from CPU port bit 2)
    // Bit 3: !EXROM (from cartridge signal)
    // Bit 4: !GAME (from cartridge signal)
    
    // Extract CPU I/O port control bits (bits 0-2 of $0001)
    uint8_t pla_mode = (~cpu_port_bits) & 0x07; // LORAM (bit 0) | HIRAM (bit 1) | CHAREN (bit 2)
    
    // Add cartridge control signals from system lines
    pla_mode |= ((c64_bus->system_lines & SYS_MASK_EXROM) ? 0 : 0x08); // EXROM (bit 3)
    pla_mode |= ((c64_bus->system_lines & SYS_MASK_GAME) ? 0 : 0x10);  // GAME (bit 4)
    printf("c64_bus_generate_pla_mode.pla_mode: %02X\n", pla_mode);

    return pla_mode;
}

// ============================================================================
// ADAPTER INTERFACES - Integrated adapter initialization
// ============================================================================

// Adapter function implementations for C64 bus
static uint8_t c64_bus_adapter_bus_read_cycle(void* context, uint16_t address) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    return c64_bus_read_cycle(c64_bus, address);
}

static void c64_bus_adapter_bus_write_cycle(void* context, uint16_t address, uint8_t value) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    c64_bus_write_cycle(c64_bus, address, value);
}

// Control lines adapter functions
static uint32_t c64_control_lines_get(void* context) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert the C64's uint8_t control_lines to the new uint32_t format
    // For now, just extend it to 32 bits
    return (uint32_t)c64_bus->state.lines;
}

static void c64_control_lines_set(void* context, uint32_t lines) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert back to the C64's uint8_t format
    // For now, just truncate (assumes lower 8 bits contain the relevant data)
    c64_bus->state.lines = (uint8_t)(lines & 0xFF);
}

void c64_bus_init_adapters(c64_bus_t* c64_bus) {
    // Initialize bus cycle adapter
    c64_bus->bus_adapter.context = c64_bus;
    c64_bus->bus_adapter.bus_read_cycle = c64_bus_adapter_bus_read_cycle;
    c64_bus->bus_adapter.bus_write_cycle = c64_bus_adapter_bus_write_cycle;
    
    // Initialize control lines adapter
    c64_bus->control_lines_adapter.get_lines = c64_control_lines_get;
    c64_bus->control_lines_adapter.set_lines = c64_control_lines_set;
    c64_bus->control_lines_adapter.context = c64_bus;
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

    c64_bus_update_pla_mode(c64_bus);
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
    
    c64_bus_update_pla_mode(c64_bus);
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
    
    c64_bus_update_pla_mode(c64_bus);
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

// Update c64_bus_get_chip_description to use the new mapping for all CHIPs
bool c64_bus_get_chip_description(const c64_bus_t* bus, uint8_t chip, chip_description_t* out) {
    if (!bus || !out || chip >= CHIP_MAX) return false;

    memset(out, 0, sizeof(*out));
    // For now, return false for chip lookup until proper registry is implemented
    chip_entry_t* entry = NULL; // c64_bus_chip_to_entry[chip];
    if (entry) {
        out->base = entry->base_address;
        out->size = entry->size;
        out->label = entry->desc && entry->desc->description ? entry->desc->description : "?";
        return true;
    }    

    // Special cases
    switch (chip) {
        case CHIP_UNMAPPED:
            out->base = 0;
            out->size = 0;
            out->label = "Unmapped";
            return true;
        default:
            break;
    }

    out->base = 0;
    out->size = 0;
    out->label = "?";
    return false;
}

// Return a concise title string for each CHIP (for legend/tooling/GUI)
const char* c64_bus_chip_to_title(uint8_t chip) {
    switch (chip) {
        case CHIP_ROML:
            return "ROML";
        case CHIP_ROMH:
            return "ROMH";
        case CHIP_KERNAL:
            return "KERNAL";
        case CHIP_BASIC:
            return "BASIC";
        case CHIP_CHARROM:
            return "CHARROM";
        case CHIP_RAM:
            return "RAM";
        case CHIP_UNMAPPED:
            return "-";
        case CHIP_D0_VIC:
        case CHIP_D1_VIC:
        case CHIP_D2_VIC:
        case CHIP_D3_VIC:
            return "VIC-II";
        case CHIP_D4_SID:
        case CHIP_D5_SID:
        case CHIP_D6_SID:
        case CHIP_D7_SID:
            return "SID";
        case CHIP_D8_COLORRAM:
        case CHIP_D9_COLORRAM:
        case CHIP_DA_COLORRAM:
        case CHIP_DB_COLORRAM:
            return "COLORRAM";
        case CHIP_DC_CIA1:
            return "CIA1";
        case CHIP_DD_CIA2:
            return "CIA2";
        case CHIP_DE_IO1:
            return "IO1";
        case CHIP_DF_IO2:
            return "IO2";
        default:
            return "?";
    }
}

// Utility: Convert a size in bytes to a human-readable string ("256B", "4KB", etc.)
const char* c64_bus_size_to_str(size_t size) {
    static char buf[32];  // Increased buffer size to prevent truncation
    if (size >= (1 << 20) && (size % (1 << 20)) == 0) {
        snprintf(buf, sizeof(buf), "%zuMB", size / (1 << 20));
    } else if (size >= 1024 && (size % 1024) == 0) {
        snprintf(buf, sizeof(buf), "%zuKB", size / 1024);
    } else {
        snprintf(buf, sizeof(buf), "%zuB", size);
    }
    return buf;
}


/**
 * Initialize RAM/ROM pointers to point into the unified memory buffer.
 * This eliminates separate memory allocations and ensures consistency.
 * Uses configuration structure to determine cartridge ROM presence.
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param c64_system Pointer to the C64 system (for pointer updates)
 * @param config Pointer to the C64 system configuration structure
 */
void c64_bus_init_unified_pointers(c64_bus_t* c64_bus, void* c64_system, const c64_config_t* config) {
    if (!c64_bus || !c64_system || !config) return;
    
    // Store cartridge ROM presence flags from configuration
    c64_bus->roml_present = config->roml_present;
    c64_bus->romh_present = config->romh_present;
    
    // Clean up any existing allocation
    if (c64_bus->allocated_buffer) {
        aiemuc_aligned_free(c64_bus->allocated_buffer);
        c64_bus->allocated_buffer = NULL;
    }
    
    // Calculate required memory size and offset
    // Full layout: ROML(8KB) + ROMH(8KB) + KERNAL(8KB) + BASIC(8KB) + CHARROM(4KB) + RAM(64KB) = 100KB
    size_t required_size = 100 * 1024; // Start with full size
    size_t offset = 0;                  // Offset to subtract from allocated buffer
    
    if (!config->roml_present && !config->romh_present) {
        // Save 16KB by not allocating space for both cartridge ROMs
        required_size = 84 * 1024;  // KERNAL(8KB) + BASIC(8KB) + CHARROM(4KB) + RAM(64KB)
        offset = 16 * 1024;         // Offset so KERNAL (offset 0x4000) becomes start of buffer
    } else if (!config->roml_present) {
        // Save 8KB by not allocating ROML
        required_size = 92 * 1024;  // ROMH(8KB) + KERNAL(8KB) + BASIC(8KB) + CHARROM(4KB) + RAM(64KB)
        offset = 8 * 1024;          // Offset so ROMH (offset 0x2000) becomes start of buffer
    } else if (!config->romh_present) {
        // Save 8KB by not allocating ROMH (more complex layout)
        // For now, keep simple and allocate full space
        // TODO: Implement optimized layout for ROML-only cartridges
        required_size = 100 * 1024;
        offset = 0;
    }
    
    // Allocate aligned memory for optimal cache performance
    c64_bus->allocated_buffer = (uint8_t*)aiemuc_aligned_alloc(64, required_size);
    
    if (!c64_bus->allocated_buffer) {
        printf("c64_bus: ERROR - Failed to allocate unified memory buffer (%zu KB)\n", required_size / 1024);
        return;
    }
    
    c64_bus->allocated_size = required_size;
    
    // Apply pointer arithmetic trick - subtract offset so unused ROM regions point before allocated memory
    c64_bus->unified_memory_buffer = c64_bus->allocated_buffer - offset;
    
    // Initialize allocated memory to zero
    memset(c64_bus->allocated_buffer, 0, required_size);
    
    c64_t* c64 = (c64_t*)c64_system;
    uint8_t* buffer = c64_bus->unified_memory_buffer;
    
#define DO(c64_device, offset, present_flag) \
    if (c64_device && present_flag) { \
        /* Free existing memory if it was dynamically allocated */ \
        if (c64_device->memory && c64_device->memory != buffer + offset) { \
            free(c64_device->memory); \
        } \
        c64_device->memory = buffer + offset; \
    } else if (c64_device) { \
        /* ROM not present - set to NULL and free existing if needed */ \
        if (c64_device->memory) { \
            free(c64_device->memory); \
            c64_device->memory = NULL; \
        } \
    }

    // Point the following devices to their respective unified buffer offset
    // Note: offsets work due to pointer arithmetic trick with offset subtraction
    DO(c64->cartridge_roml, 0x0000, c64_bus->roml_present); // CHIP_ROML
    DO(c64->cartridge_romh, 0x2000, c64_bus->romh_present); // CHIP_ROMH
    DO(c64->kernal, 0x4000, true); // CHIP_KERNAL - always present
    DO(c64->basic, 0x6000, true);  // CHIP_BASIC - always present
    DO(c64->charrom, 0x8000, true); // CHIP_CHARROM - always present
    DO(c64->ram, 0x9000, true);     // CHIP_RAM - always present
#undef DO    
    
    printf("c64_bus: Allocated %zu KB unified buffer (saved %zu KB), ROML:%s ROMH:%s\n",
           required_size / 1024,
           (100 * 1024 - required_size) / 1024,
           config->roml_present ? "yes" : "no",
           config->romh_present ? "yes" : "no");
}
