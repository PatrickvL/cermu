#include "systems/commodore/c64/c64_bus.h"
#include "systems/commodore/c64/c64_system.h"
#include "systems/commodore/c64/c64_constants.h"
#include "chip/io/mos6526.h"
#include "chip/logic/pla.h"
#include "core/cermu.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
// ============================================================================
// BANKING CHANGE CALLBACK
// ============================================================================

void c64_bus_t::on_banking_change(uint8_t banking_state) {
    c64_bus_t* bus = this;
    if (!bus || !bus->c64) return;
    
    // Convert MOS6510Traits banking state to C64 bus mode
    // banking_state contains LORAM (bit 0), HIRAM (bit 1), CHAREN (bit 2)
    // Need to add EXROM and GAME bits from system lines
    uint8_t exrom = (bus->system_lines & SYS_MASK_EXROM) ? 1 : 0;
    uint8_t game = (bus->system_lines & SYS_MASK_GAME) ? 1 : 0;
    
    // Construct full PLA mode: GAME | EXROM | CHAREN | HIRAM | LORAM
    uint8_t pla_mode = (game << 4) | (exrom << 3) | (banking_state & 0x07);
    
    // Switch to new memory mapping mode
    mode_switch(pla_mode);
}

// Chip accessor functions - these use the chip descriptor's read/write callbacks using bus_state_t pattern

// NOTE: Cartridge expansion port IO functions (io1_read, io1_write, io2_read, io2_write)
// will be implemented when cartridge support is added. For now, I/O1 and I/O2 regions
// ($DE00-$DEFF and $DF00-$DFFF) use floating bus behavior (see init_io_handlers).

// Simple 4KB bank calculation for optimized system (0-15)
static inline int8_t c64_get_address_bank(uint16_t address) {
    return address >> 12;  // Extract 4KB bank (0-15)
}

// ULTRA-OPTIMIZED VIC-II MEMORY READ - Better performance than CPU version
// VIC-II uses pre-selected active array (indexed by CHIP), can only read, never write
// Uses unified memory buffer for branchless access to ROM and RAM
bus_state_t c64_bus_t::vic_read(bus_state_t bus_state, uint16_t address) {
    c64_bus_t* c64_bus = this;
    BUS_SET_ADDR(bus_state, address); // Perhaps this is no longer needed
    // Extract 4KB bank from address (0-15 for VIC-II's 64KB addressable space)
    const uint8_t vicii_bank = c64_get_address_bank(address);
    // Get raw CHIP directly from pre-selected active array (no mode indexing)
    const uint8_t chip = c64_bus->vicii_chip_per_bank[vicii_bank];

    // Early return for unmapped regions - use whatever is on the bus
    if (unlikely(chip == CHIP_UNMAPPED)) {
        // Leave bus data unchanged (floating bus state)
        return bus_state;
    }
    
    const uint8_t data = read_chip_byte(chip, address);

    BUS_SET_DATA(bus_state, data);
    return bus_state;
}

