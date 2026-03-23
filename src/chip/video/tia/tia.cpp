/*
 * tia.cpp — Atari TIA (Television Interface Adapter) implementation
 *
 * This implements the video and audio generation for the Atari 2600.
 * The TIA runs at 3× the CPU clock, generating 228 color clocks per
 * scanline (68 HBLANK + 160 visible).
 *
 * Object rendering:
 *   - Playfield:  20-bit pattern (PF0[4:7] + PF1[0:7] + PF2[0:7]),
 *                 repeated or reflected for the right half
 *   - Player 0/1: 8-bit pattern, positioned via RESPx/HMPx
 *   - Missile 0/1: Single pixel (scalable), tied to player positioning
 *   - Ball:       Single pixel (scalable), uses playfield color
 *
 * Priority (normal):  PF/BL > P0/M0 > P1/M1 > BK
 * Priority (bit 2 of CTRLPF set): P0/M0 > P1/M1 > PF/BL > BK
 *
 * Audio: Two channels, each with a 5-bit frequency divider controlling
 *        how often polynomial counters update. Various waveform modes.
 */

#include "chip/video/tia/tia.hpp"
#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"
#include <cstring>
#include <algorithm>

REGISTER_CHIP_TYPE("TIA", tia_t)

// ============================================================================
// NTSC PALETTE (128 colors — index is upper 7 bits of color register)
// ============================================================================

const uint32_t tia_t::ntsc_palette[128] = {
    // Hue 0 (grey)
    0xFF000000, 0xFF1A1A1A, 0xFF393939, 0xFF5B5B5B,
    0xFF7E7E7E, 0xFFA2A2A2, 0xFFC7C7C7, 0xFFEDEDED,
    // Hue 1 (gold)
    0xFF190200, 0xFF3A1F00, 0xFF5D4100, 0xFF826400,
    0xFFA78800, 0xFFCCAD00, 0xFFD2D200, 0xFFEDE800,
    // Hue 2 (orange)
    0xFF2B0000, 0xFF521600, 0xFF763700, 0xFF9B5C00,
    0xFFC08100, 0xFFE5A600, 0xFFEDCB00, 0xFFEDE800,
    // Hue 3 (red-orange)
    0xFF340000, 0xFF580500, 0xFF7D2500, 0xFFA14900,
    0xFFCA6E00, 0xFFEF9300, 0xFFEDB800, 0xFFEDE800,
    // Hue 4 (pink)
    0xFF2F0004, 0xFF550016, 0xFF7B0F3B, 0xFFA03561,
    0xFFC55B8A, 0xFFEB81B0, 0xFFEDA6D2, 0xFFEDE8ED,
    // Hue 5 (purple)
    0xFF1F0028, 0xFF450048, 0xFF6B136D, 0xFF903993,
    0xFFB55FB8, 0xFFDB85DE, 0xFFEDAAED, 0xFFEDE8ED,
    // Hue 6 (purple-blue)
    0xFF0C0043, 0xFF300066, 0xFF55158B, 0xFF7B3BB1,
    0xFFA161D6, 0xFFC787ED, 0xFFEDADED, 0xFFEDE8ED,
    // Hue 7 (blue)
    0xFF000050, 0xFF140670, 0xFF371C95, 0xFF5C42BA,
    0xFF8168DF, 0xFFA78EED, 0xFFCCB3ED, 0xFFEDE8ED,
    // Hue 8 (blue)
    0xFF00004E, 0xFF001A6E, 0xFF143B93, 0xFF3961B8,
    0xFF5E87DD, 0xFF84ADED, 0xFFA9D2ED, 0xFFEDE8ED,
    // Hue 9 (light blue)
    0xFF00003E, 0xFF002C5A, 0xFF004F7F, 0xFF1076A4,
    0xFF369CC9, 0xFF5CC2ED, 0xFF81E8ED, 0xFFEDE8ED,
    // Hue 10 (turquoise)
    0xFF000824, 0xFF003240, 0xFF005765, 0xFF077D8A,
    0xFF2DA3AF, 0xFF53C9D4, 0xFF78EDED, 0xFFEDE8ED,
    // Hue 11 (green-blue)
    0xFF001007, 0xFF003420, 0xFF005840, 0xFF0E7E60,
    0xFF34A481, 0xFF5ACAA2, 0xFF7FEDC3, 0xFFEDE8ED,
    // Hue 12 (green)
    0xFF001700, 0xFF003500, 0xFF005600, 0xFF287700,
    0xFF4E9B0F, 0xFF74BF35, 0xFF99E45A, 0xFFBFED81,
    // Hue 13 (yellow-green)
    0xFF0C1500, 0xFF2E3200, 0xFF515200, 0xFF757200,
    0xFF9A9700, 0xFFBFBB28, 0xFFE5E04E, 0xFFEDE875,
    // Hue 14 (orange-green)
    0xFF191000, 0xFF3D2A00, 0xFF624800, 0xFF876A00,
    0xFFAC8E00, 0xFFD2B329, 0xFFEDD84F, 0xFFEDE876,
    // Hue 15 (light orange)
    0xFF1F0700, 0xFF451E00, 0xFF6C3E00, 0xFF916200,
    0xFFB68700, 0xFFDCAD1A, 0xFFEDD240, 0xFFEDE866,
};

