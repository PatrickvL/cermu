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

#include "chip/video/video_chip_base.hpp"
#include "core/system_lines.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// MC6847 Constants
// ============================================================================

// ============================================================================
// MC6847 UNIFIED DECLARATION TABLE — single source of truth
// ============================================================================
//
//   REG(offset, symbol, description)
//   FLD(reg_sym, field_sym, hi:lo, description, kind, display_shift, display_scale)
//   CMP(symbol, description, kind, total_bits, display_shift, display_scale,
//       reg1, hilo1, dst1, reg2, hilo2, dst2)

#define MC6847_DECL(REG, FLD, CMP) \
    REG(0x00, REG_MODE, "Mode pin state mirror")                               \
      FLD(REG_MODE, AG,     0:0, "Graphics mode",     Flag, 0, 0)             \
      FLD(REG_MODE, AS,     1:1, "Alphanum/Semigraph", Flag, 0, 0)            \
      FLD(REG_MODE, INTEXT, 2:2, "Internal/External",  Flag, 0, 0)            \
      FLD(REG_MODE, INV,    3:3, "Invert",             Flag, 0, 0)            \
      FLD(REG_MODE, CSS,    4:4, "Color set select",   Flag, 0, 0)            \
      FLD(REG_MODE, GM,     7:5, "Graphics mode",      Value, 0, 0)

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

    // Mode register bit positions (pin state mirror)
    inline constexpr uint8_t MODE_AG     = 0x01;
    inline constexpr uint8_t MODE_AS     = 0x02;
    inline constexpr uint8_t MODE_INTEXT = 0x04;
    inline constexpr uint8_t MODE_INV    = 0x08;
    inline constexpr uint8_t MODE_CSS    = 0x10;
    inline constexpr uint8_t MODE_GM0    = 0x20;
    inline constexpr uint8_t MODE_GM1    = 0x40;
    inline constexpr uint8_t MODE_GM2    = 0x80;

    // Register constants from DECL
    MC6847_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    inline constexpr uint8_t REG_COUNT = 1;

} // namespace mc6847_const

DECL_EXTRACT_ALL(MC6847, MC6847_DECL)

// ============================================================================
// MC6847 Video Display Generator
// ============================================================================

class mc6847_t : public VideoChipBase {
public:
    mc6847_t()
        : VideoChipBase(ChipInfo("MC6847", "Motorola"))
    {
        init_regs(mc6847_const::REG_COUNT);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
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
        update_mode_reg();
    }

    void reset() { init(); }

    // === Mode pins (directly driven by system address decoding / PIA) ===

    void set_ag(bool v)     { mode_ag_ = v; update_mode_reg(); }
    void set_gm(uint8_t v)  { mode_gm_ = v & 0x07; update_mode_reg(); }
    void set_css(bool v)    { mode_css_ = v; update_mode_reg(); }
    void set_as(bool v)     { mode_as_ = v; update_mode_reg(); }
    void set_intext(bool v) { mode_intext_ = v; update_mode_reg(); }
    void set_inv(bool v)    { mode_inv_ = v; update_mode_reg(); }

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

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

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

    // Register mirror (packed mode pins — backed by ChipBase::regs_)

    void update_mode_reg() {
        regs_[mc6847_const::REG_MODE] =
            (mode_ag_     ? mc6847_const::MODE_AG     : 0) |
            (mode_as_     ? mc6847_const::MODE_AS     : 0) |
            (mode_intext_ ? mc6847_const::MODE_INTEXT : 0) |
            (mode_inv_    ? mc6847_const::MODE_INV    : 0) |
            (mode_css_    ? mc6847_const::MODE_CSS    : 0) |
            ((mode_gm_ & 0x01) ? mc6847_const::MODE_GM0 : 0) |
            ((mode_gm_ & 0x02) ? mc6847_const::MODE_GM1 : 0) |
            ((mode_gm_ & 0x04) ? mc6847_const::MODE_GM2 : 0);
    }

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using S = const mc6847_t;
        auto& r = debug_registry_;
        r.set_registers(regs_, mc6847_const::REG_COUNT, MC6847_REG_INFO);
        r.set_decl_order(MC6847_DECL_ORDER.data(), MC6847_DECL_ORDER.size(),
                         MC6847_FLD_INFO, MC6847_NUM_FIELDS,
                         nullptr, 0, nullptr);

        r.category("Video Timing");
        r.value("Scanline", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->scanline_;
        }, 9);
        r.value("Frame", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->frame_counter_;
        }, 32);
        r.flag("FS", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->fs_;
        });
        r.flag("HS", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->hs_;
        });
    }
#endif
};
