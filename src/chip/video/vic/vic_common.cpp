#include "vic_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// VIC color palette (16 colors) - Hardware accurate VIC-20 colors
// Format: 0xAABBGGRR (ABGR byte order for little-endian OpenGL GL_RGBA texture format)
// On little-endian systems, memory layout is [R][G][B][A] which GL_RGBA reads correctly
// Based on Colodore measurements of real VIC-20 hardware
static const uint32_t vic_palette[16] = {
    0xFF000000, // 0: Black
    0xFFFFFFFF, // 1: White
    0xFF383381, // 2: Red
    0xFFC8CE75, // 3: Cyan
    0xFF973C8E, // 4: Purple/Magenta
    0xFF4DAC56, // 5: Green
    0xFF8D332B, // 6: Blue
    0xFF71F1ED, // 7: Yellow
    0xFF716CC4, // 8: Orange/Brown
    0xFFA1D4FF, // 9: Light Orange/Tan
    0xFF59679A, // 10: Light Red/Pink
    0xFFFFFFC7, // 11: Light Cyan
    0xFFFFADC9, // 12: Light Purple/Lavender
    0xFF9BE29A, // 13: Light Green
    0xFFC47378, // 14: Light Blue
    0xFFB0FFFF  // 15: Light Yellow
};

// Get default VIC palette
uint32_t* vic_get_default_palette(void) {
    return (uint32_t*)vic_palette;
}

// Set memory callbacks for VIC
void vic_set_memory_callbacks(vic_base_t* vic, vic_mem_read_fn_t mem_read, void* mem_user_data,
                              vic_mem_read_fn_t color_read, void* color_user_data) {
    if (!vic) return;
    vic->mem_read = mem_read;
    vic->mem_user_data = mem_user_data;
    vic->color_read = color_read;
    vic->color_user_data = color_user_data;
}

// Voice clock divisors (chip cycles per prescaler tick)
// These match VICE's chspeed model: 1<<4, 1<<3, 1<<2, 1<<1
static const uint32_t vic_voice_divisor[VIC_NUM_VOICES] = {
    VIC_BASS_DIVISOR,       // Voice 0: Bass    — ÷16
    VIC_ALTO_DIVISOR,       // Voice 1: Alto    — ÷8
    VIC_SOPRANO_DIVISOR,    // Voice 2: Soprano — ÷4
    VIC_NOISE_DIVISOR       // Voice 3: Noise   — ÷2
};