/**
 * New cycle-accurate memory tick function for the refactored architecture.
 * This function will be used by the new MOS6510Traits implementation to handle
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
bus_state_t REGISTER_CALL c64_bus_t::memory_tick(bus_state_t bus_state) {
    c64_bus_t* c64_bus = this;
    
    // Determine if this is a read or write operation (direct bit test)
    const bool is_read = BUS_GET_BIT(bus_state, BUS_RW_BIT);
    const uint16_t address = BUS_GET_ADDR(bus_state);
    const uint8_t address_bank = c64_get_address_bank(address);  // Extract 4KB bank (0-15)
    
    if (is_read) {
        // === READ OPERATION ===
        // CRITICAL: Determine memory mapping based on AEC line (bus ownership)
        //
        // The AEC line indicates who owns the bus:
        // - AEC HIGH: CPU owns bus → use CPU memory mapping
        // - AEC LOW: VIC-II owns bus → use VIC-II memory mapping
        //
        // From VIC-II documentation (lines 228-234):
        // "AEC reflects the state of the data and address line drivers of the VIC.
        //  If AEC is high, they are in tri-state. AEC is normally low during the
        //  first clock phase (Φ2 low) and high during the second phase so that the
        //  VIC can access the bus during the first phase and the 6510 during the
        //  second phase. If the VIC also needs the bus in the second phase, AEC remains low."
        //
        // The VIC-II has its own memory map where Character ROM appears at $1000-$1FFF
        // in banks 0 and 2 (not at $D000 like the CPU sees it).
        const bool cpu_has_bus = BUS_GET_BIT(bus_state, BUS_AEC_BIT);
        const uint8_t chip = cpu_has_bus ?
            decode_read_chip(c64_bus->cpu_encoded_chip_per_bank[address_bank]) :
            c64_bus->vicii_chip_per_bank[address_bank];

        // ENHANCED FAST PATH: Direct unified buffer access for all memory chips
        // Fast path handles: CHIP_ROML, CHIP_ROMH, CHIP_KERNAL, CHIP_BASIC, CHIP_CHARROM, CHIP_RAM
        if (likely(chip <= CHIP_RAM)) {
            // Use unified buffer read helper for all ROM/RAM types
            const uint8_t data = read_chip_byte(chip, address);
            BUS_SET_DATA(bus_state, data);
        } else if (chip == CHIP_IO) {
            // OPTIMIZED IO PAGE HANDLING: Direct dispatch using pre-initialized handlers
            // Calculate IO page number from address (0-15 for $D000-$DFFF)
            const uint8_t io_page = (address >> 8) & 0x0F; // Extract page number from $Dx00 addresses

            // Straight call to the appropriate handler - no conditionals needed
            bus_state = c64_bus->io_handlers[io_page].read_handler(
                c64_bus->io_handlers[io_page].chip_instance, bus_state);
        } else {
            // CHIP_UNMAPPED and others: floating bus behavior
            // Leave BUS_GET_DATA(bus_state) unchanged (floating bus state)
        }
    } else {
        // === WRITE OPERATION ===
        // Writes always use CPU memory mapping regardless of AEC state
        // The CPU can complete writes even when BA/RDY is low (early warning from VIC-II)
        // BA goes low 3 cycles early to allow the CPU to complete up to 3
        // consecutive write operations before halting on the first read access.
        const uint8_t chip = decode_write_chip(c64_bus->cpu_encoded_chip_per_bank[address_bank]);

        // ENHANCED FAST PATH: Handle all writable unified buffer regions
        if (likely(chip == CHIP_RAM)) {
            const uint8_t data = BUS_GET_DATA(bus_state);

            write_ram_byte(address, data);
        } else if (chip == CHIP_IO) {
            // OPTIMIZED IO PAGE HANDLING: Direct dispatch using pre-initialized handlers
            // Calculate IO page number from address (0-15 for $D000-$DFFF)
            const uint8_t io_page = (address >> 8) & 0x0F; // Extract page number from $Dx00 addresses

            // Straight call to the appropriate handler - no conditionals needed
            bus_state = c64_bus->io_handlers[io_page].write_handler(
                c64_bus->io_handlers[io_page].chip_instance, bus_state);
        } else {
            // Writes to UNMAPPED and ROM areas are ignored (no action needed)
            // ROM chips (BASIC, KERNAL, CHARROM, ROML, ROMH) are read-only in hardware
        }
    }
    
    return bus_state;
}

// ============================================================================
// MEMORY UTILITY FUNCTIONS - For debugging and testing
// ============================================================================

/**
 * Read a byte from C64 system memory.
 * This is a convenience function for debugging and test programs.
 *
 * @param bus Pointer to c64_bus_t
 * @param addr Address to read from
 * @return Byte value at the specified address
 */
uint8_t c64_bus_t::read_memory(uint16_t addr) {
    bus_state_t read_state = state;
    BUS_SET_ADDR(read_state, addr);
    BUS_SET_BIT(read_state, BUS_RW_BIT);
    read_state = memory_tick(read_state);
    return BUS_GET_DATA(read_state);
}

/**
 * Write a byte to C64 system memory.
 * This is a convenience function for debugging and test programs.
 *
 * @param addr Address to write to
 * @param value Byte value to write
 */
