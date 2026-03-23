/*
 * ted7360.cpp — TED 7360/8360 cycle-accurate implementation
 *
 * Cycle-accurate emulation of the TED video/sound/IO chip using a state-machine
 * architecture driven by x_cycle position.  Each call to ted7360_tick_phi1()
 * processes one CPU cycle (= 2 TED single-clock cycles) and outputs 8 pixels.
 *
 * Key differences from VIC-II implementation:
 *   - No sprites: no per-cycle callback table needed, state enum suffices
 *   - TED fetches character AND color data itself (no separate color RAM chip)
 *   - 128 colors (16 hues × 8 luminances) vs VIC-II's 16 fixed colors
 *   - 114 TED clocks per line (57 CPU cycles) vs 63/65 VIC-II cycles
 *   - Banking latches at $FF3E/$FF3F for ROM/RAM switching
 *   - Reverse screen mode (RVS bit in $FF07) — per-character inversion
 *   - Hardware text blink (color attribute bit 7)
 *   - Timer 1 auto-reloads; Timers 2/3 wrap to $FFFF (no auto-reload)
 *
 * Screen memory layout ($800 bytes at screen_base):
 *   screen_base + $000..$3FF = colour/attribute memory
 *     Text: bits 3-0 = hue, bits 6-4 = luminance, bit 7 = blink
 *     Bitmap: bits 4-6 = "off" luminance, bits 0-2 = "on" luminance
 *   screen_base + $400..$7FF = screen code memory
 *     Text: character indices (8-bit, or 7-bit + inversion flag if RVS)
 *     Bitmap: bits 3-0 = "off" hue, bits 7-4 = "on" hue
 *
 * Memory access model (matching VIC-II phi1/phi2):
 *   PHI1: TED reads character generator / bitmap data (g-access)
 *         TED directly reads colour/attribute data on DMA lines
 *   PHI2: CPU has bus (unless stolen by TED DMA)
 *         On DMA lines, screen codes delivered from memory system (c-access)
 */

#include "chip/video/ted/ted7360.hpp"
#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"
#include "core/signal/audio_port.hpp"
#include <algorithm>
#include <array>
#include <cstring>

using namespace ted::reg;

REGISTER_CHIP_TYPE("TED7360", ted7360_t)

// ============================================================================
// TED 7360 COLOR PALETTE — compile-time computed (16 hues × 8 luminances = 128 entries)
// ============================================================================
// Register color format: bits 6-4 = luminance (0-7), bits 3-0 = hue (0-15).
// Palette index = (luminance << 4) | hue.  Hue 0 = black at all luminances.
//
// Palette values derived from Levente Hársfalvi's TED color measurements
// and cross-referenced with VICE's ted-color.c.
//
// Stored as RGBA8888 (R=byte0, G=byte1, B=byte2, A=byte3 in little-endian):
// uint32_t = 0xAABBGGRR.

// Base hue RGB values at maximum luminance (luminance level 7)
static constexpr uint8_t TED_HUE_R[16] = {
      0, 255, 109,  41, 145,  41,  41, 178,
    145, 109, 182, 109,  78, 109, 109, 150
};
static constexpr uint8_t TED_HUE_G[16] = {
      0, 255,  41, 178,  41, 145,  41, 178,
     72,  41, 109, 109,  78, 178,  41, 150
};
static constexpr uint8_t TED_HUE_B[16] = {
      0, 255,  41,  73, 145,  41, 178,  41,
     41, 109,  41, 178,  78,  73, 178, 150
};

// Luminance scaling factors: index 0 = darkest, index 7 = full brightness.
// Index 0 is not zero — true black comes from hue 0 regardless of luminance.
static constexpr uint8_t TED_LUM_SCALE[8] = {
     0,  40,  64,  96, 128, 168, 212, 255
};

// Builds the 128-entry RGBA palette at compile time.
// Result lives in rodata — zero runtime initialization cost.
static constexpr std::array<uint32_t, 128> build_ted_palette() noexcept {
    std::array<uint32_t, 128> pal{};
    for (int lum = 0; lum < 8; ++lum) {
        for (int hue = 0; hue < 16; ++hue) {
            const int idx = (lum << 4) | hue;
            if (hue == 0) {
                // Hue 0 = black regardless of luminance
                pal[idx] = 0xFF000000u;
            } else {
                const uint32_t scale = TED_LUM_SCALE[lum];
                const uint8_t  r = static_cast<uint8_t>((TED_HUE_R[hue] * scale) / 255u);
                const uint8_t  g = static_cast<uint8_t>((TED_HUE_G[hue] * scale) / 255u);
                const uint8_t  b = static_cast<uint8_t>((TED_HUE_B[hue] * scale) / 255u);
                pal[idx] = 0xFF000000u
                         | (static_cast<uint32_t>(b) << 16)
                         | (static_cast<uint32_t>(g) <<  8)
                         |  static_cast<uint32_t>(r);
            }
        }
    }
    return pal;
}

static constexpr std::array<uint32_t, 128> TED_PALETTE = build_ted_palette();

// Convert 7-bit TED color register value to palette index.
// Bits: [6:4] = luminance, [3:0] = hue.
static inline uint8_t ted_color_index(uint8_t color_reg) noexcept {
    return color_reg & 0x7Fu;
}

// ============================================================================
// PALETTE ACCESS
// ============================================================================

[[nodiscard]] const uint32_t* ted7360_t::get_palette() {
    return TED_PALETTE.data();
}

// ============================================================================
// REGISTER HELPERS
// ============================================================================

uint8_t ted7360_t::get_graphics_mode() const {
    const uint8_t cr1 = regs_[CONTROL1];
    const uint8_t cr2 = regs_[CONTROL2];
    return ((cr1 & TED_CR1_ECM) ? 4u : 0u)
         | ((cr1 & TED_CR1_BMM) ? 2u : 0u)
         | ((cr2 & TED_CR2_MCM) ? 1u : 0u);
}

uint16_t ted7360_t::get_raster_compare() const {
    return static_cast<uint16_t>(
        ((regs_[IRQ_MASK] & 0x01u) << 8)
      |  regs_[RASTER_CMP]);
}

// ============================================================================
// MEMORY MAPPING — address calculation from registers
// ============================================================================
// $FF12 (MEM_CTRL): ROM bank selection — decoded by the system memory layer,
//                   not directly consumed by the TED emulation core.
// $FF13 (CHAR_HI):  bits 2-7 = character base address A10..A15
// $FF14 (BITMAP_ADDR): bits 3-7 = screen/bitmap base A10..A14; bit 3 = bitmap toggle

void ted7360_t::update_memory_addresses() {
    const uint8_t char_hi  = regs_[CHAR_HI];
    const uint8_t bmp_addr = regs_[BITMAP_ADDR];

    memory.screen_base  = static_cast<uint16_t>((bmp_addr & 0xF8u) << 8);
    memory.char_base    = static_cast<uint16_t>((char_hi  & 0xFCu) << 8);
    memory.bitmap_base  = (bmp_addr & 0x08u) ? 0x2000u : 0x0000u;
}

// ============================================================================
// BORDER LOGIC — update comparison limits from registers
// ============================================================================

void ted7360_t::update_border_limits() {
    const uint8_t cr1 = regs_[CONTROL1];
    const uint8_t cr2 = regs_[CONTROL2];
    // Vertical: RSEL — 25-row (1) or 24-row (0) display window
    border.top    = (cr1 & TED_CR1_RSEL) ?    4u :    8u;
    border.bottom = (cr1 & TED_CR1_RSEL) ? 0xCBu : 0xC7u;
    // Horizontal: CSEL — 40-column (1) or 38-column (0) display window
    // Pixel positions derived from VICE: TED_40COL_START_PIXEL = screen_leftborderwidth (32),
    // TED_40COL_STOP_PIXEL = screen_leftborderwidth + 320 (352).
    // TED_38COL_START_PIXEL = screen_leftborderwidth + 7 (39),
    // TED_38COL_STOP_PIXEL  = screen_leftborderwidth + 311 (343).
    border.left   = (cr2 & TED_CR2_CSEL) ?  32u :  39u;
    border.right  = (cr2 & TED_CR2_CSEL) ? 352u : 343u;
}

// ============================================================================
// DMA LINE (BAD LINE) DETECTION
// ============================================================================
// A DMA line occurs when:
//   1. Raster counter is in the display range (0 to $CB)
//   2. Lower 3 bits of raster counter match (YSCROLL + 1) & 7
//   3. DEN has been set (latched for the frame)
//
// The +1 offset matches VICE's do_matrix_fetch() condition:
//   (ted_raster_counter & 7) == ((ysmooth + 1) & 7)
// This ensures RC=0 aligns with the border opening (border.top=4 with
// KERNAL default yscroll=3 → DMA at raster & 7 == 4).
//
// Once triggered, display_state and dma_line_occurred are set for the line.