// System reset function
void vic_system_reset(vic_base_t* vic) {
    if (!vic) return;

    // Reset registers to default values matching C# initialization
    memset(vic->registers, 0, sizeof(vic->registers));
    vic->raster_counter = 0;
    vic->current_cycle = 0;

    // Default register values for VIC-20 PAL (hardware power-on defaults)
    // Based on VIC-I (6560/6561) hardware specifications (usual values from datasheet)
    // Standard VIC-20 screen setup: screen at CPU $1E00 = VIC $3E00
    // Default $9005 = $F0 puts screen at VIC $3C00-$3DFF, char ROM at VIC $0000
    vic->registers[VIC_REG_CONTROL1] = 12;   // $9000: CR0 usual value=12 (Horizontal centering, PAL: 12, NTSC: 5)
    vic->registers[VIC_REG_CONTROL2] = 38;   // $9001: CR1 usual value=38 (Vertical centering)
    vic->registers[VIC_REG_VIDEO_MATRIX] = 0x96;  // $9002: CR2 (bits 6-0: 22 columns, bit 7: video matrix bit 9 = 1 for $3E00)
    vic->registers[VIC_REG_ROWS] = 46;  // $9003: CR3 usual value=46 (23 rows, 8x8 chars)
    vic->registers[VIC_REG_RASTER] = 0;  // $9004: CR4 (TV raster counter, read-only)
    vic->registers[VIC_REG_CHAR_BASE] = 0xF0;  // $9005: CR5 (bits 7-4: $F for screen at VIC $3C00, bits 3-0: $0 for char ROM)
    vic->registers[VIC_REG_LIGHTPEN_X] = 0;  // $9006: CR6 usual value=0 (Light pen X)
    vic->registers[VIC_REG_LIGHTPEN_Y] = 1;  // $9007: CR7 usual value=1 (Light pen Y)
    vic->registers[VIC_REG_PADDLE_X] = 255;  // $9008: CR8 usual value=255 (Paddle 1)
    vic->registers[VIC_REG_PADDLE_Y] = 255;  // $9009: CR9 usual value=255 (Paddle 2)
    vic->registers[VIC_REG_BASS_FREQ] = 0;     // $900A: Voice 1 bass (off)
    vic->registers[VIC_REG_ALTO_FREQ] = 0;     // $900B: Voice 2 alto (off)
    vic->registers[VIC_REG_SOPRANO_FREQ] = 0;  // $900C: Voice 3 soprano (off)
    vic->registers[VIC_REG_NOISE_FREQ] = 0;    // $900D: Noise generator (off)
    vic->registers[VIC_REG_AUX_COLOR] = 0;     // $900E: Volume=0, Aux color=black
    // $900F: CRF power-on default (Border=Cyan(3), Reverse=OFF, Background=Blue(6))
    // VIC-20 powers up with blue background, KERNAL will configure as needed
    vic->registers[VIC_REG_BACKGROUND] = VIC_COLOR_CYAN | (VIC_COLOR_BLUE << VIC_BG_BACKGROUND_SHIFT);

    // Reset video generation state
    vic->in_display_area = false;
    vic->matrix_index = 0;
    vic->matrix_video_byte = 0;
    vic->matrix_color_byte = 0;
    vic->matrix_char_data = 0;
    vic->pixel_line_index = 0;

    // Reset audio state (preserves cycles_per_sample_fp set by vic_audio_reset)
    for (int i = 0; i < VIC_NUM_VOICES; i++) {
        vic->audio.prescaler[i] = vic_voice_divisor[i]; // Must match vic_audio_reset!
        vic->audio.counter[i] = 0;
        vic->audio.shift_reg[i] = 0;
        vic->audio.output[i] = 0;
    }
    vic->audio.noise_lfsr = VIC_NOISE_LFSR_INIT;
    vic->audio.noise_lfsr0_old = 0;
    vic->audio.sample_accum = 0;
    vic->audio.sample_tick_count = 0;
    vic->audio.sample_frac = 0;
    vic->audio.lowpass_buf = 0.0f;
    vic->audio.highpass_buf = 0.0f;
    vic->audio.write_pos = 0;
    vic->audio.read_pos = 0;
    memset(vic->audio.buffer, 128, sizeof(vic->audio.buffer)); // silence = centre

    // Initialize memory callbacks to NULL (system must set them)
    vic->mem_read = NULL;
    vic->mem_user_data = NULL;
    vic->color_read = NULL;
    vic->color_user_data = NULL;
}

// Bus attach function
void vic_bus_attach(void* chip, void* bus) {
    vic_base_t* vic = (vic_base_t*)chip;
    if (!vic) return;
    vic->bus = bus;
}

// Set framebuffer function
void vic_set_framebuffer(vic_base_t* vic, uint32_t* framebuffer, int width, int height) {
    if (!vic) return;
    vic->framebuffer = framebuffer;
    vic->framebuffer_width = width;
    vic->framebuffer_height = height;
}

// Register read function
uint8_t vic_read_register(vic_base_t* vic, uint8_t reg) {
    if (!vic || reg >= 16) return 0;

    // Handle special registers that require computed values from tick state
    switch (reg) {
        case VIC_REG_ROWS: // RasterLine bit 0 | NoOfVideoMatrixRows | DoubleHeight
            return ((vic->raster_counter << 7) & VIC_ROWS_RASTER_BIT0) | (vic->registers[reg] & 0x7F);
        case VIC_REG_RASTER: // RasterLine bits 8-1
            return (uint8_t)(vic->raster_counter >> 1);
        default:
            return vic->registers[reg];
    }
}

// Register write function
void vic_write_register(vic_base_t* vic, uint8_t reg, uint8_t value) {
    if (!vic || reg >= 16) return;
    vic->registers[reg] = value;
}