// ============================================================================
// COLLISION HELPER
// ============================================================================

bool tia_t::has_collision(uint8_t px_a, uint8_t px_b) const {
    // px_a and px_b are single PX_* bits; cermu_ctz gives the bit index.
    return (cx[cermu_ctz(px_a)] & px_b) != 0;
}

// ============================================================================
// INIT / RESET
// ============================================================================

void tia_t::init() {
    reset();
#ifdef CERMU_HAS_CHIP_DEBUG
    register_debug_fields();
#endif

    // Convert ARGB palette to ABGR (GL_RGBA little-endian convention).
    // The static ntsc_palette[] stores 0xAARRGGBB; the GL texture pipeline
    // expects 0xAABBGGRR so that byte order on little-endian == R,G,B,A.
    for (int i = 0; i < 128; ++i) {
        uint32_t c = ntsc_palette[i];
        palette_rgba_[i] = (c & 0xFF00FF00u)
                         | ((c >> 16) & 0x000000FFu)
                         | ((c << 16) & 0x00FF0000u);
    }
    system_palette_ = palette_rgba_;
    palette_size_   = 128;
}

void tia_t::reset() {
    memset(regs_, 0, num_regs_);
    memset(read_regs_, 0, num_read_regs_);

    h_counter = 0;
    scanline = 0;
    wsync_pending = false;
    hmove_blank_active = false;
    visible_row = -1;
    prev_vblank = false;

    grp0_old = grp1_old = 0;
    enabl_old = false;

    pos_p0 = pos_p1 = 0;
    pos_m0 = pos_m1 = 0;
    pos_bl = 0;

    memset(cx, 0, sizeof(cx));

    // Input ports default to not-pressed (bit 7 high)
    read_regs_[TIA_INPT0] = 0x80;
    read_regs_[TIA_INPT1] = 0x80;
    read_regs_[TIA_INPT2] = 0x80;
    read_regs_[TIA_INPT3] = 0x80;
    read_regs_[TIA_INPT4] = 0x80;
    read_regs_[TIA_INPT5] = 0x80;

    audio[0] = {};
    audio[1] = {};

    audio_buffer_.reset();
    audio_cycle_counter = 0;
}

// ============================================================================
// ============================================================================
// AUDIO
// ============================================================================

void tia_t::set_audio_sample_rate(int sample_rate_hz) {
    if (sample_rate_hz > 0) {
        audio_cycles_per_sample = tia_constants::CPU_FREQ_NTSC / static_cast<uint32_t>(sample_rate_hz);
    }
}

uint32_t tia_t::audio_available() const {
    return static_cast<uint32_t>(audio_buffer_.available());
}

uint32_t tia_t::audio_read(float* buffer, uint32_t max_samples) {
    return static_cast<uint32_t>(audio_buffer_.read(buffer, max_samples));
}

