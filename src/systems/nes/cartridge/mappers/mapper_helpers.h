#pragma once
/*
 * mapper_helpers.h — Common mapper bank configuration utilities
 *
 * Inline helper functions for frequent PRG/CHR bank layout patterns.
 * Also provides a composable MMC3-style A12 IRQ counter.
 * Zero-overhead: all functions inlined, no vtables or indirection.
 */

#include "systems/nes/cartridge/nes_mapper.h"

namespace nes_system {
namespace mapper_helpers {

// ============================================================================
// PRG bank configuration helpers
// ============================================================================

/// Set 16KB switchable PRG at $8000, 16KB fixed (last) at $C000.
inline void set_prg_16k_lo(MapperBankConfig& config,
                            const uint8_t* prg_rom, size_t prg_size,
                            uint8_t bank) {
    uint32_t num_16k = static_cast<uint32_t>(prg_size / 0x4000);
    if (num_16k == 0) num_16k = 1;
    uint32_t lo = (bank % num_16k) * 0x4000;
    uint32_t hi = (num_16k - 1) * 0x4000;
    for (int i = 0; i < 4; i++) {
        config.prg_pages[i]     = prg_rom + lo + i * 0x1000;
        config.prg_pages[4 + i] = prg_rom + hi + i * 0x1000;
    }
    config.prg_ram_enabled = false;
}

/// Set 16KB fixed (first) at $8000, 16KB switchable at $C000.
inline void set_prg_16k_hi(MapperBankConfig& config,
                            const uint8_t* prg_rom, size_t prg_size,
                            uint8_t bank) {
    uint32_t num_16k = static_cast<uint32_t>(prg_size / 0x4000);
    if (num_16k == 0) num_16k = 1;
    uint32_t hi = (bank % num_16k) * 0x4000;
    for (int i = 0; i < 4; i++) {
        config.prg_pages[i]     = prg_rom + i * 0x1000;     // fixed first
        config.prg_pages[4 + i] = prg_rom + hi + i * 0x1000;
    }
    config.prg_ram_enabled = false;
}

/// Set 32KB switchable PRG bank covering $8000-$FFFF.
inline void set_prg_32k(MapperBankConfig& config,
                         const uint8_t* prg_rom, size_t prg_size,
                         uint8_t bank) {
    uint32_t num_32k = static_cast<uint32_t>(prg_size / 0x8000);
    if (num_32k == 0) num_32k = 1;
    uint32_t base = (bank % num_32k) * 0x8000;
    for (int i = 0; i < 8; i++)
        config.prg_pages[i] = prg_rom + base + i * 0x1000;
    config.prg_ram_enabled = false;
}

/// Set fixed PRG — mirrors smaller ROMs across 32KB window.
/// Used by NROM, CNROM, and similar boards with no PRG switching.
inline void set_prg_fixed(MapperBankConfig& config,
                           const uint8_t* prg_rom, size_t prg_size) {
    for (int i = 0; i < 8; i++)
        config.prg_pages[i] = prg_rom + (i * 0x1000) % prg_size;
    config.prg_ram_enabled = false;
}

// ============================================================================
// CHR bank configuration helpers
// ============================================================================

/// Set 8KB switchable CHR bank at $0000-$1FFF.
inline void set_chr_8k(MapperChrConfig& config,
                        const uint8_t* chr_mem, size_t chr_size,
                        bool is_ram, uint8_t bank) {
    uint32_t num_8k = chr_size > 0 ? static_cast<uint32_t>(chr_size / 0x2000) : 1;
    if (num_8k == 0) num_8k = 1;
    uint32_t base = (bank % num_8k) * 0x2000;
    for (int i = 0; i < 8; i++) {
        uint32_t off = base + i * 0x0400;
        config.chr_pages[i] = (off < chr_size) ? chr_mem + off : chr_mem;
        config.chr_writable[i] = is_ram;
    }
}

/// Set fixed 8KB CHR (bank 0 — no switching).
inline void set_chr_8k_fixed(MapperChrConfig& config,
                              const uint8_t* chr_mem, size_t chr_size,
                              bool is_ram) {
    for (int i = 0; i < 8; i++) {
        uint32_t off = i * 0x0400;
        config.chr_pages[i] = (off < chr_size) ? chr_mem + off : chr_mem;
        config.chr_writable[i] = is_ram;
    }
}

// ============================================================================
// MMC3-style A12 IRQ counter (composable utility)
// ============================================================================

/// Composable MMC3 IRQ counter with A12 filter.
/// Embed as a member in any MMC3-derived mapper.  Zero vtable overhead.
struct MMC3IRQ {
    uint8_t counter = 0;
    uint8_t reload_value = 0;
    bool enabled = false;
    bool active = false;
    bool reload_flag = false;
    uint64_t a12_low_since = 0;

    static constexpr uint16_t A12_FILTER_DELAY = 16;

    void reset() {
        counter = 0;
        reload_value = 0;
        enabled = false;
        active = false;
        reload_flag = false;
        a12_low_since = 0;
    }

    /// Process A12 signal transition (call from mapper's notify_a12).
    void notify_a12(bool a12_high, uint64_t ppu_cycle) {
        if (!a12_high) { a12_low_since = ppu_cycle; return; }
        if (ppu_cycle - a12_low_since < A12_FILTER_DELAY) return;

        if (counter == 0 || reload_flag) {
            counter = reload_value;
            reload_flag = false;
        } else {
            counter--;
        }
        if (counter == 0 && enabled) active = true;
    }

    /// Handle writes to $C000-$FFFF IRQ registers.
    /// Returns false (IRQ register changes don't affect banking).
    bool write(uint16_t addr, uint8_t data) {
        bool even = !(addr & 0x0001);
        if (addr <= 0xDFFF) {
            if (even) {
                reload_value = data;
            } else {
                counter = 0;
                reload_flag = true;
            }
        } else {
            if (even) {
                enabled = false;
                active = false;
            } else {
                enabled = true;
            }
        }
        return false;
    }
};

} // namespace mapper_helpers
} // namespace nes_system
