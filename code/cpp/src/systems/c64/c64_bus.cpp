#include "c64_bus.h"
#include "c64.h"
#include "../../chip/io/mos6526.h"
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
// UNIFIED ADDRESS CALCULATION FUNCTION - Shared address calculation for performance
// ============================================================================

/**
 * Ultra-optimized unified address calculation function for memory access.
 * Pure branchless arithmetic using strategic CHIP_* numbering for maximum performance.
 * CHIP values are chosen so that (chip << 12) directly maps to buffer offsets.
 *
 * CRITICAL DEPENDENCY: This function relies on the specific CHIP_* enum values
 * in c64_bus.h. The calculation uses chip << 12 (chip * 4096) for base offsets:
 *
 * - CHIP_ROML     = 0  -> base_offset = 0x0000 (0 << 12 = 0x0000)
 * - CHIP_ROMH     = 2  -> base_offset = 0x2000 (2 << 12 = 0x2000)
 * - CHIP_KERNAL   = 4  -> base_offset = 0x4000 (4 << 12 = 0x4000)
 * - CHIP_BASIC    = 6  -> base_offset = 0x6000 (6 << 12 = 0x6000)
 * - CHIP_CHARROM  = 8  -> base_offset = 0x8000 (8 << 12 = 0x8000)
 * - CHIP_RAM      = 9  -> base_offset = 0x9000 (9 << 12 = 0x9000)
 *
 * WARNING: Changing these CHIP_* values will break address calculation!
 *
 * OPTIMIZATION: Single shift + mask operation, completely branchless.
 * Total buffer size: 0x9000 + 64KB RAM = 100KB (36KB + 64KB)
 *
 * @param chip The target chip ID (must be 0-9 for unified buffer chips)
 * @param addr The 16-bit address to access
 * @return The calculated offset into the unified memory buffer
 */
static inline uint32_t c64_bus_unified_address_calc(uint8_t chip, uint16_t addr) {
    // Ultra-branchless calculation using strategic numbering
    uint32_t base = (uint32_t)chip << 12;  // Direct offset calculation via strategic numbering
    
    // CRITICAL: addr contains original C64 memory map addresses (e.g. KERNAL 0xE000-0xFFFF)
    // Mask strips base address to get chip-relative offset (e.g. 0xE000 & 0x1FFF = 0x0000)
    // RAM uses full 0xFFFF, ROMs use 0x1FFF to prevent buffer overflow
    // CHARROM (4KB) is safe with 0x1FFF mask: max 0xDFFF & 0x1FFF = 0x0FFF stays within 4KB buffer
    return base + (addr & (0x1FFF | -(chip == CHIP_RAM)));
}

