#pragma once
/*
 * bbc_vidproc.hpp — BBC Micro Video ULA (Video Processor, "VIDPROC")
 *
 * The Video ULA is the custom chip in the BBC Micro that handles:
 *   - Pixel serialization: unpacks bytes from screen RAM into pixels
 *     based on the current mode (1/2/4 bpp)
 *   - Palette mapping: 16-entry logical→physical color lookup
 *   - Clock rate selection: 2 MHz pixel clock (Modes 4-6) or
 *     1 MHz pixel clock (Modes 0-3)
 *   - Cursor display (XOR'd from CRTC cursor signal)
 *   - Teletext mode routing (bypasses pixel serialization, delegates
 *     to SAA5050 Teletext character generator)
 *
 * I/O registers (active accent accent accent accent — active-low accent on A0):
 *   $FE20: Control register (write only)
 *     Bits 7:   Master cursor width
 *     Bits 6-4: Characters per line / clock rate select
 *     Bit  3:   Teletext output select (1 = Mode 7)
 *     Bits 2-1: Cursor width
 *     Bit  0:   Flash color select
 *   $FE21: Palette register (write only)
 *     Bits 7-4: Logical color index (0-15)
 *     Bits 3-1: Physical color (3-bit RGB, XOR inverted)
 *     Bit  0:   Additional color bit (used in some modes)
 *
 * The CRTC (MC6845) drives timing.  On each active character clock,
 * the VIDPROC receives the memory address (MA) and raster address (RA)
 * from the CRTC, reads the corresponding screen byte, and serializes
 * it into pixels according to the current mode.
 *
 * This chip lives in src/chip/video/ and is shared across BBC Micro
 * variants (Model B, B+, Master).
 */

#include "chip/video/video_chip_base.hpp"
#include "core/signal/composite_video_out.hpp"
#include "systems/bbc/bbc_micro_constants.hpp"
#include <cstdint>
#include <cstring>

class bbc_vidproc_t : public VideoChipBase {
public:
    bbc_vidproc_t()
        : VideoChipBase(ChipInfo{"Video ULA", "Acorn", "Acorn Video ULA"})
    {
        category_ = "Video";
        system_palette_ = get_palette();
        palette_size_   = static_cast<uint16_t>(get_palette_size());
    }

    void reset() override {
        control_ = 0;
        std::memset(palette_, 0, sizeof(palette_));
        memory_ = nullptr;
        std::memset(frame_indices_, 0, sizeof(frame_indices_));
    }

    // === Palette ===

    static const uint32_t* get_palette()     { return bbc_constants::PALETTE; }
    static int             get_palette_size() { return 8; }

    // === Memory pointer — set by system during init ===

    void set_memory(const uint8_t* mem) { memory_ = mem; }

    // === ChipBase MMIO interface ===
    //
    // Write-only: $FE20 = control, $FE21 = palette.  A0 selects register.
    // Reads return open bus (the real chip doesn't drive the data bus).