void ted7360_t::update_dma_condition() {
    const uint16_t raster = timing.raster_counter;
    const uint8_t  cr1    = regs_[CONTROL1];

    if (cr1 & TED_CR1_DEN) {
        video_logic.den_latched = true;
    }

    if (raster <= 0xCBu && video_logic.den_latched) {
        const uint8_t yscroll = cr1 & TED_CR1_YSCROLL_MASK;
        video_logic.is_dma_line = ((raster & 0x07u) == ((yscroll + 1u) & 7u));
        if (video_logic.is_dma_line) {
            // On FIRST detection of the DMA line (before g-access runs),
            // reset RC=0 and reload VC from VCBASE.  This matches VICE's
            // do_matrix_fetch() which resets ycounter and mem_counter at
            // the start of the DMA fetch, not at end-of-line.
            if (!video_logic.dma_line_occurred) {
                video_logic.rc   = 0;
                video_logic.vc   = video_logic.vcbase;
                video_logic.vmli = 0;
            }
            video_logic.display_state     = true;
            video_logic.dma_line_occurred = true;
        }
    } else {
        video_logic.is_dma_line = false;
    }
}

// ============================================================================
// RASTER IRQ — edge-triggered on line transition
// ============================================================================

void ted7360_t::check_raster_interrupt() {
    if (timing.raster_counter == timing.raster_compare) {
        irq_status |= TED_IRQ_RASTER;
    }
}

// ============================================================================
// TIMER TICK — all three timers share the same decrement/underflow pattern
// ============================================================================
// TED timers count down at the TED single-clock rate, which is 2× the CPU
// clock.  VICE models this as an internal counter of (value × 2) that
// decrements once per CPU cycle, with reads returning internal / 2.
//
// We use a simpler equivalent: a phase toggle that gates decrement to every
// other CPU cycle.  The visible counter value matches the real hardware.
//
// Timer 1 auto-reloads from its latch on underflow.
// Timers 2 and 3 do NOT auto-reload — they wrap to $FFFF and continue.

static inline void tick_one_timer(ted_timer_unit_t& t, uint32_t irq_flag,
                                   uint8_t& irq_status, bool auto_reload) noexcept {
    if (t.counter != 0) {
        --t.counter;
    } else {
        irq_status |= irq_flag;
        t.counter = auto_reload ? t.latch : TED_TIMER_WRAP_VALUE;
    }
}

void ted7360_t::tick_timers() {
    // Toggle phase — timers only decrement on one phase (every other CPU cycle).
    timer_tick_phase = !timer_tick_phase;
    if (!timer_tick_phase) return;

    tick_one_timer(timer1, TED_IRQ_TIMER1, irq_status, true);
    tick_one_timer(timer2, TED_IRQ_TIMER2, irq_status, false);
    tick_one_timer(timer3, TED_IRQ_TIMER3, irq_status, false);
}

// ============================================================================
// SOUND — TED 2-channel audio (square wave + noise)
// ============================================================================
// The TED sound unit has two voices, each with a 10-bit frequency counter:
//   Channel 1: square wave only
//   Channel 2: square wave OR noise (8-bit LFSR)
//
// Counters decrement at the TED master clock rate (2 × CPU clock).
// Each call to audio_tick() processes 2 TED clock ticks (= 1 CPU cycle).
//
// Volume table derived from SDL-YAPE measurements (Csaba Czető).
// Index: (voice1_high << 1) | voice0_high, × 16 volume levels.
// Values 0–0x4E08 (int16 range), normalized to float in audio_reset().
//
// The volume table encodes the non-linear output of the TED DAC including
// the interaction when both voices are active simultaneously (compression).

static const int16_t ted_volume_table[4 * 16] = {
    // Neither voice active (both low)
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    // Voice 0 (ch1) high only
    0x0000, 0x024a, 0x064a, 0x0a4a, 0x0e4a, 0x124a, 0x164a, 0x1a4a,
    0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a,
    // Voice 1 (ch2) high only
    0x0000, 0x024a, 0x064a, 0x0a4a, 0x0e4a, 0x124a, 0x164a, 0x1a4a,
    0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a, 0x1e4a,
    // Both voices high
    0x0000, 0x0494, 0x0cd4, 0x1596, 0x1f30, 0x29a2, 0x34ec, 0x410e,
    0x4e08, 0x4e08, 0x4e08, 0x4e08, 0x4e08, 0x4e08, 0x4e08, 0x4e08
};

static inline void ted_clock_noise_lfsr(uint8_t& sr) noexcept {
    // 8-bit Fibonacci LFSR, left-shifting, taps at bits 7,5,4,1
    // Feedback: new bit 0 = ~(bit7 ^ bit5 ^ bit4 ^ bit1)
    uint8_t feedback = ((sr >> 7) ^ (sr >> 5) ^ (sr >> 4) ^ (sr >> 1)) & 1;
    sr = static_cast<uint8_t>((sr << 1) | (feedback ^ 1));
}

void ted7360_t::audio_reset(uint32_t ted_clock_hz, uint32_t sample_rate_hz) {
    if (sample_rate_hz == 0) return;

    // Fixed-point 16.16: TED clocks per output sample
    // Note: ted_clock_hz is the TED master clock (2 × CPU clock)
    sound.cycles_per_sample_fp =
        static_cast<uint32_t>(((uint64_t)ted_clock_hz << 16) / sample_rate_hz);

    // First-order IIR filter coefficients for analog output stage:
    //   Lowpass:  ~1600 Hz (smooths square wave harmonics)
    //   Highpass: ~160 Hz  (removes DC offset)
    float dt = 1.0f / static_cast<float>(sample_rate_hz);
    sound.lowpass_alpha  = dt / (dt + 1.0e-4f);   // RC ≈ 100µs → fc ≈ 1592 Hz
    sound.highpass_alpha = dt / (dt + 1.0e-3f);    // RC ≈ 1ms   → fc ≈  159 Hz

    // Scale: max table value (both voices, vol=8) is 0x4E08 = 19976.
    // Map to roughly ±0.8 float range for comfortable headroom.
    sound.output_gain = 0.8f / static_cast<float>(ted_volume_table[3 * 16 + 8]);

    // Chip-rate IIR coefficients (for AudioPort path)
    float dt_chip = 1.0f / static_cast<float>(ted_clock_hz);
    sound.lp_alpha_chip = dt_chip / (dt_chip + 1.0e-4f);
    sound.hp_alpha_chip = dt_chip / (dt_chip + 1.0e-3f);
    sound.lp_buf_chip = 0.0f;
    sound.hp_buf_chip = 0.0f;

    // Zero runtime state (preserving decoded register values)
    sound.ch1_counter    = 0;
    sound.ch2_counter    = 0;
    sound.ch1_output     = false;
    sound.ch2_output     = false;
    sound.noise_shift_reg = 0xFF;   // VICE/YAPE: initial shift register state

    sound.sample_accum     = 0;
    sound.sample_tick_count = 0;
    sound.sample_frac      = 0;

    sound.lowpass_buf  = 0.0f;
    sound.highpass_buf = 0.0f;

    sound.audio_buffer.reset();
}