// ULTRA-OPTIMIZED VIC-II MEMORY READ - Better performance than CPU version
// VIC-II uses pre-selected active array (indexed by CHIP), can only read, never write
// Uses unified memory buffer for branchless access to ROM and RAM
void c64_bus_vic_read(c64_bus_t* c64_bus, uint16_t address) {
    BUS_SET_ADDR(c64_bus->state, address); // Perhaps this is no longer needed
    // Extract 4KB bank from address (0-15 for VIC-II's 64KB addressable space)
    uint8_t vicii_bank = c64_bus_get_bank(address);
    // Get raw CHIP directly from pre-selected active array (no mode indexing)
    uint8_t chip = c64_bus->vicii_chip_per_bank[vicii_bank];
    
    // Early return for unmapped regions - use whatever is on the bus
    if (chip == CHIP_UNMAPPED) {
        return; // Leave bus data unchanged (floating bus state)
    }
    
    // BRANCHLESS unified address calculation using shared function
    // Address calculation depends on CHIP_* enum ordering (see function documentation)
    uint32_t unified_addr = c64_bus_unified_address_calc(chip, address);
    BUS_SET_DATA(c64_bus->state, c64_bus->unified_memory_buffer[unified_addr]);
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
 * PHASE 4.1: Enhanced fast path implementation for all unified buffer chips
 * - Direct unified buffer access for CHIP_RAM, CHIP_BASIC, CHIP_KERNAL, CHIP_CHARROM, CHIP_ROML, CHIP_ROMH
 * - Bypass callback system for ROM/RAM access using pointer arithmetic
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param bus_state Current bus state (passed by value for register optimization)
 * @return Updated bus state (for register-to-register operation)
 */
bus_state_t REGISTER_CALL c64_memory_tick(c64_bus_t* c64_bus, bus_state_t bus_state) {
    if (unlikely(!c64_bus)) return bus_state;
    
    // Determine if this is a read or write operation
    bool is_read = BUS_GET_LINES(bus_state) & BUS_MASK_RW;
    uint16_t address = BUS_GET_ADDR(bus_state);
    uint8_t cpu_bank = c64_bus_get_bank(address);     // Extract 4KB bank (0-15)
    
    // NOTE: I/O port addresses (0-1) are now handled by mos6510_tick() early in the CPU tick
    // This prevents the memory system from overwriting I/O port read data with RAM data
    
    // Check if this is VIC-II cycle-stealing (RDY low) or CPU access (RDY high)
    bool is_vicii_cycle_stealing = !(BUS_GET_LINES(bus_state) & BUS_MASK_RDY);

    if (is_read) {
        // === READ OPERATION ===
        uint8_t chip;

        if (is_vicii_cycle_stealing) {
            // VIC-II cycle-stealing: use VIC-II memory mapping
            chip = c64_bus->vicii_chip_per_bank[cpu_bank];
        } else {
            // Normal CPU access: use CPU memory mapping
            chip = decode_read_chip(c64_bus->cpu_encoded_chip_per_bank[cpu_bank]);
        }

        // ENHANCED FAST PATH: Direct unified buffer access for all memory chips
        // Fast path handles: CHIP_ROML, CHIP_ROMH, CHIP_KERNAL, CHIP_BASIC, CHIP_CHARROM, CHIP_RAM
        if (likely(chip <= CHIP_RAM)) {
            // Unified address calculation using shared function - covers all ROM/RAM types
            // Address calculation depends on CHIP_* enum ordering (see function documentation)
            uint32_t unified_addr = c64_bus_unified_address_calc(chip, address);
            BUS_SET_DATA(bus_state, c64_bus->unified_memory_buffer[unified_addr]);
        } else if (chip == CHIP_IO) {
            // OPTIMIZED IO PAGE HANDLING: Direct dispatch using pre-initialized handlers
            // Calculate IO page number from address (0-15 for $D000-$DFFF)
            uint8_t io_page = (address >> 8) & 0x0F; // Extract page number from $Dx00 addresses

            // Straight call to the appropriate handler - no conditionals needed
            bus_state = c64_bus->io_handlers[io_page].read_handler(
                c64_bus->io_handlers[io_page].chip_instance, bus_state);
        } else {
            // CHIP_UNMAPPED and others: floating bus behavior
            // Leave BUS_GET_DATA(bus_state) unchanged (floating bus state)
        }
    } else {
        // === WRITE OPERATION ===
        // Write operations are only performed by CPU, not during VIC-II cycle-stealing
        // VIC-II can only read memory, never write
        if (is_vicii_cycle_stealing) {
            // During VIC-II cycle-stealing, writes are ignored (VIC-II is reading)
            // Leave bus state unchanged
        } else {
            uint8_t chip = decode_write_chip(c64_bus->cpu_encoded_chip_per_bank[cpu_bank]);

            // ENHANCED FAST PATH: Handle all writable unified buffer regions
            if (likely(chip == CHIP_RAM)) {
                // Direct unified buffer write for RAM (most common writable case)
                // RAM is at offset 0x7000 in the strategic layout
                uint32_t unified_addr = c64_bus_unified_address_calc(chip, address);
                c64_bus->unified_memory_buffer[unified_addr] = BUS_GET_DATA(bus_state);
            } else if (chip == CHIP_IO) {
                // OPTIMIZED IO PAGE HANDLING: Direct dispatch using pre-initialized handlers
                // Calculate IO page number from address (0-15 for $D000-$DFFF)
                uint8_t io_page = (address >> 8) & 0x0F; // Extract page number from $Dx00 addresses

                // Straight call to the appropriate handler - no conditionals needed
                bus_state = c64_bus->io_handlers[io_page].write_handler(
                    c64_bus->io_handlers[io_page].chip_instance, bus_state);
            } else {
                // Writes to UNMAPPED and ROM areas are ignored (no action needed)
                // ROM chips (BASIC, KERNAL, CHARROM, ROML, ROMH) are read-only in hardware
            }
        }
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
    // Initialize bus state with reset line inactive (active-low, so set bit high)
    c64_bus->state = BUS_STATE(0, 0, BUS_MASK_BA | BUS_MASK_AEC | BUS_MASK_RDY) | BUS_BIT(BUS_RES_BIT);
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

    // Initialize compact IO page handlers for efficient I/O access
    c64_bus_init_io_handlers(c64_bus);
}

chip_descriptor_t c64_bus_descriptor = {
    /* description */ "C64 System Bus Controller",
    /* create */ c64_bus_system_create,
    /* destroy */ c64_bus_system_destroy,
    /* bus_attach */ NULL,
    /* bank_change */ NULL
#ifdef IMGUI_VERSION
    , /* render_debug_window */ NULL, // No GUI debug window implemented yet
    /* render_settings_window */ NULL // No GUI settings window implemented yet
#endif
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
    c64_t* c64 = static_cast<c64_t*>(c64_bus->c64);

    if (is_read_cycle) {
        // A CPU read must wait for VIC to release the bus (AEC high) AND
        // for the BA/RDY line to be high.
        uint8_t lines = BUS_GET_LINES(c64_bus->state);
        while (!(lines & BUS_MASK_AEC) || !(lines & BUS_MASK_BA)) {
            c64_non_cpu_cycle(c64);
            lines = BUS_GET_LINES(c64_bus->state);
        }
    } else {
        // A CPU write only needs to wait for VIC to release the address bus.
        // It is NOT affected by the BA/RDY line.
        uint8_t lines = BUS_GET_LINES(c64_bus->state);
        while (!(lines & BUS_MASK_AEC)) {
            c64_non_cpu_cycle(c64);
            lines = BUS_GET_LINES(c64_bus->state);
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
    BUS_SET_ADDR(bus_state, addr);
    BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) | BUS_MASK_RW); // Set read mode
    c64_bus->state = c64_memory_tick(c64_bus, bus_state);
    // Tick system through complete cycle
    c64_non_cpu_cycle(c64_bus->c64);
    return BUS_GET_DATA(c64_bus->state);
}

void c64_bus_write_cycle(c64_bus_t* c64_bus, uint16_t addr, uint8_t value) {
    // The core cycle function handles all bus contention (waiting) and performs
    // the final tick for all non-CPU chips.
    c64_wait_for_bus_ready(c64_bus, false);
    // The bus is now guaranteed to be ready for the CPU.
    bus_state_t bus_state = c64_bus->state;
    BUS_SET_ADDR(bus_state, addr);
    BUS_SET_DATA(bus_state, value);
    BUS_SET_LINES(bus_state, BUS_GET_LINES(bus_state) & ~BUS_MASK_RW); // Clear read/write bit for write mode
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
        // I/O region - includes VIC-II, SID, Color RAM, CIA1, CIA2 (read/write)
        return CHIP_IO;
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
    // Convert the C64's bus state to the new uint32_t format
    return (uint32_t)BUS_GET_LINES(c64_bus->state);
}

static void c64_control_lines_set(void* context, uint32_t lines) {
    c64_bus_t* c64_bus = (c64_bus_t*)context;
    // Convert back to the C64's bus state format
    BUS_SET_LINES(c64_bus->state, (uint8_t)(lines & 0xFF));
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

// Chip entry lookup table for description and validation (handles irregular numbering)
typedef struct {
    uint16_t base_address;
    size_t size;
    const char* label;
} c64_chip_entry_t;

// Sparse lookup table indexed by CHIP_* values (supports irregular numbering)
static const c64_chip_entry_t c64_bus_chip_to_entry[] = {
    /* [CHIP_ROML] = */     { 0x8000, 8*1024, "Cartridge ROM Low" },
    { 0, 0, nullptr }, // CHIP_1 unused
    /* [CHIP_ROMH] = */     { 0xA000, 8*1024, "Cartridge ROM High" }, // Note: Can also map to 0xE000
    { 0, 0, nullptr }, // CHIP_3 unused
    /* [CHIP_KERNAL] = */   { 0xE000, 8*1024, "KERNAL ROM" },
    { 0, 0, nullptr }, // CHIP_5 unused
    /* [CHIP_BASIC] = */    { 0xA000, 8*1024, "BASIC ROM" },
    { 0, 0, nullptr }, // CHIP_7 unused
    /* [CHIP_CHARROM] = */  { 0xD000, 4*1024, "Character ROM" },
    /* [CHIP_RAM] = */      { 0x0000, 64*1024, "RAM" },
    /* [CHIP_UNMAPPED] = */ { 0x0000, 0, "Unmapped" },
    /* [CHIP_IO] = */       { 0xD000, 4*1024, "I/O" },
};
static const size_t CHIP_ENTRY_COUNT = sizeof(c64_bus_chip_to_entry) / sizeof(c64_bus_chip_to_entry[0]);

// Update c64_bus_get_chip_description to use the chip entry lookup table
bool c64_bus_get_chip_description(const c64_bus_t* bus, uint8_t chip, chip_description_t* out) {
    if (!bus || !out) return false;

    memset(out, 0, sizeof(*out));
    
    // Validate chip ID and get entry (handles irregular numbering via sparse array)
    if (chip >= CHIP_ENTRY_COUNT) return false;
    
    const c64_chip_entry_t* entry = &c64_bus_chip_to_entry[chip];
    
    // Entry exists if it has a label (even UNMAPPED has a label)
    if (entry->label) {
        out->base = entry->base_address;
        out->size = entry->size;
        out->label = entry->label;
        return true;
    }

    // Fallback for undefined entries
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
        case CHIP_IO:
            return "I/O";
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
    
    // Calculate required memory size for strategic layout (4KB step size)
    // Strategic layout: ROML(0x0000) + ROMH(0x2000) + KERNAL(0x4000) + BASIC(0x6000) + CHARROM(0x8000) + RAM(0x9000)
    // Total: 0x9000 (36KB) + RAM(64KB) = 100KB maximum
    size_t required_size = 100 * 1024; // Base size: 36KB + 64KB RAM
    size_t offset = 0;                  // No offset needed for strategic layout
    
    // Adjust for missing cartridge ROMs (can save space at beginning)
    if (!config->roml_present && !config->romh_present) {
        // Skip first 16KB: start at KERNAL (0x4000)
        required_size = 84 * 1024;  // (36KB - 16KB) + 64KB RAM = 84KB
        offset = 16 * 1024;         // Offset buffer start by 16KB
    } else if (!config->roml_present) {
        // Skip first 8KB: start at ROMH (0x2000)
        required_size = 92 * 1024;  // (36KB - 8KB) + 64KB RAM = 92KB
        offset = 8 * 1024;          // Offset buffer start by 8KB
    } else if (!config->romh_present) {
        // Keep ROML, skip ROMH: need custom layout
        // For simplicity, allocate full size for now
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
    
    // Apply offset for missing cartridge ROMs (pointer arithmetic for memory savings)
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
    // Strategic layout: ROML=0x0000, ROMH=0x2000, KERNAL=0x4000, BASIC=0x6000, CHARROM=0x8000, RAM=0x9000
    DO(c64->cartridge_roml, 0x0000, c64_bus->roml_present); // CHIP_ROML = 0 -> 0x0000 - optional
    DO(c64->cartridge_romh, 0x2000, c64_bus->romh_present); // CHIP_ROMH = 2 -> 0x2000 - optional
    DO(c64->kernal, 0x4000, true);                          // CHIP_KERNAL = 4 -> 0x4000 - always present
    DO(c64->basic, 0x6000, true);                           // CHIP_BASIC = 6 -> 0x6000 - always present
    DO(c64->charrom, 0x8000, true);                         // CHIP_CHARROM = 8 -> 0x8000 - always present
    DO(c64->ram, 0x9000, true);                             // CHIP_RAM = 9 -> 0x9000 - always present
#undef DO    
    
    printf("c64_bus: Allocated %zu KB unified buffer (saved %zu KB), ROML:%s ROMH:%s\n",
           required_size / 1024,
           (100 * 1024 - required_size) / 1024,
           config->roml_present ? "yes" : "no",
           config->romh_present ? "yes" : "no");
}

// ============================================================================
// CHIP CALLBACK FUNCTIONS - Optimized chip-specific operations for hybrid approach
// ============================================================================

/**
 * Initialize IO page handlers for optimized I/O access.
 * Sets up direct chip callbacks that eliminate wrapper functions and provide
 * efficient targeted access to IO chips, indexed by IO page number.
 */
void c64_bus_init_io_handlers(c64_bus_t* c64_bus) {
    c64_t* c64 = (c64_t*)c64_bus->c64;

    // Set up direct callbacks and chip instances for each IO page (0-15 for $D000-$DFFF)
    // Each page gets the appropriate chip register function and chip instance directly

    // Pages 0-3 ($D000-$D3FF): VIC-II
    for (int page = 0; page <= 3; page++) {
        c64_bus->io_handlers[page].read_handler = vicii_registers_read;
        c64_bus->io_handlers[page].write_handler = vicii_registers_write;
        c64_bus->io_handlers[page].chip_instance = c64->vicii;
    }

    // Pages 4-7 ($D400-$D7FF): SID
    for (int page = 4; page <= 7; page++) {
        c64_bus->io_handlers[page].read_handler = mos6581_registers_read;
        c64_bus->io_handlers[page].write_handler = mos6581_registers_write;
        c64_bus->io_handlers[page].chip_instance = c64->sid;
    }

    // Pages 8-11 ($D800-$DBFF): Color RAM
    for (int page = 8; page <= 11; page++) {
        c64_bus->io_handlers[page].read_handler = mos2114_read;
        c64_bus->io_handlers[page].write_handler = mos2114_write;
        c64_bus->io_handlers[page].chip_instance = c64->colorram;
    }

    // Pages 12-13 ($DC00-$DCFF): CIA1
    for (int page = 12; page <= 13; page++) {
        c64_bus->io_handlers[page].read_handler = mos6526_registers_read;
        c64_bus->io_handlers[page].write_handler = mos6526_registers_write;
        c64_bus->io_handlers[page].chip_instance = c64->cia1;
    }

    // Pages 14-15 ($DD00-$DDFF): CIA2
    for (int page = 14; page <= 15; page++) {
        c64_bus->io_handlers[page].read_handler = mos6526_registers_read;
        c64_bus->io_handlers[page].write_handler = mos6526_registers_write;
        c64_bus->io_handlers[page].chip_instance = c64->cia2;
    }
}
