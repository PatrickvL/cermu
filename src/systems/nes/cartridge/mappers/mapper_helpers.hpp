#pragma once
/*
 * mapper_helpers.h — Common mapper bank configuration utilities
 *
 * Inline helper functions for frequent PRG/CHR bank layout patterns.
 * Also provides composable IRQ counter types.
 * Zero-overhead: all functions inlined, no vtables or indirection.
 *
 * PRG helpers:
 *   set_prg_fixed       — no switching, mirrors smaller ROMs across 32KB
 *   set_prg_32k         — 32KB switchable
 *   set_prg_16k_lo      — 16KB switchable @ $8000, last 16KB fixed @ $C000
 *   set_prg_16k_hi      — first 16KB fixed @ $8000, 16KB switchable @ $C000
 *   set_prg_8k_banks    — N×8KB arbitrary banks → 8 page pointers
 *
 * CHR helpers:
 *   set_chr_8k_fixed    — 8KB fixed (bank 0)
 *   set_chr_8k          — 8KB switchable
 *   set_chr_4k_split    — 2×4KB banks
 *   set_chr_2k_pages    — 4×2KB banks
 *   set_chr_1k_pages    — 8×1KB banks
 *
 * IRQ composables:
 *   MMC3IRQ             — A12-based scanline counter with filter
 *   CPUCycleIRQ         — CPU-clocked countdown IRQ (Jaleco, Taito, etc.)
 *   VRCIRQ              — VRC-style 8-bit up-counter with prescaler + latch
 */

#include "systems/nes/cartridge/nes_mapper.hpp"

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

/// Map N×8KB bank numbers to 8 page pointers.
/// `banks` is an array of 4 bank numbers (each an 8KB slot index).
/// Unused entries above `n_slots` are filled with fixed banks from the end.
inline void set_prg_8k_banks(MapperBankConfig& config,
                              const uint8_t* prg_rom, size_t prg_size,
                              const uint32_t banks[4]) {
    for (int slot = 0; slot < 4; slot++) {
        uint32_t base = banks[slot] * 0x2000;
        config.prg_pages[slot * 2]     = (base         < prg_size) ? prg_rom + base         : nullptr;
        config.prg_pages[slot * 2 + 1] = (base + 0x1000 < prg_size) ? prg_rom + base + 0x1000 : nullptr;
    }
}

/// Compute the total number of 8KB PRG banks, with a floor of 1.
inline uint32_t prg_8k_count(size_t prg_size) {
    uint32_t n = static_cast<uint32_t>(prg_size / 0x2000);
    return n ? n : 1;
}