void ted7360_t::audio_tick() {
    // Skip if audio not initialized (cycles_per_sample_fp == 0)
    if (sound.cycles_per_sample_fp == 0 && !audio_port_) return;

    // Process 2 TED clock ticks per CPU cycle (TED master clock = 2 × CPU)
    for (int half = 0; half < 2; ++half) {
        // --- Channel 1: square wave ---
        if (sound.ch1_counter == 0) {
            // Reload: period = 1024 - freq_value (10-bit counter)
            uint16_t period = static_cast<uint16_t>(1024 - sound.freq1);
            sound.ch1_counter = (period > 0) ? period : 1u;
            sound.ch1_output = !sound.ch1_output;
        } else {
            --sound.ch1_counter;
        }

        // --- Channel 2: square wave or noise ---
        if (sound.ch2_counter == 0) {
            uint16_t period = static_cast<uint16_t>(1024 - sound.freq2);
            sound.ch2_counter = (period > 0) ? period : 1u;
            if (sound.noise_enabled) {
                // Clock the LFSR; output is bit 0 of shift register
                ted_clock_noise_lfsr(sound.noise_shift_reg);
            } else {
                sound.ch2_output = !sound.ch2_output;
            }
        } else {
            --sound.ch2_counter;
        }

        // --- Mix voices using volume table ---
        bool v0_high, v1_high;

        if (sound.da_mode) {
            v0_high = true;
            v1_high = true;
        } else {
            v0_high = sound.ch1_enabled && sound.ch1_output;
            if (sound.noise_enabled) {
                v1_high = sound.ch2_enabled && !(sound.noise_shift_reg & 1);
            } else {
                v1_high = sound.ch2_enabled && sound.ch2_output;
            }
        }

        uint8_t table_index = static_cast<uint8_t>(
            (v1_high ? 2u : 0u) | (v0_high ? 1u : 0u));
        uint8_t vol = sound.volume;
        if (vol > 8) vol = 8;

        uint32_t dac_value = static_cast<uint32_t>(
            ted_volume_table[table_index * 16 + vol]);

        if (audio_port_) {
            // ---- AudioPort path: per-TED-clock IIR → drive AudioPort ----
            float raw = static_cast<float>(dac_value);
            sound.lp_buf_chip += sound.lp_alpha_chip * (raw - sound.lp_buf_chip);
            float ac = sound.lp_buf_chip - sound.hp_buf_chip;
            sound.hp_buf_chip += sound.hp_alpha_chip * (sound.lp_buf_chip - sound.hp_buf_chip);
            audio_port_->drive(ac * sound.output_gain);
        } else {
            // ---- Legacy path: accumulate → downsample → IIR → float ring ----
            sound.sample_accum += dac_value;
            sound.sample_tick_count++;

            sound.sample_frac += (1u << 16);
            if (sound.sample_frac >= sound.cycles_per_sample_fp) {
                sound.sample_frac -= sound.cycles_per_sample_fp;

                float raw = 0.0f;
                if (sound.sample_tick_count > 0) {
                    raw = static_cast<float>(sound.sample_accum)
                        / static_cast<float>(sound.sample_tick_count);
                }

                sound.lowpass_buf += sound.lowpass_alpha * (raw - sound.lowpass_buf);
                float ac = sound.lowpass_buf - sound.highpass_buf;
                sound.highpass_buf += sound.highpass_alpha
                                    * (sound.lowpass_buf - sound.highpass_buf);

                float out = ac * sound.output_gain;
                if (out > 1.0f) out = 1.0f;
                if (out < -1.0f) out = -1.0f;

                sound.audio_buffer.write(&out, 1);

                sound.sample_accum = 0;
                sound.sample_tick_count = 0;
            }
        }
    }
}

uint32_t ted7360_t::audio_available() const {
    return static_cast<uint32_t>(sound.audio_buffer.available());
}

uint32_t ted7360_t::audio_read(float* dest, uint32_t max_samples) {
    if (!dest || max_samples == 0) return 0;
    return static_cast<uint32_t>(
        sound.audio_buffer.read(dest, static_cast<size_t>(max_samples)));
}

// ============================================================================
// PIXEL SEQUENCER — 8 pixels per CPU cycle
// ============================================================================
// Handles border flip-flops, XSCROLL delay, shift register, and per-mode color
// selection.  Processes exactly 8 pixels per CPU cycle.
//
// Structural notes vs VIC-II:
//   - 128-color palette (7-bit color values)
//   - Reverse mode (RVS bit inverts fg/bg per character in text mode)
//   - No sprite overlay
//
// Implementation uses a single unified pass (no per_pixel_in_border[] array).
// The idle and active display paths are separated before the pixel loop to
// eliminate a branch inside the hot loop body.
//
// Border flag combine uses bitwise | instead of logical || to avoid the
// branch implicit in short-circuit evaluation (both sides are simple bool reads
// with no side effects, so the single-OR form is always safe).

void ted7360_t::pixel_sequencer() {
    // Hoist null check: entire function is a no-op if no output buffer.
    uint8_t* const cline = color_line_;
    if (!cline) [[unlikely]] return;

    const uint16_t x_base  = timing.x_pixel;
    const uint16_t raster  = timing.raster_counter;
    const uint8_t  cr1     = regs_[CONTROL1];
    const bool     den_set = (cr1 & TED_CR1_DEN) != 0;

    // Hoist border limits to locals — avoids repeated struct member loads.
    const uint16_t bleft   = border.left;
    const uint16_t bright  = border.right;
    const uint16_t btop    = border.top;
    const uint16_t bbottom = border.bottom;

    // Border and background colors are hoisted before any writes through cline
    // (which aliases regs_[]) to prevent aliasing-induced reloads.
    const uint8_t border_color = ted_color_index(regs_[BORDER]);
    const uint8_t bg0_color    = ted_color_index(regs_[COLOR_BG0]);

    ted_sequencer_unit_t* const seq = &sequencer;

    // Per-pixel border flip-flop update — shared between both execution paths.
    // Implements the same 6-rule flip-flop model as the VIC-II.
    // Called once per pixel before any color decision is made.
    auto update_border_ff = [&](uint16_t px) noexcept {
        // Rule 1: right edge → set main border flip-flop
        if (px == bright) {
            border.main_ff = true;
        }
        if (px == bleft) {
            // Rule 4: left edge at bottom raster → set vertical border
            if (raster == bbottom) {
                border.vert_ff = true;
            }
            // Rule 5: left edge at top raster with DEN → clear vertical border
            else if (raster == btop && den_set) {
                border.vert_ff = false;
            }
            // Rule 6: left edge with vert_ff clear → open display, clear main border
            if (!border.vert_ff) {
                if (border.main_ff) {
                    // Latch XSCROLL and reset column counters at display open
                    seq->xscroll           = regs_[CONTROL2]
                                             & TED_CR2_XSCROLL_MASK;
                    seq->pixel_in_char     = 0;
                    seq->display_vmli      = 0;
                }
                border.main_ff = false;
            }
        }
    };

    // ---- Idle path: display_state is off, all non-border pixels are bg0 ----
    // Separated from the active path to remove the display_state branch from
    // the hot inner loop.

    if (!video_logic.display_state) [[unlikely]] {
        for (int pi = 0; pi < 8; ++pi) {
            const uint16_t px = x_base + static_cast<uint16_t>(pi);
            if (px >= TED_VISIBLE_WIDTH) [[unlikely]] break;
            update_border_ff(px);
            cline[px] = (border.main_ff | border.vert_ff) ? border_color : bg0_color;
        }
        return;
    }

    // ---- Active display path ----

    // Background colors 1-3 only needed in active path — avoid loading them
    // when idle (which is the case for most non-display lines).
    const uint8_t bg1_color = ted_color_index(regs_[COLOR_BG1]);
    const uint8_t bg2_color = ted_color_index(regs_[COLOR_BG2]);
    const uint8_t bg3_color = ted_color_index(regs_[COLOR_BG3]);

    for (int pi = 0; pi < 8; ++pi) {
        const uint16_t px = x_base + static_cast<uint16_t>(pi);
        // Remaining pixels are also out-of-bounds once we exceed visible width.
        if (px >= TED_VISIBLE_WIDTH) [[unlikely]] break;

        update_border_ff(px);

        // Border pixels — output border color and skip graphics pipeline.
        if (border.main_ff | border.vert_ff) {
            cline[px] = border_color;
            continue;
        }

        // XSCROLL delay: output bg0 for the scroll-offset pixels at left edge.
        if (seq->xscroll > 0) {
            --seq->xscroll;
            cline[px] = bg0_color;
            continue;
        }

        // Reload shift register at the start of each new character cell.
        if (seq->pixel_in_char == 0 && seq->display_vmli < TED_SCREEN_TEXTCOLS) {
            seq->shift_reg             = seq->char_data[seq->display_vmli];
            seq->active_display_column = seq->display_vmli;
            ++seq->display_vmli;
        }

        const uint8_t vmli        = seq->active_display_column;
        // color_line[] = attribute memory (hue + luminance + blink)
        // screen_line[] = screen code memory (character index or bitmap hue)
        const uint8_t attr        = video_data.color_line[vmli];
        const uint8_t screen_code = video_data.screen_line[vmli];

        // Default to bg0 — any mode that doesn't explicitly set color_idx falls back cleanly.
        uint8_t color_idx = bg0_color;

        switch (seq->graphics_mode) {
            case TED_GM_STANDARD_TEXT: {
                // Standard text mode:
                //   Foreground = color from attribute bits [6:0], background = BG0.
                //   RVS mode ($FF07 bit 7): screen code bit 7 selects per-character
                //     inversion (only bits [6:0] are then the character index).
                //   Blink: attribute bit 7 → character alternates with BG0 color.
                uint8_t pixel_bit = (seq->shift_reg >> 7) & 1u;

                if (reverse_mode && (screen_code >> 7)) {
                    pixel_bit ^= 1u;
                }

                const bool blink_hide = (attr & 0x80u) && !(flash_counter & TED_FLASH_PHASE_BIT);
                if (blink_hide) {
                    color_idx = bg0_color;
                } else if (pixel_bit) {
                    color_idx = ted_color_index(attr);
                } else {
                    color_idx = bg0_color;
                }
                seq->shift_reg <<= 1;
                break;
            }

            case TED_GM_MULTICOLOR_TEXT: {
                // Multicolor text mode:
                //   If attribute bit 3 set: 2 bits/pixel, double-width (MCM).
                //   Otherwise: 1 bit/pixel standard text (inversion/blink disabled).
                if (attr & 0x08u) {
                    const uint8_t pixel_bits = (seq->shift_reg >> 6) & 3u;
                    switch (pixel_bits) {
                        case 0:  color_idx = bg0_color;                        break;
                        case 1:  color_idx = bg1_color;                        break;
                        case 2:  color_idx = bg2_color;                        break;
                        default: color_idx = ted_color_index(attr & 0x77u);    break;
                        // attr & 0x77 masks out the MCM flag (bit 3) and blink (bit 7)
                    }
                    // Shift only on odd pixels — each 2-bit value spans 2 screen pixels.
                    if (seq->pixel_in_char & 1u) {
                        seq->shift_reg <<= 2;
                    }
                } else {
                    // Non-MCM character in multicolor mode — standard 1bpp.
                    const uint8_t pixel_bit = (seq->shift_reg >> 7) & 1u;
                    color_idx = pixel_bit ? ted_color_index(attr) : bg0_color;
                    seq->shift_reg <<= 1;
                }
                break;
            }

            case TED_GM_STANDARD_BITMAP: {
                // Standard (hires) bitmap mode — $800 screen area split into:
                //   color_line[] (attribute): bits [2:0] = "on" luminance,
                //                             bits [6:4] = "off" luminance
                //   screen_line[] (colour):   bits [3:0] = "off" hue,
                //                             bits [7:4] = "on" hue
                // 7-bit color = (luminance << 4) | hue
                const uint8_t pixel_bit = (seq->shift_reg >> 7) & 1u;
                if (pixel_bit) {
                    // "on" pixel: hue from upper nibble, lum from bits [2:0]
                    const uint8_t hue = (screen_code >> 4) & 0x0Fu;
                    const uint8_t lum =  attr & 0x07u;
                    color_idx = static_cast<uint8_t>((lum << 4) | hue);
                } else {
                    // "off" pixel: hue from lower nibble, lum from bits [6:4]
                    const uint8_t hue =  screen_code & 0x0Fu;
                    const uint8_t lum = (attr >> 4) & 0x07u;
                    color_idx = static_cast<uint8_t>((lum << 4) | hue);
                }
                seq->shift_reg <<= 1;
                break;
            }

            case TED_GM_MULTICOLOR_BITMAP: {
                // Multicolor bitmap: 2 bits/pixel, double-width.
                // Same hue/luminance encoding as hires bitmap:
                //   00 = BG0 register
                //   01 = "off" color (lower nibble hue, upper nibble lum)
                //   10 = "on"  color (upper nibble hue, lower nibble lum)
                //   11 = BG1 register
                const uint8_t pixel_bits = (seq->shift_reg >> 6) & 3u;
                switch (pixel_bits) {
                    case 0: color_idx = bg0_color; break;
                    case 1: {
                        const uint8_t hue =  screen_code & 0x0Fu;
                        const uint8_t lum = (attr >> 4) & 0x07u;
                        color_idx = static_cast<uint8_t>((lum << 4) | hue);
                        break;
                    }
                    case 2: {
                        const uint8_t hue = (screen_code >> 4) & 0x0Fu;
                        const uint8_t lum =  attr & 0x07u;
                        color_idx = static_cast<uint8_t>((lum << 4) | hue);
                        break;
                    }
                    default: color_idx = bg1_color; break;
                }
                if (seq->pixel_in_char & 1u) {
                    seq->shift_reg <<= 2;
                }
                break;
            }

            case TED_GM_ECM_TEXT: {
                // Extended Color Mode text:
                //   Upper 2 bits of screen code select background (BG0..BG3).
                //   Lower 6 bits = character index (char generator A9-A10 held low).
                //   Inversion and blink are disabled in ECM.
                const uint8_t pixel_bit = (seq->shift_reg >> 7) & 1u;
                if (pixel_bit) {
                    color_idx = ted_color_index(attr);
                } else {
                    const uint8_t bgs[4] = { bg0_color, bg1_color, bg2_color, bg3_color };
                    color_idx = bgs[(screen_code >> 6) & 0x03u];
                }
                seq->shift_reg <<= 1;
                break;
            }

            default:
                // Invalid modes (ECM+BMM, ECM+MCM, ECM+BMM+MCM): output black.
                color_idx = 0;
                seq->shift_reg <<= 1;
                break;
        }

        seq->pixel_in_char = (seq->pixel_in_char + 1u) & 7u;
        cline[px] = color_idx;
    }
}

