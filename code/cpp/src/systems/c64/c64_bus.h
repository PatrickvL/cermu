#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../core/aiemuc.h"
#include "../../core/chip.h"
#include "../../core/system.h"
#include "../../core/system_lines.h"
#include "../../core/bus_cycle_interface.h"
#include "../../core/control_lines_interface.h"
#include "c64_config.h"
#include "c64_chips.h"

// =============================
// Bus Types & Macros
// =============================

// System line masks for cartridge signals (moved out of control lines to separate field)
#define SYS_MASK_EXROM (1 << 0)   // EXROM signal (bit 0)
#define SYS_MASK_GAME  (1 << 1)   // GAME signal (bit 1)

// C64 default bus state with pull-up resistors
// This represents the hardware state at the start of each cycle before any chip asserts lines:
// - Data bus: 0xFF (pull-ups on all 8 data lines)
// - IRQ, NMI: HIGH via pull-ups (inactive, active-low signals)
// - RDY: HIGH via pull-up (CPU ready)
// - BA: HIGH via pull-up (bus available, VIC-II pulls LOW during badlines)
// - AEC: HIGH via pull-up (CPU controls address bus, VIC-II pulls LOW to take control)
// - RW: HIGH (READ mode, pull-up on R/W line defaults to read)
// - RES: HIGH (not in reset, active-low signal)
#define C64_BUS_DEFAULT_STATE() \
    (BUS_STATE(0, 0xFF, BUS_MASK_BA | BUS_MASK_AEC | BUS_MASK_RDY | BUS_MASK_RW | BUS_MASK_IRQ | BUS_MASK_NMI) | BUS_BIT(BUS_RES_BIT))

// C64 bus controller structure
typedef struct c64_bus_s {
    chip_descriptor_t* desc;
    void* c64;  // c64_t* - opaque pointer to avoid circular dependency
    bus_state_t default_state; // Default bus state with pull-up resistors (start of each tick)
    bus_state_t state;         // Current bus state (after all chip ticks)
    // System lines for control signals (includes EXROM and GAME)
    uint8_t system_lines;  // System-wide control lines including cartridge signals

    // Current PLA banking mode (0-31) derived from CPU port + cartridge signals
    uint8_t pla_banking_mode;  // Current banking mode for fast switching

    // UNIFIED MEMORY BUFFER FOR OPTIMIZED OPCODE FETCH - STRATEGIC LAYOUT
    // Layout optimized for branchless calculation: ROML + ROMH + KERNAL + BASIC + CHARROM + RAM
    // Offsets: ROML=0x0000, ROMH=0x2000, KERNAL=0x4000, BASIC=0x6000, CHARROM=0x7000, RAM=0x9000
    // Total: Up to 100KB unified buffer (36KB ROM space + 64KB RAM) for branchless memory access
    // Strategic CHIP numbering enables pure arithmetic: offset = chip << 12 (chip * 4096)
    // Dynamic allocation skips unused cartridge ROMs at buffer start to save memory
    uint8_t* unified_memory_buffer;   // Points to usable memory (may be offset from allocated memory)
    uint8_t* allocated_buffer;        // Points to actual allocated memory
    size_t allocated_size;            // Actual allocated size
    bool roml_present;                // Whether ROML cartridge ROM is attached
    bool romh_present;                // Whether ROMH cartridge ROM is attached

    // OPTIMIZED MEMORY BANKING - Cache-friendly layout
    // Banking configurations per mode (32 modes x 16 banks = 512 bytes)
    alignas(64) uint8_t cpu_encoded_chip_per_bank_per_mode[32][16]; // Encoded chip select for all PLA modes
    // 16 bytes: cpu_encoded_chip_per_bank mapping (4KB banks 0-15) - fits in single cache line
    alignas(16) uint8_t cpu_encoded_chip_per_bank[16]; // CPU banking configurations per mode

    // VIC-II active array for optimized access (raw CHIPs, no encoding)
    alignas(16) uint8_t vicii_chip_per_bank[16];

    // VIC-II banking configurations per mode (32 modes x 16 banks = 512 bytes)
    // VIC-II uses direct CHIP values, not encoded, since it only does read accesses
    alignas(64) uint8_t vicii_chip_per_bank_per_mode[32][16]; // VIC-II direct CHIP per mode

    // COMPACT I/O PAGE MAPPING - Efficient approach using IO page numbers
    // Maps IO page numbers (0-15 for $D000-$DFFF) directly to chip handlers
    // This is more efficient than the previous callback array approach
    typedef struct {
        bus_state_t (*read_handler)(void* context, bus_state_t bus_state);  // Direct chip register function signature
        void* chip_instance;  // Direct pointer to the chip instance for this IO page
        bus_state_t (*write_handler)(void* context, bus_state_t bus_state); // Direct chip register function signature
    } io_page_handlers_t;
    io_page_handlers_t io_handlers[16]; // One handler per IO page (0-15)

    // Control lines adapter interface
    control_lines_interface_t control_lines_adapter;
} c64_bus_t;

