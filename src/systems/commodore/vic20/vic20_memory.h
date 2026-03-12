#pragma once

#include <cstdint>

#include "core/system_lines.h"
#include "core/cermu.h"  // For REGISTER_CALL macro
#include "systems/commodore/vic20/vic20_chips.h"
/**
 * VIC-20 Memory Banking System - Optimized Encoded Bank Type Design
 * 
 * Unlike the C64 with its PLA-based remapping, the VIC-20 has a simpler,
 * fixed memory map with expansion support. This implementation uses:
 * 
 * - Single 64-byte encoded bank array (1 byte per 1KB bank)
 * - Each byte encodes both read and write types for branchless decode
 * - Encoding: bits[3:0]=read_type, bits[7:4]=write_type
 * - Types: UNMAPPED=0, IO=1, ROM=2, RAM=3
 * 
 * Performance advantages:
 * - Single array lookup (vs 2 bitmap extractions)
 * - Simpler decode: (type >= 2) catches both RAM and ROM
 * - 64 bytes fits in single cache line
 * - Direct unified buffer access: buffer[addr] for all RAM/ROM
 * - No offset calculation needed (unlike C64's strategic chip numbering)
 */

// Forward declarations
struct vic_base_t;
struct mos6522_t;

// ============================================================================
// I/O Handler Types
// ============================================================================

struct vic20_io_handler_t {
    bus_state_t (*read_handler)(void* context, bus_state_t bus_state);
    void* read_context;
    bus_state_t (*write_handler)(void* context, bus_state_t bus_state);
    void* write_context;
};

// ============================================================================
// VIC-20 Memory System Structure
// ============================================================================

struct vic20_memory_t {
    // ========================================================================
    // OPTIMIZED BANK TYPE ARRAYS - Cache-line aligned (64 bytes each)
    // ========================================================================
    // Contains encoded bank types: bits[3:0]=read, bits[7:4]=write
    // Must be first member for optimal alignment
    alignas(64) vic20_bank_map_t cpu_bank_map;    // CPU memory map (all 64 banks)
    alignas(64) vic20_bank_map_t vic_bank_map;    // VIC chip memory map (banks 0-15 used)
    
    // ========================================================================
    // UNIFIED 64KB BUFFER
    // ========================================================================
    // Single buffer for all RAM and ROM access at natural addresses
    // buffer[addr] for all RAM/ROM - no offset calculation needed
    // ROM writes are blocked except during loading
    // I/O and UNMAPPED types don't access this buffer
    uint8_t* buffer;              // 64KB unified memory buffer (allocated aligned)
    size_t buffer_size;           // Allocated buffer size
    
    // ========================================================================
    // I/O HANDLERS
    // ========================================================================
    // I/O page handlers for $9000-$9FFF region (4 pages × 1KB each)
    // Page 0: $9000-$93FF - VIC + VIA registers with mirrors
    // Page 1: $9400-$97FF - Color RAM
    // Page 2: $9800-$9BFF - I/O expansion 2
    // Page 3: $9C00-$9FFF - I/O expansion 3
    vic20_io_handler_t io_handlers[4];
    
    // Direct chip pointers for I/O access
    void* vic_chip;   // vic_base_t* — works for both MOS6561 (PAL) and MOS6560 (NTSC)
    void* via1_chip;  // mos6522_t*
    void* via2_chip;  // mos6522_t*
    
    // Color RAM (1KB, 4-bit wide) - stored in unified buffer at $9400
    // Upper 4 bits read as garbage/undefined
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    void* vic20;                  // Pointer to VIC20System
    uint8_t expansion_flags;      // Which expansion blocks are present
    bool cartridge_present;       // Whether cartridge ROM is present
    
};

// ============================================================================
// Memory System Functions
// ============================================================================

/**
 * Create and initialize the VIC-20 memory system.
 * 
 * @param expansion_flags Bitmask of expansion blocks present
 * @param cartridge_present Whether a cartridge ROM is present at $A000-$BFFF
 * @return Pointer to initialized memory system, or NULL on failure
 */