// ============================================================================
// SCANLINE FLUSH — drive color index line buffer to video stream
// ============================================================================

void ted7360_t::flush_line(uint16_t raster_line) {
    if (!color_line_) return;

    // Video stream output is driven per-dot in the pixel pipeline,
    // so this function is now a no-op for scanline-level flushing.
    (void)raster_line;
}

// ============================================================================
// TIMING ADVANCE — advance x_cycle; handle end-of-line and end-of-frame
// ============================================================================

void ted7360_t::timing_advance() {
    // Common case: mid-line advance
    if (timing.x_cycle < timing.cpu_cycles_per_line - 1) {
        ++timing.x_cycle;
        timing.x_pixel = static_cast<uint16_t>(timing.x_cycle * 8);
        return;
    }

    // --- End of line ---

    timing.x_cycle = 0;
    timing.x_pixel = 0;
    video_logic.dma_line_occurred = false;

    uint16_t new_raster = timing.raster_counter + 1u;
    if (new_raster >= timing.lines_per_frame) {
        new_raster = 0;

        // --- End of frame ---
        ++timing.frame_count;
        flash_counter  = (flash_counter + 1u) & 0x1Fu; // 5-bit: bits [3:0] counter, bit 4 visibility
        cursor_visible = (flash_counter & TED_FLASH_PHASE_BIT) != 0;

        // Reset video counters
        video_logic.vcbase        = 0;
        video_logic.vc            = 0;
        video_logic.rc            = 0;
        video_logic.display_state = false;
        video_logic.den_latched   = false;

        // Reset border: start of frame is fully in border state
        border.vert_ff = true;
        border.main_ff = true;
    }

    // FrameEnd at first_visible_line so the stream frame starts at the top
    // of the visible display, not at raster 0.  This ensures the stream
    // shader sees all top-border lines before the text area.
    if (new_raster == timing.first_visible_line) {
        frame_wrapped_ = true;
    }

    timing.raster_counter = new_raster;

    // Update drive_flags_ for the new raster line:
    //   HSync active on cycle 0 only (cleared at cycle 1 in tick_phi1).
    //   VSync + Blank during vertical blanking.
    {
        const uint16_t fvl = timing.first_visible_line;
        const uint16_t lines = timing.lines_per_frame;
        const uint16_t vis_h = timing.is_pal ? TED_VISIBLE_HEIGHT_PAL : TED_VISIBLE_HEIGHT_NTSC;
        uint16_t fb_row = (new_raster + lines - fvl) % lines;
        bool in_vblank = (fb_row >= vis_h);
        drive_flags_ = VideoFlags::HSync;
        if (in_vblank)
            drive_flags_ = drive_flags_ | VideoFlags::VSync | VideoFlags::Blank;
    }

    // Raster IRQ is edge-triggered on line transition
    check_raster_interrupt();

    // Clear color index buffer for the new scanline
    if (color_line_) {
        memset(color_line_, 0, TED_VISIBLE_WIDTH);
    }
}

// ============================================================================
// CREATE / DESTROY / RESET
// ============================================================================

// Cycle position at which RC and VCBASE are updated — analogous to VIC-II cycle 58.
// On the TED's 57-cycle line (0..56), cycle 55 is the last display data cycle.
static constexpr uint8_t TED_RC_UPDATE_CYCLE = 55u;

void ted7360_t::init(const ted7360_desc_t& desc) {
    init_regs(TED_NUM_REGS);
    timing.is_pal          = desc.is_pal;
    info_                  = ChipInfo{"TED7360", "Commodore"};
    keyboard_scan          = desc.keyboard_scan;
    keyboard_user_data     = desc.keyboard_user_data;
    bus.mem_read           = desc.mem_read;
    bus.mem_read_user_data = desc.mem_read_user_data;
    banking_change             = desc.banking_change;
    banking_change_user_data   = desc.banking_change_user_data;

    if (timing.is_pal) {
        timing.lines_per_frame      = 312;
        timing.first_visible_line   = TED_FIRST_VISIBLE_LINE_PAL;
        timing.cpu_cycles_per_line  = 57;
    } else {
        timing.lines_per_frame      = 262;
        timing.first_visible_line   = TED_FIRST_VISIBLE_LINE_NTSC;
        timing.cpu_cycles_per_line  = 57;
    }

    color_line_ = new uint8_t[TED_VISIBLE_WIDTH]();

    system_palette_ = get_palette();
    palette_size_   = 128;

    reset();
#ifdef CERMU_HAS_CHIP_DEBUG
    register_debug_fields();
#endif
}

