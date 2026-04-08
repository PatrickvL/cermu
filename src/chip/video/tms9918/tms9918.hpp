#pragma once
/*
 * tms9918.hpp — TMS9918 VDP family template
 *
 * Template class for the entire TMS9918 VDP family:
 *   TMS9918, TMS9918A, TMS9928A, TMS9929, TMS9929A,
 *   V9938, V9958, Sega 315-5124, Sega 315-5246.
 *
 * Uses NTTP (VDPTraits) + conditional mixins for zero-overhead variant dispatch.
 * Follows the fam65xx_t<CPUTraits> pattern.
 *
 * Per-dot-clock cycle-accurate rendering:
 *   - One pixel emitted per VDP dot clock during active display
 *   - Background tiles fetched via pipelined 8-cycle (or 6-cycle text)
 *     repeating pattern with shift-register pixel emission
 *   - Sprite evaluation at line start, composited per-pixel inline
 *   - Flush to video output at end of each visible scanline
 */

#include "chip/video/tms9918/tms9918_traits.hpp"
#include "chip/video/tms9918/tms9918_registers.hpp"
#include "chip/video/tms9918/tms9918_palette.hpp"
#include "chip/video/tms9918/tms9918_mixins.hpp"
#include "chip/video/video_chip_base.hpp"
#include "core/signal/composite_video_out.hpp"
#include "core/system_lines.hpp"
#include <cstring>