void tia_t::tick_audio_channel(int ch_idx) {
    auto& ch = audio[ch_idx];
    uint8_t control   = regs_[TIA_AUDC0 + ch_idx];
    uint8_t frequency = regs_[TIA_AUDF0 + ch_idx];

    // The TIA audio divider counts down from the frequency value
    if (ch.div_counter == 0) {
        ch.div_counter = frequency;

        // Update polynomial counters and compute output based on control register
        // Advance 4-bit LFSR (polynomial counter)
        bool poly4_feedback = ((ch.poly4 >> 0) ^ (ch.poly4 >> 1)) & 1;
        ch.poly4 = ((ch.poly4 >> 1) | (poly4_feedback ? 0x08 : 0)) & 0x0F;

        // Advance 5-bit LFSR
        bool poly5_feedback = ((ch.poly5 >> 0) ^ (ch.poly5 >> 2)) & 1;
        ch.poly5 = ((ch.poly5 >> 1) | (poly5_feedback ? 0x10 : 0)) & 0x1F;

        // Advance 9-bit polynomial (implemented as two bytes)
        uint16_t poly9_val = ch.poly9 | (static_cast<uint16_t>(ch.poly9_hi) << 8);
        bool poly9_feedback = ((poly9_val >> 0) ^ (poly9_val >> 4)) & 1;
        poly9_val = ((poly9_val >> 1) | (poly9_feedback ? 0x100 : 0)) & 0x1FF;
        ch.poly9 = static_cast<uint8_t>(poly9_val);
        ch.poly9_hi = static_cast<uint8_t>(poly9_val >> 8);

        // Determine output based on AUDC mode
        switch (control & 0x0F) {
            case 0x00:  // Set to 1 (constant)
            case 0x0B:  // Set to 1 (constant)
                ch.output = true;
                break;
            case 0x01:  // 4-bit poly
                ch.output = ch.poly4 & 1;
                break;
            case 0x02:  // div15 → 4-bit poly
                if (ch.poly5 == 0x00) ch.output = ch.poly4 & 1;
                break;
            case 0x03:  // 5-bit poly → 4-bit poly
                if (ch.poly5 & 1) ch.output = ch.poly4 & 1;
                break;
            case 0x04:  // div2 (pure tone)
            case 0x05:  // div2 (pure tone)
                ch.output = !ch.output;
                break;
            case 0x06:  // div31 (pure tone, lower frequency)
            case 0x0A:  // div31 (pure tone, lower frequency)
                if (ch.poly5 == 0x00) ch.output = !ch.output;
                break;
            case 0x07:  // 5-bit poly → div2
            case 0x09:  // 5-bit poly → div2
                if (ch.poly5 & 1) ch.output = !ch.output;
                break;
            case 0x08:  // 9-bit poly
                ch.output = poly9_val & 1;
                break;
            case 0x0C:  // div6 (pure bass tone)
            case 0x0D:  // div6 (pure bass tone)
                if (ch.poly5 == 0x00) ch.output = !ch.output;
                break;
            case 0x0E:  // div93 (very low bass)
                if (ch.poly5 == 0x00) {
                    if (ch.poly4 & 1) ch.output = !ch.output;
                }
                break;
            case 0x0F:  // 5-bit poly → div6
                if (ch.poly5 & 1) {
                    if (ch.poly4 & 1) ch.output = !ch.output;
                }
                break;
        }
    } else {
        ch.div_counter--;
    }
}

// ============================================================================
// PLAYFIELD PIXEL LOOKUP
// ============================================================================

uint8_t tia_t::get_playfield_pixel(int x) const {
    // Playfield is 40 pixels wide (20 pixels repeated or reflected)
    // PF0: bits 4-7 (4 pixels, displayed left to right: D4, D5, D6, D7)
    // PF1: bits 7-0 (8 pixels, displayed left to right: D7, D6, D5, D4, D3, D2, D1, D0)
    // PF2: bits 0-7 (8 pixels, displayed left to right: D0, D1, D2, D3, D4, D5, D6, D7)

    // Convert pixel position to playfield bit index (0-19 for left half)
    int pfx;
    bool reflect = (regs_[TIA_CTRLPF] & 0x01) != 0;

    if (x < 80) {
        // Left half (first 80 pixels = PF bits 0-19)
        pfx = x / 4;  // Each PF bit = 4 color clocks
    } else {
        if (reflect) {
            // Mirror: right half is PF bits 19-0
            pfx = 19 - (x - 80) / 4;
        } else {
            // Repeat: right half is PF bits 0-19 again
            pfx = (x - 80) / 4;
        }
    }

    // Look up the bit — return PX_PF bitmask if set, 0 otherwise
    uint8_t bit;
    if (pfx < 4) {
        // PF0: bits 4-7 (pfx 0 = D4, pfx 3 = D7)
        bit = (regs_[TIA_PF0] >> (4 + pfx)) & 1;
    } else if (pfx < 12) {
        // PF1: bits 7-0 (pfx 4 = D7, pfx 11 = D0)
        bit = (regs_[TIA_PF1] >> (11 - pfx)) & 1;
    } else {
        // PF2: bits 0-7 (pfx 12 = D0, pfx 19 = D7)
        bit = (regs_[TIA_PF2] >> (pfx - 12)) & 1;
    }
    return bit ? PX_PF : 0;
}