ted7360_t::~ted7360_t() {
    delete[] color_line_;
    color_line_ = nullptr;
}

void ted7360_t::reset() {
    // Preserve configuration established at construction time.
    // Using local copies rather than memset-then-restore avoids UB on structs
    // that contain function pointers (memset is undefined behavior on non-POD).
    const bool                 is_pal   = timing.is_pal;
    const uint16_t             lines    = timing.lines_per_frame;
    const uint16_t             fvl      = timing.first_visible_line;
    const uint8_t              cycles   = timing.cpu_cycles_per_line;
    const ted_keyboard_scan_fn kb       = keyboard_scan;
    void* const                kb_data  = keyboard_user_data;
    const ted_mem_read_fn      mem      = bus.mem_read;
    void* const                mem_data = bus.mem_read_user_data;
    const ted_banking_change_fn bc      = banking_change;
    void* const                bc_data  = banking_change_user_data;
    uint8_t* const             cline    = color_line_;

    // Preserve audio configuration (set by audio_reset(), survives chip reset)
    const uint32_t             snd_cps  = sound.cycles_per_sample_fp;
    const float                snd_lpa  = sound.lowpass_alpha;
    const float                snd_hpa  = sound.highpass_alpha;
    const float                snd_gain = sound.output_gain;

    // Zero all mutable state using aggregate initialization (well-defined in C++).
    regs_.clear();
    timing      = {};
    video_logic = {};
    video_data  = {};
    sequencer   = {};
    border      = {};
    memory      = {};
    // Reset sound unit — zero POD fields individually because
    // ted_sound_unit_t contains AudioRingBuffer (non-trivially copyable)
    sound.freq1 = 0; sound.freq2 = 0;
    sound.volume = 0;
    sound.ch1_enabled = false; sound.ch2_enabled = false;
    sound.noise_enabled = false; sound.da_mode = false;
    sound.ch1_counter = 0; sound.ch2_counter = 0;
    sound.ch1_output = false; sound.ch2_output = false;
    sound.noise_shift_reg = 0;
    sound.sample_accum = 0; sound.sample_tick_count = 0;
    sound.cycles_per_sample_fp = 0; sound.sample_frac = 0;
    sound.lowpass_buf = 0; sound.highpass_buf = 0;
    sound.lowpass_alpha = 0; sound.highpass_alpha = 0;
    sound.output_gain = 0;
    sound.lp_alpha_chip = 0; sound.hp_alpha_chip = 0;
    sound.lp_buf_chip = 0; sound.hp_buf_chip = 0;
    sound.audio_buffer.reset();
    bus         = {};
    // color_line_ is not zeroed — it is a direct member, not inside any unit.

    // Restore construction-time configuration
    timing.is_pal              = is_pal;
    timing.lines_per_frame     = lines;
    timing.first_visible_line  = fvl;
    timing.cpu_cycles_per_line = cycles;
    keyboard_scan              = kb;
    keyboard_user_data         = kb_data;
    bus.mem_read               = mem;
    bus.mem_read_user_data     = mem_data;
    banking_change             = bc;
    banking_change_user_data   = bc_data;
    color_line_                = cline;

    // Restore audio configuration and initial shift register
    sound.cycles_per_sample_fp = snd_cps;
    sound.lowpass_alpha        = snd_lpa;
    sound.highpass_alpha       = snd_hpa;
    sound.output_gain          = snd_gain;
    sound.noise_shift_reg      = 0xFF;

    // Default register values after reset
    regs_[CONTROL1]   = 0x00;   // Display disabled
    regs_[CONTROL2]   = timing.is_pal ? 0x00 : TED_CR2_PAL_NTSC;
    regs_[IRQ_STATUS] = 0x00;
    regs_[IRQ_MASK]   = 0x00;
    regs_[BORDER]     = 0x00;   // Black border
    regs_[COLOR_BG0]  = 0x00;   // Black background

    // Timers: reset latches and counters to $FFFF
    timer1.counter = TED_TIMER_WRAP_VALUE;
    timer1.latch   = TED_TIMER_WRAP_VALUE;
    timer2.counter = TED_TIMER_WRAP_VALUE;
    timer2.latch   = TED_TIMER_WRAP_VALUE;
    timer3.counter = TED_TIMER_WRAP_VALUE;
    timer3.latch   = TED_TIMER_WRAP_VALUE;

    // Raster state
    timing.raster_counter  = 0;
    timing.raster_compare  = 0;
    timing.x_cycle         = 0;
    timing.x_pixel         = 0;
    timing.frame_count     = 0;

    // IRQ
    irq_status = 0;
    irq_mask   = 0;

    // Memory banking: ROM enabled after reset
    rom_enabled = true;

    // Border: fully in border at reset
    border.vert_ff = true;
    border.main_ff = true;
    update_border_limits();

    // Keyboard
    keyboard_latch = 0xFF;

    // Flash / cursor / mode
    flash_counter   = 0;
    cursor_visible  = false;
    reverse_mode    = false;

    // Per-dot-clock stream state
    frame_wrapped_ = false;
    // Raster 0 is in vblank for TED — initialize drive_flags_ accordingly
    drive_flags_ = VideoFlags::HSync | VideoFlags::VSync | VideoFlags::Blank;

    // Derive memory addresses from default register values
    update_memory_addresses();

    // Clear color index line buffer
    if (color_line_) {
        memset(color_line_, 0, TED_VISIBLE_WIDTH);
    }
}

// ============================================================================
// PHI1 TICK — main TED processing (one CPU cycle)
// ============================================================================
//
// Execution order per CPU cycle (matching VIC-II model):
//   1. Determine DMA window and set BA/AEC/RDY signals
//   2. PHI1 g-access: read chargen / bitmap data
//   3. Set up c-access address for PHI2 screen code delivery
//   4. Pixel sequencer (8 pixels)
//   5. Timer countdown
//   6. IRQ line update
//   7. Timing advance (x_cycle++)
//   8. DMA condition re-evaluation
//   9. RC / VCBASE update at end of display window (cycle 55)
//  10. Drive bus address for c-access in PHI2