// Emit a single pixel to the line buffer
static inline void vic_emit_pixel(vic_base_t* vic, uint8_t color_index) {
    if (!vic) return;
    if (vic->pixel_line_index < 284) {  // Max line width
        vic->pixel_line_buffer[vic->pixel_line_index++] = vic_palette[color_index & 0x0F];
    }
}

// Flush accumulated pixel line to framebuffer
static void vic_flush_pixel_line(vic_base_t* vic, int raster_line) {
    if (!vic || !vic->framebuffer) return;
    if (raster_line < 0 || raster_line >= vic->framebuffer_height) return;

    // Copy pixel line buffer to framebuffer
    int pixels_to_copy = vic->pixel_line_index;
    if (pixels_to_copy > vic->framebuffer_width) {
        pixels_to_copy = vic->framebuffer_width;
    }

    uint32_t* dest = vic->framebuffer + (raster_line * vic->framebuffer_width);
    memcpy(dest, vic->pixel_line_buffer, pixels_to_copy * sizeof(uint32_t));
    
    // Fill remaining pixels with border color if line is shorter
    if (pixels_to_copy < vic->framebuffer_width) {
        uint8_t border_color = vic->registers[VIC_REG_BACKGROUND] & VIC_BG_BORDER_MASK;
        uint32_t border_pixel = vic_palette[border_color];
        for (int i = pixels_to_copy; i < vic->framebuffer_width; i++) {
            dest[i] = border_pixel;
        }
    }

    // Reset pixel line index for next line
    vic->pixel_line_index = 0;
}

// ============================================================================
// Audio generation
// ============================================================================

// ---------------------------------------------------------------------------
// Non-linear amplitude table: vic_mix_table[active_voices][volume]
// ---------------------------------------------------------------------------
// Models two hardware characteristics of the MOS 6560/6561 DAC:
//
//   1. Volume curve (4-bit resistor ladder)
//      Measured VIC hardware shows a roughly power-law response.
//      Approximated here as  vol_curve(v) = (v / 15)^1.8
//
//   2. Voice summation compression
//      Multiple voices sharing the output node saturate due to limited
//      supply current.  Derived from VICE's measured voltage function:
//        0 voices → 0.00   (baseline)
//        1 voice  → 0.54   (first voice takes >50% of headroom)
//        2 voices → 0.94   (second voice adds most of the rest)
//        3 voices → 0.99   (diminishing returns)
//        4 voices → 1.00   (full scale)
//
// Entry = round(voice_compress[v] × vol_curve[vol] × 4095)
// Range: 0-4095 (12-bit unsigned).  DC component is removed by the
// highpass filter in the output stage, so no explicit centering here.
// ---------------------------------------------------------------------------
static const uint16_t vic_mix_table[5][16] = {
    // 0 voices active — no signal regardless of volume
    {   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 },
    // 1 voice  (×0.54)
    {   0,   17,   59,  122,  205,  306,  425,  561,  714,  883, 1066, 1266, 1480, 1709, 1954, 2211 },
    // 2 voices (×0.94)
    {   0,   29,  102,  212,  356,  532,  738,  975, 1240, 1533, 1852, 2200, 2572, 2969, 3395, 3841 },
    // 3 voices (×0.99)
    {   0,   31,  107,  223,  375,  560,  777, 1026, 1304, 1613, 1949, 2315, 2706, 3123, 3572, 4042 },
    // 4 voices (×1.00)
    {   0,   31,  109,  226,  380,  567,  787, 1039, 1321, 1634, 1974, 2345, 2742, 3165, 3618, 4095 }
};

