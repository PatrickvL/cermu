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
 * Phase 2: Core template — tick skeleton, register I/O, VRAM access.
 * Phase 3: Rendering (included from tms9918_render.inc.hpp).
 */

#include "chip/video/tms9918/tms9918_traits.hpp"
#include "chip/video/tms9918/tms9918_registers.hpp"
#include "chip/video/tms9918/tms9918_palette.hpp"
#include "chip/video/tms9918/tms9918_mixins.hpp"
#include "chip/video/video_chip_base.hpp"
#include "core/indexed_frame_buffer.hpp"
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
        info_ = ChipInfo{Traits.chip_id, Traits.vendor};
        init_regs(Traits.num_registers);

        // Register fixed palette for base TMS9918 variants
        if constexpr (!Traits.has_programmable_palette() && !Traits.is_sega()) {
            set_named_palettes(NAMED_PALETTES, NAMED_PALETTE_COUNT);
        }
    }

    // ====================================================================
    // Display output
    // ====================================================================

    void set_display(IndexedFrameBuffer* d) { display_ = d; }
    IndexedFrameBuffer* display() const { return display_; }

    // ====================================================================
    // Static I/O dispatch helpers (registered in system I/O handler table)
    // ====================================================================

    static bus_state_t port_read(void* ctx, bus_state_t bus) {
        auto* vdp = static_cast<tms9918_t*>(ctx);
        const uint8_t port = BUS_GET_ADDR(bus) & 0x01;
        uint8_t val;

        if (port == 0) {
            // Port 0: VRAM data read
            val = vdp->read_vram_data();
        } else {
            // Port 1: Status register read
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
            // Port 0: VRAM data write
            vdp->write_vram_data(val);
        } else {
            // Port 1: Register/address latch write
            vdp->write_control(val);
        }

        return bus;
    }

    // ====================================================================
    // Tick — master dot clock
    // ====================================================================
    // Called once per VDP dot clock. Drives the scanline/frame counters
    // and triggers rendering and interrupts.

    bus_state_t tick(bus_state_t bus) {
        // Advance dot counter
        dot_++;

        if (dot_ >= VDPTraits::dots_per_line) {
            dot_ = 0;
            end_of_scanline();

            line_++;
            if (line_ >= Traits.total_lines) {
                line_ = 0;
                frame_++;
            }
        }

        // Vblank IRQ: set F flag at start of vblank
        if (line_ == Traits.visible_lines && dot_ == 0) {
            status_ |= regs::STATUS_F;
            if (irq_enabled()) {
                BUS_CLR_BIT(bus, BUS_IRQ_BIT);  // Active-low IRQ
            }
        }

        // Clear IRQ when outside vblank (IRQ is level-triggered)
        if (!(status_ & regs::STATUS_F) || !irq_enabled()) {
            BUS_SET_BIT(bus, BUS_IRQ_BIT);
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
        std::memset(color_line_, 0, sizeof(color_line_));
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
        return decode_screen_mode(regs_[regs::R0], regs_[regs::R1]);
    }

    // ====================================================================
    // VRAM table address calculations
    // ====================================================================

    uint16_t name_table_addr() const {
        return static_cast<uint16_t>(
            BF_GET(regs_[regs::R2], 3:0) << 10);
    }

    uint16_t color_table_addr() const {
        return static_cast<uint16_t>(regs_[regs::R3] << 6);
    }

    uint16_t pattern_gen_addr() const {
        return static_cast<uint16_t>(
            BF_GET(regs_[regs::R4], 2:0) << 11);
    }

    uint16_t sprite_attr_addr() const {
        return static_cast<uint16_t>(
            BF_GET(regs_[regs::R5], 6:0) << 7);
    }

    uint16_t sprite_pattern_addr() const {
        return static_cast<uint16_t>(
            BF_GET(regs_[regs::R6], 2:0) << 11);
    }

    uint8_t backdrop_color() const {
        return BF_GET(regs_[regs::R7], 3:0);
    }

    uint8_t text_color() const {
        return BF_GET(regs_[regs::R7], 7:4);
    }

    bool blank_enabled() const {
        return BF_GET(regs_[regs::R1], 6:6) != 0;
    }

    bool sprite_16x16() const {
        return BF_GET(regs_[regs::R1], 1:1) != 0;
    }

    bool sprite_magnified() const {
        return BF_GET(regs_[regs::R1], 0:0) != 0;
    }

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

    // ====================================================================
    // Status register
    // ====================================================================

    uint8_t status_ = 0;

    // ====================================================================
    // Scanline rendering buffers
    // ====================================================================

    uint8_t color_line_[256] = {};   // Palette index per pixel (current scanline)

    // ====================================================================
    // Display output
    // ====================================================================

    IndexedFrameBuffer* display_ = nullptr;

    // ====================================================================
    // INTERNAL: Register / VRAM I/O
    // ====================================================================

    bool irq_enabled() const {
        return BF_GET(regs_[regs::R1], 5:5) != 0;
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
                // Register write: val[2:0] = register number, latch_byte_ = value
                const uint8_t reg = val & 0x07;
                if (reg < Traits.num_registers) {
                    regs_[reg] = latch_byte_;
                    on_register_write(reg, latch_byte_);
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
    void on_register_write([[maybe_unused]] uint8_t reg,
                           [[maybe_unused]] uint8_t val) {
        // Sega: scroll register writes update mixin state
        if constexpr (Traits.has_scroll()) {
            if (reg == 8) this->scroll_x_ = val;
            if (reg == 9) this->scroll_y_ = val;
        }
    }

    // ====================================================================
    // INTERNAL: Scanline lifecycle
    // ====================================================================

    void end_of_scanline() {
        if (line_ < Traits.visible_lines && blank_enabled()) {
            render_scanline(line_);
        } else if (line_ < Traits.visible_lines) {
            // Screen blanked: fill scanline with backdrop color
            std::memset(color_line_, backdrop_color(), 256);
            flush_scanline(line_);
        }
    }

    void flush_scanline(int row) {
        if (!display_) return;

        // Select palette source
        const uint32_t* palette = system_palette();
        if constexpr (Traits.has_programmable_palette()) {
            palette = this->palette_cache_;
        }
        if constexpr (Traits.is_sega()) {
            palette = this->cram_cache_;
        }

        if (palette) {
            display_->flush_line(row, color_line_, palette, 256);
        }
    }

    // ====================================================================
    // RENDERING — included from separate .inc.hpp files
    // ====================================================================

    void render_scanline(int line);
    void render_mode0_line(int line);   // Graphics I
    void render_mode1_line(int line);   // Text (240 pixels)
    void render_mode2_line(int line);   // Graphics II
    void render_mode3_line(int line);   // Multicolor
    void render_sprites(int line);
};

// ============================================================================
// RENDERING IMPLEMENTATION (Phase 3)
// ============================================================================

#include "chip/video/tms9918/tms9918_render.inc.hpp"
#include "chip/video/tms9918/tms9918_sprites.inc.hpp"

} // namespace tms9918
