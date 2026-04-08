#pragma once
/*
 * tms9918_mixins.hpp — Conditional feature mixins for TMS9918 VDP family
 *
 * Each mixin adds data and methods for a specific feature set.
 * When a feature is disabled by VDPTraits, std::conditional_t selects
 * an empty base struct — zero overhead via empty base optimization.
 *
 * Follows the pattern from fam65xx_mixins.hpp.
 */

#include "chip/video/tms9918/tms9918_traits.hpp"
#include <cstdint>
#include <type_traits>

namespace tms9918 {

// ============================================================================
// EMPTY BASES FOR DISABLED FEATURES
// ============================================================================
// Separate empty types per feature to prevent duplicate base class errors.

struct vdp_empty_palette_mixin_t {};
struct vdp_empty_command_mixin_t {};
struct vdp_empty_scroll_mixin_t {};
struct vdp_empty_sega_mixin_t {};

// ============================================================================
// PROGRAMMABLE PALETTE MIXIN (V9938 / V9958)
// ============================================================================
// The V9938 has a 16-entry palette from a 512-color (9-bit RGB) space.
// The V9958 extends this with YJK/YAE color encoding.
//
// Palette writes go through an indirect port (register 16 selects the
// palette index, then two bytes per entry via port 2).

template <const VDPTraits& Traits>
struct vdp_palette_mixin_t {
    uint16_t palette_ram_[16] = {};       // 9-bit RGB entries (3 bits per channel)
    uint32_t palette_cache_[16] = {};     // Decoded RGBA for rendering
    uint8_t  palette_ptr_ = 0;            // Auto-incrementing palette index (R#16)
    bool     palette_latch_first_ = true; // First/second byte toggle

    // Decode a single 9-bit RGB entry to 0xAABBGGRR
    static constexpr uint32_t decode_9bit_rgb(uint16_t entry) {
        // V9938 palette format: 0000'0RRR'0BBB'0GGG
        const uint8_t r3 = (entry >> 4) & 0x07;
        const uint8_t b3 = (entry >> 2) & 0x07; // NOTE: no shift needed, but use mask
        const uint8_t g3 = entry & 0x07;
        // Expand 3-bit to 8-bit: (val << 5) | (val << 2) | (val >> 1)
        auto expand = [](uint8_t v3) -> uint8_t {
            return static_cast<uint8_t>((v3 << 5) | (v3 << 2) | (v3 >> 1));
        };
        return 0xFF000000u
             | (static_cast<uint32_t>(expand(b3)) << 16)
             | (static_cast<uint32_t>(expand(g3)) << 8)
             | static_cast<uint32_t>(expand(r3));
    }

    void rebuild_palette_cache() {
        for (int i = 0; i < 16; ++i) {
            palette_cache_[i] = decode_9bit_rgb(palette_ram_[i]);
        }
    }

    // Called when a byte is written to the palette data port.
    // V9938: two writes per entry (byte 0 = RB, byte 1 = 0G).
    uint8_t palette_latch_byte_ = 0;

    void write_palette_data(uint8_t val) {
        if (palette_latch_first_) {
            palette_latch_byte_ = val;
            palette_latch_first_ = false;
        } else {
            // Combine: byte0 = 0RRR'0BBB, byte1 = 0000'0GGG
            uint16_t entry = static_cast<uint16_t>(
                ((val & 0x07) << 0) |           // GGG
                ((palette_latch_byte_ & 0x07) << 2) | // BBB
                (((palette_latch_byte_ >> 4) & 0x07) << 4)  // RRR
            );
            // Clamp to 16 entries, auto-increment
            palette_ram_[palette_ptr_ & 0x0F] = entry;
            palette_cache_[palette_ptr_ & 0x0F] = decode_9bit_rgb(entry);
            palette_ptr_ = (palette_ptr_ + 1) & 0x0F;
            palette_latch_first_ = true;
        }
    }
};

// ============================================================================
// COMMAND ENGINE MIXIN (V9938 / V9958)
// ============================================================================
// Hardware blitter supporting ~30 commands (HMMC, YMMM, HMMM, HMMV,
// LMMC, LMCM, LMMM, LMMV, LINE, SRCH, PSET, POINT, etc.).
//
// The command engine operates on VRAM and is driven by register writes
// (R32–R46). Commands execute asynchronously, draining over multiple
// dot clocks. The CE (Command Execute) status bit indicates busy.

template <const VDPTraits& Traits>
struct vdp_command_mixin_t {
    // Command parameter registers (loaded from R32–R46)
    uint16_t cmd_sx_ = 0, cmd_sy_ = 0;   // Source X, Y
    uint16_t cmd_dx_ = 0, cmd_dy_ = 0;   // Destination X, Y
    uint16_t cmd_nx_ = 0, cmd_ny_ = 0;   // Width, Height (or major/minor axis)
    uint8_t  cmd_clr_ = 0;               // Color argument
    uint8_t  cmd_arg_ = 0;               // Direction bits + logical operation
    uint8_t  cmd_op_ = 0;                // Command code (R46 bits 7–4)