void c64_bus_mode_switch(c64_bus_t* c64_bus, uint8_t mode);

// PLA mode generation function - maps CPU I/O port bits + cartridge signals to 5-bit PLA mode
uint8_t c64_bus_generate_pla_mode(c64_bus_t* c64_bus, uint8_t cpu_port_bits);

// Memory functions
bus_state_t c64_bus_vic_read(c64_bus_t* c64_bus, bus_state_t bus_state, uint16_t address);

/**
 * New cycle-accurate memory tick function for the refactored architecture.
 * This function will be used by the new MOS6510 implementation to handle
 * memory access in a cycle-accurate manner.
 *
 * Optimized for register-based calling convention to avoid host stack accesses.
 * Takes bus state by value and returns updated bus state for efficient register usage.
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param bus_state Current bus state (passed by value for register optimization)
 * @return Updated bus state (for register-to-register operation)
 */
bus_state_t REGISTER_CALL c64_memory_tick(c64_bus_t* c64_bus, bus_state_t bus_state);

/**
 * Initialize RAM/ROM pointers to point into the unified memory buffer.
 * This eliminates separate memory allocations and ensures consistency.
 * Uses configuration structure to determine cartridge ROM presence.
 * Should be called after system is attached.
 *
 * @param c64_bus Pointer to the C64 bus controller
 * @param c64_system Pointer to the C64 system (for pointer updates)
 * @param config Pointer to the C64 system configuration structure
 */
void c64_bus_init_unified_pointers(c64_bus_t* c64_bus, void* c64_system, const c64_config_t* config);