// ============================================================================
// PLAYER PIXEL LOOKUP
// ============================================================================

uint8_t tia_t::get_player_pixel(int x, uint8_t grp, uint8_t pos, uint8_t nusiz, bool reflect, uint8_t px_bit) const {
    if (grp == 0) return 0;

    int size = nusiz & 0x07;   // Player number-size field

    // Player pixel width (stretch factor)
    int stretch = 1;
    if (size == 5)      stretch = 2;   // Double-width
    else if (size == 7) stretch = 4;   // Quad-width

    // Number of copies and spacing
    // 0: one copy         1: two close  2: two medium  3: three close
    // 4: two wide         5: double     6: three medium  7: quad
    static constexpr int copy_offsets[8][3] = {
        {0, -1, -1},   // 0: one copy
        {0, 16, -1},   // 1: two close
        {0, 32, -1},   // 2: two medium
        {0, 16, 32},   // 3: three close
        {0, 64, -1},   // 4: two wide
        {0, -1, -1},   // 5: double-width
        {0, 32, 64},   // 6: three medium
        {0, -1, -1},   // 7: quad-width
    };

    int pixel_width = 8 * stretch;

    for (int c = 0; c < 3; ++c) {
        int offset = copy_offsets[size][c];
        if (offset < 0) continue;

        int rel = x - static_cast<int>(pos) - offset;
        // Wrap around 160
        if (rel < 0) rel += 160;
        if (rel >= 160) rel -= 160;

        if (rel >= 0 && rel < pixel_width) {
            int bit_index = rel / stretch;
            if (reflect) {
                if ((grp >> bit_index) & 1) return px_bit;
            } else {
                if ((grp >> (7 - bit_index)) & 1) return px_bit;
            }
        }
    }

    return 0;
}

// ============================================================================
// MISSILE / BALL PIXEL LOOKUP
// ============================================================================

uint8_t tia_t::get_missile_pixel(int x, uint8_t pos, uint8_t size_bits, bool enabled, uint8_t px_bit) const {
    return get_missile_pixel(x, pos, size_bits, enabled, 0, px_bit);
}

uint8_t tia_t::get_missile_pixel(int x, uint8_t pos, uint8_t size_bits, bool enabled, uint8_t nusiz, uint8_t px_bit) const {
    if (!enabled) return 0;

    // Size: 1, 2, 4, or 8 pixels wide (encoded in 2 bits)
    int width = 1 << size_bits;

    // Missile copies follow the same copy positions as the associated player,
    // determined by the low 3 bits of the NUSIZ register.
    int copy_mode = nusiz & 0x07;
    static constexpr int copy_offsets[8][3] = {
        {0, -1, -1},   // 0: one copy
        {0, 16, -1},   // 1: two close
        {0, 32, -1},   // 2: two medium
        {0, 16, 32},   // 3: three close
        {0, 64, -1},   // 4: two wide
        {0, -1, -1},   // 5: double-width player (one copy missile)
        {0, 32, 64},   // 6: three medium
        {0, -1, -1},   // 7: quad-width player (one copy missile)
    };

    for (int c = 0; c < 3; ++c) {
        int offset = copy_offsets[copy_mode][c];
        if (offset < 0) continue;

        int rel = x - static_cast<int>(pos) - offset;
        if (rel < 0) rel += 160;
        if (rel >= 160) rel -= 160;

        if (rel >= 0 && rel < width) return px_bit;
    }

    return 0;
}

// ============================================================================
// RENDER ONE PIXEL
// ============================================================================

