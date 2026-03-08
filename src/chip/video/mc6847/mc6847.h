#pragma once
/*
 * mc6847.h — Motorola MC6847 Video Display Generator (VDG)
 *
 * The MC6847 (1978) is a character/graphics display controller used in
 * many early home computers including the Acorn Atom, TRS-80 Color Computer,
 * Dragon 32/64, and several others.
 *
 * Features:
 *   - 32×16 text mode (semi-graphics or alphanumeric)
 *   - 256×192 full-graphics mode (up to 4 colors)
 *   - 128×192, 128×96, 64×64 lower-resolution modes
 *   - Internal character ROM (64 chars, 5×7 matrix)
 *   - External video RAM access via DMA (active-high FS/HS sync outputs)
 *
 * Video modes selected by input pins AG, AS, INTEXT, INV, GM0-GM2, CSS:
 *   AG=0: Alphanumeric/semigraphics mode
 *   AG=1: Full graphics modes (GM0-GM2 select resolution/colors)
 *
 * Display timing: 262 lines (NTSC) or 312 lines (PAL) equivalent.
 *
 * 40-pin DIP package.
 */

#include "../../core/chip.h"
#include "../../core/system_lines.h"
#include <cstdint>
#include <cstring>

// ============================================================================
// MC6847 Constants
// ============================================================================

namespace mc6847_const {

    inline constexpr int DISPLAY_WIDTH  = 256;
    inline constexpr int DISPLAY_HEIGHT = 192;
    inline constexpr int BORDER_PIXELS  = 32;
    inline constexpr int TOTAL_WIDTH    = DISPLAY_WIDTH + BORDER_PIXELS * 2;  // 320
    inline constexpr int TOTAL_HEIGHT   = DISPLAY_HEIGHT + 48;                // 240

    inline constexpr int TEXT_COLS      = 32;
    inline constexpr int TEXT_ROWS      = 16;

    // Video RAM size: 512 bytes (text) to 6144 bytes (hires graphics)
    inline constexpr int VRAM_TEXT_SIZE     = 512;
    inline constexpr int VRAM_GRAPHICS_MAX = 6144;

} // namespace mc6847_const

// ============================================================================
// MC6847 Video Display Generator
// ============================================================================

class mc6847_t : public ChipBase {
public:
    mc6847_t()
        : ChipBase(ChipInfo("MC6847", "Motorola"))
    {
        category_ = "Video";
    }

    void init() {
        mode_ag_ = false;
        mode_gm_ = 0;
        mode_css_ = false;
        mode_as_ = false;
        mode_intext_ = false;
        mode_inv_ = false;
        scanline_ = 0;
        dot_ = 0;
        frame_counter_ = 0;
        fs_ = false;
        hs_ = false;
    }

    void reset() { init(); }

    // === Mode pins (directly driven by system address decoding / PIA) ===

    void set_ag(bool v)     { mode_ag_ = v; }       // Alphanumeric / Graphics
    void set_gm(uint8_t v)  { mode_gm_ = v & 0x07; }// Graphics mode (GM0-GM2)
    void set_css(bool v)    { mode_css_ = v; }       // Color set select
    void set_as(bool v)     { mode_as_ = v; }        // Alpha semigraphics
    void set_intext(bool v) { mode_intext_ = v; }    // Internal/external character
    void set_inv(bool v)    { mode_inv_ = v; }       // Inverse video

    // === Video generation ===

    /// Tick one pixel clock.  Advances display timing.
    void tick() {
        dot_++;
        if (dot_ >= mc6847_const::TOTAL_WIDTH) {
            dot_ = 0;
            scanline_++;
            hs_ = true;  // Horizontal sync pulse
            if (scanline_ >= 262) {  // NTSC
                scanline_ = 0;
                frame_counter_++;
                fs_ = true;  // Frame/field sync
            }
        }
    }

    // === Sync signals ===

    bool fs() const { return fs_; }   // Field sync (vertical)
    bool hs() const { return hs_; }   // Horizontal sync

    /// Check and clear field sync flag
    bool check_fs() { bool f = fs_; fs_ = false; return f; }
    bool check_hs() { bool h = hs_; hs_ = false; return h; }

    // === Mode queries ===
    bool     is_graphics_mode() const { return mode_ag_; }
    uint8_t  graphics_mode()    const { return mode_gm_; }
    int      scanline()         const { return scanline_; }
    uint32_t frame_counter()    const { return frame_counter_; }

private:
    // Mode pins
    bool    mode_ag_ = false;
    uint8_t mode_gm_ = 0;
    bool    mode_css_ = false;
    bool    mode_as_ = false;
    bool    mode_intext_ = false;
    bool    mode_inv_ = false;

    // Timing
    int      scanline_ = 0;
    int      dot_ = 0;
    uint32_t frame_counter_ = 0;

    // Sync outputs
    bool     fs_ = false;
    bool     hs_ = false;
};
