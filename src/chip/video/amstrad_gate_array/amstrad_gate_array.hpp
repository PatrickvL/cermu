#pragma once
/*
 * amstrad_gate_array.hpp — Amstrad Gate Array (40007/40010/40226)
 *
 * The Gate Array is the custom ASIC in the Amstrad CPC that handles:
 *   - Video mode selection and pixel serialization (3 modes)
 *   - Pen/ink palette mapping (16 pens + border → 32 hardware colors)
 *   - ROM banking (lower/upper ROM overlays)
 *   - RAM banking (6128 extended memory configuration)
 *   - Interrupt generation (HSYNC counter → IRQ every 52 scans)
 *
 * The Gate Array reads screen RAM at addresses determined by the MC6845 CRTC
 * and serializes pixels according to the current screen mode, looking up each
 * pixel's pen through the ink[] palette to produce a hardware color index
 * into the 27-entry Amstrad hardware color table.
 *
 * This chip is cross-system: used by CPC 464, 664, 6128, and CPC+.
 */

#include "chip/video/video_chip_base.hpp"
#include "core/indexed_frame_buffer.hpp"
#include "systems/amstrad_cpc/amstrad_cpc_constants.hpp"
#include <cstdint>
#include <cstring>

class amstrad_gate_array_t : public VideoChipBase {
public:
    amstrad_gate_array_t() : VideoChipBase(ChipInfo{"Amstrad Gate Array", "Amstrad"}) {
        category_ = "Video";
    }

    // ChipBase override — called by Board::reset_chips()
    void reset() override {
        pen_select = 0;
        std::memset(ink, 0, sizeof(ink));
        screen_mode = 1;
        lower_rom_enabled = true;
        upper_rom_enabled = true;
        ram_config = 0;
        interrupt_counter = 0;
        interrupt_pending = false;
    }

    // === Palette ===

    static const uint32_t* get_palette()     { return amstrad_cpc_constants::HARDWARE_PALETTE; }
    static int             get_palette_size() { return amstrad_cpc_constants::GA_INK_VALUES; }

    // === Gate Array registers ===

    uint8_t  pen_select = 0;                                      // Selected pen (0-16, 16=border)
    uint8_t  ink[amstrad_cpc_constants::GA_PEN_COUNT]{};          // Pen→hardware color mapping
    uint8_t  screen_mode = 1;                                     // 0, 1, or 2
    bool     lower_rom_enabled = true;                            // BIOS ROM at $0000-$3FFF
    bool     upper_rom_enabled = true;                            // BASIC ROM at $C000-$FFFF
    uint8_t  ram_config = 0;                                      // 6128 RAM banking register
    uint8_t  interrupt_counter = 0;                               // Counts HSYNC, fires IRQ every 52
    bool     interrupt_pending = false;

    // === Frame rendering ===
    //
    // Renders the CPC display (640×400 at mode 2 resolution, each native
    // line drawn twice) into the internal frame_indices_ buffer, then
    // flushes through the pixel unit.
    //
    // The system calls this once per frame, passing:
    //   - ram: pointer to full 64K/128K RAM
    //   - crtc_start: CRTC display start address (R12:R13)

