#pragma once
/*
 * nes_mapper.h — NES Mapper base class + bank configuration structs
 *
 * Defines the abstract Mapper interface that all NES mapper implementations
 * must satisfy.  Provides two dispatch mechanisms:
 *
 *   Phase 1 (legacy): cpu_map_read/write returning byte offsets — used by
 *       NsfCartridge and any code that hasn't been migrated yet.
 *
 *   Phase 2 (current): get_prg_bank_config / get_chr_bank_config returning
 *       direct page pointers into ROM/RAM.  The bus copies these into its
 *       page tables.  Mappers precompute translations once when registers
 *       change, not on every access.
 *
 * Each concrete mapper lives in its own header under cartridge/mappers/.
 */

#include <cstdint>
#include <cstddef>

namespace nes_system {

// Nametable mirroring modes — select how the 2KB CIRAM is mapped
// across the four nametable slots ($2000/$2400/$2800/$2C00).
enum class Mirror : uint8_t {
    HORIZONTAL,     // Vertical arrangement  (CIRAM pages [0,0,1,1])
    VERTICAL,       // Horizontal arrangement (CIRAM pages [0,1,0,1])
    ONESCREEN_LO,   // Single screen page 0  (CIRAM pages [0,0,0,0])
    ONESCREEN_HI,   // Single screen page 1  (CIRAM pages [1,1,1,1])
    FOUR_SCREEN,    // Four-screen (4KB on-cart RAM, no CIRAM sharing)
};

// ============================================================================
// BANK CONFIGURATION STRUCTS (Phase 2)
// ============================================================================
//
// Mappers produce these structs; the bus consumes them to set page pointers.
// Pointers aim directly into Cartridge's ROM/RAM vectors — no copies.

/// PRG bank configuration for CPU address space $5000-$FFFF.
struct MapperBankConfig {
    // 8 × 4KB page pointers for CPU $8000-$FFFF
    // prg_pages[0] → $8000-$8FFF, prg_pages[7] → $F000-$FFFF
    const uint8_t* prg_pages[8] = {};

    // PRG-RAM at $6000-$7FFF
    uint8_t* prg_ram_base = nullptr;
    uint32_t prg_ram_size = 0;
    bool prg_ram_enabled = true;
    bool prg_ram_write_protected = false;

    // Expansion space $5000-$5FFF (MMC5, etc.)
    const uint8_t* expansion_read = nullptr;
    uint8_t* expansion_write = nullptr;
};

/// CHR bank configuration for PPU address space $0000-$1FFF + nametable mirroring.
struct MapperChrConfig {
    // 8 × 1KB page pointers for PPU $0000-$1FFF
    // chr_pages[0] → $0000-$03FF, chr_pages[7] → $1C00-$1FFF
    const uint8_t* chr_pages[8] = {};
    bool chr_writable[8] = {};  // true for CHR-RAM pages

    // Nametable mirroring — 4 entries, each selects a CIRAM page (0 or 1)
    // Slot 0 → $2000, Slot 1 → $2400, Slot 2 → $2800, Slot 3 → $2C00
    // HORIZONTAL: {0,0,1,1}  VERTICAL: {0,1,0,1}
    // ONESCREEN_LO: {0,0,0,0}  ONESCREEN_HI: {1,1,1,1}
    uint8_t nt_page[4] = {0, 1, 0, 1};  // default: vertical
};

// ============================================================================
// MAPPER BASE CLASS
// ============================================================================
//
// Two interfaces coexist during migration:
//
// Legacy (Phase 1): cpu_map_read/write, ppu_map_read/write
//   — Still used by NsfCartridge and Cartridge::cpu_bus_tick fallback.
//
// Current (Phase 2): get_prg_bank_config, get_chr_bank_config, register_write
//   — Used by nes_bus_t page pointer dispatch.

class Mapper {
public:
    virtual ~Mapper() = default;

    // =======================================================================
    // Phase 2 interface — page-pointer bank configuration
    // =======================================================================

    /// Set ROM/RAM pointers from Cartridge after creation.
    /// Must be called before get_prg_bank_config / get_chr_bank_config.
    void set_memory_pointers(const uint8_t* prg_rom, size_t prg_rom_size,
                             uint8_t* chr_mem, size_t chr_mem_size,
                             bool chr_is_ram,
                             uint8_t* prg_ram, size_t prg_ram_size) {
        prg_rom_ = prg_rom;
        prg_rom_size_ = prg_rom_size;
        chr_mem_ = chr_mem;
        chr_mem_size_ = chr_mem_size;
        chr_is_ram_ = chr_is_ram;
        prg_ram_ = prg_ram;
        prg_ram_size_ = prg_ram_size;
    }

    /// Fill config with 8 × 4KB page pointers for CPU $8000-$FFFF
    /// plus PRG-RAM and expansion configuration.
    virtual void get_prg_bank_config(MapperBankConfig& config) const { (void)config; }

    /// Fill config with 8 × 1KB page pointers for PPU $0000-$1FFF
    /// plus nametable mirroring indices.
    virtual void get_chr_bank_config(MapperChrConfig& config) const { (void)config; }

    /// Handle CPU write to mapper register space ($8000-$FFFF).
    /// Returns true if banking changed (requires page pointer update).
    virtual bool register_write(uint16_t addr, uint8_t data) {
        (void)addr; (void)data; return false;
    }

    // =======================================================================
    // Legacy interface (Phase 1) — address-mapping approach
    // =======================================================================

    // --- CPU address mapping ---
    // Returns true if the mapper claims this address.
    // mapped_addr receives the byte offset into Cartridge's PRG memory.
    // Special sentinel: 0xFFFFFFFF = PRG-RAM region.
    virtual bool cpu_map_read(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool cpu_map_write(uint16_t addr, uint32_t& mapped_addr, uint8_t data = 0) = 0;

    // --- PPU address mapping ---
    // Returns true if the mapper claims this address.
    // mapped_addr receives the byte offset into Cartridge's CHR memory.
    virtual bool ppu_map_read(uint16_t addr, uint32_t& mapped_addr) = 0;
    virtual bool ppu_map_write(uint16_t addr, uint32_t& mapped_addr) = 0;

    // =======================================================================
    // Shared interface
    // =======================================================================

    // --- Lifecycle ---
    virtual void reset() = 0;

    // Dynamic mirroring — mappers like MMC1/MMC3 change this at runtime.
    virtual Mirror mirror() { return Mirror::HORIZONTAL; }

    // IRQ support (MMC3 scanline counter, etc.)
    virtual bool irq_state() { return false; }
    virtual void irq_clear() {}

    // Scanline notification — PPU calls this once per visible scanline.
    // Used by MMC3 for A12-based IRQ counting.
    virtual void scanline() {}

protected:
    // ROM/RAM pointers — set by Cartridge via set_memory_pointers()
    const uint8_t* prg_rom_ = nullptr;
    size_t prg_rom_size_ = 0;
    uint8_t* chr_mem_ = nullptr;        // CHR-ROM or CHR-RAM data
    size_t chr_mem_size_ = 0;
    bool chr_is_ram_ = false;           // true when CHR is RAM (writable)
    uint8_t* prg_ram_ = nullptr;
    size_t prg_ram_size_ = 0;
};

} // namespace nes_system
