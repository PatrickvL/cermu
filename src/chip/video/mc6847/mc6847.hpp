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
#include "core/indexed_frame_buffer.hpp"
#include "core/signal/composite_video_stream.hpp"
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
    namespace reg {
        MC6847_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
        inline constexpr uint8_t REG_COUNT = 1;
    }
    using namespace reg;

    namespace fld {
#define MC6847_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
        inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
        inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
    MC6847_DECL(DECL_REG_NOP, MC6847_X_FLD_NS_, DECL_CMP_NOP)
#undef MC6847_X_FLD_NS_
    } // namespace fld

} // namespace mc6847_const

DECL_EXTRACT(MC6847, MC6847_DECL)

// ============================================================================
// MC6847 Internal Character ROM — 64 chars, 5×7 matrix in 8×8 cells
//
// Character set layout:
//   Index  0–31: @ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_
//   Index 32–63:  !"#$%&'()*+,-./0123456789:;<=>?
//
// Each entry is 8 bytes, one per pixel row, bit 7 = leftmost pixel.
// ============================================================================

namespace mc6847_font {

inline constexpr uint8_t DATA[64][8] = {
    {0x3C,0x42,0x9A,0xA2,0xA2,0x9E,0x40,0x3C},  //  0: @
    {0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00},  //  1: A
    {0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00},  //  2: B
    {0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00},  //  3: C
    {0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00},  //  4: D
    {0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00},  //  5: E
    {0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00},  //  6: F
    {0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00},  //  7: G
    {0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00},  //  8: H
    {0x3E,0x08,0x08,0x08,0x08,0x08,0x3E,0x00},  //  9: I
    {0x02,0x02,0x02,0x02,0x02,0x42,0x3C,0x00},  // 10: J
    {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00},  // 11: K
    {0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00},  // 12: L
    {0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00},  // 13: M
    {0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00},  // 14: N
    {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},  // 15: O
    {0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00},  // 16: P
    {0x3C,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00},  // 17: Q
    {0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00},  // 18: R
    {0x3E,0x40,0x40,0x3C,0x02,0x02,0x7C,0x00},  // 19: S
    {0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x00},  // 20: T
    {0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},  // 21: U
    {0x42,0x42,0x42,0x42,0x42,0x24,0x18,0x00},  // 22: V
    {0x42,0x42,0x42,0x42,0x5A,0x66,0x42,0x00},  // 23: W
    {0x42,0x42,0x24,0x18,0x24,0x42,0x42,0x00},  // 24: X
    {0x41,0x41,0x22,0x14,0x08,0x08,0x08,0x00},  // 25: Y
    {0x7E,0x02,0x04,0x18,0x20,0x40,0x7E,0x00},  // 26: Z
    {0x3E,0x20,0x20,0x20,0x20,0x20,0x3E,0x00},  // 27: [
    {0x40,0x20,0x10,0x08,0x04,0x02,0x01,0x00},  // 28: backslash
    {0x7C,0x04,0x04,0x04,0x04,0x04,0x7C,0x00},  // 29: ]
    {0x18,0x24,0x42,0x00,0x00,0x00,0x00,0x00},  // 30: ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00},  // 31: _
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},  // 32: space
    {0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x00},  // 33: !
    {0x28,0x28,0x00,0x00,0x00,0x00,0x00,0x00},  // 34: "
    {0x24,0x24,0x7E,0x24,0x7E,0x24,0x24,0x00},  // 35: #
    {0x08,0x3E,0x48,0x38,0x0A,0x7C,0x08,0x00},  // 36: $
    {0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00},  // 37: %
    {0x30,0x48,0x50,0x20,0x52,0x4C,0x32,0x00},  // 38: &
    {0x10,0x20,0x00,0x00,0x00,0x00,0x00,0x00},  // 39: '
    {0x08,0x10,0x20,0x20,0x20,0x10,0x08,0x00},  // 40: (
    {0x10,0x08,0x04,0x04,0x04,0x08,0x10,0x00},  // 41: )
    {0x00,0x22,0x14,0x7F,0x14,0x22,0x00,0x00},  // 42: *
    {0x00,0x08,0x08,0x3E,0x08,0x08,0x00,0x00},  // 43: +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x08,0x10},  // 44: ,
    {0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00},  // 45: -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},  // 46: .
    {0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00},  // 47: /
    {0x3C,0x42,0x46,0x4A,0x52,0x62,0x3C,0x00},  // 48: 0
    {0x08,0x18,0x08,0x08,0x08,0x08,0x1C,0x00},  // 49: 1
    {0x3C,0x42,0x02,0x0C,0x30,0x40,0x7E,0x00},  // 50: 2
    {0x3C,0x42,0x02,0x1C,0x02,0x42,0x3C,0x00},  // 51: 3
    {0x04,0x0C,0x14,0x24,0x7E,0x04,0x04,0x00},  // 52: 4
    {0x7E,0x40,0x7C,0x02,0x02,0x42,0x3C,0x00},  // 53: 5
    {0x1C,0x20,0x40,0x7C,0x42,0x42,0x3C,0x00},  // 54: 6
    {0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x00},  // 55: 7
    {0x3C,0x42,0x42,0x3C,0x42,0x42,0x3C,0x00},  // 56: 8
    {0x3C,0x42,0x42,0x3E,0x02,0x04,0x38,0x00},  // 57: 9
    {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},  // 58: :
    {0x00,0x18,0x18,0x00,0x18,0x08,0x10,0x00},  // 59: ;
    {0x06,0x08,0x10,0x20,0x10,0x08,0x06,0x00},  // 60: <
    {0x00,0x00,0x3E,0x00,0x3E,0x00,0x00,0x00},  // 61: =
    {0x60,0x10,0x08,0x04,0x08,0x10,0x60,0x00},  // 62: >
    {0x3C,0x42,0x02,0x0C,0x08,0x00,0x08,0x00},  // 63: ?
};