    bool has_mmio() const override { return true; }

    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        // Write-only — don't drive data bus
        return bus;
    }

    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        uint8_t data = BUS_GET_DATA(bus);
        if (BUS_GET_ADDR(bus) & 0x01)
            write_palette(data);
        else
            write_control(data);
        return bus;
    }

    // CS-tick: MMIO self-dispatch
    bus_state_t tick(bus_state_t bus) noexcept {
        if (is_cs_selected(bus)) {
            if (!BUS_GET_BIT(bus, BUS_RW_BIT))
                on_bus_write(bus);
            // Reads are write-only → no-op
            mark_cs_serviced(bus);
        }
        return bus;
    }

    // === I/O register writes ===

    /// Write to Video ULA control register ($FE20)
    void write_control(uint8_t data) { control_ = data; }

    /// Write to Video ULA palette register ($FE21)
    /// High nibble = logical color index, low nibble encodes physical color
    void write_palette(uint8_t data) {
        uint8_t logical  = (data >> 4) & 0x0F;
        uint8_t physical = ((data >> 1) & 0x07) ^ 0x07;  // XOR inverted
        palette_[logical] = physical;
    }

    /// Read control register (for mode detection helpers)
    uint8_t control() const { return control_; }

    // === Mode queries ===

    /// Current display mode (0-7), derived from control register and CRTC state.
    /// The crtc_r9 parameter is MC6845 R9 (max scanline) needed for Mode 7
    /// detection: Mode 7 uses 18+ scanlines per character row.
    int get_display_mode(uint8_t crtc_r9) const {
        // Mode 7: CRTC uses 18+ scanlines per row
        if (crtc_r9 >= 18) return 7;
        // Bits 6-4 of control register select characters per line
        uint8_t chars_per_line_sel = (control_ >> 4) & 0x07;
        switch (chars_per_line_sel) {
            case 0: return 2;   // 10 chars → Mode 2 (16 colors)
            case 1: return 5;   // 20 chars → Mode 5 (4 colors)
            case 2: return 1;   // 20 chars → Mode 1 (4 colors)
            case 3: return 4;   // 40 chars → Mode 4 (2 colors)
            case 4: return 6;   // 40 chars → Mode 6 (text)
            case 5: return 3;   // 40 chars → Mode 3 (text)
            case 6: return 0;   // 80 chars → Mode 0 (2 colors)
            default: return 0;
        }
    }

    /// Pixels packed per screen byte (mode-dependent)
    int get_pixels_per_byte(int mode) const {
        switch (mode) {
            case 0: case 3: case 4: case 6: return 8;   // 1 bpp
            case 1: case 5:                 return 4;   // 2 bpp
            case 2:                         return 2;   // 4 bpp
            default:                        return 8;
        }
    }

    /// Number of logical colors available in the given mode
    int get_colors_per_mode(int mode) const {
        switch (mode) {
            case 0: case 3: case 4: case 6: return 2;
            case 1: case 5:                 return 4;
            case 2:                         return 16;
            default:                        return 2;
        }
    }

    // === Rendering ===

    /// Called by the system once per CRTC character clock during active display.
    /// The VIDPROC reads the screen byte from memory and serializes pixels
    /// into the frame buffer.
    ///
    /// @param ma        CRTC memory address (14-bit)
    /// @param ra        CRTC raster address (row within character)
    /// @param cursor    True if cursor is active at this position
    /// @param crtc_r9   MC6845 R9 — needed for Mode 7 detection
    void display_char(uint16_t ma, uint8_t ra, bool cursor, uint8_t crtc_r9) {
        if (!memory_) return;

        int mode = get_display_mode(crtc_r9);

        if (mode == 7) {
            render_mode7_char(ma & 0x03FF, ra, cursor);
        } else {
            render_bitmap_pixels(ma, ra, cursor, mode);
        }
    }

    /// Called at VSYNC — drives the video output.
    void vsync() {
        if (video_out_) {
            const uint8_t* idx = frame_indices_;
            for (uint32_t y = 0; y < bbc_constants::DISPLAY_HEIGHT; y++) {
                const uint8_t* line = idx + y * bbc_constants::DISPLAY_WIDTH;
                video_out_->drive({0, SyncFlag::HSync});
                for (uint32_t x = 0; x < bbc_constants::DISPLAY_WIDTH; x++) {
                    video_out_->drive({line[x], SyncFlag::BeamOn});
                }
            }
            video_out_->drive({0, SyncFlag::FrameEnd});
        }
    }

    /// Clear the frame buffer (called at start of frame or on mode change)
    void clear() {
        std::memset(frame_indices_, 0, sizeof(frame_indices_));
    }

    CompositeVideoOut* video_out_ = nullptr;
    void set_video_out(CompositeVideoOut* s) { video_out_ = s; }

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    // === Mode 7 Teletext rendering ===
    //
    // Mode 7 screen RAM is at $7C00-$7FFF (1000 bytes for 40×25).
    // Characters are 7-bit Teletext codes.
    // TODO: Implement SAA5050 Teletext character generator for proper 12×20 cells.
    // Current placeholder renders printable ASCII with an 8-pixel-wide glyph,
    // doubled to 16 pixels wide, scaled vertically to fit 640×256.

    void render_mode7_char(uint16_t screen_offset, uint8_t ra, bool cursor) {
        static constexpr uint16_t MODE7_SCREEN_RAM = 0x7C00;

        if (screen_offset >= 1000) return;

        uint8_t char_code = memory_[MODE7_SCREEN_RAM + screen_offset];

        // Simple ASCII rendering for characters 0x20-0x7F
        // Teletext control codes (0x00-0x1F) are used for color switching etc.
        // For initial implementation, render printable chars white on black
        uint8_t pixel_row = 0;
        bool is_control = (char_code < 0x20);

        if (!is_control) {
            // Use a minimal built-in 8×8 font for printable ASCII
            // (The real SAA5050 Teletext chip has a 12×20 character cell
            //  but for initial bring-up, 8-pixel-wide chars are sufficient)
            // TODO: Implement SAA5050 Teletext character generator
            pixel_row = 0;  // Placeholder — all blank until font is loaded
        }

        if (cursor && ra < 8) {
            pixel_row = ~pixel_row;
        }

        // Calculate framebuffer position
        uint32_t char_col = screen_offset % bbc_constants::MODE7_COLS;
        uint32_t char_row = screen_offset / bbc_constants::MODE7_COLS;

        if (char_row >= bbc_constants::MODE7_ROWS) return;

        // Mode 7: each character is 16×20 pixels (to fill 640×500 → scaled to 640×256)
        // But we render at native DISPLAY_HEIGHT=256, so scale vertically
        uint32_t pixel_x = char_col * 16;
        uint32_t pixel_y = char_row * 10 + (ra / 2);  // 20 scanlines → 10 pixels visible

        if (pixel_y >= bbc_constants::DISPLAY_HEIGHT) return;
        if (pixel_x + 16 > bbc_constants::DISPLAY_WIDTH) return;

        uint8_t fg_idx = 7;  // White
        uint8_t bg_idx = 0;  // Black
        uint8_t* row_ptr = frame_indices_ + pixel_y * bbc_constants::DISPLAY_WIDTH;

        // Render 8 source pixels, doubled to 16 output pixels
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t idx = (pixel_row & (1 << bit)) ? fg_idx : bg_idx;
            uint32_t px = pixel_x + (7 - bit) * 2;
            if (px < bbc_constants::DISPLAY_WIDTH) row_ptr[px] = idx;
            if (px + 1 < bbc_constants::DISPLAY_WIDTH) row_ptr[px + 1] = idx;
        }
    }

    // === Bitmap mode rendering (Modes 0-6) ===
    //
    // The CRTC address (MA) and raster address (RA) combine to address
    // screen RAM.  The Video ULA unpacks bytes into pixels according to
    // the mode's bits-per-pixel setting.

    void render_bitmap_pixels(uint16_t ma, uint8_t ra, bool cursor, int mode) {
        uint16_t byte_addr = (ma * 8) + ra;
        if (byte_addr >= bbc_constants::RAM_SIZE) return;

        uint8_t screen_byte = memory_[byte_addr];

        if (cursor) screen_byte = ~screen_byte;

        int ppb = get_pixels_per_byte(mode);
        int colors = get_colors_per_mode(mode);

        // Determine pixel width (how many framebuffer pixels per source pixel)
        int pixel_width = bbc_constants::DISPLAY_WIDTH / (ppb * 40);
        if (pixel_width < 1) pixel_width = 1;

        // Calculate screen position from CRTC counters
        uint32_t col = (ma % 40);
        uint32_t row = (ma / 40);

        uint32_t pixel_x = col * ppb * pixel_width;
        uint32_t pixel_y = row * 8 + ra;

        if (pixel_y >= bbc_constants::DISPLAY_HEIGHT) return;

        uint8_t* row_ptr = frame_indices_ + pixel_y * bbc_constants::DISPLAY_WIDTH;

        // Unpack screen byte into pixels based on bits-per-pixel
        for (int p = 0; p < ppb; p++) {
            uint8_t color_index = 0;

            if (colors == 2) {
                // 1 bpp: 8 pixels per byte (Mode 0, 3, 4, 6)
                color_index = (screen_byte >> (7 - p)) & 0x01;
            } else if (colors == 4) {
                // 2 bpp: 4 pixels per byte (Mode 1, 5)
                // Bits are interleaved: pixel N uses bits (7-N) and (3-N)
                int bit_hi = (screen_byte >> (7 - p)) & 0x01;
                int bit_lo = (screen_byte >> (3 - p)) & 0x01;
                color_index = (bit_hi << 1) | bit_lo;
            } else if (colors == 16) {
                // 4 bpp: 2 pixels per byte (Mode 2)
                if (p == 0) {
                    color_index = ((screen_byte >> 7) & 1) << 3 |
                                  ((screen_byte >> 5) & 1) << 2 |
                                  ((screen_byte >> 3) & 1) << 1 |
                                  ((screen_byte >> 1) & 1);
                } else {
                    color_index = ((screen_byte >> 6) & 1) << 3 |
                                  ((screen_byte >> 4) & 1) << 2 |
                                  ((screen_byte >> 2) & 1) << 1 |
                                  ((screen_byte >> 0) & 1);
                }
            }

            // Map logical color through Video ULA palette to physical color index
            uint8_t physical = palette_[color_index & 0x0F] & 0x07;

            // Write pixel(s) as palette index to index buffer
            for (int w = 0; w < pixel_width; w++) {
                uint32_t px = pixel_x + p * pixel_width + w;
                if (px < bbc_constants::DISPLAY_WIDTH) {
                    row_ptr[px] = physical;
                }
            }
        }
    }

    // === Registers ===
    uint8_t control_ = 0;              // $FE20 control register
    uint8_t palette_[16]{};            // Logical→physical color mapping

    // === Memory access ===
    const uint8_t* memory_ = nullptr;  // Pointer to system RAM (set by system)

    // Internal pixel buffer — replaces the former IndexedFrameBuffer dependency.
    uint8_t frame_indices_[bbc_constants::DISPLAY_WIDTH * bbc_constants::DISPLAY_HEIGHT] = {};
};