/// Compute the total number of 1KB CHR banks, with a floor of 1.
inline uint32_t chr_1k_count(size_t chr_size) {
    uint32_t n = (chr_size > 0) ? static_cast<uint32_t>(chr_size / 0x0400) : 1;
    return n ? n : 1;
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

/// Set 2×4KB CHR banks.
/// bank_lo → $0000-$0FFF (4 pages), bank_hi → $1000-$1FFF (4 pages).
inline void set_chr_4k_split(MapperChrConfig& config,
                              const uint8_t* chr_mem, size_t chr_size,
                              bool is_ram, uint8_t bank_lo, uint8_t bank_hi) {
    uint32_t num_4k = chr_size > 0 ? static_cast<uint32_t>(chr_size / 0x1000) : 1;
    if (num_4k == 0) num_4k = 1;
    uint32_t lo = (bank_lo % num_4k) * 0x1000;
    uint32_t hi = (bank_hi % num_4k) * 0x1000;
    for (int i = 0; i < 4; i++) {
        config.chr_pages[i]     = chr_mem + lo + i * 0x0400;
        config.chr_pages[4 + i] = chr_mem + hi + i * 0x0400;
        config.chr_writable[i]     = is_ram;
        config.chr_writable[4 + i] = is_ram;
    }
}

/// Set 4×2KB CHR banks.
/// banks[0] → $0000, banks[1] → $0800, banks[2] → $1000, banks[3] → $1800.
inline void set_chr_2k_pages(MapperChrConfig& config,
                              const uint8_t* chr_mem, size_t chr_size,
                              bool is_ram, const uint8_t banks[4]) {
    uint32_t num_2k = chr_size > 0 ? static_cast<uint32_t>(chr_size / 0x0800) : 1;
    if (num_2k == 0) num_2k = 1;
    for (int i = 0; i < 4; i++) {
        uint32_t base = (banks[i] % num_2k) * 0x0800;
        config.chr_pages[i * 2]     = chr_mem + base;
        config.chr_pages[i * 2 + 1] = chr_mem + base + 0x0400;
        config.chr_writable[i * 2]     = is_ram;
        config.chr_writable[i * 2 + 1] = is_ram;
    }
}

/// Set 8×1KB CHR pages from an array of bank numbers.
inline void set_chr_1k_pages(MapperChrConfig& config,
                              const uint8_t* chr_mem, size_t chr_size,
                              bool is_ram, const uint8_t banks[8]) {
    uint32_t num_1k = chr_1k_count(chr_size);
    for (int i = 0; i < 8; i++) {
        uint32_t offset = (banks[i] % num_1k) * 0x0400;
        config.chr_pages[i] = (offset < chr_size) ? chr_mem + offset : chr_mem;
        config.chr_writable[i] = is_ram;
    }
}

/// Set 8×1KB CHR pages from an array of wide (16-bit) bank numbers.
/// Used by mappers with >256 1KB CHR banks (Namco 163, Bandai FCG, etc.).
inline void set_chr_1k_pages_wide(MapperChrConfig& config,
                                   const uint8_t* chr_mem, size_t chr_size,
                                   bool is_ram, const uint16_t banks[8]) {
    uint32_t num_1k = chr_1k_count(chr_size);
    for (int i = 0; i < 8; i++) {
        uint32_t offset = (banks[i] % num_1k) * 0x0400;
        config.chr_pages[i] = (offset < chr_size) ? chr_mem + offset : chr_mem;
        config.chr_writable[i] = is_ram;
    }
}

// ============================================================================
// Mirroring helper
// ============================================================================

/// Convert a 2-bit mirroring value to Mirror enum (VRC convention).
/// 0=vertical, 1=horizontal, 2=one-screen low, 3=one-screen high.
inline Mirror mirror_from_2bit(uint8_t val) {
    switch (val & 0x03) {
        case 0:  return Mirror::VERTICAL;
        case 1:  return Mirror::HORIZONTAL;
        case 2:  return Mirror::ONESCREEN_LO;
        case 3:  return Mirror::ONESCREEN_HI;
        default: return Mirror::VERTICAL;
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

// ============================================================================
// CPU-cycle countdown IRQ (composable utility)
// ============================================================================

/// Generic CPU-clocked 16-bit countdown IRQ.
/// Used by Jaleco SS88006 (mapper 18), Taito X1-005/017 (80/82), etc.
/// Call tick() once per CPU cycle from notify_cpu_cycle().
struct CPUCycleIRQ {
    uint16_t counter = 0;
    uint16_t reload = 0;
    bool enabled = false;
    bool active = false;

    void reset() {
        counter = 0;
        reload = 0;
        enabled = false;
        active = false;
    }

    /// Clock the IRQ counter once per CPU cycle.
    void tick() {
        if (!enabled) return;
        if (counter == 0) {
            active = true;
            enabled = false;
        } else {
            counter--;
        }
    }
};

// ============================================================================
// VRC-style 8-bit up-counter IRQ with prescaler (composable utility)
// ============================================================================

/// VRC4/VRC6/VRC7 IRQ counter — 8-bit up-counter with latch + prescaler.
/// Supports both scanline mode (A12-clocked) and cycle mode (M2-clocked).
struct VRCIRQ {
    uint8_t latch = 0;
    uint8_t counter = 0;
    bool enabled = false;
    bool enable_after_ack = false;
    bool cycle_mode = false;
    bool active = false;
    uint16_t prescaler = 0;

    void reset() {
        latch = 0;
        counter = 0;
        enabled = false;
        enable_after_ack = false;
        cycle_mode = false;
        active = false;
        prescaler = 0;
    }

    /// Clock the counter once — fires IRQ on overflow (0xFF→0x00).
    void clock() {
        counter++;
        if (counter == 0) {
            counter = latch;
            active = true;
        }
    }

    /// Call once per CPU cycle.  In cycle mode, clocks the counter directly
    /// at M2 rate.  In scanline mode, does nothing (use clock_scanline via
    /// A12 rising edges instead).
    void tick_cpu() {
        if (!enabled || !cycle_mode) return;
        clock();
    }

    /// Call from A12 rising edge (scanline mode).
    void clock_scanline() {
        if (!enabled || cycle_mode) return;
        clock();
    }

    /// Write to VRC IRQ registers ($F000-$F003 for VRC4, etc.).
    /// Typical layout: $F000=latch_lo, $F001=latch_hi, $F002=control, $F003=ack.
    /// Returns false (no banking change).
    bool write(uint8_t sub_reg, uint8_t data) {
        switch (sub_reg & 0x03) {
            case 0:  // Latch low nybble
                latch = (latch & 0xF0) | (data & 0x0F);
                return false;
            case 1:  // Latch high nybble
                latch = (latch & 0x0F) | ((data & 0x0F) << 4);
                return false;
            case 2:  // Control
                active = false;
                enable_after_ack = (data & 0x01) != 0;
                enabled = (data & 0x02) != 0;
                cycle_mode = (data & 0x04) != 0;
                if (enabled) {
                    counter = latch;
                    prescaler = 0;
                }
                return false;
            case 3:  // Acknowledge
                active = false;
                enabled = enable_after_ack;
                return false;
        }
        return false;
    }
};

} // namespace mapper_helpers
} // namespace nes_system