void vic_audio_reset(vic_base_t* vic, uint32_t chip_clock_hz, uint32_t sample_rate_hz) {
    if (!vic || sample_rate_hz == 0) return;

    // Compute fixed-point (16.16) cycles-per-sample ratio
    // This controls the downsampling from chip clock to audio output rate
    vic->audio.cycles_per_sample_fp =
        (uint32_t)(((uint64_t)chip_clock_hz << 16) / sample_rate_hz);

    // --- Compute first-order IIR filter coefficients ---
    // Models the VIC-20 output stage (see schematic in VICE vic20sound.c):
    //   Lowpass:  R=1kΩ, C=100nF → RC = 1e-4 s → f_c ≈ 1592 Hz
    //   Highpass: R=1kΩ, C=1µF   → RC = 1e-3 s → f_c ≈  159 Hz
    // alpha = dt / (dt + RC), where dt = 1 / sample_rate
    float dt = 1.0f / (float)sample_rate_hz;
    vic->audio.lowpass_alpha  = dt / (dt + 1.0e-4f);
    vic->audio.highpass_alpha = dt / (dt + 1.0e-3f);

    // Output gain: maps table-scale values through the filter into uint8 range.
    // A single voice at max volume swings ~0 to 2211 (table units).
    // After highpass DC removal, the AC component is roughly ±1100.
    // We want that to map to about ±70 in the uint8 output (128 ± 70 = 58–198),
    // leaving headroom for multi-voice peaks (which compress naturally via the table).
    vic->audio.output_gain = 70.0f / (float)(vic_mix_table[1][15] / 2);

    // Zero all runtime state
    for (int i = 0; i < VIC_NUM_VOICES; i++) {
        vic->audio.prescaler[i] = vic_voice_divisor[i];
        vic->audio.counter[i] = 0;
        vic->audio.shift_reg[i] = 0;
        vic->audio.output[i] = 0;
    }
    vic->audio.noise_lfsr = VIC_NOISE_LFSR_INIT;
    vic->audio.noise_lfsr0_old = 0;
    vic->audio.sample_accum = 0;
    vic->audio.sample_tick_count = 0;
    vic->audio.sample_frac = 0;
    vic->audio.lowpass_buf = 0.0f;
    vic->audio.highpass_buf = 0.0f;
    vic->audio.write_pos = 0;
    vic->audio.read_pos = 0;
    memset(vic->audio.buffer, 128, sizeof(vic->audio.buffer));
}

