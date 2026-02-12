#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/system_lines.h"

// VIC Register indices
#define VIC_REG_CONTROL1 0x00         // Interlace | ScreenOriginX
#define VIC_REG_CONTROL2 0x01         // ScreenOriginY / 2
#define VIC_REG_VIDEO_MATRIX 0x02     // BaseVideo bit 9 | NoOfColumns
#define VIC_REG_ROWS 0x03             // RasterLine bit 0 | NoOfRows | DoubleHeight
#define VIC_REG_RASTER 0x04           // RasterLine bits 8-1
#define VIC_REG_CHAR_BASE 0x05        // BaseVideo bits 13-10 | BaseChar bits 13-10
#define VIC_REG_LIGHTPEN_X 0x06
#define VIC_REG_LIGHTPEN_Y 0x07
#define VIC_REG_PADDLE_X 0x08
#define VIC_REG_PADDLE_Y 0x09
// Audio / waveform registers ($900A-$900E)
// The MOS 6560/6561 contains three square-wave tone generators and one
// white-noise generator.  Each voice register has a 7-bit frequency value
// (bits 0-6) and an enable bit (bit 7).  Output frequency formulas:
//   Bass    freq = Phi2 / (128 * (128 - N))   (lowest octave)
//   Alto    freq = Phi2 / (64  * (64  - N))   (middle octave)
//   Soprano freq = Phi2 / (32  * (32  - N))   (highest octave)
//   Noise   freq = Phi2 / (32  * (32  - N))   (LFSR clocked at same rate as soprano)
// where Phi2 = chip master clock and N = 7-bit register value.
#define VIC_REG_BASS_FREQ    0x0A     // Voice 1 – Bass square wave   (bit 7 = on, bits 0-6 = freq)
#define VIC_REG_ALTO_FREQ    0x0B     // Voice 2 – Alto square wave   (bit 7 = on, bits 0-6 = freq)
#define VIC_REG_SOPRANO_FREQ 0x0C     // Voice 3 – Soprano square wave(bit 7 = on, bits 0-6 = freq)
#define VIC_REG_NOISE_FREQ   0x0D     // Voice 4 – Noise (LFSR)       (bit 7 = on, bits 0-6 = freq)
#define VIC_REG_AUX_COLOR    0x0E     // bits 4-7 = auxiliary colour, bits 0-3 = master volume
#define VIC_REG_BACKGROUND   0x0F     // bits 4-7 = background colour, bit 3 = reverse, bits 0-2 = border colour

// Backward-compatible aliases for the old generic names
#define VIC_REG_OSC1_FREQ    VIC_REG_BASS_FREQ
#define VIC_REG_OSC2_FREQ    VIC_REG_ALTO_FREQ
#define VIC_REG_OSC3_FREQ    VIC_REG_SOPRANO_FREQ
#define VIC_REG_OSC4_FREQ    VIC_REG_NOISE_FREQ

// Control register 1 bit masks
#define VIC_C1_INTERLACE 0x80
#define VIC_C1_SCREEN_ORIGIN_X_MASK 0x7F

// Control register 2 bit masks
#define VIC_C2_SCREEN_ORIGIN_Y_MASK 0xFF

// Video matrix register bit masks
#define VIC_VM_BASE_VIDEO_BIT9 0x80
#define VIC_VM_COLUMNS_MASK 0x7F

// Rows register bit masks
#define VIC_ROWS_RASTER_BIT0 0x80
#define VIC_ROWS_ROWS_MASK 0x7E
#define VIC_ROWS_ROWS_SHIFT 1
#define VIC_ROWS_DOUBLE_HEIGHT 0x01

// Character base register bit masks
#define VIC_CB_BASE_VIDEO_MASK 0xF0
#define VIC_CB_BASE_VIDEO_SHIFT 6
#define VIC_CB_BASE_CHAR_MASK 0x0F
#define VIC_CB_BASE_CHAR_SHIFT 10