void tia_t::render_pixel() {
    int x = h_counter - tia_constants::HBLANK_CLOCKS;
    if (x < 0 || x >= tia_constants::DISPLAY_WIDTH) return;

    // Use visible_row (tracks only non-VBLANK lines) so the first
    // visible scanline maps to framebuffer row 0.
    int row = visible_row;
    if (row < 0 || row >= tia_constants::DISPLAY_HEIGHT) return;

    // Determine which objects are present at this pixel.
    // Each function returns its bitmask constant (PX_*) or 0.
    uint8_t pixel_bits = get_playfield_pixel(x);

    bool vdelp0 = (regs_[TIA_VDELP0] & 0x01) != 0;
    bool vdelp1 = (regs_[TIA_VDELP1] & 0x01) != 0;
    uint8_t p0_grp = vdelp0 ? grp0_old : regs_[TIA_GRP0];
    uint8_t p1_grp = vdelp1 ? grp1_old : regs_[TIA_GRP1];
    uint8_t nusiz0 = regs_[TIA_NUSIZ0];
    uint8_t nusiz1 = regs_[TIA_NUSIZ1];
    bool refp0 = (regs_[TIA_REFP0] & 0x08) != 0;
    bool refp1 = (regs_[TIA_REFP1] & 0x08) != 0;
    pixel_bits |= get_player_pixel(x, p0_grp, pos_p0, nusiz0, refp0, PX_P0);
    pixel_bits |= get_player_pixel(x, p1_grp, pos_p1, nusiz1, refp1, PX_P1);

    // Missile 0 — locked to player 0 if RESMP0 set
    bool resmp0 = (regs_[TIA_RESMP0] & 0x02) != 0;
    uint8_t m0_pos = resmp0 ? pos_p0 : pos_m0;
    uint8_t m0_size = (nusiz0 >> 4) & 0x03;
    bool enam0 = (regs_[TIA_ENAM0] & 0x02) != 0;
    pixel_bits |= get_missile_pixel(x, m0_pos, m0_size, enam0 && !resmp0, nusiz0, PX_M0);

    // Missile 1 — locked to player 1 if RESMP1 set
    bool resmp1 = (regs_[TIA_RESMP1] & 0x02) != 0;
    uint8_t m1_pos = resmp1 ? pos_p1 : pos_m1;
    uint8_t m1_size = (nusiz1 >> 4) & 0x03;
    bool enam1 = (regs_[TIA_ENAM1] & 0x02) != 0;
    pixel_bits |= get_missile_pixel(x, m1_pos, m1_size, enam1 && !resmp1, nusiz1, PX_M1);

    // Ball — uses simple single-position check (no copies)
    bool vdelbl = (regs_[TIA_VDELBL] & 0x01) != 0;
    bool enabl = (regs_[TIA_ENABL] & 0x02) != 0;
    bool bl_enabled = vdelbl ? enabl_old : enabl;
    uint8_t ctrlpf = regs_[TIA_CTRLPF];
    uint8_t bl_size = (ctrlpf >> 4) & 0x03;
    pixel_bits |= get_missile_pixel(x, pos_bl, bl_size, bl_enabled, PX_BL);

    // Update per-object collision accumulators.
    // Only meaningful when 2+ objects overlap at this pixel.
    if (pixel_bits & (pixel_bits - 1)) {
        if (pixel_bits & PX_M0) cx[CX_M0] |= pixel_bits;
        if (pixel_bits & PX_M1) cx[CX_M1] |= pixel_bits;
        if (pixel_bits & PX_P0) cx[CX_P0] |= pixel_bits;
        if (pixel_bits & PX_P1) cx[CX_P1] |= pixel_bits;
        if (pixel_bits & PX_BL) cx[CX_BL] |= pixel_bits;
        if (pixel_bits & PX_PF) cx[CX_PF] |= pixel_bits;
    }

    // Priority-based color selection using bitmask tests
    uint8_t color;
    bool priority = (ctrlpf & 0x04) != 0;
    bool score_mode = (ctrlpf & 0x02) != 0;
    uint8_t colup0 = regs_[TIA_COLUP0];
    uint8_t colup1 = regs_[TIA_COLUP1];
    uint8_t colupf = regs_[TIA_COLUPF];
    uint8_t colubk = regs_[TIA_COLUBK];

    if (regs_[TIA_VBLANK] & 0x02) {
        // During VBLANK, output black
        color = 0;
    } else if (hmove_blank_active && x < 8) {
        // HMOVE blanking: first 8 pixels blanked to background after HMOVE strobe
        color = colubk;
    } else if (priority) {
        // Playfield/Ball priority over players
        if (pixel_bits & (PX_PF | PX_BL)) {
            if (score_mode) {
                // Score mode overrides PF color even with priority flag
                color = (x < 80) ? colup0 : colup1;
            } else {
                color = colupf;
            }
        } else if (pixel_bits & (PX_P0 | PX_M0)) {
            color = colup0;
        } else if (pixel_bits & (PX_P1 | PX_M1)) {
            color = colup1;
        } else {
            color = colubk;
        }
    } else {
        // Players have priority over playfield
        if (pixel_bits & (PX_P0 | PX_M0)) {
            color = colup0;
        } else if (pixel_bits & (PX_P1 | PX_M1)) {
            color = colup1;
        } else if (pixel_bits & (PX_PF | PX_BL)) {
            if (score_mode) {
                // Score mode: left half uses P0 color, right half uses P1 color
                color = (x < 80) ? colup0 : colup1;
            } else {
                color = colupf;
            }
        } else {
            color = colubk;
        }
    }

    // Store palette index in scanline buffer — deferred to flush at end of scanline.
    // Color register upper 7 bits select palette entry.
    color_line_buffer[x] = (color >> 1) & 0x7F;
}