/**
 * Ultra-optimized unified address calculation function for memory access.
 * Pure branchless arithmetic using strategic CHIP_* numbering for maximum performance.
 * CHIP values are chosen so that (chip << 12) directly maps to buffer offsets.
 *
 * CRITICAL DEPENDENCY: This function relies on the specific CHIP_* enum values
 * in c64_chips.h. The calculation uses chip << 12 (chip * 4096) for base offsets:
 *
 * - CHIP_ROML     = 0  -> base_offset = 0x0000 (0 << 12 = 0x0000)
 * - CHIP_ROMH     = 2  -> base_offset = 0x2000 (2 << 12 = 0x2000)
 * - CHIP_KERNAL   = 4  -> base_offset = 0x4000 (4 << 12 = 0x4000)
 * - CHIP_BASIC    = 6  -> base_offset = 0x6000 (6 << 12 = 0x6000)
 * - CHIP_CHARROM  = 7  -> base_offset = 0x7000 (7 << 12 = 0x7000)
 * - CHIP_RAM      = 9  -> base_offset = 0x9000 (9 << 12 = 0x9000)
 *
 * WARNING: Changing these CHIP_* values will break address calculation!
 *
 * CHARROM ADDRESSING EXPLANATION (4KB chip at buffer offset 0x7000):
 * The 0x1FFF mask works correctly for CHARROM despite being 4KB because:
 *
 * Example 1 - VIC-II reads CHARROM at 0x1000:
 *   base   = 7 << 12        = 0x7000  (CHARROM buffer offset)
 *   offset = 0x1000 & 0x1FFF = 0x1000  (address within range)
 *   result = 0x7000 + 0x1000 = 0x8000  ✓ Correct buffer position
 *
 * Example 2 - VIC-II reads CHARROM at 0x9000:
 *   base   = 7 << 12        = 0x7000  (CHARROM buffer offset)
 *   offset = 0x9000 & 0x1FFF = 0x1000  (wraps to 0x1000 due to 13-bit mask)
 *   result = 0x7000 + 0x1000 = 0x8000  ✓ Same buffer position as 0x1000
 *
 * Example 3 - CPU reads at 0xD000:
 *   base   = 7 << 12        = 0x7000  (CHARROM buffer offset)
 *   offset = 0xD000 & 0x1FFF = 0x1000  (wraps to 0x1000 due to 13-bit mask)
 *   result = 0x7000 + 0x1000 = 0x8000  ✓ Same buffer position
 *
 * PROOF OF NO OVERLAP BETWEEN BASIC ROM AND CHARROM:
 * BASIC ROM occupies buffer range 0x6000-0x7FFF (8KB at CHIP_BASIC=6)
 * CHARROM occupies buffer range 0x8000-0x8FFF (4KB starting at base 0x7000)
 *
 * BASIC ROM address calculation (8KB at addresses 0xA000-0xBFFF):
 *   base   = 6 << 12        = 0x6000  (BASIC buffer offset)
 *   offset = 0xA000 & 0x1FFF = 0x0000  (wraps to start)
 *   result = 0x6000 + 0x0000 = 0x6000  (buffer start)
 *
 *   offset = 0xBFFF & 0x1FFF = 0x1FFF  (8KB-1 offset)
 *   result = 0x6000 + 0x1FFF = 0x7FFF  (buffer end)
 *
 * CHARROM address calculation (4KB, but accessed via specific addresses):
 *   Minimum result = 0x7000 + (0x1000 & 0x1FFF) = 0x8000
 *   Maximum result = 0x7000 + (0x1FFF & 0x1FFF) = 0x8FFF
 *
 * CRITICAL INSIGHT: CHARROM is NEVER accessed with addresses in range 0x0000-0x0FFF!
 * - CPU accesses CHARROM at 0xD000-0xDFFF (masks to 0x1000-0x1FFF)
 * - VIC-II accesses CHARROM at 0x1000-0x1FFF or 0x9000-0x9FFF (both mask to 0x1000-0x1FFF)
 *
 * Therefore, CHARROM calculations always produce: 0x7000 + [0x1000 to 0x1FFF] = 0x8000-0x8FFF
 * While BASIC ROM calculations produce: 0x6000 + [0x0000 to 0x1FFF] = 0x6000-0x7FFF
 *
 * Result: BASIC ends at 0x7FFF, CHARROM starts at 0x8000 → NO OVERLAP! ✓
 *
 * This strategic placement at 0x7000 means all CHARROM addresses (0x1000, 0x9000, 0xD000)
 * mask to the same offset range (0x1000-0x1FFF) and map to buffer range 0x8000-0x8FFF,
 * which is safely above BASIC ROM's range. This enables unified 0x1FFF mask for all ROMs.
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
    const uint32_t base = (uint32_t)chip << 12;  // Direct offset calculation via strategic numbering
    
    // CRITICAL: addr contains original C64 memory map addresses (e.g. KERNAL 0xE000-0xFFFF)
    // Mask strips bank/base address to get chip-relative offset
    // Unified 0x1FFF mask works for all ROMs (8KB and 4KB) due to strategic CHARROM placement
    // RAM uses special mask 0xFFFF for full 64KB address space
    const uint32_t mask = 0x1FFF | -(chip == CHIP_RAM);  // Branchless: 0x1FFF for ROMs, 0xFFFF for RAM
    return base + (addr & mask);
}

/**
 * Helper function to write a byte to unified buffer using chip-based addressing.
 * This is used for direct unified buffer writes during memory operations.
 * Uses c64_bus_unified_address_calc for correct buffer offset calculation.
 *
 * @param bus Pointer to the C64 bus controller
 * @param chip The target chip ID (typically CHIP_RAM for write operations)
 * @param address 16-bit address in the chip's address range
 * @param value The byte value to write
 */
static inline void c64_bus_write_chip_byte(c64_bus_t* bus, uint8_t chip, uint16_t address, uint8_t value) {
    // Use the unified address calculation function with the specified chip
    const uint32_t unified_addr = c64_bus_unified_address_calc(chip, address);
    bus->unified_memory_buffer[unified_addr] = value;
}

/**
 * Helper function to write a byte to RAM using direct unified buffer access.
 * This is a specialized convenience function for RAM writes (the most common write case).
 * Uses c64_bus_write_chip_byte with CHIP_RAM.
 *
 * @param bus Pointer to the C64 bus controller
 * @param address 16-bit address in RAM range ($0000-$FFFF)
 * @param value The byte value to write
 */
static inline void c64_bus_write_ram_byte(c64_bus_t* bus, uint16_t address, uint8_t value) {
    c64_bus_write_chip_byte(bus, CHIP_RAM, address, value);
}