namespace tms9918 {

// ============================================================================
// TMS9918 VDP TEMPLATE
// ============================================================================

template <const VDPTraits& Traits>
class tms9918_t : public VideoChipBase,
                  public vdp_palette_base_t<Traits>,
                  public vdp_command_base_t<Traits>,
                  public vdp_scroll_base_t<Traits>,
                  public vdp_sega_base_t<Traits> {
public:
    // ====================================================================
    // Construction / identity
    // ====================================================================

    tms9918_t() {
        info_ = ChipInfo{Traits.chip_id, Traits.vendor, Traits.display_name};
        init_regs(Traits.num_registers);
        active_lines_ = Traits.visible_lines;

        // Register fixed palette for base TMS9918 variants
        if constexpr (!Traits.has_programmable_palette() && !Traits.is_sega()) {
            set_named_palettes(NAMED_PALETTES, NAMED_PALETTE_COUNT);
        }
    }

    // ====================================================================
    // Video output
    // ====================================================================

    CompositeVideoOut* video_out_ = nullptr;
    void set_video_out(CompositeVideoOut* s) { video_out_ = s; }

    // ====================================================================
    // Static I/O dispatch helpers (registered in system I/O handler table)
    // ====================================================================

    static bus_state_t port_read(void* ctx, bus_state_t bus) {
        auto* vdp = static_cast<tms9918_t*>(ctx);
        const uint8_t port = BUS_GET_ADDR(bus) & 0x01;
        uint8_t val;

        if (port == 0) {
            val = vdp->read_vram_data();
        } else {
            val = vdp->read_status();
        }

        BUS_SET_DATA(bus, val);
        return bus;
    }

    static bus_state_t port_write(void* ctx, bus_state_t bus) {
        auto* vdp = static_cast<tms9918_t*>(ctx);
        const uint8_t port = BUS_GET_ADDR(bus) & 0x01;
        const uint8_t val = BUS_GET_DATA(bus);

        if (port == 0) {
            vdp->write_vram_data(val);
        } else {
            vdp->write_control(val);
        }

        return bus;
    }

    // ====================================================================
    // Tick — master dot clock (per-dot cycle-accurate)
    // ====================================================================
    //
    // Called once per VDP dot clock (342 dots per line).
    //
    // Scanline phases:
    //   Dots 0-255:   Active display — emit one pixel per dot.
    //                 Background tile data fetched via 8-cycle pipeline
    //                 (6-cycle for text mode). Sprites composited inline.
    //   Dot 256:      End-of-active — flush scanline to framebuffer.
    //   Dots 257-341: Horizontal blank — sprite evaluation for next line,
    //                 prefetch first tile data for next line.
    //
    // Rendering is suppressed when the BL (blank) bit in R1 is cleared
    // or when the line is in the vertical blanking interval.

    bus_state_t tick(bus_state_t bus) {
        const bool visible_line = line_ < active_lines_;

        if (visible_line) {
            if (dot_ < 256) {
                // --- Active display: one pixel per dot ---
                if (blank_enabled()) {
                    // Background fetch pipeline (mode-dependent)
                    bg_fetch_step();
                    // Emit background pixel from shift register
                    uint8_t pixel = emit_bg_pixel();
                    // Store BG priority before sprite compositing
                    bg_priority_[dot_] = bg_.priority;
                    // Sprite overlay (not in text mode)
                    if (screen_mode_ != ScreenMode::TEXT) {
                        pixel = composite_sprite_pixel(dot_, pixel);
                    }
                    // R0.D5: mask leftmost 8 pixels to backdrop (Sega mode 4)
                    if constexpr (Traits.is_sega()) {
                        if (dot_ < 8 && (regs_[reg::R0] & 0x20) &&
                            screen_mode_ >= ScreenMode::SEGA_MODE4) {
                            pixel = backdrop_color();
                        }
                    }
                    color_line_[dot_] = pixel;
                } else {
                    // Screen blanked — backdrop
                    color_line_[dot_] = backdrop_color();
                }
            } else if (dot_ == 256) {
                // --- End of active display: flush line ---
                flush_scanline(line_);
            }
        }

        // --- Sprite evaluation: runs during HBlank of the PREVIOUS line ---
        // At dot 0 of each visible line, sprites have already been evaluated
        // during the HBlank of the line above. For the first visible line,
        // evaluation runs during the pre-render period.
        if (dot_ == 258 && visible_line) {
            // Evaluate sprites for the NEXT visible line
            const uint16_t next_line = line_ + 1;
            if (next_line < active_lines_) {
                evaluate_sprites(next_line);
            }
        }

        // --- VBlank interrupt ---
        // F flag set at first dot of the first VBlank line
        if (line_ == active_lines_ && dot_ == 0) {
            status_ |= reg::STATUS_F;
        }

        // --- IRQ output (active-low, level-triggered) ---
        // Assert if VBlank IRQ (F flag + IE) or line counter IRQ (pending + IE1)
        bool irq_active = ((status_ & reg::STATUS_F) && irq_enabled());
        if constexpr (Traits.is_sega()) {
            irq_active |= (line_irq_pending_ && line_irq_enabled());
        }
        if (irq_active) {
            BUS_CLR_BIT(bus, BUS_IRQ_BIT);
        } else {
            BUS_SET_BIT(bus, BUS_IRQ_BIT);
        }

        // --- Advance timing ---
        dot_++;
        if (dot_ >= VDPTraits::dots_per_line) {
            dot_ = 0;

            // Drive VBlank line markers to the video output (non-visible scanlines)
            if (video_out_ && !visible_line) {
                SyncFlag flags = SyncFlag::HSync | SyncFlag::VSync | SyncFlag::Blank;
                video_out_->drive({0, flags});
            }

            line_++;
            if (line_ >= Traits.total_lines) {
                line_ = 0;
                frame_++;
                // FrameEnd marker in the video output
                if (video_out_) {
                    video_out_->drive({0, SyncFlag::FrameEnd});
                }
                // Reload line counter at frame start
                if constexpr (Traits.is_sega()) {
                    line_counter_ = regs_[10];  // R10 — line counter
                }
            }

            // --- Sega line counter IRQ ---
            // Decremented at each visible scanline boundary.
            // On underflow: set pending flag, reload from R10.
            // During VBlank: reload from R10 each line (no IRQ).
            if constexpr (Traits.is_sega()) {
                if (line_ < active_lines_) {
                    if (line_counter_ == 0) {
                        line_counter_ = regs_[10];  // Reload from R10
                        line_irq_pending_ = true;
                    } else {
                        line_counter_--;
                    }
                } else {
                    // VBlank: reload each line, no IRQ
                    line_counter_ = regs_[10];
                }
            }

            // Begin-of-line setup for the new line
            if (line_ < active_lines_) {
                begin_scanline();
            }
        }

        return bus;
    }

    // ====================================================================
    // Reset
    // ====================================================================

    void reset() override {
        VideoChipBase::reset();
        std::memset(vram_, 0, sizeof(vram_));
        status_ = 0;
        latch_byte_ = 0;
        latch_first_ = true;
        vram_addr_ = 0;
        read_ahead_ = 0;
        dot_ = 0;
        line_ = 0;
        frame_ = 0;
        screen_mode_ = 0;
        active_lines_ = Traits.visible_lines;
        std::memset(color_line_, 0, sizeof(color_line_));
        line_counter_ = 0xFF;
        line_irq_pending_ = false;
        // Clear rendering pipeline
        bg_ = {};
        sprite_count_ = 0;
        std::memset(sprite_collision_, 0, sizeof(sprite_collision_));
    }

    // ====================================================================
    // Public accessors for system integration / debug
    // ====================================================================

    const uint8_t* vram() const { return vram_; }
    uint8_t* vram() { return vram_; }
    static constexpr uint32_t vram_size() { return Traits.vram_size_kb * 1024u; }

    uint16_t current_line() const { return line_; }
    uint16_t current_dot() const { return dot_; }
    uint32_t current_frame() const { return frame_; }
    uint8_t  status_register() const { return status_; }

    uint8_t current_screen_mode() const {
        return decode_screen_mode(regs_[reg::R0], regs_[reg::R1]);
    }

    // ====================================================================
    // VRAM table address calculations
    // ====================================================================

    uint16_t name_table_addr() const {
        return static_cast<uint16_t>(
            BF_GET(regs_[reg::R2], 3:0) << 10);
    }

    uint16_t color_table_addr() const {
        return static_cast<uint16_t>(regs_[reg::R3] << 6);
    }

    uint16_t pattern_gen_addr() const {
        return static_cast<uint16_t>(
            BF_GET(regs_[reg::R4], 2:0) << 11);
    }

    uint16_t sprite_attr_addr() const {
        return static_cast<uint16_t>(
            BF_GET(regs_[reg::R5], 6:0) << 7);
    }

    uint16_t sprite_pattern_addr() const {
        return static_cast<uint16_t>(
            BF_GET(regs_[reg::R6], 2:0) << 11);
    }

    uint8_t backdrop_color() const {
        return BF_GET(regs_[reg::R7], 3:0);
    }

    uint8_t text_color() const {
        return BF_GET(regs_[reg::R7], 7:4);
    }

    bool blank_enabled() const {
        return BF_GET(regs_[reg::R1], 6:6) != 0;
    }

    bool sprite_16x16() const {
        return BF_GET(regs_[reg::R1], 1:1) != 0;
    }

    bool sprite_magnified() const {
        return BF_GET(regs_[reg::R1], 0:0) != 0;
    }

    // Layout virtuals — defined in tms9918_gui.cpp (GUI builds only)
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    // ====================================================================
    // VRAM — compile-time sized from traits
    // ====================================================================

    uint8_t vram_[Traits.vram_size_kb * 1024] = {};

    // ====================================================================
    // Register latch (two-byte FIFO for control port writes)
    // ====================================================================

    uint8_t  latch_byte_ = 0;
    bool     latch_first_ = true;  // true = next write is first byte

    // ====================================================================
    // VRAM address pointer and read-ahead buffer
    // ====================================================================

    uint16_t vram_addr_ = 0;
    uint8_t  read_ahead_ = 0;

    // ====================================================================
    // Timing state
    // ====================================================================

    uint16_t dot_ = 0;              // Horizontal dot counter (0–341)
    uint16_t line_ = 0;             // Vertical line counter (0–261/312)
    uint32_t frame_ = 0;            // Frame counter
    uint16_t active_lines_ = Traits.visible_lines;  // Runtime visible line count
                                    // (192/224/240 for Sega extended modes)

    // ====================================================================
    // Status register
    // ====================================================================

    uint8_t status_ = 0;

    // ====================================================================
    // Line counter IRQ (Sega VDP)
    // ====================================================================

    uint8_t line_counter_ = 0xFF;   // Decremented each visible line; reloads from R10
    bool    line_irq_pending_ = false;

    // ====================================================================
    // Per-dot rendering state — background tile pipeline
    // ====================================================================

    // Cached screen mode for current scanline (set at begin_scanline)
    uint8_t screen_mode_ = 0;

    struct BgPipeline {
        // Fetch latches — filled during pipelined VRAM reads
        uint8_t name_latch  = 0;    // Name table byte (tile index)
        uint8_t pattern_latch = 0;  // Pattern generator byte (pixel data)
        uint8_t color_latch = 0;    // Color table byte (fg/bg or per-row)

        // Shift register — loaded from latches, shifted out per pixel
        uint8_t shift_reg   = 0;    // 8-bit pattern shift register
        uint8_t fg_color    = 0;    // Foreground color for current tile
        uint8_t bg_color    = 0;    // Background color for current tile

        // Multicolor mode: direct color storage (no shift register)
        uint8_t mc_left     = 0;    // Left 4-pixel color
        uint8_t mc_right    = 0;    // Right 4-pixel color

        // Pipeline position tracking
        uint8_t pixel_in_char = 0;  // Current pixel within character (0-7 or 0-5)
        uint8_t column      = 0;    // Current tile column being rendered
        bool    priority    = false; // BG-over-sprite priority (Sega Mode 4)

        // Cached per-line values (set at begin_scanline)
        uint16_t nt_base    = 0;    // Name table base address
        uint16_t ct_base    = 0;    // Color table base address
        uint16_t pg_base    = 0;    // Pattern generator base address
        uint16_t ct_mask    = 0;    // Color table address mask (mode 2)
        uint16_t pg_mask    = 0;    // Pattern gen address mask (mode 2)
        uint16_t region_offset = 0; // Mode 2: third-of-screen offset
        uint8_t  tile_row   = 0;    // Row within tile (0-7, from line & 7)
        uint8_t  row        = 0;    // Tile row index (line >> 3)
        uint8_t  char_width = 8;    // Pixels per character (8 or 6 for text)
    } bg_ = {};

    // ====================================================================
    // Per-dot rendering state — sprite evaluation buffer
    // ====================================================================

    // Maximum sprites per line across all variants
    static constexpr int MAX_SPRITES_LINE = 8;

    struct SpriteEntry {
        int16_t  x;                 // X position (signed for early clock)
        uint8_t  color;             // Sprite color (0 = transparent)
        uint8_t  pattern[4];        // Pattern bytes (up to 4 for 16×16)
        uint8_t  width;             // Pixel width before magnification (8 or 16)
        bool     magnified;         // 2x magnification
    };

    SpriteEntry sprite_buf_[MAX_SPRITES_LINE] = {};
    uint8_t     sprite_count_ = 0;

    // Per-pixel collision tracking for current scanline
    // Each byte is a count of sprite pixels at that X position
    uint8_t sprite_collision_[256] = {};

    // ====================================================================
    // Scanline rendering buffer
    // ====================================================================

    uint8_t color_line_[256] = {};   // Palette index per pixel (current scanline)
    bool    bg_priority_[256] = {};  // BG-over-sprite priority per pixel (Sega Mode 4)

    // ====================================================================
    // INTERNAL: Register / VRAM I/O
    // ====================================================================

    bool irq_enabled() const {
        return BF_GET(regs_[reg::R1], 5:5) != 0;
    }

    /// Line counter interrupt enable (Sega R0.D4 — IE1)
    bool line_irq_enabled() const {
        return (regs_[reg::R0] & 0x10) != 0;
    }

    // --- VRAM data port read (port 0) ---
    uint8_t read_vram_data() {
        // TMS9918 read-ahead: return buffered byte, then fetch next
        const uint8_t val = read_ahead_;
        read_ahead_ = vram_[vram_addr_ & Traits.vram_mask()];
        vram_addr_ = (vram_addr_ + 1) & 0x3FFF;
        latch_first_ = true;  // Any data port access resets latch toggle
        return val;
    }

    // --- VRAM data port write (port 0) ---
    void write_vram_data(uint8_t val) {
        vram_[vram_addr_ & Traits.vram_mask()] = val;
        vram_addr_ = (vram_addr_ + 1) & 0x3FFF;
        read_ahead_ = val;    // Read-ahead is also updated on write
        latch_first_ = true;
    }

    // --- Status register read (port 1) ---
    uint8_t read_status() {
        const uint8_t val = status_;
        // Reading status clears all flags and the latch toggle
        status_ = 0;
        latch_first_ = true;
        // Sega: reading status also clears the line IRQ pending flag
        if constexpr (Traits.is_sega()) {
            line_irq_pending_ = false;
        }
        return val;
    }

    // --- Control port write (port 1) ---
    // Two-byte sequence:
    //   Byte 1: data value (latched)
    //   Byte 2: register number (bit 7 set) or VRAM address setup (bit 7 clear)
    void write_control(uint8_t val) {
        if (latch_first_) {
            latch_byte_ = val;
            latch_first_ = false;
        } else {
            latch_first_ = true;

            if (val & 0x80) {
                // Register write: register number from upper nibble bits
                // Sega: 4-bit field (R0–R10); V9938+: 6-bit; base TMS: 3-bit
                constexpr uint8_t reg_mask = Traits.is_v9938_class() ? 0x3F
                                           : Traits.is_sega() ? 0x0F
                                           : 0x07;
                const uint8_t r = val & reg_mask;
                if (r < Traits.num_registers) {
                    regs_[r] = latch_byte_;
                    on_register_write(r, latch_byte_);
                }
            } else {
                // VRAM address setup
                vram_addr_ = static_cast<uint16_t>(latch_byte_)
                           | (static_cast<uint16_t>(val & 0x3F) << 8);

                // If bit 6 is clear, initiate a read-ahead (read mode)
                if (!(val & 0x40)) {
                    read_ahead_ = vram_[vram_addr_ & Traits.vram_mask()];
                    vram_addr_ = (vram_addr_ + 1) & 0x3FFF;
                }
            }
        }
    }

    // --- Register-write side effects ---
    void on_register_write([[maybe_unused]] uint8_t r,
                           [[maybe_unused]] uint8_t val) {
        // Sega: scroll register writes update mixin state
        if constexpr (Traits.has_scroll()) {
            if (r == 8) this->scroll_x_ = val;
            if (r == 9) this->scroll_y_ = val;
        }
        // Mode register writes may change active line count (Sega 315-5246)
        if constexpr (Traits.has_ext_lines()) {
            if (r == 0 || r == 1) {
                active_lines_ = compute_active_lines();
            }
        }
    }

    // --- Compute runtime active line count from mode bits (315-5246 only) ---
    uint16_t compute_active_lines() const {
        if constexpr (Traits.has_ext_lines()) {
            const uint8_t mode = decode_screen_mode(regs_[reg::R0], regs_[reg::R1]);
            if (mode == ScreenMode::SEGA_MODE4_240) return 240;
            if (mode == ScreenMode::SEGA_MODE4_224) return 224;
        }
        return Traits.visible_lines;  // Default: 192
    }

    // ====================================================================
    // INTERNAL: Scanline lifecycle
    // ====================================================================

    // Flush completed scanline to the video output
    void flush_scanline(int row) {
        // Drive video output with the completed scanline
        if (video_out_) {
            video_out_->drive({0, SyncFlag::HSync | SyncFlag::BeamOn});
            for (int i = 0; i < 256; i++) {
                video_out_->drive({color_line_[i], SyncFlag::BeamOn});
            }
        }
    }

    // ====================================================================
    // RENDERING — per-dot background pipeline + sprite compositing
    // ====================================================================
    // Included from separate .inc.hpp files for organization.

    // Background: begin-of-line setup + first tile prefetch
    void begin_scanline();

    // Background: pipelined tile fetch + shift register pixel emission
    void bg_fetch_step();
    uint8_t emit_bg_pixel();

    // Background: VRAM reads for a specific tile column (mode-aware)
    void prefetch_tile(uint8_t col);
    void load_bg_shifter();

    // Sega mode 4 helpers (compiled only when Traits.is_sega())
    void prefetch_tile_sega(uint8_t col);
    void load_bg_shifter_sega();
    uint8_t emit_bg_pixel_sega();

    // Sprites: evaluation during HBlank, per-pixel compositing
    void evaluate_sprites(uint16_t line);
    uint8_t composite_sprite_pixel(uint16_t pixel_x, uint8_t bg_pixel);
};

// ============================================================================
// RENDERING IMPLEMENTATION
// ============================================================================

#include "chip/video/tms9918/tms9918_render.inc.hpp"
#include "chip/video/tms9918/tms9918_sprites.inc.hpp"

} // namespace tms9918