// Color palette: index 0 = black, index 1 = green (phosphor)
inline constexpr uint32_t PALETTE[2] = {
    0xFF000000,  // 0: Black
    0xFF00CC00,  // 1: Green (MC6847 phosphor green)
};
inline constexpr int PALETTE_SIZE = 2;

} // namespace mc6847_font

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

    // ====================================================================
    // FRAME RENDERING — self-contained indexed pixel output
    // ====================================================================
    //
    // The MC6847 owns the palette, internal font ROM, and rendering logic.
    // Systems call render_frame(vram) once per field sync; the chip fills
    // frame_indices with palette indices, then flushes through the
    // IndexedFrameBuffer (GPU path → index_buffer, CPU path → RGBA framebuffer).
    //
    // This renders the 256×192 active display area only (no border).
    //
    // Alphanumeric mode (AG=0):
    //   Each VRAM byte encodes: bit 7 = INV, bit 6 = semigraphics,
    //   bits 5-0 = character index (0–63) for the internal font ROM.
    //   Cell size: 8 wide × 12 tall (font 8×8 centred in rows 2–9).
    //
    // Graphics modes (AG=1): not yet rendered; frame is left black.
    //
    // Parameters:
    //   vram  — pointer to video RAM (at least TEXT_COLS * TEXT_ROWS bytes
    //           for text, up to VRAM_GRAPHICS_MAX for full graphics)
    // ====================================================================

    void render_frame(const uint8_t* vram) {
        static constexpr int W     = mc6847_const::DISPLAY_WIDTH;   // 256
        static constexpr int H     = mc6847_const::DISPLAY_HEIGHT;  // 192
        static constexpr int CELL_W = 8;
        static constexpr int CELL_H = 12;
        static constexpr int COLS  = mc6847_const::TEXT_COLS;        // 32
        static constexpr int ROWS  = mc6847_const::TEXT_ROWS;        // 16

        if (!vram || !display_) return;

        uint8_t* fb = display_->indices();

        if (mode_ag_) {
            // Full-graphics modes: clear to black (placeholder)
            std::memset(fb, 0, W * H);
            display_->flush(mc6847_font::PALETTE);
            drive_stream_from_indices(fb, W, H);
            return;
        }

        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                uint8_t byte = vram[row * COLS + col];
                bool     inv = (byte & 0x80) != 0;
                bool     sem = (byte & 0x40) != 0;
                uint8_t  chr = byte & 0x3F;

                int fb_x = col * CELL_W;
                int fb_y = row * CELL_H;

                if (sem) {
                    // Semigraphics-4: 4 quadrants, 2 colours
                    uint8_t fg_idx = 1;  // green
                    uint8_t bg_idx = 0;  // black
                    for (int qr = 0; qr < 2; qr++) {
                        for (int qc = 0; qc < 2; qc++) {
                            int bit = (1 - qr) * 2 + (1 - qc);
                            uint8_t idx = (chr & (1 << bit)) ? fg_idx : bg_idx;
                            int px0 = fb_x + qc * (CELL_W / 2);
                            int py0 = fb_y + qr * (CELL_H / 2);
                            for (int dy = 0; dy < CELL_H / 2; dy++) {
                                for (int dx = 0; dx < CELL_W / 2; dx++) {
                                    fb[(py0 + dy) * W + (px0 + dx)] = idx;
                                }
                            }
                        }
                    }
                } else {
                    // Alphanumeric: render from internal font ROM
                    const uint8_t* glyph = mc6847_font::DATA[chr];
                    uint8_t fg_idx = inv ? 0 : 1;
                    uint8_t bg_idx = inv ? 1 : 0;

                    for (int gy = 0; gy < CELL_H; gy++) {
                        // Font rows 0–7 mapped to cell rows 2–9, blank above/below
                        uint8_t bits = (gy >= 2 && gy < 10) ? glyph[gy - 2] : 0x00;
                        for (int gx = 0; gx < CELL_W; gx++) {
                            bool set = (bits & (0x80u >> gx)) != 0;
                            fb[(fb_y + gy) * W + (fb_x + gx)] = set ? fg_idx : bg_idx;
                        }
                    }
                }
            }
        }

        display_->flush(mc6847_font::PALETTE);
        drive_stream_from_indices(display_->indices(), W, H);
    }

    /// Get the palette for GPU indexed rendering registration.
    static const uint32_t* get_palette()     { return mc6847_font::PALETTE; }
    static int             get_palette_size() { return mc6847_font::PALETTE_SIZE; }

    // Display output — set by system via set_display().
    IndexedFrameBuffer* display_ = nullptr;
    void set_display(IndexedFrameBuffer* d) { display_ = d; }

    CompositeVideoStream* video_stream_ = nullptr;
    void set_stream(CompositeVideoStream* s) { video_stream_ = s; }

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    void drive_stream_from_indices(const uint8_t* idx, int w, int h) {
        if (!video_stream_) return;
        for (int y = 0; y < h; y++) {
            const uint8_t* line = idx + y * w;
            video_stream_->drive({0, VideoFlags::HSync});
            for (int i = 0; i < w; i++) {
                video_stream_->drive({line[i], VideoFlags::BeamOn});
            }
        }
        video_stream_->drive({0, VideoFlags::FrameEnd});
    }

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
        r.set_decl_entries(MC6847_DECL_ENTRIES.data(), MC6847_DECL_ENTRIES.size());

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