/**
 * Helper function to read a byte from unified buffer using chip-based addressing.
 * This is the general-purpose read function for all unified buffer chips.
 * Uses c64_bus_unified_address_calc for correct buffer offset calculation.
 *
 * @param bus Pointer to the C64 bus controller
 * @param chip The target chip ID (CHIP_ROML, CHIP_ROMH, CHIP_KERNAL, CHIP_BASIC, CHIP_CHARROM, CHIP_RAM)
 * @param address 16-bit address in the chip's address range
 * @return The byte value at the specified address
 */
static inline uint8_t c64_bus_read_chip_byte(c64_bus_t* bus, uint8_t chip, uint16_t address) {
    // Use the unified address calculation function with the specified chip
    const uint32_t unified_addr = c64_bus_unified_address_calc(chip, address);
    return bus->unified_memory_buffer[unified_addr];
}

/**
 * Helper function to read a byte from KERNAL ROM using direct unified buffer access.
 * This is used for reading the reset vector at $FFFC-$FFFD.
 * Uses c64_bus_read_chip_byte with CHIP_KERNAL.
 *
 * @param bus Pointer to the C64 bus controller
 * @param address 16-bit address in KERNAL ROM range ($E000-$FFFF)
 * @return The byte value at the specified address
 */
static inline uint8_t c64_bus_read_kernal_byte(c64_bus_t* bus, uint16_t address) {
    // Use the general unified buffer read function with CHIP_KERNAL
    return c64_bus_read_chip_byte(bus, CHIP_KERNAL, address);
}

/**
 * Helper function to read the reset vector from KERNAL ROM.
 * The reset vector is located at $FFFC-$FFFD and points to the
 * KERNAL reset routine (typically $FCE2 on C64).
 *
 * @param bus Pointer to the C64 bus controller
 * @return The 16-bit reset vector address
 */
static inline uint16_t c64_read_kernal_reset_vector(c64_bus_t* bus) {
    uint8_t reset_low = c64_bus_read_kernal_byte(bus, 0xFFFC);
    uint8_t reset_high = c64_bus_read_kernal_byte(bus, 0xFFFD);
    return (reset_high << 8) | reset_low;
}

// System functions
void* c64_bus_system_create(chip_descriptor_t* desc);
void c64_bus_system_destroy(void* chip);
void c64_bus_system_attach(c64_bus_t* c64_bus, void* c64);  // c64_t*

// Cartridge interface functions for controlling EXROM and GAME signals
void c64_bus_set_exrom_signal(c64_bus_t* c64_bus, bool active);
void c64_bus_set_game_signal(c64_bus_t* c64_bus, bool active);
void c64_bus_set_cartridge_signals(c64_bus_t* c64_bus, bool exrom_active, bool game_active);
bool c64_bus_get_exrom_signal(c64_bus_t* c64_bus);
bool c64_bus_get_game_signal(c64_bus_t* c64_bus);

// Banking change callback function for MOS6510
void c64_bus_on_banking_change(void* bus_ptr, uint8_t banking_state);

// Forward declaration for PLA
struct pla_906114_01_s;

// PLA-based bus mapping functions
void c64_bus_populate_cpu_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla);
void c64_bus_populate_vicii_pla_mapping(c64_bus_t* bus, struct pla_906114_01_s* pla);

// Generate all 32 memory modes using PLA
void c64_bus_generate_all_pla_modes(c64_bus_t* bus, struct pla_906114_01_s* pla);

// COMPACT I/O PAGE MAPPING FUNCTIONS - Efficient IO page-based dispatch
void c64_bus_init_io_handlers(c64_bus_t* c64_bus);

extern chip_descriptor_t c64_bus_descriptor;

// ============================================================================
// ADAPTER INTERFACES - Control lines adapter access
// ============================================================================

/**
 * Initialize the integrated adapter interfaces in the C64 bus.
 * This sets up the control lines adapter interface.
 * Should be called during bus initialization.
 *
 * @param c64_bus Pointer to the C64 bus implementation
 */
void c64_bus_init_adapters(c64_bus_t* c64_bus);

/**
 * Get a pointer to the control lines adapter interface.
 * This allows any chip to access the shared control lines.
 *
 * @param c64_bus Pointer to the existing C64 bus implementation
 * @return Pointer to the control lines interface structure configured for the C64 bus
 */
static inline control_lines_interface_t* c64_control_lines_get_adapter(c64_bus_t* c64_bus) {
    return &c64_bus->control_lines_adapter;
}