// Voice register bit masks (shared by all four sound registers $900A-$900D)
#define VIC_VOICE_ENABLE     0x80     // Bit 7: voice enable
#define VIC_VOICE_FREQ_MASK  0x7F     // Bits 0-6: frequency value

// Backward-compatible aliases
#define VIC_OSC_ENABLE       VIC_VOICE_ENABLE
#define VIC_OSC_FREQ_MASK    VIC_VOICE_FREQ_MASK

// Per-voice clock divisors (chip cycles per half-period unit)
// The counter for each voice decrements every N chip cycles.
// When the counter reaches zero it reloads from (128 - freq) and
// the voice output toggles (tone) or shifts (noise LFSR).
#define VIC_BASS_DIVISOR     128
#define VIC_ALTO_DIVISOR     64
#define VIC_SOPRANO_DIVISOR  32
#define VIC_NOISE_DIVISOR    32

// Noise LFSR polynomial (Galois form, 16-bit)
// Tap bits 0 and 3 (x^16 + x^3 + 1) match the real VIC shift register
#define VIC_NOISE_LFSR_POLY  0xD008
#define VIC_NOISE_LFSR_INIT  0x0001

// Auxiliary color register bit masks
#define VIC_AUX_COLOR_MASK 0xF0
#define VIC_AUX_COLOR_SHIFT 4
#define VIC_AUX_VOLUME_MASK 0x0F

// Background register bit masks ($900F) per MOS 6561 VIC documentation
// 900F XXXXYZZZ
// Bits 0-2 (Z): Border colour (8 colors: 0-7)
// Bit 3 (Y): Reverse field control bit
// Bits 4-7 (X): Screen/background colour (16 colors: 0-15)
#define VIC_BG_BORDER_MASK 0x07
#define VIC_BG_BORDER_SHIFT 0
#define VIC_BG_REVERSE 0x08
#define VIC_BG_BACKGROUND_MASK 0xF0
#define VIC_BG_BACKGROUND_SHIFT 4

// Color register bit masks (for color RAM reads)
#define VIC_COLOR_MULTICOLOR 0x08
#define VIC_COLOR_FOREGROUND_MASK 0x07

// VIC timing constants
#define VIC_PAL_CYCLES_PER_LINE 63
#define VIC_PAL_TOTAL_LINES 312
#define VIC_NTSC_CYCLES_PER_LINE 65
#define VIC_NTSC_TOTAL_LINES 262

// VIC color palette (16 colors)
typedef enum {
    VIC_COLOR_BLACK = 0,
    VIC_COLOR_WHITE = 1,
    VIC_COLOR_RED = 2,
    VIC_COLOR_CYAN = 3,
    VIC_COLOR_PURPLE = 4,
    VIC_COLOR_GREEN = 5,
    VIC_COLOR_BLUE = 6,
    VIC_COLOR_YELLOW = 7,
    VIC_COLOR_ORANGE = 8,
    VIC_COLOR_BROWN = 9,
    VIC_COLOR_LIGHT_RED = 10,
    VIC_COLOR_DARK_GREY = 11,
    VIC_COLOR_MEDIUM_GREEN = 12,
    VIC_COLOR_LIGHT_GREEN = 13,
    VIC_COLOR_LIGHT_BLUE = 14,
    VIC_COLOR_LIGHT_GREY = 15
} vic_color_t;

// VIC chip configuration
typedef struct {
    uint8_t cycles_per_line;
    uint16_t total_lines;
    uint32_t clock_frequency;
    const char* chip_name;
    bool is_pal;
} vic_chip_config_t;

// Memory read callback type for VIC to access system memory
typedef uint8_t (*vic_mem_read_fn_t)(void* user_data, uint16_t addr);