void c64_bus_t::write_memory(uint16_t addr, uint8_t value) {
    bus_state_t write_state = state;
    BUS_SET_ADDR(write_state, addr);
    BUS_SET_DATA(write_state, value);
    BUS_CLR_BIT(write_state, BUS_RW_BIT);
    memory_tick(write_state);
}

// Destructor — frees unified memory buffer
c64_bus_t::~c64_bus_t() {
    if (allocated_buffer) {
        cermu_aligned_free(allocated_buffer);
        allocated_buffer = nullptr;
    }
}

void c64_bus_t::system_attach(C64System* c64) {
    this->c64 = c64;

    // Initialize ROM/RAM pointers and allocate unified buffer (no cartridge ROMs by default)
    init_unified_pointers(c64);

    // Initialize compact IO page handlers for efficient I/O access
    init_io_handlers();
}

void c64_bus_t::mode_switch(uint8_t mode) {
    // Update the optimized banking for the current mode
    pla_banking_mode = mode & 0x1F;
    
    memcpy(cpu_encoded_chip_per_bank, cpu_encoded_chip_per_bank_per_mode[mode], 16);
    // Also copy VIC-II active array for optimal performance
    memcpy(vicii_chip_per_bank, vicii_chip_per_bank_per_mode[mode], 16);
}

void c64_bus_t::update_pla_mode() {
    uint8_t cpu_port_bits = pla_banking_mode & 0x07;
    uint8_t pla_mode = generate_pla_mode(cpu_port_bits);
    mode_switch(pla_mode);
}

// ============================================================================
// PLA integration functions
// ============================================================================

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
    } else if (!pla->outputs.n_io) {
        // I/O region - includes VIC-II, SID, Color RAM, CIA1, CIA2 (read/write)
        return CHIP_IO;
    } else if (!pla->outputs.n_charrom) {
        // Character ROM (read-only)
        // Only reached when IO is not active, ensuring Character ROM is only
        // selected for reads when CHAREN=1 but not for writes.
        return CHIP_CHARROM;
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

void c64_bus_t::populate_cpu_pla_mapping(struct pla_906114_01_t* pla) {
    // CAS active for CPU operations (CAS low = enable RAM access)
    pla->inputs.n_cas = false;

    // CPU cycle: AEC high, BA high — encode these into a base bus state
    // that pla_906114_01_tick will read.
    for (uint32_t bank = 0; bank < 16; bank++) {
        bus_state_t bus = 0;
        BUS_SET_ADDR(bus, bank << 12);
        BUS_SET_BIT(bus, BUS_AEC_BIT);   // CPU has bus control
        BUS_SET_BIT(bus, BUS_BA_BIT);    // Bus available

        // Read mode: R/W high
        BUS_SET_BIT(bus, BUS_RW_BIT);
        pla_906114_01_tick((pla_906114_01_t*)pla, bus);
        uint8_t read_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);

        // Write mode: R/W low (clear the bit)
        BUS_CLR_BIT(bus, BUS_RW_BIT);
        pla_906114_01_tick((pla_906114_01_t*)pla, bus);
        uint8_t write_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);

        cpu_encoded_chip_per_bank[bank] = encode_chip_rw(read_chip, write_chip);
    }
}

void c64_bus_t::populate_vicii_pla_mapping(struct pla_906114_01_t* pla) {
    // CAS active for VIC-II regular memory access
    pla->inputs.n_cas = false;

    // VIC-II cycle: AEC low (VIC has bus), BA low, read-only
    for (uint32_t bank = 0; bank < 16; bank++) {
        // Set VIC-II address lines (VA12, VA13, VA14)
        pla->inputs.va12   = (bank & 0x01) != 0;
        pla->inputs.va13   = (bank & 0x02) != 0;
        pla->inputs.n_va14 = (bank & 0x04) == 0;  // Active-low

        // Tick with AEC=0, BA=0, R/W=read
        bus_state_t bus = 0;
        BUS_SET_ADDR(bus, bank << 12);
        BUS_SET_BIT(bus, BUS_RW_BIT);   // Read mode
        // AEC and BA bits not set → VIC-II has bus control, DMA active
        pla_906114_01_tick((pla_906114_01_t*)pla, bus);
        uint8_t read_chip = pla_906114_01_outputs_to_chip((pla_906114_01_t*)pla);

        vicii_chip_per_bank[bank] = read_chip;
    }
}