// Called once per chip cycle from vic_tick()
void vic_audio_tick(vic_base_t* vic) {
    vic_audio_state_t* a = &vic->audio;

    // --- Step each voice's prescaler; on expiry clock the voice counter ---
    for (int v = 0; v < VIC_NUM_VOICES; v++) {
        if (--a->prescaler[v] == 0) {
            a->prescaler[v] = vic_voice_divisor[v]; // reload prescaler

            uint8_t reg_val = vic->registers[VIC_REG_BASS_FREQ + v];
            uint8_t enabled = (reg_val & VIC_VOICE_ENABLE) >> 7;  // 0 or 1

            a->counter[v]--;
            if (a->counter[v] <= 0) {
                // Reload counter: period = (~reg) & 127, or 128 if zero
                // This matches VICE's formula exactly.
                int16_t period = (~reg_val) & VIC_VOICE_FREQ_MASK;
                if (period == 0) period = 128;
                a->counter[v] += period;  // += preserves phase accuracy

                if (v < VIC_NUM_TONE_VOICES) {
                    // ------ Tone voice: 8-bit shift register ------
                    // Shift left; the complement of the outgoing MSB re-enters
                    // at bit 0, gated by the enable bit.  When enabled, this
                    // naturally produces a 50% duty cycle square wave (period
                    // = 16 shifts).  When disabled, zeros are shifted in,
                    // gradually silencing the register.
                    //
                    // Custom waveforms ("viznut waveforms") are created by
                    // toggling the enable bit with cycle-exact timing while
                    // the frequency is set to maximum shift rate, injecting
                    // arbitrary bit patterns.  Once loaded, the pattern
                    // rotates indefinitely at the playback frequency.
                    uint8_t shift = a->shift_reg[v];
                    uint8_t msb = (shift >> 7) & 1;
                    shift = (shift << 1) | (((msb ^ 1)) & enabled);
                    a->shift_reg[v] = shift;
                    a->output[v] = shift & 1;
                } else {
                    // ------ Noise voice: Fibonacci LFSR + shift register ------
                    // The noise channel uses a 16-bit Fibonacci LFSR (left-
                    // shifting) with taps at bits 3, 12, 14, 15.  The shift
                    // register is only clocked on a *rising edge* of the
                    // LFSR output (bit 0 going from 0 to 1), which gives
                    // the VIC-20's characteristic noise texture.
                    //
                    // LFSR feedback (matching VICE decapped-die analysis):
                    //   gate1 = bit3 ^ bit12
                    //   gate2 = bit14 ^ bit15
                    //   gate3 = ~(gate1 ^ gate2)
                    //   gate4 = ~(gate3 & enabled)
                    //   LFSR  = (LFSR << 1) | gate4
                    uint16_t lfsr = a->noise_lfsr;
                    int bit3  = (lfsr >> 3) & 1;
                    int bit12 = (lfsr >> 12) & 1;
                    int bit14 = (lfsr >> 14) & 1;
                    int bit15 = (lfsr >> 15) & 1;
                    int gate1 = bit3 ^ bit12;
                    int gate2 = bit14 ^ bit15;
                    int gate3 = (gate1 ^ gate2) ^ 1;
                    int gate4 = (gate3 & enabled) ^ 1;
                    uint8_t lfsr0_old = a->noise_lfsr0_old;
                    a->noise_lfsr0_old = lfsr & 1;
                    a->noise_lfsr = (lfsr << 1) | gate4;

                    // Edge-triggered shift: only shift on rising edge of LFSR[0]
                    int edge_trigger = (lfsr & 1) & (!lfsr0_old);
                    if (edge_trigger) {
                        uint8_t shift = a->shift_reg[v];
                        uint8_t msb = (shift >> 7) & 1;
                        shift = (shift << 1) | (((msb ^ 1)) & enabled);
                        a->shift_reg[v] = shift;
                    }
                    a->output[v] = a->shift_reg[v] & enabled;
                }
            }
        }
    }

    // --- Accumulate non-linear mix for this cycle ---
    // Count active voices (0-4), look up the combined non-linear amplitude
    // that models the VIC's DAC compression + volume ladder in one step.
    uint8_t voices_active = a->output[0] + a->output[1] + a->output[2] + a->output[3];
    uint8_t volume = vic->registers[VIC_REG_AUX_COLOR] & VIC_AUX_VOLUME_MASK;
    a->sample_accum += vic_mix_table[voices_active][volume];
    a->sample_tick_count++;

    // --- Downsample: emit one output sample when enough cycles have elapsed ---
    a->sample_frac += (1u << 16); // one cycle in 16.16 fixed point
    if (a->sample_frac >= a->cycles_per_sample_fp) {
        a->sample_frac -= a->cycles_per_sample_fp;

        // Average the accumulated DAC values over this sample window
        float raw = 0.0f;
        if (a->sample_tick_count > 0) {
            raw = (float)a->sample_accum / (float)a->sample_tick_count;
        }

        // Lowpass filter: smooths the square-wave steps (models 1kΩ + 100nF)
        a->lowpass_buf += a->lowpass_alpha * (raw - a->lowpass_buf);

        // Highpass filter: removes DC offset (models 1µF coupling capacitor)
        // The AC component is the difference between lowpass output and the
        // slowly-tracking highpass buffer.
        float ac = a->lowpass_buf - a->highpass_buf;
        a->highpass_buf += a->highpass_alpha * (a->lowpass_buf - a->highpass_buf);

        // Scale to unsigned 8-bit centered at 128
        int32_t out = 128 + (int32_t)(ac * a->output_gain);
        if (out < 0) out = 0;
        if (out > 255) out = 255;

        // Write to ring buffer (drop sample if full)
        uint32_t next_write = (a->write_pos + 1) % VIC_AUDIO_BUFFER_SIZE;
        if (next_write != a->read_pos) {
            a->buffer[a->write_pos] = (uint8_t)out;
            a->write_pos = next_write;
        }

        a->sample_accum = 0;
        a->sample_tick_count = 0;
    }
}

uint32_t vic_audio_available(const vic_base_t* vic) {
    if (!vic) return 0;
    const vic_audio_state_t* a = &vic->audio;
    return (a->write_pos + VIC_AUDIO_BUFFER_SIZE - a->read_pos) % VIC_AUDIO_BUFFER_SIZE;
}

uint32_t vic_audio_read(vic_base_t* vic, uint8_t* dest, uint32_t max_samples) {
    if (!vic || !dest || max_samples == 0) return 0;
    vic_audio_state_t* a = &vic->audio;
    uint32_t count = 0;
    while (count < max_samples && a->read_pos != a->write_pos) {
        dest[count++] = a->buffer[a->read_pos];
        a->read_pos = (a->read_pos + 1) % VIC_AUDIO_BUFFER_SIZE;
    }
    return count;
}