// ============================================================================
// TICK COLOR CLOCK
// ============================================================================

void tia_t::tick_color_clock() {
    bool vblank = (regs_[TIA_VBLANK] & 0x02) != 0;

    // Detect VBLANK→visible transition before first pixel is rendered.
    // This ensures visible_row=0 is available for the first visible scanline.
    if (!vblank && visible_row < 0) {
        visible_row = 0;
    }

    // Render visible pixel
    if (h_counter >= tia_constants::HBLANK_CLOCKS) {
        render_pixel();
    }

    // Advance horizontal counter
    h_counter++;

    if (h_counter >= tia_constants::CLOCKS_PER_LINE) {
        h_counter = 0;

        // Release WSYNC at end of scanline
        wsync_pending = false;

        // Clear HMOVE blanking at start of new scanline
        hmove_blank_active = false;

        // Drive video stream at end of each scanline
        if (video_stream_) {
            bool vsync_active = (regs_[TIA_VSYNC] & 0x02) != 0;

            VideoFlags sync_flags = VideoFlags::HSync;
            if (vblank || vsync_active)
                sync_flags = sync_flags | VideoFlags::VSync | VideoFlags::Blank;
            // FrameEnd on VSYNC rising edge (program declares frame boundary)
            if (vsync_active && !prev_vsync_stream_)
                sync_flags = sync_flags | VideoFlags::FrameEnd;
            prev_vsync_stream_ = vsync_active;

            video_stream_->drive({0, sync_flags});

            if (!vblank && visible_row >= 0) {
                for (int i = 0; i < tia_constants::DISPLAY_WIDTH; i++) {
                    video_stream_->drive({color_line_buffer[i], VideoFlags::BeamOn});
                }
            }
        }

        // Track visible row for framebuffer mapping.
        if (!vblank) {
            // Blit the scanline to the test framebuffer (if attached).
            if (test_framebuffer_ && visible_row >= 0 && visible_row < test_fb_height_) {
                uint32_t* row_ptr = test_framebuffer_ + visible_row * test_fb_width_;
                int cols = (test_fb_width_ < tia_constants::DISPLAY_WIDTH)
                           ? test_fb_width_ : tia_constants::DISPLAY_WIDTH;
                for (int i = 0; i < cols; i++) {
                    row_ptr[i] = palette_rgba_[color_line_buffer[i] & 0x7F];
                }
            }

            // Row was already set to 0 before first pixel (see above).
            // At end of each visible scanline, advance to next row.
            visible_row++;
        } else {
            visible_row = -1;      // In VBLANK — no visible row
        }

        // Advance scanline
        scanline++;
    }
}

void tia_t::tick_cpu_cycle() {
    tick_color_clock();
    tick_color_clock();
    tick_color_clock();

    // Audio: tick both channels at CPU cycle rate (every 1/2 scanline concept,
    // but TIA actually ticks audio at color clock / 114 rate for each channel).
    // Simplified: tick audio at CPU cycle rate (standard approach for emulators).
    tick_audio_channel(0);
    tick_audio_channel(1);

    // Generate audio sample if enough cycles have passed
    if (audio_cycles_per_sample > 0) {
        audio_cycle_counter++;
        if (audio_cycle_counter >= audio_cycles_per_sample) {
            audio_cycle_counter -= audio_cycles_per_sample;

            // Mix both channels
            float sample = 0.0f;
            for (int c = 0; c < 2; ++c) {
                uint8_t vol = regs_[TIA_AUDV0 + c];
                if (audio[c].output && vol > 0) {
                    sample += (static_cast<float>(vol) / 15.0f);
                }
            }
            sample *= 0.5f;  // Average the two channels
            sample = std::max(-1.0f, std::min(1.0f, sample));

            audio_buffer_.write(&sample, 1);
            if (audio_port_) {
                audio_port_->drive_sample(sample);
            }
        }
    }
}

// ============================================================================
// WRITE REGISTER
// ============================================================================