void c64_bus_t::generate_all_pla_modes(struct pla_906114_01_t* pla) {
    c64_bus_t* bus = this;

    // Generate all 32 CPU memory modes (5-bit combinations of LORAM, HIRAM, CHAREN, EXROM, GAME)
    for (int mode = 0; mode < 32; mode++) {
        pla_906114_01_set_banking_mode((pla_906114_01_t*)pla, (uint8_t)mode);
        populate_cpu_pla_mapping(pla);
        memcpy(bus->cpu_encoded_chip_per_bank_per_mode[mode], bus->cpu_encoded_chip_per_bank, 16);
    }

    // Generate VIC-II memory modes — only GAME and EXROM matter for VIC-II,
    // but we iterate all 32 modes for table uniformity.
    for (int vic_mode = 0; vic_mode < 32; vic_mode++) {
        pla_906114_01_set_banking_mode((pla_906114_01_t*)pla, (uint8_t)vic_mode);
        populate_vicii_pla_mapping(pla);
        memcpy(bus->vicii_chip_per_bank_per_mode[vic_mode], bus->vicii_chip_per_bank, 16);
    }
}

// ============================================================================
// PLA MODE GENERATION - Convert CPU port bits + cartridge signals to 5-bit PLA mode
// ============================================================================

uint8_t c64_bus_t::generate_pla_mode(uint8_t cpu_port_bits) {
    // The mode value is used as an index into the pre-generated PLA mode tables.
    // The c64_bus_generate_all_pla_modes() function generates these tables with inverted logic:
    //   mode bit 0 set → n_loram = false (LORAM enabled)
    //   mode bit 0 clear → n_loram = true (LORAM disabled)
    //
    // CPU port bits are active-high: 1 = enable ROM/CHAR
    // Mode bits should also be active-high to match the table generation
    // When CPU port = $37 (bits 0-2 = 111), mode should = $07 to enable all ROMs
    //
    // NO INVERSION NEEDED - direct mapping from CPU port bits to mode bits
    
    // Extract CPU I/O port control bits (bits 0-2 of $0001)
    uint8_t pla_mode = cpu_port_bits & 0x07;
    
    // Add cartridge control signals from system lines
    // System lines are active-high: bit set = signal inactive
    // Mode bits should also be active-high: bit set = signal inactive
    pla_mode |= ((system_lines & SYS_MASK_EXROM) ? 0x08 : 0); // EXROM (bit 3)
    pla_mode |= ((system_lines & SYS_MASK_GAME) ? 0x10 : 0);  // GAME (bit 4)

    return pla_mode;
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
void c64_bus_t::set_exrom_signal(bool active) {
    if (active) {
        system_lines &= ~SYS_MASK_EXROM;  // Clear bit (signal low)
    } else {
        system_lines |= SYS_MASK_EXROM;   // Set bit (signal high)
    }

    update_pla_mode();
}

/**
 * Set the GAME signal state for cartridge control.
 * The GAME signal controls cartridge ROM banking and Ultimax mode.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @param active true = GAME active (signal low), false = GAME inactive (signal high)
 */
void c64_bus_t::set_game_signal(bool active) {
    if (active) {
        system_lines &= ~SYS_MASK_GAME;   // Clear bit (signal low)
    } else {
        system_lines |= SYS_MASK_GAME;    // Set bit (signal high)
    }
    
    update_pla_mode();
}

/**
 * Set both EXROM and GAME signals simultaneously for cartridge control.
 * This is more efficient than calling the individual functions separately.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @param exrom_active true = EXROM active (signal low), false = EXROM inactive (signal high)
 * @param game_active true = GAME active (signal low), false = GAME inactive (signal high)
 */
void c64_bus_t::set_cartridge_signals(bool exrom_active, bool game_active) {
    // Update EXROM signal
    if (exrom_active) {
        system_lines &= ~SYS_MASK_EXROM;  // Clear bit (signal low)
    } else {
        system_lines |= SYS_MASK_EXROM;   // Set bit (signal high)
    }
    // Update GAME signal
    if (game_active) {
        system_lines &= ~SYS_MASK_GAME;   // Clear bit (signal low)
    } else {
        system_lines |= SYS_MASK_GAME;    // Set bit (signal high)
    }
    
    update_pla_mode();
}

/**
 * Get the current EXROM signal state.
 * 
 * @param c64_bus Pointer to the C64 bus
 * @return true = EXROM active (signal low), false = EXROM inactive (signal high)
 */
bool c64_bus_t::get_exrom_signal() const {
    // Return inverted state (bit set = signal high = inactive)
    return (system_lines & SYS_MASK_EXROM) == 0;
}

/**
 * Get the current GAME signal state.
 * 
 * @return true = GAME active (signal low), false = GAME inactive (signal high)
 */
bool c64_bus_t::get_game_signal() const {
    // Return inverted state (bit set = signal high = inactive)
    return (system_lines & SYS_MASK_GAME) == 0;
}

// Chip entry lookup table for description and validation (handles irregular numbering)
struct c64_chip_entry_t {
    uint16_t base_address;
    size_t size;
    const char* label;
};

// Sparse lookup table indexed by CHIP_* values (supports irregular numbering)
static const c64_chip_entry_t c64_bus_chip_to_entry[] = {
    /* [CHIP_ROML] = */     { c64_constants::ROML_BASE, 8*1024, "Cartridge ROM Low" },
    { 0, 0, nullptr }, // CHIP_1 unused
    /* [CHIP_ROMH] = */     { c64_constants::BASIC_ROM_BASE, 8*1024, "Cartridge ROM High" }, // Note: Can also map to 0xE000
    { 0, 0, nullptr }, // CHIP_3 unused
    /* [CHIP_KERNAL] = */   { c64_constants::KERNAL_BASE, 8*1024, "KERNAL ROM" },
    { 0, 0, nullptr }, // CHIP_5 unused
    /* [CHIP_BASIC] = */    { c64_constants::BASIC_ROM_BASE, 8*1024, "BASIC ROM" },
    /* [CHIP_CHARROM] = */  { c64_constants::CHAR_ROM_BASE, 4*1024, "Character ROM" },
    { 0, 0, nullptr }, // CHIP_8 unused
    /* [CHIP_RAM] = */      { 0x0000, 64*1024, "RAM" },
    /* [CHIP_UNMAPPED] = */ { 0x0000, 0, "Unmapped" },
    /* [CHIP_IO] = */       { c64_constants::CHAR_ROM_BASE, 4*1024, "I/O" },
};
static const size_t CHIP_ENTRY_COUNT = sizeof(c64_bus_chip_to_entry) / sizeof(c64_bus_chip_to_entry[0]);

// Update get_chip_description to use the chip entry lookup table
bool c64_bus_t::get_chip_description(uint8_t chip, chip_description_t* out) const {
    if (!out) return false;

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
void c64_bus_t::init_unified_pointers(C64System* c64_system, bool roml_present, bool romh_present) {
    c64_bus_t* c64_bus = this;
    if (!c64_bus || !c64_system) return;
    
    // Store cartridge ROM presence flags
    c64_bus->roml_present = roml_present;
    c64_bus->romh_present = romh_present;
    
    // If buffer already exists, just update the ROM pointers to preserve loaded data
    if (c64_bus->allocated_buffer) {
        C64System* c64 = c64_system;
        uint8_t* buffer = c64_bus->unified_memory_buffer;
        
        // Update chip pointers — bind() copies existing data to the
        // unified buffer and redirects the chip's storage there.
#define DO(c64_device, offset, present_flag) \
    if (c64_device && present_flag) { \
        c64_device->bind(buffer + offset); \
    } else if (c64_device) { \
        c64_device->release(); \
    }

        // Update ROM pointers to preserve loaded data
        DO(c64->cartridge_roml, 0x0000, c64_bus->roml_present);
        DO(c64->cartridge_romh, 0x2000, c64_bus->romh_present);
        DO(c64->kernal, 0x4000, true);
        DO(c64->basic, 0x6000, true);
        DO(c64->charrom, 0x8000, true);
        DO(c64->ram, 0x9000, true);
#undef DO
        
        printf("c64_bus: Updated ROM pointers in existing unified buffer\n");
        return;
    }
    
    // Clean up any existing allocation (shouldn't happen, but be safe)
    if (c64_bus->allocated_buffer) {
        cermu_aligned_free(c64_bus->allocated_buffer);
        c64_bus->allocated_buffer = NULL;
    }
    
    // Calculate required memory size for strategic layout (4KB step size)
    // Strategic layout: ROML(0x0000) + ROMH(0x2000) + KERNAL(0x4000) + BASIC(0x6000) + CHARROM(0x8000) + RAM(0x9000)
    // Total: 0x9000 (36KB) + RAM(64KB) = 100KB maximum
    size_t required_size = 100 * 1024; // Base size: 36KB + 64KB RAM
    size_t offset = 0;                  // No offset needed for strategic layout
    
    // Adjust for missing cartridge ROMs (can save space at beginning)
    if (!roml_present && !romh_present) {
        // Skip first 16KB: start at KERNAL (0x4000)
        required_size = 84 * 1024;  // (36KB - 16KB) + 64KB RAM = 84KB
        offset = 16 * 1024;         // Offset buffer start by 16KB
    } else if (!roml_present) {
        // Skip first 8KB: start at ROMH (0x2000)
        required_size = 92 * 1024;  // (36KB - 8KB) + 64KB RAM = 92KB
        offset = 8 * 1024;          // Offset buffer start by 8KB
    } else if (!romh_present) {
        // Keep ROML, skip ROMH: need custom layout
        // For simplicity, allocate full size for now
        required_size = 100 * 1024;
        offset = 0;
    }
    
    // Allocate aligned memory for optimal cache performance
    c64_bus->allocated_buffer = (uint8_t*)cermu_aligned_alloc(64, required_size);
    
    if (!c64_bus->allocated_buffer) {
        printf("c64_bus: ERROR - Failed to allocate unified memory buffer (%zu KB)\n", required_size / 1024);
        return;
    }
    
    c64_bus->allocated_size = required_size;
    
    // Apply offset for missing cartridge ROMs (pointer arithmetic for memory savings)
    c64_bus->unified_memory_buffer = c64_bus->allocated_buffer - offset;
    
    // Initialize allocated memory to zero
    memset(c64_bus->allocated_buffer, 0, required_size);
    
    C64System* c64 = c64_system;
    uint8_t* buffer = c64_bus->unified_memory_buffer;
    
#define DO(c64_device, offset, present_flag) \
    if (c64_device && present_flag) { \
        c64_device->bind(buffer + offset); \
    } else if (c64_device) { \
        c64_device->release(); \
    }

    // Point the following devices to their respective unified buffer offset
    // Strategic layout: ROML=0x0000, ROMH=0x2000, KERNAL=0x4000, BASIC=0x6000, CHARROM=0x8000, RAM=0x9000
    DO(c64->cartridge_roml, 0x0000, c64_bus->roml_present); // CHIP_ROML = 0 -> 0x0000 - optional
    DO(c64->cartridge_romh, 0x2000, c64_bus->romh_present); // CHIP_ROMH = 2 -> 0x2000 - optional
    DO(c64->kernal, 0x4000, true);                          // CHIP_KERNAL = 4 -> 0x4000 - always present
    DO(c64->basic, 0x6000, true);                           // CHIP_BASIC = 6 -> 0x6000 - always present
    DO(c64->charrom, 0x8000, true);                         // CHIP_CHARROM = 7 -> 0x8000 - FIXED to avoid overlap with BASIC
    DO(c64->ram, 0x9000, true);                             // CHIP_RAM = 9 -> 0x9000 - always present
#undef DO
    
    printf("c64_bus: Allocated %zu KB unified buffer (saved %zu KB), ROML:%s ROMH:%s\n",
           required_size / 1024,
           (100 * 1024 - required_size) / 1024,
           roml_present ? "yes" : "no",
           romh_present ? "yes" : "no");
}

// ============================================================================
// CHIP CALLBACK FUNCTIONS - Optimized chip-specific operations for hybrid approach
// ============================================================================

/**
 * Floating bus behavior for unmapped IO regions.
 * Returns bus state unchanged (floating bus), writes are ignored.
 */
static bus_state_t c64_bus_unmapped_read(void* chip, bus_state_t bus_state) {
    (void)chip; // Unused - floating bus has no chip instance
    // Leave bus data unchanged - floating bus behavior
    return bus_state;
}

static bus_state_t c64_bus_unmapped_write(void* chip, bus_state_t bus_state) {
    (void)chip; // Unused - floating bus has no chip instance
    // Write is ignored - no operation
    return bus_state;
}

/**
 * Initialize IO page handlers for optimized I/O access.
 * Sets up direct chip callbacks that eliminate wrapper functions and provide
 * efficient targeted access to IO chips, indexed by IO page number.
 *
 * C64 IO Memory Map ($D000-$DFFF):
 * $D000-$D3FF (pages 0-3):   VIC-II (64 bytes mirrored)
 * $D400-$D7FF (pages 4-7):   SID (32 bytes mirrored)
 * $D800-$DBFF (pages 8-11):  Color RAM (1KB)
 * $DC00-$DCFF (page 12):     CIA1 (16 bytes mirrored)
 * $DD00-$DDFF (page 13):     CIA2 (16 bytes mirrored)
 * $DE00-$DEFF (page 14):     I/O1 expansion (unmapped by default)
 * $DF00-$DFFF (page 15):     I/O2 expansion (unmapped by default)
 */
void c64_bus_t::init_io_handlers() {
    C64System* c64 = this->c64;

    // Set up direct callbacks and chip instances for each IO page (0-15 for $D000-$DFFF)
    // Each page gets the appropriate chip register function and chip instance directly

    // Pages 0-3 ($D000-$D3FF): VIC-II (64 bytes mirrored across 1KB)
    for (int page = 0; page <= 3; page++) {
        io_handlers[page].read_handler = vicii_t::registers_read;
        io_handlers[page].write_handler = vicii_t::registers_write;
        io_handlers[page].chip_instance = c64->vicii;
    }

    // Pages 4-7 ($D400-$D7FF): SID (32 bytes mirrored across 1KB)
    for (int page = 4; page <= 7; page++) {
        io_handlers[page].read_handler = mos6581_t::registers_read;
        io_handlers[page].write_handler = mos6581_t::registers_write;
        io_handlers[page].chip_instance = c64->sid;
    }

    // Pages 8-11 ($D800-$DBFF): Color RAM (1KB, 1024 bytes)
    for (int page = 8; page <= 11; page++) {
        io_handlers[page].read_handler = MOS2114::bus_read;
        io_handlers[page].write_handler = MOS2114::bus_write;
        io_handlers[page].chip_instance = c64->colorram;
    }

    // Page 12 ($DC00-$DCFF): CIA1 (16 bytes mirrored across 256 bytes)
    io_handlers[12].read_handler = mos6526_t::registers_read;
    io_handlers[12].write_handler = mos6526_t::registers_write;
    io_handlers[12].chip_instance = c64->cia1;

    // Page 13 ($DD00-$DDFF): CIA2 (16 bytes mirrored across 256 bytes)
    io_handlers[13].read_handler = mos6526_t::registers_read;
    io_handlers[13].write_handler = mos6526_t::registers_write;
    io_handlers[13].chip_instance = c64->cia2;

    // Page 14 ($DE00-$DEFF): I/O1 expansion port (unmapped by default - floating bus)
    io_handlers[14].read_handler = c64_bus_unmapped_read;
    io_handlers[14].write_handler = c64_bus_unmapped_write;
    io_handlers[14].chip_instance = NULL;

    // Page 15 ($DF00-$DFFF): I/O2 expansion port (unmapped by default - floating bus)
    io_handlers[15].read_handler = c64_bus_unmapped_read;
    io_handlers[15].write_handler = c64_bus_unmapped_write;
    io_handlers[15].chip_instance = NULL;
}