    void render_frame(const uint8_t* ram, uint16_t crtc_start) {
        if (!ram) return;

        // Gate Array maps CRTC address bits [13:12] → 16 KB bank
        uint32_t screen_base = (crtc_start & 0x3000) << 2;

        constexpr int FB_W = amstrad_cpc_constants::FB_WIDTH;

        std::memset(frame_indices_, 0, sizeof(frame_indices_));

        for (int y = 0; y < 200; y++) {
            // Interleaved address: scan lines within character row spaced 2048 bytes apart
            uint32_t line_base = screen_base + (y / 8) * 80 + (y % 8) * 2048;
            uint8_t* dst0 = frame_indices_ + (y * 2) * FB_W;
            uint8_t* dst1 = frame_indices_ + (y * 2 + 1) * FB_W;

            for (int x_byte = 0; x_byte < 80; x_byte++) {
                uint8_t byte = ram[(line_base + x_byte) & 0xFFFF];
                int fb_x;

                switch (screen_mode) {
                    case 0: {
                        // Mode 0: 2 pixels per byte, 16 colors
                        // pixel 0: bits {7,5,3,1} → value 0-15, pixel 1: bits {6,4,2,0}
                        uint8_t px0 = ((byte >> 7) & 1) | (((byte >> 5) & 1) << 1)
                                    | (((byte >> 3) & 1) << 2) | (((byte >> 1) & 1) << 3);
                        uint8_t px1 = ((byte >> 6) & 1) | (((byte >> 4) & 1) << 1)
                                    | (((byte >> 2) & 1) << 2) | (((byte >> 0) & 1) << 3);
                        uint8_t c0 = ink[px0 & 0x0F] & 0x1F;
                        uint8_t c1 = ink[px1 & 0x0F] & 0x1F;
                        // Each mode 0 pixel = 4 framebuffer pixels wide
                        fb_x = x_byte * 8;
                        dst0[fb_x] = dst0[fb_x+1] = dst0[fb_x+2] = dst0[fb_x+3] = c0;
                        dst0[fb_x+4] = dst0[fb_x+5] = dst0[fb_x+6] = dst0[fb_x+7] = c1;
                        dst1[fb_x] = dst1[fb_x+1] = dst1[fb_x+2] = dst1[fb_x+3] = c0;
                        dst1[fb_x+4] = dst1[fb_x+5] = dst1[fb_x+6] = dst1[fb_x+7] = c1;
                        break;
                    }
                    case 1: {
                        // Mode 1: 4 pixels per byte, 4 colors
                        // px0={b7,b3}, px1={b6,b2}, px2={b5,b1}, px3={b4,b0}
                        uint8_t px0 = ((byte >> 7) & 1) | (((byte >> 3) & 1) << 1);
                        uint8_t px1 = ((byte >> 6) & 1) | (((byte >> 2) & 1) << 1);
                        uint8_t px2 = ((byte >> 5) & 1) | (((byte >> 1) & 1) << 1);
                        uint8_t px3 = ((byte >> 4) & 1) | (((byte >> 0) & 1) << 1);
                        uint8_t c0 = ink[px0] & 0x1F;
                        uint8_t c1 = ink[px1] & 0x1F;
                        uint8_t c2 = ink[px2] & 0x1F;
                        uint8_t c3 = ink[px3] & 0x1F;
                        // Each mode 1 pixel = 2 framebuffer pixels wide
                        fb_x = x_byte * 8;
                        dst0[fb_x] = dst0[fb_x+1] = c0;
                        dst0[fb_x+2] = dst0[fb_x+3] = c1;
                        dst0[fb_x+4] = dst0[fb_x+5] = c2;
                        dst0[fb_x+6] = dst0[fb_x+7] = c3;
                        dst1[fb_x] = dst1[fb_x+1] = c0;
                        dst1[fb_x+2] = dst1[fb_x+3] = c1;
                        dst1[fb_x+4] = dst1[fb_x+5] = c2;
                        dst1[fb_x+6] = dst1[fb_x+7] = c3;
                        break;
                    }
                    default:
                    case 2: {
                        // Mode 2: 8 pixels per byte, 2 colors
                        fb_x = x_byte * 8;
                        for (int bit = 7; bit >= 0; --bit) {
                            uint8_t px = (byte >> bit) & 1;
                            uint8_t c = ink[px] & 0x1F;
                            dst0[fb_x] = c;
                            dst1[fb_x] = c;
                            fb_x++;
                        }
                        break;
                    }
                }
            }
        }

        if (display_) display_->flush_frame(frame_indices_, amstrad_cpc_constants::HARDWARE_PALETTE);
    }

    // Display output — set by system via set_display().
    IndexedFrameBuffer* display_ = nullptr;
    void set_display(IndexedFrameBuffer* d) { display_ = d; }

private:
    // Per-frame index buffer for rendering (one byte per pixel)
    uint8_t frame_indices_[amstrad_cpc_constants::FB_WIDTH *
                           amstrad_cpc_constants::FB_HEIGHT] = {};
};