vic20_memory_t* vic20_memory_create(uint8_t expansion_flags, bool cartridge_present);

/**
 * Destroy the VIC-20 memory system and free all resources.
 */
void vic20_memory_destroy(vic20_memory_t* mem);

/**
 * Attach the memory system to a VIC-20 system instance.
 */
void vic20_memory_attach_system(vic20_memory_t* mem, void* vic20_system);

/**
 * Initialize I/O handlers for the $9000-$9FFF region.
 * Called after chips are created and attached.
 */
void vic20_memory_init_io_handlers(vic20_memory_t* mem);

/**
 * Ultra-optimized CPU memory tick function - handles memory access from CPU.
 * Uses encoded bank types for minimal branch overhead.
 * 
 * Fast path: RAM and ROM access via direct buffer[addr] lookup
 * I/O path: Handler dispatch via pre-initialized handler array
 * 
 * @param mem Pointer to memory system
 * @param bus_state Current bus state with address, data, and control lines
 * @return Updated bus state after memory access
 */
bus_state_t REGISTER_CALL vic20_memory_cpu_tick(vic20_memory_t* mem, bus_state_t bus_state);

/**
 * VIC memory read function - handles memory reads from VIC chip.
 * VIC has a 14-bit address space (16KB) and can only read.
 * 
 * @param mem Pointer to memory system
 * @param addr 14-bit address from VIC
 * @return Data byte at the specified address
 */
uint8_t vic20_memory_vic_read(vic20_memory_t* mem, uint16_t addr);

/**
 * Color RAM read function - used by VIC for character color data.
 * Color RAM is 4-bit wide (upper 4 bits are undefined/garbage).
 * 
 * @param mem Pointer to memory system
 * @param addr Address within color RAM (0-1023)
 * @return Color value (lower 4 bits valid)
 */
uint8_t vic20_memory_color_read(vic20_memory_t* mem, uint16_t addr);

/**
 * Direct read from unified buffer at a given address.
 * Used for debugging and ROM vector reading.
 */
uint8_t vic20_memory_read_byte(vic20_memory_t* mem, uint16_t addr);

/**
 * Direct write to unified buffer at a given address.
 * Used for initialization and debugging.
 */
void vic20_memory_write_byte(vic20_memory_t* mem, uint16_t addr, uint8_t value);

/**
 * Load ROM data into the memory system.
 * ROM writes bypass the normal write protection.
 * 
 * @param mem Pointer to memory system
 * @param addr Start address in memory map
 * @param data Pointer to ROM data
 * @param size Size of ROM data in bytes
 * @return true on success, false on failure
 */
bool vic20_memory_load_rom(vic20_memory_t* mem, uint16_t addr, const uint8_t* data, size_t size);

/**
 * Get pointer to a memory region in the unified buffer.
 * Used for direct ROM/RAM access (e.g., reading reset vector).
 */
uint8_t* vic20_memory_get_ptr(vic20_memory_t* mem, uint16_t addr);

/**
 * Get pointer to a ROM region in the unified buffer.
 * Alias for vic20_memory_get_ptr for semantic clarity.
 */
static inline uint8_t* vic20_memory_get_rom_ptr(vic20_memory_t* mem, uint16_t addr) {
    return vic20_memory_get_ptr(mem, addr);
}

/**
 * Get pointer to Color RAM (at $9400 in unified buffer).
 */
uint8_t* vic20_memory_get_colorram_ptr(vic20_memory_t* mem);

/**
 * Update expansion configuration at runtime.
 * Reinitializes bank maps for new configuration.
 */
void vic20_memory_set_expansion(vic20_memory_t* mem, uint8_t expansion_flags);

/**
 * Check if a given expansion block is present.
 */
static inline bool vic20_memory_has_expansion(vic20_memory_t* mem, uint8_t block) {
    return (mem->expansion_flags & block) != 0;
}