bus_state_t ted7360_t::tick_phi1(bus_state_t bus_state) {
    const uint8_t  x      = timing.x_cycle;
    const uint16_t raster = timing.raster_counter;

    // ===== STEP 1: DMA window and bus signal state =====
    // DMA (character + color fetch) occurs during CPU cycles 4..46 on DMA lines.
    // BA goes LOW 3 cycles before the first stolen cycle (the warning window).
    // AEC follows BA low after 3 cycles; RDY mirrors BA.
    const bool in_dma_window  = (x >= TED_FETCH_CYCLE && x <= TED_FETCH_END_CYCLE);
    const bool dma_active     = in_dma_window && video_logic.dma_line_occurred;

    // BA early-out: use subtraction form to avoid addition overflow on uint8_t.
    // Equivalent to (x + 3 >= TED_FETCH_CYCLE) but without wrapping concern.
    const bool ba_should_be_low = video_logic.dma_line_occurred
                                && (x >= TED_FETCH_CYCLE - 3u)
                                && (x <= TED_FETCH_END_CYCLE);

    bus.ba_low = ba_should_be_low;

    if (ba_should_be_low) {
        if (bus.ba_low_count < 4u) ++bus.ba_low_count;
        BUS_CLR_BIT(bus_state, BUS_BA_BIT);
        // AEC goes low after 3 cycles of BA low (CPU bus stolen at PHI2)
        if (bus.ba_low_count >= 3u) {
            BUS_CLR_BIT(bus_state, BUS_AEC_BIT);
        }
        // RDY mirrors BA — halts CPU reads while DMA is stealing cycles
        BUS_CLR_BIT(bus_state, BUS_RDY_BIT);
    } else {
        bus.ba_low_count = 0;
        BUS_SET_BIT(bus_state, BUS_BA_BIT);
        BUS_SET_BIT(bus_state, BUS_AEC_BIT);
        BUS_SET_BIT(bus_state, BUS_RDY_BIT);
    }

    // ===== STEP 2: PHI1 g-access (chargen / bitmap read) =====
    // During display state, TED reads character or bitmap data for each column.
    // g-access starts at TED_FETCH_CYCLE (cycle 4) — the same cycle at which
    // the display area begins (pixel 32 = cycle 4 × 8).  This ensures the
    // pixel_sequencer reads freshly-fetched char_data on the same cycle, not
    // stale data from the previous line.  BA goes low 3 cycles earlier for
    // bus arbitration, but PHI1 g-access does not need the stolen bus.
    const bool in_display_window = (x >= TED_FETCH_CYCLE)
                                && (x <  TED_FETCH_CYCLE + TED_SCREEN_TEXTCOLS);

    if (video_logic.display_state && in_display_window) {
        uint16_t address;
        const uint8_t vmli = video_logic.vmli;
        const uint8_t rc   = video_logic.rc;

        // On DMA lines, fetch screen code + color attribute directly here
        // (PHI1 time) rather than waiting for PHI2 c-access delivery.
        // The real TED reads both screen code and chargen in the same stolen
        // cycle.  If we relied on screen_line[] here, we'd use stale data
        // from the previous character row (PHI2 hasn't delivered yet), causing
        // RC=0 to render the wrong character.
        if (video_logic.is_dma_line && bus.mem_read && vmli < TED_SCREEN_TEXTCOLS) {
            const uint16_t vc = static_cast<uint16_t>(
                (video_logic.vcbase + vmli) & TED_VC_MASK);
            // Screen code: screen_base + $400 + VC
            video_data.screen_line[vmli] =
                bus.mem_read(bus.mem_read_user_data,
                             static_cast<uint16_t>(memory.screen_base + 0x0400u + vc));
            // Color/attribute: screen_base + VC
            video_data.color_line[vmli] =
                bus.mem_read(bus.mem_read_user_data,
                             static_cast<uint16_t>(memory.screen_base + vc));
        }

        if (sequencer.graphics_mode & 2u) {
            // Bitmap mode: bitmap_base | (VC << 3) | RC
            const uint16_t vc = static_cast<uint16_t>(
                (video_logic.vcbase + vmli) & TED_VC_MASK);
            address = static_cast<uint16_t>(memory.bitmap_base | (vc << 3) | rc);
        } else {
            // Text mode: char_base | (screen_code << 3) | RC
            uint8_t screen_code = video_data.screen_line[
                vmli < TED_SCREEN_TEXTCOLS ? vmli : 0u];

            // RVS mode: screen code bit 7 is the inversion flag, not part of index
            if (reverse_mode) {
                screen_code &= 0x7Fu;
            }
            // ECM: hold A9-A10 low (only 64 characters addressable)
            if (sequencer.graphics_mode & 4u) {
                screen_code &= 0x3Fu;
            }
            address = static_cast<uint16_t>(
                memory.char_base | (static_cast<uint16_t>(screen_code) << 3) | rc);
        }

        uint8_t gdata = 0xFF;
        if (bus.mem_read) {
            gdata = bus.mem_read(bus.mem_read_user_data, address);
        }

        // Hardware cursor: XOR the fetched pattern byte at g-access time.
        // The cursor position register holds a 10-bit index into the screen
        // matrix.  The absolute VC for this column is (vcbase + vmli); when
        // it matches and the cursor blink phase is active, invert the byte
        // before it reaches the shift register.
        if (cursor_visible) {
            const uint16_t abs_vc = (video_logic.vcbase + vmli) & TED_VC_MASK;
            if (abs_vc == get_cursor_position()) {
                gdata ^= 0xFFu;
            }
        }

        if (vmli < TED_SCREEN_TEXTCOLS) {
            sequencer.char_data[vmli] = gdata;
        }

        ++video_logic.vmli;
        video_logic.vc = static_cast<uint16_t>((video_logic.vc + 1u) & TED_VC_MASK);
    }

    // ===== STEP 3: c-access address setup for PHI2 delivery =====
    // Screen code c-access: screen_base + $400 + VC (PHI2 bus delivery).
    // Color/attribute c-access: screen_base + VC (TED reads directly at PHI1).
    //
    // Note: Uses addition (+) not bitwise OR (|) for address construction.
    // Although screen_base is always a multiple of $800 (making | equivalent here),
    // + correctly communicates offset arithmetic and requires no alignment assumption.
    uint16_t c_access_address = 0;
    bool     c_access_pending = false;

    if (dma_active && in_display_window) {
        // Use pre-increment vc: the g-access already incremented vc by 1,
        // but the c-access targets the same column that was just g-accessed.
        const uint16_t vc = (video_logic.vc - 1u) & TED_VC_MASK;

        // Screen code read via PHI2 bus: offset $400 into screen block
        c_access_address = static_cast<uint16_t>(memory.screen_base + 0x0400u + vc);
        c_access_pending = true;

        // Color/attribute read directly by TED during PHI1
        if (bus.mem_read && video_logic.vmli <= TED_SCREEN_TEXTCOLS) {
            const uint8_t vmli_for_color = video_logic.vmli - 1u;
            if (vmli_for_color < TED_SCREEN_TEXTCOLS) {
                const uint16_t color_addr = static_cast<uint16_t>(
                    memory.screen_base
                    + ((video_logic.vcbase + vmli_for_color) & TED_VC_MASK));
                video_data.color_line[vmli_for_color] =
                    bus.mem_read(bus.mem_read_user_data, color_addr);
            }
        }
    }

    // ===== STEP 4: Pixel sequencer (8 pixels) =====
    // Always run — border flip-flop state must stay consistent across all lines.
    pixel_sequencer();

    // ===== STEP 4.1: Per-dot-clock stream driving (8 pixels) =====
    // drive_flags_ is maintained at line transitions (HSync on cycle 0,
    // VSync during vblank).  Clear HSync after cycle 0 so the falling
    // edge creates the sync event.
    if (x == 1) {
        drive_flags_ = drive_flags_ & ~VideoFlags::HSync;
    }

    if (video_stream_) {
        VideoFlags flags = drive_flags_;
        if (frame_wrapped_) {
            frame_wrapped_ = false;
            flags = flags | VideoFlags::FrameEnd;
        }
        const bool is_vblank = has_flag(flags, VideoFlags::VSync);
        const uint16_t x_base = timing.x_pixel;
        for (int pi = 0; pi < 8; ++pi) {
            uint8_t color = 0;
            if (!is_vblank && color_line_) {
                const uint16_t px = x_base + static_cast<uint16_t>(pi);
                if (px < TED_VISIBLE_WIDTH)
                    color = color_line_[px];
            }
            video_stream_->drive({color, (pi == 0) ? flags : (flags & ~VideoFlags::FrameEnd)});
        }
    }

    // ===== STEP 5: Timer countdown =====
    tick_timers();

    // ===== STEP 5.1: Sound oscillators =====
    audio_tick();

    // ===== STEP 6: IRQ signaling =====
    // TED IRQ is active-LOW; assert by clearing the IRQ bit.
    // De-assertion is handled by the pull-up model (system default state).
    if (irq_status & irq_mask) {
        BUS_CLR_BIT(bus_state, BUS_IRQ_BIT);
    }

    // ===== STEP 7: Timing advance =====
    bus.pending_access  = c_access_pending ? TED_ACCESS_C : TED_ACCESS_IDLE;
    bus.pending_address = c_access_address;

    timing_advance();
    update_dma_condition();

    // ===== STEP 8: RC and VCBASE update at end of display window =====
    // Equivalent to VIC-II's cycle-58 update:
    //   - RC == 7: row complete → latch VCBASE, leave display state
    //   - RC < 7:  advance row counter
    //   - DMA line: reset RC to 0 for the new character row
    if (x == TED_RC_UPDATE_CYCLE) {
        if (video_logic.display_state) {
            if (video_logic.rc == 7u) {
                video_logic.vcbase        = video_logic.vc;
                video_logic.display_state = false;
            } else {
                video_logic.rc = (video_logic.rc + 1u) & 7u;
            }
            // Reset VC and VMLI for next line's g-access.  The VIC-II loads
            // VC from VCBASE at cycle 14 of every line; the TED must do the
            // same or g-access fails for character rows RC >= 2 (vmli stays
            // at 40+ from the previous line and the < TED_SCREEN_TEXTCOLS
            // guard prevents char_data updates).
            video_logic.vc   = video_logic.vcbase;
            video_logic.vmli = 0;
        }
        // NOTE: DMA line RC=0 reset is handled in update_dma_condition() at
        // first detection (before g-access).  No separate DMA block needed here.
    }

    // ===== STEP 9: Drive bus address for c-access PHI2 delivery =====
    if (c_access_pending) {
        BUS_SET_ADDR(bus_state, c_access_address);
        BUS_SET_BIT(bus_state, BUS_RW_BIT);  // Read mode
    }

    return bus_state;
}