void tia_t::write(uint16_t addr, uint8_t data) {
    addr &= 0x3F;  // 6 bits for write address space
    if (addr >= WRITE_REG_COUNT) return;

    switch (addr) {
        case TIA_VBLANK: {
            bool old_vblank = (regs_[TIA_VBLANK] & 0x02) != 0;
            bool new_vblank = (data & 0x02) != 0;
            // Detect VBLANK off transition mid-scanline so the rest of the
            // current scanline renders to the framebuffer immediately.
            if (old_vblank && !new_vblank && visible_row < 0) {
                visible_row = 0;
            }
            regs_[TIA_VBLANK] = data;
            // Bit 7: dump paddle capacitors (INPT0-3) — not implemented
            break;
        }

        case TIA_WSYNC:
            regs_[addr] = data;
            wsync_pending = true;
            break;

        case TIA_RSYNC:
            regs_[addr] = data;
            // Reset horizontal sync counter — rarely used
            h_counter = 0;
            break;

        // Object position resets — set position to current horizontal counter
        case TIA_RESP0:
            regs_[addr] = data;
            pos_p0 = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;
        case TIA_RESP1:
            regs_[addr] = data;
            pos_p1 = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;
        case TIA_RESM0:
            regs_[addr] = data;
            pos_m0 = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;
        case TIA_RESM1:
            regs_[addr] = data;
            pos_m1 = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;
        case TIA_RESBL:
            regs_[addr] = data;
            pos_bl = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;

        // Audio (frequency and volume need masks — consumers use raw values)
        case TIA_AUDF0:  regs_[addr] = data & 0x1F; break;
        case TIA_AUDF1:  regs_[addr] = data & 0x1F; break;
        case TIA_AUDV0:  regs_[addr] = data & 0x0F; break;
        case TIA_AUDV1:  regs_[addr] = data & 0x0F; break;

        // Graphics — latching side effects
        case TIA_GRP0:
            grp1_old = regs_[TIA_GRP1];  // Writing GRP0 latches current GRP1 into GRP1-OLD
            regs_[TIA_GRP0] = data;
            break;
        case TIA_GRP1:
            grp0_old = regs_[TIA_GRP0];  // Writing GRP1 latches current GRP0 into GRP0-OLD
            regs_[TIA_GRP1] = data;
            // Writing GRP1 also updates the old ball enable
            enabl_old = (regs_[TIA_ENABL] & 0x02) != 0;
            break;

        case TIA_HMOVE: {
            regs_[addr] = data;
            // Apply horizontal motion to all objects
            // Motion value is subtracted (reversed sign convention)
            auto apply_motion = [](uint8_t& pos, uint8_t hm_reg) {
                int8_t hm = static_cast<int8_t>(hm_reg) >> 4;
                int new_pos = static_cast<int>(pos) - hm;
                // Wrap to 0-159 range
                while (new_pos < 0) new_pos += 160;
                while (new_pos >= 160) new_pos -= 160;
                pos = static_cast<uint8_t>(new_pos);
            };
            apply_motion(pos_p0, regs_[TIA_HMP0]);
            apply_motion(pos_p1, regs_[TIA_HMP1]);
            apply_motion(pos_m0, regs_[TIA_HMM0]);
            apply_motion(pos_m1, regs_[TIA_HMM1]);
            apply_motion(pos_bl, regs_[TIA_HMBL]);

            // HMOVE blanking: if strobed during HBLANK, blank first 8 visible pixels
            if (h_counter < tia_constants::HBLANK_CLOCKS) {
                hmove_blank_active = true;
            }
            break;
        }

        case TIA_HMCLR:
            regs_[TIA_HMP0] = 0;
            regs_[TIA_HMP1] = 0;
            regs_[TIA_HMM0] = 0;
            regs_[TIA_HMM1] = 0;
            regs_[TIA_HMBL] = 0;
            break;

        case TIA_CXCLR:
            regs_[addr] = data;
            memset(cx, 0, sizeof(cx));
            break;

        // All other registers store raw value (VSYNC, REFP0, REFP1, PF0-2,
        // ENAM0, ENAM1, ENABL, HMP0-HMBL, VDELP0, VDELP1, VDELBL, RESMP0, RESMP1)
        default:
            regs_[addr] = data;
            break;
    }
}

// ============================================================================
// READ REGISTER
// ============================================================================

