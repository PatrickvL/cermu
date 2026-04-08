#pragma once
/*
 * nes_mapper.h — NES Mapper base class + bank configuration structs
 *
 * Defines the abstract Mapper interface that all NES mapper implementations
 * must satisfy.
 *
 * Each mapper's register_write() updates internal state; get_prg_bank_config()
 * and get_chr_bank_config() produce page pointers that the bus copies into
 * its block arrays.  Mappers precompute translations once when registers
 * change, not on every access.
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

// Precalculated nametable page assignments per mirroring mode.
// Indexed by static_cast<int>(Mirror).  Each entry holds the
// CIRAM page (0 or 1) for nametable slots $2000/$2400/$2800/$2C00.
inline constexpr uint8_t MIRROR_NT_PAGES[5][4] = {
    {0, 0, 1, 1},  // HORIZONTAL
    {0, 1, 0, 1},  // VERTICAL
    {0, 0, 0, 0},  // ONESCREEN_LO
    {1, 1, 1, 1},  // ONESCREEN_HI
    {0, 1, 2, 3},  // FOUR_SCREEN (requires 4KB on-cart VRAM)
};

// ============================================================================
// BANK CONFIGURATION STRUCTS (Phase 2)
// ============================================================================
//
// Mappers produce these structs; the bus consumes them to set page pointers.
// Pointers aim directly into Cartridge's ROM/RAM vectors — no copies.

/// PRG bank configuration for CPU address space $5000-$FFFF.
struct MapperBankConfig {
    // 8 × 4KB page pointers for CPU $8000-$FFFF (read)
    // prg_pages[0] → $8000-$8FFF, prg_pages[7] → $F000-$FFFF
    const uint8_t* prg_pages[8] = {};

    // Optional writable PRG pages (for self-modifying NSFs).
    // When non-null, the corresponding $8000-$FFFF write block uses this pointer.
    // When null, writes go to BLOCK_OPEN_BUS (normal mapper dispatch).
    uint8_t* prg_write_pages[8] = {};

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

    // When true, the cartridge respects the mapper's nt_page[] values
    // instead of overwriting them from mirror().  Used by TxSROM/118
    // and similar mappers that derive per-slot nametable mapping from
    // CHR bank registers.
    bool custom_nt = false;

    // Optional direct nametable pointers — when non-null, the bus uses
    // these 1KB pointers instead of ciram + nt_page[i] * 0x400.
    // Used by Mapper 068 (Sunsoft-4) to map CHR-ROM into nametable slots.
    // Read-only; writes still go to CIRAM via the standard nt_page mirror.
    const uint8_t* nt_ptr[4] = {};
};

// ============================================================================
// MAPPER BASE CLASS
// ============================================================================

class Mapper {
public:
    virtual ~Mapper() = default;

    // =======================================================================
    // Bank configuration interface
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

    /// Handle CPU read from expansion area ($5000-$5FFF).
    /// Override for mappers with readable registers in this range (MMC5).
    /// Sets `handled` to true if the address was serviced.
    virtual uint8_t expansion_read(uint16_t addr, bool& handled) {
        (void)addr; handled = false; return 0;
    }

    /// Extra CHR-RAM bytes needed beyond CHR-ROM (e.g. on-board cart RAM).
    /// The cartridge appends this to chr_memory so it lives in flat_mem.
    /// Override for mappers with mixed CHR-ROM + CHR-RAM (mapper 077).
    virtual uint32_t extra_chr_ram_size() const { return 0; }

    // =======================================================================
    // Shared interface
    // =======================================================================

    // --- Lifecycle ---
    virtual void reset() = 0;

    // Dynamic mirroring — mappers like MMC1/MMC3 change this at runtime.
    // Default returns the header mirroring set by set_header_mirror().
    virtual Mirror mirror() { return header_mirror_; }

    /// Store the iNES header mirroring mode.  Called by Cartridge after
    /// mapper creation so that the default mirror() returns the correct mode.
    void set_header_mirror(Mirror m) { header_mirror_ = m; }

    /// Store the NES 2.0 submapper number.  Called by Cartridge when
    /// a NES 2.0 header is detected.  Mappers that care can read submapper_.
    void set_submapper(uint8_t s) { submapper_ = s; }

    /// Give the mapper direct access to the CIRAM / nametable area.
    /// Mappers that place custom data at nt_page indices 2-3 (MMC5 ExRAM,
    /// fill mode) override this to store data at ciram + 0x800 / 0xC00.
    virtual void set_ciram(uint8_t* /*ciram*/) {}

    // IRQ support (MMC3 scanline counter, etc.)
    virtual bool irq_state() { return false; }
    virtual void irq_clear() {}

    // A12 transition notification — called by the PPU on any 0→1 or 1→0
    // change of PPU address bus bit 12.  The mapper receives the raw
    // signal state and current PPU dot count, allowing it to implement
    // hardware-specific filtering (e.g. MMC3's RC-delay requirement
    // that A12 was low for >= ~16 dots before a rising edge counts).
    virtual void notify_a12(bool /*a12_high*/, uint64_t /*ppu_cycle*/) {}

    // CPU cycle notification — called once per CPU cycle for mappers
    // with CPU-clocked IRQ counters (FME-7, VRC6, VRC7, etc.).
    virtual void notify_cpu_cycle() {}

    // Expansion audio — mappers with extra sound hardware (MMC5 pulse/PCM,
    // VRC6 pulse/saw, VRC7 FM, Sunsoft 5B PSG, Namco 163 wavetable).
    // audio_tick() is called once per CPU cycle.  audio_output() returns
    // the current expansion sample in [-1.0, 1.0] range for mixing with
    // the main APU output.
    virtual void audio_tick() {}
    virtual float audio_output() const { return 0.0f; }

    // PPUCTRL notification — called when CPU writes PPU $2000.
    // MMC5 uses bits 3-5 to split CHR bank sets between sprite and BG
    // pattern table halves.  Returns true if banking changed.
    virtual bool notify_ppuctrl(uint8_t /*value*/) { return false; }

    // ====================================================================
    // Bus-mediated PPU memory access hooks
    // ====================================================================
    //
    // Mappers that need to intercept PPU bus transactions override these.
    // Called by Cartridge::ppu_memory_tick() on every PPU dot.
    //
    // ppu_bus_intercept: Called BEFORE default block dispatch on PPU
    // reads.  If the mapper returns true, `data` is placed on the bus
    // and block dispatch is skipped.  Used by MMC5 vertical split mode
    // and extended attribute mode to substitute nametable / pattern data
    // mid-scanline.
    //
    // ppu_bus_read: Called AFTER default block dispatch has placed data
    // on the bus.  The mapper can inspect the address (e.g. $0FD8-$0FEF
    // for MMC2/MMC4 latch switching) and return true if it changed CHR
    // banking (triggers an update_bank_map).  The data has already been
    // read from the current page pointers, so the latch switch takes
    // effect on the NEXT fetch — matching real hardware behavior.
    virtual bool ppu_bus_intercept(uint16_t /*addr*/, uint8_t& /*data*/) { return false; }
    virtual bool ppu_bus_read(uint16_t /*addr*/) { return false; }

    /// Whether CHR memory is RAM (writable by PPU) vs ROM (read-only).
    bool chr_is_ram() const { return chr_is_ram_; }

protected:
    // ROM/RAM pointers — set by Cartridge via set_memory_pointers()
    const uint8_t* prg_rom_ = nullptr;
    size_t prg_rom_size_ = 0;
    uint8_t* chr_mem_ = nullptr;        // CHR-ROM or CHR-RAM data
    size_t chr_mem_size_ = 0;
    bool chr_is_ram_ = false;           // true when CHR is RAM (writable)
    uint8_t* prg_ram_ = nullptr;
    size_t prg_ram_size_ = 0;
    Mirror header_mirror_ = Mirror::HORIZONTAL;
    uint8_t submapper_ = 0;
};

} // namespace nes_system
