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

#include "tia.h"
#include <cstring>
#include <algorithm>

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
// INIT / RESET
// ============================================================================

void tia_t::init() {
    reset();
#ifdef CERMU_HAS_GUI
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
}

void tia_t::reset() {
    h_counter = 0;
    scanline = 0;
    vsync_active = false;
    vblank_active = false;
    wsync_pending = false;
    hmove_blank_active = false;
    visible_row = -1;
    prev_vblank = false;

    pf0 = pf1 = pf2 = 0;
    ctrlpf = 0;
    grp0 = grp1 = 0;
    grp0_old = grp1_old = 0;
    nusiz0 = nusiz1 = 0;
    refp0 = refp1 = false;
    vdelp0 = vdelp1 = false;

    enam0 = enam1 = false;
    resmp0 = resmp1 = false;
    enabl = enabl_old = false;
    vdelbl = false;

    pos_p0 = pos_p1 = 0;
    pos_m0 = pos_m1 = 0;
    pos_bl = 0;

    hm_p0 = hm_p1 = 0;
    hm_m0 = hm_m1 = 0;
    hm_bl = 0;

    colup0 = colup1 = 0;
    colupf = colubk = 0;

    collision = 0;

    inpt0 = inpt1 = inpt2 = inpt3 = true;
    inpt4 = inpt5 = true;
    input_latch_enabled = false;

    audio[0] = {};
    audio[1] = {};

    audio_write_pos = 0;
    audio_read_pos = 0;
    audio_cycle_counter = 0;
}

// ============================================================================
// FRAMEBUFFER
// ============================================================================

void tia_t::set_framebuffer(uint32_t* buf, int w, int h) {
    framebuffer = buf;
    fb_width = w;
    fb_height = h;
}

// ============================================================================
// AUDIO
// ============================================================================

void tia_t::set_audio_sample_rate(int sample_rate_hz) {
    if (sample_rate_hz > 0) {
        audio_cycles_per_sample = tia_constants::CPU_FREQ_NTSC / static_cast<uint32_t>(sample_rate_hz);
    }
}

uint32_t tia_t::audio_available() const {
    return (audio_write_pos - audio_read_pos) & 4095;
}

uint32_t tia_t::audio_read(float* buffer, uint32_t max_samples) {
    uint32_t available = audio_available();
    uint32_t count = std::min(available, max_samples);
    for (uint32_t i = 0; i < count; ++i) {
        buffer[i] = audio_ring_buffer[audio_read_pos & 4095];
        audio_read_pos = (audio_read_pos + 1) & 4095;
    }
    return count;
}

void tia_t::tick_audio_channel(tia_audio_channel_t& ch) {
    // The TIA audio divider counts down from the frequency value
    if (ch.div_counter == 0) {
        ch.div_counter = ch.frequency;

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
        switch (ch.control & 0x0F) {
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

bool tia_t::get_playfield_pixel(int x) const {
    // Playfield is 40 pixels wide (20 pixels repeated or reflected)
    // PF0: bits 4-7 (4 pixels, displayed left to right: D4, D5, D6, D7)
    // PF1: bits 7-0 (8 pixels, displayed left to right: D7, D6, D5, D4, D3, D2, D1, D0)
    // PF2: bits 0-7 (8 pixels, displayed left to right: D0, D1, D2, D3, D4, D5, D6, D7)

    // Convert pixel position to playfield bit index (0-19 for left half)
    int pfx;
    bool reflect = (ctrlpf & 0x01) != 0;

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

    // Look up the bit
    if (pfx < 4) {
        // PF0: bits 4-7 (pfx 0 = D4, pfx 3 = D7)
        return (pf0 >> (4 + pfx)) & 1;
    } else if (pfx < 12) {
        // PF1: bits 7-0 (pfx 4 = D7, pfx 11 = D0)
        return (pf1 >> (11 - pfx)) & 1;
    } else {
        // PF2: bits 0-7 (pfx 12 = D0, pfx 19 = D7)
        return (pf2 >> (pfx - 12)) & 1;
    }
}

// ============================================================================
// PLAYER PIXEL LOOKUP
// ============================================================================

bool tia_t::get_player_pixel(int x, uint8_t grp, uint8_t pos, uint8_t nusiz, bool reflect) const {
    if (grp == 0) return false;

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
                if ((grp >> bit_index) & 1) return true;
            } else {
                if ((grp >> (7 - bit_index)) & 1) return true;
            }
        }
    }

    return false;
}

