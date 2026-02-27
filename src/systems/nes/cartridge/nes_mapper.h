#pragma once
/*
 * nes_mapper.h — NES Mapper base class
 *
 * Defines the abstract Mapper interface that all NES mapper implementations
 * must satisfy.  Currently uses the address-mapping approach (cpu_map_read /
 * cpu_map_write returning a mapped_addr offset into ROM/RAM vectors).
 *
 * Phase 2 of the migration will add MapperBankConfig / MapperChrConfig for
 * the page-pointer unified buffer approach.  For now this mirrors the
 * existing Cartridge::Mapper inner class exactly — extract only, no change.
 *
 * Each concrete mapper lives in its own header under cartridge/mappers/.
 */

#include <cstdint>

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
// MAPPER BASE CLASS
// ============================================================================
//
// Mappers translate CPU and PPU addresses into offsets within the
// cartridge's PRG-ROM, CHR-ROM/RAM, and PRG-RAM vectors.
//
// The current interface uses address mapping (Phase 1):
//   - cpu_map_read/write:  given a CPU address → return mapped byte offset
//   - ppu_map_read/write:  given a PPU address → return mapped byte offset
//   - The caller (Cartridge) uses the offset to access its memory vectors.
//
// Phase 2 will add get_prg_bank_config / get_chr_bank_config for direct
// page-pointer generation.

class Mapper {
public:
    virtual ~Mapper() = default;

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

    // --- Lifecycle ---
    virtual void reset() = 0;

    // --- Extended interface (overridden by mappers that need it) ---

    // Dynamic mirroring — mappers like MMC1/MMC3 change this at runtime.
    // Default: HORIZONTAL (no override).
    virtual Mirror mirror() { return Mirror::HORIZONTAL; }

    // IRQ support (MMC3 scanline counter, etc.)
    virtual bool irq_state() { return false; }
    virtual void irq_clear() {}

    // Scanline notification — PPU calls this once per visible scanline.
    // Used by MMC3 for A12-based IRQ counting.
    virtual void scanline() {}
};

} // namespace nes_system