// ============================================================================
// Audio state for the four VIC voices
// ============================================================================
#define VIC_AUDIO_BUFFER_SIZE 2048   // Ring buffer capacity (mono 8-bit samples)
#define VIC_NUM_TONE_VOICES   3
#define VIC_NUM_VOICES        4      // 3 tones + 1 noise

typedef struct {
    // Per-voice state --------------------------------------------------
    // Prescaler counters: count down chip cycles per voice-specific divisor
    uint32_t prescaler[VIC_NUM_VOICES];

    // Period counters: count down from (128 - freq_reg); on underflow the
    // voice output toggles (tones) or the LFSR shifts (noise)
    uint16_t counter[VIC_NUM_VOICES];

    // Current digital output for each voice (0 or 1 for tones, LFSR bit 0 for noise)
    uint8_t  output[VIC_NUM_VOICES];

    // Noise LFSR (16-bit linear feedback shift register)
    uint16_t noise_lfsr;

    // Downsampling accumulator ----------------------------------------
    // Accumulates mixed sample values between output-sample boundaries
    uint32_t sample_accum;           // Sum of per-cycle mixed values
    uint32_t sample_tick_count;      // Cycles accumulated so far

    // Fixed-point step: how many chip cycles per output sample (16.16)
    uint32_t cycles_per_sample_fp;

    // Fractional cycle accumulator for sample timing (16.16 fixed point)
    uint32_t sample_frac;

    // Output ring buffer (mono, unsigned 8-bit, centre = 128) ----------
    uint8_t  buffer[VIC_AUDIO_BUFFER_SIZE];
    uint32_t write_pos;              // Next write index (wraps)
    uint32_t read_pos;               // Next read  index (wraps)
} vic_audio_state_t;

// VIC chip structure (common base)
typedef struct {
    void* desc;
    void* bus;

    // Registers
    uint8_t registers[16];

    // Timing
    uint16_t raster_counter;
    uint32_t current_cycle;
    uint32_t cycles_per_line;
    uint32_t total_lines;
    uint32_t clock_frequency;

    // Video state
    uint8_t current_line[40];
    uint8_t color_ram[1024];
    uint32_t pixel_line_buffer[284];  // Max line width for rendering
    int pixel_line_index;

    // Framebuffer
    uint32_t* framebuffer;
    int framebuffer_width;
    int framebuffer_height;

    // Configuration
    bool is_pal;
    const vic_chip_config_t* config;

    // Memory access callbacks
    vic_mem_read_fn_t mem_read;
    void* mem_user_data;
    vic_mem_read_fn_t color_read;
    void* color_user_data;

    // Video generation state (updated every tick, not just register copies)
    bool in_display_area;
    uint16_t matrix_index;
    uint8_t matrix_video_byte;
    uint8_t matrix_color_byte;
    uint8_t matrix_char_data;

    // Audio generation state
    vic_audio_state_t audio;
} vic_base_t;

// Function prototypes
void vic_system_reset(vic_base_t* vic);
bus_state_t vic_tick(void* chip, bus_state_t bus_state);
void vic_bus_attach(void* chip, void* bus);
void vic_set_framebuffer(vic_base_t* vic, uint32_t* framebuffer, int width, int height);
void vic_set_memory_callbacks(vic_base_t* vic, vic_mem_read_fn_t mem_read, void* mem_user_data,
                              vic_mem_read_fn_t color_read, void* color_user_data);
uint8_t vic_read_register(vic_base_t* vic, uint8_t reg);
void vic_write_register(vic_base_t* vic, uint8_t reg, uint8_t value);
uint32_t* vic_get_default_palette(void);

// Audio API
void vic_audio_reset(vic_base_t* vic, uint32_t chip_clock_hz, uint32_t sample_rate_hz);
void vic_audio_tick(vic_base_t* vic);       // Called once per chip cycle from vic_tick
uint32_t vic_audio_available(const vic_base_t* vic);
uint32_t vic_audio_read(vic_base_t* vic, uint8_t* dest, uint32_t max_samples);