// ============================================================================
// MISSILE / BALL PIXEL LOOKUP
// ============================================================================

bool tia_t::get_missile_pixel(int x, uint8_t pos, uint8_t size_bits, bool enabled) const {
    return get_missile_pixel(x, pos, size_bits, enabled, 0);
}

bool tia_t::get_missile_pixel(int x, uint8_t pos, uint8_t size_bits, bool enabled, uint8_t nusiz) const {
    if (!enabled) return false;

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

        if (rel >= 0 && rel < width) return true;
    }

    return false;
}

// ============================================================================
// RENDER ONE PIXEL
// ============================================================================

void tia_t::render_pixel() {
    int x = h_counter - tia_constants::HBLANK_CLOCKS;
    if (x < 0 || x >= tia_constants::DISPLAY_WIDTH) return;
    if (!framebuffer) return;

    // Use visible_row (tracks only non-VBLANK lines) so the first
    // visible scanline maps to framebuffer row 0.
    int row = visible_row;
    if (row < 0 || row >= fb_height) return;

    // Determine which objects are present at this pixel
    bool pf_pixel = get_playfield_pixel(x);

    uint8_t p0_grp = vdelp0 ? grp0_old : grp0;
    uint8_t p1_grp = vdelp1 ? grp1_old : grp1;
    bool p0_pixel = get_player_pixel(x, p0_grp, pos_p0, nusiz0, refp0);
    bool p1_pixel = get_player_pixel(x, p1_grp, pos_p1, nusiz1, refp1);

    // Missile 0 — locked to player 0 if RESMP0 set
    uint8_t m0_pos = resmp0 ? pos_p0 : pos_m0;
    uint8_t m0_size = (nusiz0 >> 4) & 0x03;
    bool m0_pixel = get_missile_pixel(x, m0_pos, m0_size, enam0 && !resmp0, nusiz0);

    // Missile 1 — locked to player 1 if RESMP1 set
    uint8_t m1_pos = resmp1 ? pos_p1 : pos_m1;
    uint8_t m1_size = (nusiz1 >> 4) & 0x03;
    bool m1_pixel = get_missile_pixel(x, m1_pos, m1_size, enam1 && !resmp1, nusiz1);

    // Ball — uses simple single-position check (no copies)
    bool bl_enabled = vdelbl ? enabl_old : enabl;
    uint8_t bl_size = (ctrlpf >> 4) & 0x03;
    bool bl_pixel = get_missile_pixel(x, pos_bl, bl_size, bl_enabled);

    // Update collision register
    if (m0_pixel && p1_pixel) collision |= CX_M0P1;
    if (m0_pixel && p0_pixel) collision |= CX_M0P0;
    if (m1_pixel && p0_pixel) collision |= CX_M1P0;
    if (m1_pixel && p1_pixel) collision |= CX_M1P1;
    if (p0_pixel && pf_pixel) collision |= CX_P0PF;
    if (p0_pixel && bl_pixel) collision |= CX_P0BL;
    if (p1_pixel && pf_pixel) collision |= CX_P1PF;
    if (p1_pixel && bl_pixel) collision |= CX_P1BL;
    if (m0_pixel && pf_pixel) collision |= CX_M0PF;
    if (m0_pixel && bl_pixel) collision |= CX_M0BL;
    if (m1_pixel && pf_pixel) collision |= CX_M1PF;
    if (m1_pixel && bl_pixel) collision |= CX_M1BL;
    if (bl_pixel && pf_pixel) collision |= CX_BLPF;
    if (p0_pixel && p1_pixel) collision |= CX_P0P1;
    if (m0_pixel && m1_pixel) collision |= CX_M0M1;

    // Priority-based color selection
    uint8_t color;
    bool priority = (ctrlpf & 0x04) != 0;
    bool score_mode = (ctrlpf & 0x02) != 0;

    if (vblank_active) {
        // During VBLANK, output black
        color = 0;
    } else if (hmove_blank_active && x < 8) {
        // HMOVE blanking: first 8 pixels blanked to background after HMOVE strobe
        color = colubk;
    } else if (priority) {
        // Playfield/Ball priority over players
        if (pf_pixel || bl_pixel) {
            if (score_mode) {
                // Score mode overrides PF color even with priority flag
                color = (x < 80) ? colup0 : colup1;
            } else {
                color = colupf;
            }
        } else if (p0_pixel || m0_pixel) {
            color = colup0;
        } else if (p1_pixel || m1_pixel) {
            color = colup1;
        } else {
            color = colubk;
        }
    } else {
        // Players have priority over playfield
        if (p0_pixel || m0_pixel) {
            color = colup0;
        } else if (p1_pixel || m1_pixel) {
            color = colup1;
        } else if (pf_pixel || bl_pixel) {
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

    // Write pixel to framebuffer — color register upper 7 bits select palette entry.
    // palette_rgba_[] is pre-swizzled from ARGB to ABGR (GL_RGBA LE convention).
    framebuffer[row * fb_width + x] = palette_rgba_[(color >> 1) & 0x7F];
}

// ============================================================================
// TICK COLOR CLOCK
// ============================================================================

void tia_t::tick_color_clock() {
    // Detect VBLANK→visible transition before first pixel is rendered.
    // This ensures visible_row=0 is available for the first visible scanline.
    if (!vblank_active && visible_row < 0) {
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

        // Track visible row for framebuffer mapping.
        if (!vblank_active) {
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
    tick_audio_channel(audio[0]);
    tick_audio_channel(audio[1]);

    // Generate audio sample if enough cycles have passed
    if (audio_cycles_per_sample > 0) {
        audio_cycle_counter++;
        if (audio_cycle_counter >= audio_cycles_per_sample) {
            audio_cycle_counter -= audio_cycles_per_sample;

            // Mix both channels
            float sample = 0.0f;
            for (int c = 0; c < 2; ++c) {
                if (audio[c].output && audio[c].volume > 0) {
                    sample += (static_cast<float>(audio[c].volume) / 15.0f);
                }
            }
            sample *= 0.5f;  // Average the two channels
            sample = std::max(-1.0f, std::min(1.0f, sample));

            audio_ring_buffer[audio_write_pos & 4095] = sample;
            audio_write_pos = (audio_write_pos + 1) & 4095;
        }
    }
}

// ============================================================================
// WRITE REGISTER
// ============================================================================

void tia_t::write(uint16_t addr, uint8_t data) {
    addr &= 0x3F;  // 6 bits for write address space

    switch (addr) {
        case TIA_VSYNC:
            vsync_active = (data & 0x02) != 0;
            break;

        case TIA_VBLANK: {
            bool new_vblank = (data & 0x02) != 0;
            // Detect VBLANK off transition mid-scanline so the rest of the
            // current scanline renders to the framebuffer immediately.
            if (vblank_active && !new_vblank && visible_row < 0) {
                visible_row = 0;
            }
            vblank_active = new_vblank;
            input_latch_enabled = (data & 0x40) != 0;
            // Bit 7: dump paddle capacitors (INPT0-3) — not implemented
            break;
        }

        case TIA_WSYNC:
            wsync_pending = true;
            break;

        case TIA_RSYNC:
            // Reset horizontal sync counter — rarely used
            h_counter = 0;
            break;

        case TIA_NUSIZ0:  nusiz0 = data & 0x37; break;
        case TIA_NUSIZ1:  nusiz1 = data & 0x37; break;

        case TIA_COLUP0:  colup0 = data & 0xFE; break;
        case TIA_COLUP1:  colup1 = data & 0xFE; break;
        case TIA_COLUPF:  colupf = data & 0xFE; break;
        case TIA_COLUBK:  colubk = data & 0xFE; break;

        case TIA_CTRLPF:  ctrlpf = data & 0x37; break;
        case TIA_REFP0:   refp0 = (data & 0x08) != 0; break;
        case TIA_REFP1:   refp1 = (data & 0x08) != 0; break;

        case TIA_PF0:     pf0 = data; break;
        case TIA_PF1:     pf1 = data; break;
        case TIA_PF2:     pf2 = data; break;

        // Object position resets — set position to current horizontal counter
        case TIA_RESP0:
            pos_p0 = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;
        case TIA_RESP1:
            pos_p1 = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;
        case TIA_RESM0:
            pos_m0 = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;
        case TIA_RESM1:
            pos_m1 = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;
        case TIA_RESBL:
            pos_bl = (h_counter >= tia_constants::HBLANK_CLOCKS)
                    ? static_cast<uint8_t>(h_counter - tia_constants::HBLANK_CLOCKS)
                    : 0;
            break;

        // Audio
        case TIA_AUDC0:  audio[0].control   = data & 0x0F; break;
        case TIA_AUDC1:  audio[1].control   = data & 0x0F; break;
        case TIA_AUDF0:  audio[0].frequency  = data & 0x1F; break;
        case TIA_AUDF1:  audio[1].frequency  = data & 0x1F; break;
        case TIA_AUDV0:  audio[0].volume    = data & 0x0F; break;
        case TIA_AUDV1:  audio[1].volume    = data & 0x0F; break;

        // Graphics
        case TIA_GRP0:
            grp1_old = grp1;    // Writing GRP0 latches current GRP1 into GRP1-OLD
            grp0 = data;
            break;
        case TIA_GRP1:
            grp0_old = grp0;    // Writing GRP1 latches current GRP0 into GRP0-OLD
            grp1 = data;
            // Writing GRP1 also updates the old ball enable
            enabl_old = enabl;
            break;

        case TIA_ENAM0:  enam0 = (data & 0x02) != 0; break;
        case TIA_ENAM1:  enam1 = (data & 0x02) != 0; break;
        case TIA_ENABL:  enabl = (data & 0x02) != 0; break;

        // Horizontal motion (signed 4-bit value in upper nibble)
        case TIA_HMP0:   hm_p0 = static_cast<int8_t>(data) >> 4; break;
        case TIA_HMP1:   hm_p1 = static_cast<int8_t>(data) >> 4; break;
        case TIA_HMM0:   hm_m0 = static_cast<int8_t>(data) >> 4; break;
        case TIA_HMM1:   hm_m1 = static_cast<int8_t>(data) >> 4; break;
        case TIA_HMBL:   hm_bl = static_cast<int8_t>(data) >> 4; break;

        case TIA_VDELP0: vdelp0 = (data & 0x01) != 0; break;
        case TIA_VDELP1: vdelp1 = (data & 0x01) != 0; break;
        case TIA_VDELBL: vdelbl = (data & 0x01) != 0; break;

        case TIA_RESMP0: resmp0 = (data & 0x02) != 0; break;
        case TIA_RESMP1: resmp1 = (data & 0x02) != 0; break;

        case TIA_HMOVE: {
            // Apply horizontal motion to all objects
            // Motion value is subtracted (reversed sign convention)
            auto apply_motion = [](uint8_t& pos, int8_t hm) {
                int new_pos = static_cast<int>(pos) - hm;
                // Wrap to 0-159 range
                while (new_pos < 0) new_pos += 160;
                while (new_pos >= 160) new_pos -= 160;
                pos = static_cast<uint8_t>(new_pos);
            };
            apply_motion(pos_p0, hm_p0);
            apply_motion(pos_p1, hm_p1);
            apply_motion(pos_m0, hm_m0);
            apply_motion(pos_m1, hm_m1);
            apply_motion(pos_bl, hm_bl);

            // HMOVE blanking: if strobed during HBLANK, blank first 8 visible pixels
            if (h_counter < tia_constants::HBLANK_CLOCKS) {
                hmove_blank_active = true;
            }
            break;
        }

        case TIA_HMCLR:
            hm_p0 = hm_p1 = 0;
            hm_m0 = hm_m1 = 0;
            hm_bl = 0;
            break;

        case TIA_CXCLR:
            collision = 0;
            break;

        default:
            break;
    }
}

// ============================================================================
// READ REGISTER
// ============================================================================

uint8_t tia_t::read(uint16_t addr) {
    addr &= 0x0F;  // 4 bits for read address space

    switch (addr) {
        case TIA_CXM0P:
            return ((collision & CX_M0P1) ? 0x80 : 0) |
                   ((collision & CX_M0P0) ? 0x40 : 0);

        case TIA_CXM1P:
            return ((collision & CX_M1P0) ? 0x80 : 0) |
                   ((collision & CX_M1P1) ? 0x40 : 0);

        case TIA_CXP0FB:
            return ((collision & CX_P0PF) ? 0x80 : 0) |
                   ((collision & CX_P0BL) ? 0x40 : 0);

        case TIA_CXP1FB:
            return ((collision & CX_P1PF) ? 0x80 : 0) |
                   ((collision & CX_P1BL) ? 0x40 : 0);

        case TIA_CXM0FB:
            return ((collision & CX_M0PF) ? 0x80 : 0) |
                   ((collision & CX_M0BL) ? 0x40 : 0);

        case TIA_CXM1FB:
            return ((collision & CX_M1PF) ? 0x80 : 0) |
                   ((collision & CX_M1BL) ? 0x40 : 0);

        case TIA_CXBLPF:
            return (collision & CX_BLPF) ? 0x80 : 0;

        case TIA_CXPPMM:
            return ((collision & CX_P0P1) ? 0x80 : 0) |
                   ((collision & CX_M0M1) ? 0x40 : 0);

        case TIA_INPT0:  return inpt0 ? 0x80 : 0x00;
        case TIA_INPT1:  return inpt1 ? 0x80 : 0x00;
        case TIA_INPT2:  return inpt2 ? 0x80 : 0x00;
        case TIA_INPT3:  return inpt3 ? 0x80 : 0x00;
        case TIA_INPT4:  return inpt4 ? 0x80 : 0x00;
        case TIA_INPT5:  return inpt5 ? 0x80 : 0x00;

        default:
            return 0;
    }
}

// ============================================================================
// ChipDebugRegistry
// ============================================================================

void tia_t::register_debug_fields() {
    using T = const tia_t;
    debug_registry_
        // ---- Timing ----
        .category("Timing")
        .counter("H Counter", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->h_counter; },
                 tia_constants::CLOCKS_PER_LINE)
        .counter("Scanline", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->scanline; },
                 tia_constants::LINES_PER_FRAME_NTSC)
        .value("Visible Row", +[](const ChipBase* c) -> uint32_t {
            auto row = static_cast<T*>(c)->visible_row;
            return static_cast<uint32_t>(row < 0 ? 0 : row);
        })
        .flag("VSYNC Active", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->vsync_active; })
        .flag("VBLANK Active", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->vblank_active; })
        .flag("WSYNC Pending", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->wsync_pending; })

        // ---- Colors ----
        .category("Colors")
        .value("COLUP0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->colup0; })
        .value("COLUP1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->colup1; })
        .value("COLUPF", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->colupf; })
        .value("COLUBK", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->colubk; })

        // ---- Players ----
        .category("Players")
        .value("GRP0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->grp0; })
        .value("GRP1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->grp1; })
        .value("NUSIZ0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->nusiz0; })
        .value("NUSIZ1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->nusiz1; })
        .value("Pos P0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_p0; })
        .value("Pos P1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_p1; })
        .signed_value("HM P0", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<T*>(c)->hm_p0); })
        .signed_value("HM P1", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<T*>(c)->hm_p1); })
        .flag("Reflect P0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->refp0; })
        .flag("Reflect P1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->refp1; })

        // ---- Missiles & Ball ----
        .category("Missiles & Ball")
        .flag("Enable M0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->enam0; })
        .flag("Enable M1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->enam1; })
        .value("Pos M0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_m0; })
        .value("Pos M1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_m1; })
        .flag("Enable Ball", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->enabl; })
        .value("Pos Ball", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pos_bl; })
        .signed_value("HM M0", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<T*>(c)->hm_m0); })
        .signed_value("HM M1", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<T*>(c)->hm_m1); })
        .signed_value("HM Ball", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<T*>(c)->hm_bl); })

        // ---- Playfield ----
        .category("Playfield")
        .value("PF0", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pf0; })
        .value("PF1", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pf1; })
        .value("PF2", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->pf2; })
        .value("CTRLPF", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->ctrlpf; })
        .flag("PF Reflect", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->ctrlpf & 0x01) != 0; })
        .flag("PF Score", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->ctrlpf & 0x02) != 0; })
        .flag("PF Priority", +[](const ChipBase* c) -> uint32_t { return (static_cast<T*>(c)->ctrlpf & 0x04) != 0; })

        // ---- Collision ----
        .category("Collision")
        .value("Collision Reg", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->collision; })

        // ---- Audio ----
        .category("Audio")
        .value("Ch0 Control", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->audio[0].control; })
        .value("Ch0 Frequency", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->audio[0].frequency; })
        .value("Ch0 Volume", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->audio[0].volume; })
        .value("Ch1 Control", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->audio[1].control; })
        .value("Ch1 Frequency", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->audio[1].frequency; })
        .value("Ch1 Volume", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->audio[1].volume; })

        // ---- Input Ports ----
        .category("Input Ports")
        .flag("INPT4 (Joy0 Fire)", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->inpt4; })
        .flag("INPT5 (Joy1 Fire)", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->inpt5; })
        .flag("INPT0 (Paddle 0)", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->inpt0; })
        .flag("INPT1 (Paddle 1)", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->inpt1; })
        .flag("INPT2 (Paddle 2)", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->inpt2; })
        .flag("INPT3 (Paddle 3)", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->inpt3; })
        .flag("Input Latch", +[](const ChipBase* c) -> uint32_t { return static_cast<T*>(c)->input_latch_enabled; });
}