// ============================================================================
// PHI2 DELIVERY — receive screen matrix data fetched by the memory system
// ============================================================================

void ted7360_t::tick_phi2(bus_state_t bus_state) {
    if (bus.pending_access == TED_ACCESS_C) {
        const uint8_t data = BUS_GET_DATA(bus_state);
        // vmli was incremented after the g-access, so the c-access slot is vmli-1.
        // Unsigned subtraction trick: if vmli == 0, (uint8_t)(0 - 1) = 0xFF >= 40,
        // which correctly skips the write without a separate zero-check branch.
        const uint8_t slot = static_cast<uint8_t>(video_logic.vmli - 1u);
        if (slot < TED_SCREEN_TEXTCOLS) {
            video_data.screen_line[slot] = data;
        }
    }

    bus.pending_access = TED_ACCESS_IDLE;

    // Re-evaluate DMA condition at PHI2 end to catch register writes by the CPU.
    update_dma_condition();

#ifdef CERMU_HAS_GUI
    bus_snapshot_ = bus_state;
#endif
}

// ============================================================================
// REGISTER READ
// ============================================================================

bus_state_t ted7360_t::registers_read(bus_state_t bus_state) {
    uint8_t reg = BUS_GET_ADDR(bus_state) & ADDR_MASK;

    // Banking latches $FF3E/$FF3F: open bus — leave data field untouched
    if (reg >= MIRROR_START) {
        if (reg == ROM_LATCH || reg == RAM_LATCH) {
            return bus_state;
        }
        reg &= UNMIRROR_MASK;
    }

    uint8_t data;
    switch (reg) {
        // Timers: reads return current counter value, not the latch
        case TIMER1_LO: data = static_cast<uint8_t>(timer1.counter);        break;
        case TIMER1_HI: data = static_cast<uint8_t>(timer1.counter >> 8);   break;
        case TIMER2_LO: data = static_cast<uint8_t>(timer2.counter);        break;
        case TIMER2_HI: data = static_cast<uint8_t>(timer2.counter >> 8);   break;
        case TIMER3_LO: data = static_cast<uint8_t>(timer3.counter);        break;
        case TIMER3_HI: data = static_cast<uint8_t>(timer3.counter >> 8);   break;

        case KEYBOARD:
            data = keyboard_scan
                ? keyboard_scan(keyboard_user_data, keyboard_latch)
                : 0xFF;  // No keyboard connected: all keys open
            break;

        case IRQ_STATUS:
            // Bit 7 = any enabled IRQ source is active
            data = irq_status | ((irq_status & irq_mask) ? TED_IRQ_ANY : 0u);
            break;

        case IRQ_MASK:
            // Bits [6:1] = IRQ enable flags; bit 0 = raster compare bit 8.
            // Bits 5, 7 are unused and read as 1 (per VICE: | 0xA0).
            data = (regs_[reg] & 0x5Fu) | 0xA0u;
            break;

        case CHARPOS_HI:
            // $FF1A read: character counter (VC) bit 8 in bit 0; bits [7:2] = 1
            data = static_cast<uint8_t>(((video_logic.vc >> 8) & 0x01u) | 0xFCu);
            break;

        case CHARPOS_LO:
            // $FF1B read: character counter (VC) low byte
            data = static_cast<uint8_t>(video_logic.vc & 0xFFu);
            break;

        case RASTER_HI:
            // $FF1C read: raster counter bit 8 in bit 0; bits [7:1] = 1
            data = static_cast<uint8_t>(((timing.raster_counter >> 8) & 0x01u) | 0xFEu);
            break;

        case RASTER_LO:
            // $FF1D read: raster counter low byte
            data = static_cast<uint8_t>(timing.raster_counter & 0xFFu);
            break;

        case HPOS: {
            // $FF1E read: horizontal position from cycle within line.
            // VICE formula: ((cycle - 16) * 4) / 2 & 0xFE, with negative wrap.
            int hpos = (static_cast<int>(timing.x_cycle) - 16) * 2;
            if (hpos < 0) hpos += timing.cpu_cycles_per_line * 2;
            data = static_cast<uint8_t>(hpos & 0xFEu);
            break;
        }

        case FLASH_RC:
            // $FF1F read: bit 7 = 1, bits [6:3] = flash counter [3:0], bits [2:0] = RC
            data = static_cast<uint8_t>(0x80u | ((flash_counter & 0x0Fu) << 3) | (video_logic.rc & 0x07u));
            break;

        default:
            data = regs_[reg];
            break;
    }

    BUS_SET_DATA(bus_state, data);
    return bus_state;
}

// ============================================================================
// REGISTER WRITE
// ============================================================================

bus_state_t ted7360_t::registers_write(bus_state_t bus_state) {
    uint8_t reg       = BUS_GET_ADDR(bus_state) & ADDR_MASK;
    const uint8_t data = BUS_GET_DATA(bus_state);

    // ROM/RAM banking latches — not mirrored, handled directly
    if (reg == ROM_LATCH) {
        if (!rom_enabled) {
            rom_enabled = true;
            if (banking_change) banking_change(banking_change_user_data, TED_BANK_ROM_LATCH);
        }
        return bus_state;
    }
    if (reg == RAM_LATCH) {
        if (rom_enabled) {
            rom_enabled = false;
            if (banking_change) banking_change(banking_change_user_data, TED_BANK_ROM_LATCH);
        }
        return bus_state;
    }

    if (reg >= MIRROR_START) {
        reg &= UNMIRROR_MASK;
    }

    switch (reg) {
        // Timer writes: low byte goes to latch only;
        //               high byte write also loads the counter immediately.
        case TIMER1_LO:
            timer1.latch = (timer1.latch & 0xFF00u) | data;
            regs_[reg] = data;
            break;
        case TIMER1_HI:
            timer1.latch   = (timer1.latch & 0x00FFu) | (static_cast<uint16_t>(data) << 8);
            timer1.counter = timer1.latch;
            regs_[reg] = data;
            break;

        case TIMER2_LO:
            timer2.latch = (timer2.latch & 0xFF00u) | data;
            regs_[reg] = data;
            break;
        case TIMER2_HI:
            timer2.latch   = (timer2.latch & 0x00FFu) | (static_cast<uint16_t>(data) << 8);
            timer2.counter = timer2.latch;
            regs_[reg] = data;
            break;

        case TIMER3_LO:
            timer3.latch = (timer3.latch & 0xFF00u) | data;
            regs_[reg] = data;
            break;
        case TIMER3_HI:
            timer3.latch   = (timer3.latch & 0x00FFu) | (static_cast<uint16_t>(data) << 8);
            timer3.counter = timer3.latch;
            regs_[reg] = data;
            break;

        case CONTROL1:
            regs_[reg] = data;
            update_border_limits();
            timing.raster_compare   = get_raster_compare();
            update_dma_condition();
            sequencer.graphics_mode = get_graphics_mode();
            break;

        case CONTROL2:
            regs_[reg] = data;
            update_border_limits();
            reverse_mode            = (data & TED_CR2_RVS) != 0;
            sequencer.graphics_mode = get_graphics_mode();
            break;

        case KEYBOARD:
            keyboard_latch      = data;
            regs_[reg] = data;
            break;

        case IRQ_STATUS:
            // Writing 1 to a bit clears that IRQ source
            irq_status &= ~(data & TED_IRQ_CLEARABLE);
            break;

        case IRQ_MASK:
            regs_[reg] = data;
            irq_mask = data & TED_IRQ_CLEARABLE;
            // Bit 0 is raster compare bit 8 — update compare value.
            timing.raster_compare = get_raster_compare();
            break;

        case RASTER_CMP:
            regs_[reg]   = data;
            timing.raster_compare = get_raster_compare();
            break;

        case CURSOR_HI:
        case CURSOR_LO:
            regs_[reg] = data;
            break;

        // ---- Sound registers ----

        case SOUND1_LO:
            // $FF0E: Channel 1 frequency low byte [7:0]
            regs_[reg] = data;
            sound.freq1 = static_cast<uint16_t>(
                data | ((regs_[MEM_CTRL] & 0x03u) << 8));
            break;

        case SOUND2_LO:
            // $FF0F: Channel 2 frequency low byte [7:0]
            regs_[reg] = data;
            sound.freq2 = static_cast<uint16_t>(
                data | ((regs_[SOUND2_HI] & 0x03u) << 8));
            break;

        case SOUND2_HI:
            // $FF10: Channel 2 frequency high bits [9:8] in data bits [1:0]
            regs_[reg] = data;
            sound.freq2 = static_cast<uint16_t>(
                regs_[SOUND2_LO] | ((data & 0x03u) << 8));
            break;

        case SOUND_CTRL: {
            // $FF11: Sound control register
            //   bits [3:0] = volume (0-8; 9-15 clamp to 8 in audio_tick)
            //   bit 4 = channel 1 enable
            //   bit 5 = channel 2 enable
            //   bit 6 = noise mode (active when bit 6 set, bit 5 clear)
            //   bit 7 = DA converter mode
            regs_[reg] = data;
            sound.volume       = data & TED_SND_VOLUME_MASK;
            sound.ch1_enabled  = (data & TED_SND_CH1_ENABLE) != 0;
            sound.ch2_enabled  = (data & TED_SND_CH2_ENABLE) != 0;
            sound.noise_enabled = ((data & (TED_SND_NOISE_ENABLE | TED_SND_CH2_ENABLE))
                                    == TED_SND_NOISE_ENABLE);
            sound.da_mode      = (data & TED_SND_DA_MODE) != 0;

            // In DA mode: reset oscillators, shift register, hold output high
            if (sound.da_mode) {
                sound.ch1_output    = true;
                sound.ch2_output    = true;
                sound.ch1_counter   = 0;
                sound.ch2_counter   = 0;
                sound.noise_shift_reg = 0xFF;
            }
            break;
        }

        case CHARPOS_HI:
            // $FF1A write: data bit 0 → VC bit 8, preserve VC low byte
            regs_[reg] = data;
            video_logic.vc = static_cast<uint16_t>(((data & 0x01u) << 8) | (video_logic.vc & 0xFFu));
            break;

        case CHARPOS_LO:
            // $FF1B write: data → VC low byte, preserve VC bit 8
            regs_[reg] = data;
            video_logic.vc = static_cast<uint16_t>((video_logic.vc & 0x100u) | data);
            break;

        case RASTER_HI:
            // $FF1C write: data bit 0 → raster counter bit 8 (force raster position)
            regs_[reg] = data;
            timing.raster_counter = static_cast<uint16_t>(
                ((data & 0x01u) << 8) | (timing.raster_counter & 0xFFu));
            break;

        case RASTER_LO:
            // $FF1D write: data → raster counter low byte (force raster position)
            regs_[reg] = data;
            timing.raster_counter = static_cast<uint16_t>(
                (timing.raster_counter & 0x100u) | data);
            break;

        case HPOS:
            // $FF1E write: horizontal counter is not writable on real hardware
            break;

        case FLASH_RC: {
            // $FF1F write: bits [6:3] → flash counter [3:0]; bits [2:0] → RC
            // When flash counter transitions away from 0x0F, visibility bit toggles.
            regs_[reg] = data;
            uint8_t new_count = (data >> 3) & 0x0Fu;
            uint8_t phase_bit = flash_counter & 0x10u;
            if ((flash_counter & 0x0Fu) == 0x0Fu && new_count != 0x0Fu) {
                phase_bit ^= 0x10u;
            }
            flash_counter  = phase_bit | new_count;
            cursor_visible = (flash_counter & TED_FLASH_PHASE_BIT) != 0;
            video_logic.rc = data & 0x07u;
            break;
        }

        case MEM_CTRL: {
            // $FF12: bits [1:0] = channel 1 frequency high bits [9:8]
            //        bits [7:2] = memory control (character/bitmap base, ROM bank)
            uint8_t old_romsel = regs_[reg] & 0x04;
            regs_[reg] = data;
            sound.freq1 = static_cast<uint16_t>(
                regs_[SOUND1_LO] | ((data & 0x03u) << 8));
            update_memory_addresses();
            if ((data & 0x04) != old_romsel) {
                if (banking_change) banking_change(banking_change_user_data, TED_BANK_VIDEO_ROMSEL);
            }
            break;
        }

        case CHAR_HI:
        case BITMAP_ADDR:
            regs_[reg] = data;
            update_memory_addresses();
            break;

        default:
            regs_[reg] = data;
            break;
    }
    return bus_state;
}