    // Active command state machine
    bool     cmd_active_ = false;
    uint16_t cmd_cx_ = 0, cmd_cy_ = 0;   // Current position during execution
    uint16_t cmd_count_x_ = 0;           // Remaining pixels in current row
    uint16_t cmd_count_y_ = 0;           // Remaining rows

    // Status: command engine transfer ready / busy
    bool command_busy() const { return cmd_active_; }

    void reset_command() {
        cmd_active_ = false;
        cmd_cx_ = cmd_cy_ = 0;
        cmd_count_x_ = cmd_count_y_ = 0;
    }

    // Placeholder: command execution will be implemented in tms9918_command.inc.hpp
    // void start_command(uint8_t* vram);
    // void step_command(uint8_t* vram);
};

// ============================================================================
// SCROLL MIXIN (Sega / V9938 / V9958)
// ============================================================================
// Holds scroll register values and provides fine-scroll offset calculation.

template <const VDPTraits& Traits>
struct vdp_scroll_mixin_t {
    uint8_t scroll_x_ = 0;
    uint8_t scroll_y_ = 0;

    // Effective horizontal scroll for a given scanline.
    // Note: Sega VDP scroll inhibit (R0.D6, top-row lock) is applied
    // inline in the rendering functions where the register is accessible.
    uint16_t effective_scroll_x([[maybe_unused]] int line) const {
        return scroll_x_;
    }

    uint16_t effective_scroll_y() const {
        return scroll_y_;
    }
};

// ============================================================================
// SEGA MODE EXTENSION MIXIN (315-5124 / 315-5246)
// ============================================================================
// Adds CRAM (Color RAM) and mode 4 tile engine support.
// SMS: 32 entries × 6-bit RGB. Game Gear: 64 entries × 12-bit RGB.

template <const VDPTraits& Traits>
struct vdp_sega_mixin_t {
    // CRAM storage
    // SMS: 32 entries, 6-bit per entry (2 bits per channel)
    // GG:  64 entries, 12-bit per entry (4 bits per channel)
    static constexpr int CRAM_SIZE =
        Traits.has(VDPFeatureFlags::SEGA_GG_MODE) ? 64 : 32;

    uint8_t  cram_[CRAM_SIZE] = {};
    uint32_t cram_cache_[CRAM_SIZE] = {};  // Decoded RGBA

    uint8_t  cram_latch_ = 0;             // Second-byte latch for GG mode
    bool     cram_latch_pending_ = false;

    // Decode SMS 6-bit CRAM entry to 0xAABBGGRR
    static constexpr uint32_t decode_sms_cram(uint8_t val) {
        // Format: --BBGGRR (2 bits per channel)
        auto expand = [](uint8_t v2) -> uint8_t {
            return static_cast<uint8_t>((v2 << 6) | (v2 << 4) | (v2 << 2) | v2);
        };
        const uint8_t r = expand(val & 0x03);
        const uint8_t g = expand((val >> 2) & 0x03);
        const uint8_t b = expand((val >> 4) & 0x03);
        return 0xFF000000u
             | (static_cast<uint32_t>(b) << 16)
             | (static_cast<uint32_t>(g) << 8)
             | static_cast<uint32_t>(r);
    }

    void rebuild_cram_cache() {
        for (int i = 0; i < CRAM_SIZE; ++i) {
            cram_cache_[i] = decode_sms_cram(cram_[i]);
        }
    }
};

// ============================================================================
// MIXIN SELECTION — std::conditional_t aliases
// ============================================================================

template <const VDPTraits& Traits>
using vdp_palette_base_t = std::conditional_t<
    Traits.has_programmable_palette(),
    vdp_palette_mixin_t<Traits>,
    vdp_empty_palette_mixin_t>;

template <const VDPTraits& Traits>
using vdp_command_base_t = std::conditional_t<
    Traits.has_command_engine(),
    vdp_command_mixin_t<Traits>,
    vdp_empty_command_mixin_t>;

template <const VDPTraits& Traits>
using vdp_scroll_base_t = std::conditional_t<
    Traits.has_scroll(),
    vdp_scroll_mixin_t<Traits>,
    vdp_empty_scroll_mixin_t>;

template <const VDPTraits& Traits>
using vdp_sega_base_t = std::conditional_t<
    Traits.is_sega(),
    vdp_sega_mixin_t<Traits>,
    vdp_empty_sega_mixin_t>;

} // namespace tms9918
