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
    // === Mode 7 Teletext rendering (SAA5050) ===
    //
    // Mode 7 screen RAM is at $7C00-$7FFF (1000 bytes for 40×25).
    // Characters are 7-bit Teletext codes (0x20-0x7F printable).
    // Control codes (0x00-0x1F) set display attributes per row.
    //
    // The SAA5050 Teletext generator stores a 5-bit-wide × 10-row character
    // ROM.  At render time each ROM row is:
    //   1. Doubled horizontally (5 → 10 active pixels)
    //   2. Smoothed via character rounding against adjacent rows
    //   3. Fitted into a 16-pixel-wide framebuffer cell (12 → 16 mapping)
    //
    // Graphics characters (0x20-0x3F, 0x60-0x7F when in graphics mode)
    // display as 2×3 mosaic blocks.

    // SAA5050 bit expansion: double each of 5 ROM bits → 10 active pixels.
    static constexpr uint16_t expand_5to10(uint8_t c) {
        return static_cast<uint16_t>(
            ((c & 0x01) * 0x03) + ((c & 0x02) * 0x06) +
            ((c & 0x04) * 0x0C) + ((c & 0x08) * 0x18) +
            ((c & 0x10) * 0x30));
    }

    // SAA5050 character rounding: fill diagonal gaps between adjacent rows.
    static constexpr uint16_t char_round(uint16_t a, uint16_t b) {
        return a | ((a >> 1) & b & ~(b >> 1)) | ((a << 1) & b & ~(b << 1));
    }

    // 12→16 pixel mapping: fb pixel index → SAA5050 sub-pixel index (0-11).
    // Pattern: each group of 3 SAA5050 pixels maps to [2,1,1] fb pixels.
    static constexpr int saa_to_fb_[16] = {
        0,0,1,2, 3,3,4,5, 6,6,7,8, 9,9,10,11
    };

    // SAA5050 character ROM — 96 chars (0x20-0x7F), 10 rows × 5 bits each.
    // Bits 4..0 = pixels left to right.  Verified against the Mullard SAA5050
    // datasheet via the Bedstead project (CC0 public domain).  Rows 0-6 main
    // body, 7-8 descenders, 9 always blank.
    static constexpr uint8_t teletext_rom_[96][10] = {
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x20 space
        {0x04,0x04,0x04,0x04,0x04,0x00,0x04,0x00,0x00,0x00}, // 0x21 !
        {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x22 "
        {0x06,0x09,0x08,0x1C,0x08,0x08,0x1F,0x00,0x00,0x00}, // 0x23 £
        {0x0E,0x15,0x14,0x0E,0x05,0x15,0x0E,0x00,0x00,0x00}, // 0x24 $
        {0x18,0x19,0x02,0x04,0x08,0x13,0x03,0x00,0x00,0x00}, // 0x25 %
        {0x08,0x14,0x14,0x08,0x15,0x12,0x0D,0x00,0x00,0x00}, // 0x26 &
        {0x04,0x04,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x27 '
        {0x02,0x04,0x08,0x08,0x08,0x04,0x02,0x00,0x00,0x00}, // 0x28 (
        {0x08,0x04,0x02,0x02,0x02,0x04,0x08,0x00,0x00,0x00}, // 0x29 )
        {0x04,0x15,0x0E,0x04,0x0E,0x15,0x04,0x00,0x00,0x00}, // 0x2A *
        {0x00,0x04,0x04,0x1F,0x04,0x04,0x00,0x00,0x00,0x00}, // 0x2B +
        {0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x08,0x00,0x00}, // 0x2C ,
        {0x00,0x00,0x00,0x0E,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x2D -
        {0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00}, // 0x2E .
        {0x00,0x01,0x02,0x04,0x08,0x10,0x00,0x00,0x00,0x00}, // 0x2F /
        {0x04,0x0A,0x11,0x11,0x11,0x0A,0x04,0x00,0x00,0x00}, // 0x30 0
        {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E,0x00,0x00,0x00}, // 0x31 1
        {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F,0x00,0x00,0x00}, // 0x32 2
        {0x1F,0x01,0x02,0x06,0x01,0x11,0x0E,0x00,0x00,0x00}, // 0x33 3
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02,0x00,0x00,0x00}, // 0x34 4
        {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E,0x00,0x00,0x00}, // 0x35 5
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E,0x00,0x00,0x00}, // 0x36 6
        {0x1F,0x01,0x02,0x04,0x08,0x08,0x08,0x00,0x00,0x00}, // 0x37 7
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E,0x00,0x00,0x00}, // 0x38 8
        {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C,0x00,0x00,0x00}, // 0x39 9
        {0x00,0x00,0x04,0x00,0x00,0x00,0x04,0x00,0x00,0x00}, // 0x3A :
        {0x00,0x00,0x04,0x00,0x00,0x04,0x04,0x08,0x00,0x00}, // 0x3B ;
        {0x02,0x04,0x08,0x10,0x08,0x04,0x02,0x00,0x00,0x00}, // 0x3C <
        {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00,0x00,0x00,0x00}, // 0x3D =
        {0x08,0x04,0x02,0x01,0x02,0x04,0x08,0x00,0x00,0x00}, // 0x3E >
        {0x0E,0x11,0x02,0x04,0x04,0x00,0x04,0x00,0x00,0x00}, // 0x3F ?
        {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E,0x00,0x00,0x00}, // 0x40 @
        {0x04,0x0A,0x11,0x11,0x1F,0x11,0x11,0x00,0x00,0x00}, // 0x41 A
        {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E,0x00,0x00,0x00}, // 0x42 B
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E,0x00,0x00,0x00}, // 0x43 C
        {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E,0x00,0x00,0x00}, // 0x44 D
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F,0x00,0x00,0x00}, // 0x45 E
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10,0x00,0x00,0x00}, // 0x46 F
        {0x0E,0x11,0x10,0x10,0x13,0x11,0x0F,0x00,0x00,0x00}, // 0x47 G
        {0x11,0x11,0x11,0x1F,0x11,0x11,0x11,0x00,0x00,0x00}, // 0x48 H
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E,0x00,0x00,0x00}, // 0x49 I
        {0x01,0x01,0x01,0x01,0x01,0x11,0x0E,0x00,0x00,0x00}, // 0x4A J
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11,0x00,0x00,0x00}, // 0x4B K
        {0x10,0x10,0x10,0x10,0x10,0x10,0x1F,0x00,0x00,0x00}, // 0x4C L
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11,0x00,0x00,0x00}, // 0x4D M
        {0x11,0x11,0x19,0x15,0x13,0x11,0x11,0x00,0x00,0x00}, // 0x4E N
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E,0x00,0x00,0x00}, // 0x4F O
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10,0x00,0x00,0x00}, // 0x50 P
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D,0x00,0x00,0x00}, // 0x51 Q
        {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11,0x00,0x00,0x00}, // 0x52 R
        {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E,0x00,0x00,0x00}, // 0x53 S
        {0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0x00,0x00,0x00}, // 0x54 T
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E,0x00,0x00,0x00}, // 0x55 U
        {0x11,0x11,0x11,0x0A,0x0A,0x04,0x04,0x00,0x00,0x00}, // 0x56 V
        {0x11,0x11,0x11,0x15,0x15,0x15,0x0A,0x00,0x00,0x00}, // 0x57 W
        {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11,0x00,0x00,0x00}, // 0x58 X
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04,0x00,0x00,0x00}, // 0x59 Y
        {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F,0x00,0x00,0x00}, // 0x5A Z
        {0x00,0x04,0x08,0x1F,0x08,0x04,0x00,0x00,0x00,0x00}, // 0x5B ←
        {0x10,0x10,0x10,0x10,0x16,0x01,0x02,0x04,0x07,0x00}, // 0x5C ½
        {0x00,0x04,0x02,0x1F,0x02,0x04,0x00,0x00,0x00,0x00}, // 0x5D →
        {0x00,0x04,0x0E,0x15,0x04,0x04,0x00,0x00,0x00,0x00}, // 0x5E ↑
        {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A,0x00,0x00,0x00}, // 0x5F #
        {0x00,0x00,0x00,0x1F,0x00,0x00,0x00,0x00,0x00,0x00}, // 0x60 —
        {0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F,0x00,0x00,0x00}, // 0x61 a
        {0x10,0x10,0x1E,0x11,0x11,0x11,0x1E,0x00,0x00,0x00}, // 0x62 b
        {0x00,0x00,0x0F,0x10,0x10,0x10,0x0F,0x00,0x00,0x00}, // 0x63 c
        {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F,0x00,0x00,0x00}, // 0x64 d
        {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E,0x00,0x00,0x00}, // 0x65 e
        {0x02,0x04,0x04,0x0E,0x04,0x04,0x04,0x00,0x00,0x00}, // 0x66 f
        {0x00,0x00,0x0F,0x11,0x11,0x11,0x0F,0x01,0x0E,0x00}, // 0x67 g
        {0x10,0x10,0x1E,0x11,0x11,0x11,0x11,0x00,0x00,0x00}, // 0x68 h
        {0x04,0x00,0x0C,0x04,0x04,0x04,0x0E,0x00,0x00,0x00}, // 0x69 i
        {0x04,0x00,0x04,0x04,0x04,0x04,0x04,0x04,0x08,0x00}, // 0x6A j
        {0x08,0x08,0x09,0x0A,0x0C,0x0A,0x09,0x00,0x00,0x00}, // 0x6B k
        {0x0C,0x04,0x04,0x04,0x04,0x04,0x0E,0x00,0x00,0x00}, // 0x6C l
        {0x00,0x00,0x1A,0x15,0x15,0x15,0x15,0x00,0x00,0x00}, // 0x6D m
        {0x00,0x00,0x1E,0x11,0x11,0x11,0x11,0x00,0x00,0x00}, // 0x6E n
        {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E,0x00,0x00,0x00}, // 0x6F o
        {0x00,0x00,0x1E,0x11,0x11,0x11,0x1E,0x10,0x10,0x00}, // 0x70 p
        {0x00,0x00,0x0F,0x11,0x11,0x11,0x0F,0x01,0x01,0x00}, // 0x71 q
        {0x00,0x00,0x0B,0x0C,0x08,0x08,0x08,0x00,0x00,0x00}, // 0x72 r
        {0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E,0x00,0x00,0x00}, // 0x73 s
        {0x04,0x04,0x0E,0x04,0x04,0x04,0x02,0x00,0x00,0x00}, // 0x74 t
        {0x00,0x00,0x11,0x11,0x11,0x11,0x0F,0x00,0x00,0x00}, // 0x75 u
        {0x00,0x00,0x11,0x11,0x0A,0x0A,0x04,0x00,0x00,0x00}, // 0x76 v
        {0x00,0x00,0x11,0x11,0x15,0x15,0x0A,0x00,0x00,0x00}, // 0x77 w
        {0x00,0x00,0x11,0x0A,0x04,0x0A,0x11,0x00,0x00,0x00}, // 0x78 x
        {0x00,0x00,0x11,0x11,0x11,0x11,0x0F,0x01,0x0E,0x00}, // 0x79 y
        {0x00,0x00,0x1F,0x02,0x04,0x08,0x1F,0x00,0x00,0x00}, // 0x7A z
        {0x08,0x08,0x08,0x08,0x09,0x03,0x05,0x07,0x01,0x00}, // 0x7B ¼
        {0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x00,0x00,0x00}, // 0x7C ‖
        {0x18,0x04,0x18,0x04,0x19,0x03,0x05,0x07,0x01,0x00}, // 0x7D ¾
        {0x00,0x04,0x00,0x1F,0x00,0x04,0x00,0x00,0x00,0x00}, // 0x7E ÷
        {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x00,0x00,0x00}, // 0x7F ■
    };

    // Teletext rendering state (reset at start of each character row)
    uint8_t teletext_fg_   = 7;     // Foreground color (0-7)
    uint8_t teletext_bg_   = 0;     // Background color (0-7)
    bool    teletext_gfx_  = false; // Graphics mode (vs alpha)
    bool    teletext_sep_  = false; // Separated graphics
    bool    teletext_hold_ = false; // Hold graphics mode
    uint8_t teletext_held_ = 0x20;  // Last graphics char for hold mode

    void render_mode7_char(uint16_t screen_offset, uint8_t ra, bool cursor) {
        static constexpr uint16_t MODE7_SCREEN_RAM = 0x7C00;

        if (screen_offset >= 1000) return;

        // Reset held attributes at the start of each row
        if ((screen_offset % bbc_constants::MODE7_COLS) == 0) {
            teletext_fg_   = 7;     // White
            teletext_bg_   = 0;     // Black
            teletext_gfx_  = false;
            teletext_sep_  = false;
            teletext_hold_ = false;
            teletext_held_ = 0x20;
        }

        uint8_t char_code = memory_[MODE7_SCREEN_RAM + screen_offset];

        // Teletext control codes (0x00-0x1F) — set-at semantics
        bool is_control = (char_code < 0x20);
        if (is_control) {
            switch (char_code) {
                case 0x01: case 0x02: case 0x03: case 0x04:
                case 0x05: case 0x06: case 0x07: // Alpha color
                    teletext_fg_ = char_code & 0x07;
                    teletext_gfx_ = false;
                    break;
                case 0x11: case 0x12: case 0x13: case 0x14:
                case 0x15: case 0x16: case 0x17: // Graphics color
                    teletext_fg_ = char_code & 0x07;
                    teletext_gfx_ = true;
                    break;
                case 0x19: teletext_sep_ = false; break; // Contiguous
                case 0x1A: teletext_sep_ = true;  break; // Separated
                case 0x1C: teletext_bg_ = 0;              break; // Black bg
                case 0x1D: teletext_bg_ = teletext_fg_;   break; // New bg
                case 0x1E: teletext_hold_ = true;  break;
                case 0x1F: teletext_hold_ = false; break;
                default: break;
            }
            // Control codes display as space (or held graphic in hold mode)
            char_code = (teletext_hold_ && teletext_gfx_)
                      ? teletext_held_ : 0x20;
        }

        // Raster address → framebuffer row (20 CRTC scanlines → 10 output)
        int row_idx = ra / 2;

        // Framebuffer position
        uint32_t char_col = screen_offset % bbc_constants::MODE7_COLS;
        uint32_t char_row = screen_offset / bbc_constants::MODE7_COLS;
        if (char_row >= bbc_constants::MODE7_ROWS) return;

        uint32_t pixel_x = char_col * 16;
        uint32_t pixel_y = char_row * 10 + row_idx;
        if (pixel_y >= bbc_constants::DISPLAY_HEIGHT) return;
        if (pixel_x + 16 > bbc_constants::DISPLAY_WIDTH) return;

        uint8_t fg = teletext_fg_;
        uint8_t bg = teletext_bg_;
        uint8_t* row_ptr = frame_indices_ + pixel_y * bbc_constants::DISPLAY_WIDTH;

        // Graphics mode: 2×3 mosaic blocks
        bool is_graphic = teletext_gfx_ && !is_control && (char_code & 0x20);
        if (is_graphic) {
            teletext_held_ = char_code;

            // b0=TL, b1=TR, b2=ML, b3=MR, b4=BL, b6=BR
            int band = (row_idx < 3) ? 0 : (row_idx < 7) ? 1 : 2;
            bool left_on, right_on;
            switch (band) {
                case 0:  left_on = char_code & 0x01; right_on = char_code & 0x02; break;
                case 1:  left_on = char_code & 0x04; right_on = char_code & 0x08; break;
                default: left_on = char_code & 0x10; right_on = char_code & 0x40; break;
            }

            // Separated graphics: blank top/bottom of each band and column edges
            if (teletext_sep_) {
                bool gap_row = (row_idx == 0 || row_idx == 3 || row_idx == 7 || row_idx == 9);
                if (gap_row) { left_on = false; right_on = false; }
            }

            for (int i = 0; i < 8; i++) {
                uint8_t c = left_on ? fg : bg;
                if (teletext_sep_ && (i == 0 || i == 7)) c = bg;
                row_ptr[pixel_x + i] = c;
            }
            for (int i = 8; i < 16; i++) {
                uint8_t c = right_on ? fg : bg;
                if (teletext_sep_ && (i == 8 || i == 15)) c = bg;
                row_ptr[pixel_x + i] = c;
            }
        } else {
            // Alpha character — ROM data with expansion and rounding
            if (char_code < 0x20 || char_code > 0x7F) char_code = 0x20;
            uint8_t idx = char_code - 0x20;

            uint16_t expanded = expand_5to10(teletext_rom_[idx][row_idx]);

            // Character rounding: smooth diagonals with row below
            if (row_idx < 9) {
                uint16_t below = expand_5to10(teletext_rom_[idx][row_idx + 1]);
                expanded = char_round(expanded, below);
            }

            if (cursor) expanded ^= 0x03FF;

            // Build 12-bit value: [margin_L | 10 active | margin_R]
            uint16_t saa12 = expanded << 1; // bits 10..1 = active, 11 & 0 = margin

            // Map 12 SAA5050 sub-pixels → 16 framebuffer pixels
            for (int i = 0; i < 16; i++) {
                bool pixel = (saa12 >> (11 - saa_to_fb_[i])) & 1;
                row_ptr[pixel_x + i] = pixel ? fg : bg;
            }
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