// Main tick function (main video generation) - based on C# ClockCycle()
bus_state_t vic_tick(void* chip, bus_state_t bus_state) {
    vic_base_t* vic = (vic_base_t*)chip;
    if (!vic) return bus_state;

    // Advance audio oscillators / noise LFSR and downsample
    if (vic->audio.cycles_per_sample_fp != 0) {
        vic_audio_tick(vic);
    }

    const uint8_t reg_video_matrix = vic->registers[VIC_REG_VIDEO_MATRIX];
    uint16_t columns = reg_video_matrix & VIC_VM_COLUMNS_MASK;

    // Increment cycle counter
    vic->current_cycle++;
    if (vic->current_cycle >= vic->cycles_per_line) {
        vic->current_cycle = 0;
        
        // Flush the previous line to framebuffer
        vic_flush_pixel_line(vic, vic->raster_counter);
        
        // Move to next raster line
        vic->raster_counter++;
        if (vic->raster_counter >= vic->total_lines) {
            vic->raster_counter = 0;
        }

        const uint8_t reg_rows = vic->registers[VIC_REG_ROWS];
        const uint8_t char_height = (reg_rows & VIC_ROWS_DOUBLE_HEIGHT) ? 16 : 8;
        // Check if entering/leaving display area
        const uint16_t screen_origin_y = vic->registers[VIC_REG_CONTROL2] << 1;
        const uint16_t num_rows = (reg_rows & VIC_ROWS_ROWS_MASK) >> VIC_ROWS_ROWS_SHIFT;
        if (vic->raster_counter == screen_origin_y) {
            vic->in_display_area = true;
            vic->matrix_index = 0;
        }
        else if (vic->raster_counter == screen_origin_y + num_rows * char_height) {
            vic->in_display_area = false;
        }
        // Reset matrix_index at the start of EVERY scanline to the appropriate character row
        // Character height is 8 (normal) or 16 (double-height, bit 0 of $9003).
        if (vic->in_display_area) {
            uint16_t scanline_in_display = vic->raster_counter - screen_origin_y;
            uint16_t char_row = scanline_in_display / char_height;
            vic->matrix_index = char_row * columns;
        }
    }

    // Derive character area status from current cycle position
    const uint16_t screen_origin_x = vic->registers[VIC_REG_CONTROL1] & VIC_C1_SCREEN_ORIGIN_X_MASK;
    const uint16_t char_area_end = screen_origin_x + (columns << 1);
    const bool in_char_area = (vic->current_cycle >= screen_origin_x) && (vic->current_cycle < char_area_end);

    const uint8_t reg_background = vic->registers[VIC_REG_BACKGROUND];
    const uint8_t border_color = reg_background & VIC_BG_BORDER_MASK;
    
    // Emit 4 pixels per cycle

    if (vic->in_display_area && in_char_area) {
        // Extract frequently accessed register values
        const uint8_t reg_char_base = vic->registers[VIC_REG_CHAR_BASE];
        
        // Extract base addresses and colors (used for pixel rendering)
        const uint16_t base_video = ((reg_video_matrix & VIC_VM_BASE_VIDEO_BIT9) << 2) |
                                   ((reg_char_base & VIC_CB_BASE_VIDEO_MASK) << VIC_CB_BASE_VIDEO_SHIFT);
        const uint16_t base_char = (reg_char_base & VIC_CB_BASE_CHAR_MASK) << VIC_CB_BASE_CHAR_SHIFT;
        // Derive is_char_fetch_cycle from cycle position: even cycles relative to screen_origin_x are fetch cycles
        const bool is_char_fetch_cycle = ((vic->current_cycle - screen_origin_x) & 1) == 0;
        
        // Double-height: bit 0 of $9003 selects 8 or 16 pixel tall characters.
        // VICE formula: addr = base_char + (code * char_height + (ycounter & ((char_height >> 1) | 7)))
        const uint8_t reg_rows = vic->registers[VIC_REG_ROWS];
        const uint8_t char_height = (reg_rows & VIC_ROWS_DOUBLE_HEIGHT) ? 16 : 8;

        if (is_char_fetch_cycle) {
            // Fetch character data from memory
            if (vic->mem_read && vic->color_read) {
                uint16_t char_index = vic->matrix_index;
                uint16_t screen_addr = base_video + char_index;
                vic->matrix_video_byte = vic->mem_read(vic->mem_user_data, screen_addr);
                uint16_t color_offset = (base_video & 0x3FF) + char_index;
                vic->matrix_color_byte = vic->color_read(vic->color_user_data, color_offset);
                
                // Character line within the cell, relative to screen origin.
                // For 8px: (ycounter & 7)  → rows 0-7
                // For 16px: (ycounter & 15) → rows 0-15
                const uint16_t screen_origin_y = vic->registers[VIC_REG_CONTROL2] << 1;
                const uint8_t char_line = (vic->raster_counter - screen_origin_y) & ((char_height >> 1) | 7);
                
                // Character ROM/RAM address (matches VICE):
                //   base_char + (char_code * char_height + char_line)
                uint16_t char_rom_addr = base_char + ((uint16_t)vic->matrix_video_byte * char_height + char_line);
                vic->matrix_char_data = vic->mem_read(vic->mem_user_data, char_rom_addr);
            } else {
                // No memory access available - emit blank
                vic->matrix_char_data = 0;
                vic->matrix_color_byte = 0;
            }
            
            vic->matrix_index++;
        }
        else {
            // Each second character cycle, emit the lower nyble
            vic->matrix_char_data <<= 4;
        }

        const uint8_t background_color = (reg_background & VIC_BG_BACKGROUND_MASK) >> VIC_BG_BACKGROUND_SHIFT;
        const uint8_t foreground_color = vic->matrix_color_byte & VIC_COLOR_FOREGROUND_MASK;

        // Emit high nyble (4 pixels)
        if (vic->matrix_color_byte & VIC_COLOR_MULTICOLOR) {  // Multicolor mode
            const uint8_t auxiliary_color = (vic->registers[VIC_REG_AUX_COLOR] & VIC_AUX_COLOR_MASK) >> VIC_AUX_COLOR_SHIFT;
            // Emit 2 pixels for bits 7-6
            uint8_t color = 0;
            switch ((vic->matrix_char_data >> 6) & 3) {
                case 0b00: color = background_color; break;
                case 0b01: color = border_color; break;
                case 0b10: color = foreground_color; break;
                case 0b11: color = auxiliary_color; break;
            }
            vic_emit_pixel(vic, color);
            vic_emit_pixel(vic, color);

            // Emit 2 pixels for bits 5-4
            switch ((vic->matrix_char_data >> 4) & 3) {
                case 0b00: color = background_color; break;
                case 0b01: color = border_color; break;
                case 0b10: color = foreground_color; break;
                case 0b11: color = auxiliary_color; break;
            }
            vic_emit_pixel(vic, color);
            vic_emit_pixel(vic, color);
        }
        else {  // Hires mode
            // Reverse mode: bit 3 of $900F controls screen inversion
            // When reverse=1 (normal): set pixels use foreground, clear pixels use background
            // When reverse=0 (inverted): set pixels use background, clear pixels use foreground
            const bool reversed = (reg_background & VIC_BG_REVERSE) == 0;  // Note: 0 means reversed!
            const uint8_t fg = reversed ? background_color : foreground_color;
            const uint8_t bg = reversed ? foreground_color : background_color;
            vic_emit_pixel(vic, (vic->matrix_char_data & 0x80) ? fg : bg);
            vic_emit_pixel(vic, (vic->matrix_char_data & 0x40) ? fg : bg);
            vic_emit_pixel(vic, (vic->matrix_char_data & 0x20) ? fg : bg);
            vic_emit_pixel(vic, (vic->matrix_char_data & 0x10) ? fg : bg);
        }
    }
    else {
        // Emit 4 border pixels
        vic_emit_pixel(vic, border_color);
        vic_emit_pixel(vic, border_color);
        vic_emit_pixel(vic, border_color);
        vic_emit_pixel(vic, border_color);
    }

    return bus_state;
}