// ============================================================================
// IRQ QUERY
// ============================================================================

[[nodiscard]] bool ted7360_t::irq_pending() const {
    return (irq_status & irq_mask) != 0;
}

// ============================================================================
// Debug field registration (populates ChipDebugRegistry for the default
// two-column debug layout provided by ChipBase)
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void ted7360_t::register_debug_fields() {
    using TD = const ted7360_t;
    auto& r = debug_registry_;
    wire_debug_registers(TED_REG_INFO, 0xFF00);
    r.set_decl_entries(TED_DECL_ENTRIES.data(), TED_DECL_ENTRIES.size());
    const uint32_t* palette = get_palette();

    static constexpr const char* gfx_mode_names[] = {
        "Standard Text", "Multicolor Text", "Standard Bitmap",
        "Multicolor Bitmap", "ECM Text", "Invalid", "Invalid", "Invalid"
    };

    // Control register bitfields and sound control bits are in the DECL walk.
    // Raster timing, graphics mode, timers, internal sound state, IRQ state,
    // memory mapping, video logic, and color palette swatches remain.

    // ---- Raster Information ----
    r.category("Raster Information")
     .raster_position("Position",
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timing.raster_counter; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timing.x_cycle; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timing.lines_per_frame; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timing.cpu_cycles_per_line; })
     .value("Frame Count", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timing.frame_count; }, 32)
     .flag("PAL Mode", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timing.is_pal; })
     .flag("DMA Line", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->video_logic.is_dma_line; })
     .flag("BA Low", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->bus.ba_low; });

    // ---- Control Registers (only derived graphics mode) ----
    r.category("Control Registers")
     .state("Graphics Mode", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->sequencer.graphics_mode; },
            gfx_mode_names, 8);

    // ---- Timers ----
    r.category("Timers")
     .timer("Timer 1",
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timer1.counter; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timer1.latch; },
         +[](const ChipBase*) -> uint32_t { return 1; })
     .timer("Timer 2",
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timer2.counter; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timer2.latch; },
         +[](const ChipBase*) -> uint32_t { return 1; })
     .timer("Timer 3",
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timer3.counter; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->timer3.latch; },
         +[](const ChipBase*) -> uint32_t { return 1; });

    // ---- Sound (internal state — combined frequencies not in DECL) ----
    r.category("Sound")
     .audio_channel("Channel 1",
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->sound.ch1_enabled; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->sound.freq1; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->sound.volume; })
     .audio_channel("Channel 2",
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->sound.ch2_enabled; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->sound.freq2; },
         +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->sound.volume; })
     .value("Noise LFSR", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->sound.noise_shift_reg; }, 8)
     .value("Buffer Samples", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->audio_available(); }, 16);

    // ---- Interrupts (reads internal irq_status/irq_mask, not register mirror) ----
    r.category("Interrupts")
     .value("IRQ Status ($FF09)", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->irq_status; }, 8)
     .value("IRQ Mask ($FF0A)", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->irq_mask; }, 8)
     .flag("IRQ Pending", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->irq_pending(); })
     .indent(1)
     .flag("Raster", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->irq_status & TED_IRQ_RASTER; })
     .flag("Timer 1", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->irq_status & TED_IRQ_TIMER1; })
     .flag("Timer 2", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->irq_status & TED_IRQ_TIMER2; })
     .flag("Timer 3", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->irq_status & TED_IRQ_TIMER3; })
     .indent(0);

    // ---- Memory Mapping ----
    r.category("Memory Mapping", false)
     .address("Screen Base", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->memory.screen_base; }, 16)
     .address("Char Base", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->memory.char_base; }, 16)
     .address("Bitmap Base", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->memory.bitmap_base; }, 16)
     .flag("ROM Enabled", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->rom_enabled; });

    // ---- Video Logic ----
    r.category("Video Logic", false)
     .flag("Display State", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->video_logic.display_state; })
     .value("VC", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->video_logic.vc; }, 16)
     .value("VCBASE", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->video_logic.vcbase; }, 16)
     .value("RC", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->video_logic.rc; }, 8)
     .value("VMLI", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->video_logic.vmli; }, 8)
     .flag("Border Main FF", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->border.main_ff; })
     .flag("Border Vert FF", +[](const ChipBase* c) -> uint32_t { return static_cast<TD*>(c)->border.vert_ff; });

    // ---- Colors (palette swatches — DataKind::Color deferred) ----
    r.category("Colors", false)
     .color("BG0 ($FF15)", COLOR_BG0, palette, 128)
     .color("BG1 ($FF16)", COLOR_BG1, palette, 128)
     .color("BG2 ($FF17)", COLOR_BG2, palette, 128)
     .color("BG3 ($FF18)", COLOR_BG3, palette, 128)
     .color("Border ($FF19)", BORDER, palette, 128);
}
#endif // CERMU_HAS_CHIP_DEBUG