uint8_t tia_t::read(uint16_t addr) {
    addr &= 0x0F;  // 4 bits for read address space
    if (addr >= READ_REG_COUNT) return 0;

    // Rebuild read register from internal state
    switch (addr) {
        // Collision registers — extract from per-object cx[] accumulators.
        // PX_* bit layout: M0(0) M1(1) P0(2) P1(3) BL(4) PF(5)
        // For FB registers: PF=bit5, BL=bit4 → (cx & 0x30) << 2 maps to D7,D6
        // For CXM0P: P1=bit3, P0=bit2 → (cx & 0x0C) << 4 maps to D7,D6
        case TIA_CXM0P:
            read_regs_[addr] = (cx[CX_M0] & (PX_P1 | PX_P0)) << 4;
            break;
        case TIA_CXM1P:
            read_regs_[addr] = ((cx[CX_M1] & PX_P0) << 5) |
                              ((cx[CX_M1] & PX_P1) << 3);
            break;
        case TIA_CXP0FB:
            read_regs_[addr] = (cx[CX_P0] & (PX_PF | PX_BL)) << 2;
            break;
        case TIA_CXP1FB:
            read_regs_[addr] = (cx[CX_P1] & (PX_PF | PX_BL)) << 2;
            break;
        case TIA_CXM0FB:
            read_regs_[addr] = (cx[CX_M0] & (PX_PF | PX_BL)) << 2;
            break;
        case TIA_CXM1FB:
            read_regs_[addr] = (cx[CX_M1] & (PX_PF | PX_BL)) << 2;
            break;
        case TIA_CXBLPF:
            read_regs_[addr] = (cx[CX_BL] & PX_PF) << 2;
            break;
        case TIA_CXPPMM:
            read_regs_[addr] = ((cx[CX_P0] & PX_P1) << 4) |
                              ((cx[CX_M0] & PX_M1) << 5);
            break;
        case TIA_INPT0:
        case TIA_INPT1:
        case TIA_INPT2:
        case TIA_INPT3:
        case TIA_INPT4:
        case TIA_INPT5:
            break;  // read directly from read_regs_ (set by system)
    }
    return read_regs_[addr];
}

// ============================================================================
// ChipDebugRegistry
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void tia_t::register_debug_fields() {
    using T = const tia_t;
    auto& r = debug_registry_;
    r.set_registers(regs_, TIA_W_NUM_REGS, TIA_W_REG_INFO, 0x00);
    r.set_decl_entries(TIA_W_DECL_ENTRIES.data(), TIA_W_DECL_ENTRIES.size());

    // Write register values, control bitfields, audio, etc. are all in
    // the DECL walk above.  Categories below cover non-register state.

    // ---- Timing (internal state, not register values) ----
    r.category("Timing")
     .counter("H Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->h_counter; },
              tia_constants::CLOCKS_PER_LINE)
     .counter("Scanline", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->scanline; },
              tia_constants::LINES_PER_FRAME_NTSC)
     .value("Visible Row", +[](const ChipBase* c) -> uint32_t {
         auto row = static_cast<T*>(c)->visible_row;
         return static_cast<uint32_t>(row < 0 ? 0 : row);
     })
     .flag("WSYNC Pending", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->wsync_pending; });

    // ---- Object Positions (internal, set by RESPx strobes + HMOVE) ----
    r.category("Object Positions")
     .value("Pos P0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_p0; })
     .value("Pos P1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_p1; })
     .value("Pos M0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_m0; })
     .value("Pos M1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_m1; })
     .value("Pos Ball", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_bl; });

    // ---- Collision (internal accumulators, not register values) ----
    r.category("Collision")
     .value("CX M0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->cx[CX_M0]; })
     .value("CX M1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->cx[CX_M1]; })
     .value("CX P0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->cx[CX_P0]; })
     .value("CX P1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->cx[CX_P1]; })
     .value("CX BL", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->cx[CX_BL]; })
     .value("CX PF", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->cx[CX_PF]; });

    // ---- Read Registers (separate address space via read_regs_[]) ----
    r.category("Input Ports")
     .flag("INPT4 (Joy0 Fire)", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->read_regs_[TIA_INPT4] & 0x80) != 0; })
     .flag("INPT5 (Joy1 Fire)", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->read_regs_[TIA_INPT5] & 0x80) != 0; })
     .flag("INPT0 (Paddle 0)", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->read_regs_[TIA_INPT0] & 0x80) != 0; })
     .flag("INPT1 (Paddle 1)", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->read_regs_[TIA_INPT1] & 0x80) != 0; })
     .flag("INPT2 (Paddle 2)", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->read_regs_[TIA_INPT2] & 0x80) != 0; })
     .flag("INPT3 (Paddle 3)", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->read_regs_[TIA_INPT3] & 0x80) != 0; });
}
#endif // CERMU_HAS_CHIP_DEBUG
