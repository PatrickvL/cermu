#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/chip.h"
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

// Per-voice clock divisors (chip cycles per prescaler tick)
// The internal oscillator uses an 8-bit shift register, NOT a simple
// flip-flop toggle.  Each counter underflow rotates the shift register
// once.  The default waveform is 8 ones followed by 8 zeros (50% duty
// cycle, 16 shifts per period), which sounds identical to a square wave.
//
// BUT the enable bit (bit 7 of the voice register) actively pushes a 1
// or 0 into the shift register on each shift event.  By toggling enable
// with cycle-exact timing at maximum shift rate, software can inject
// arbitrary bit patterns into the register to produce 15+ distinct
// timbres.  This is the "viznut waveform" technique discovered by
// Ville-Matias Heikkilä (Viznut) of PWP, first used in "Robotic
// Liberation" (Assembly 2003).
//
// Divisors match VICE's chspeed model (1 << chspeed):
//   Bass=16, Alto=8, Soprano=4, Noise=2
#define VIC_BASS_DIVISOR     16
#define VIC_ALTO_DIVISOR     8
#define VIC_SOPRANO_DIVISOR  4
#define VIC_NOISE_DIVISOR    2

// Noise LFSR — Fibonacci form, 16-bit, left-shifting
// Taps at bits 3, 12, 14, 15 (matching VICE's decapped-die analysis)
// The noise voice shift register is edge-triggered: it only shifts on
// a rising edge of the LFSR output (bit 0), not on every counter tick.
#define VIC_NOISE_LFSR_INIT  0x0000

// Auxiliary color register bit masks
#define VIC_AUX_COLOR_MASK 0xF0
#define VIC_AUX_COLOR_SHIFT 4
#define VIC_AUX_VOLUME_MASK 0x0F

// Audio non-linear mix table scale (12-bit)
// Derived from VIC hardware: quadratic volume DAC + compressed voice summing
#define VIC_MIX_TABLE_MAX    4095

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
#define VIC_AUDIO_BUFFER_SIZE (1 << 11)                    // 2048 — ring buffer capacity (mono 8-bit samples)
#define VIC_AUDIO_BUFFER_MASK (VIC_AUDIO_BUFFER_SIZE - 1)   // 0x7FF — index wrap mask
#define VIC_NUM_TONE_VOICES   3
#define VIC_NUM_VOICES        4      // 3 tones + 1 noise

typedef struct {
    // Per-voice state --------------------------------------------------
    // Prescaler counters: count down chip cycles per voice-specific divisor
    uint32_t prescaler[VIC_NUM_VOICES];

    // Period counters (signed, matching VICE's ctr model)
    // On underflow (ctr <= 0) the counter reloads and the voice's
    // 8-bit shift register rotates one position.
    int16_t  counter[VIC_NUM_VOICES];

    // 8-bit shift registers — the actual waveform generators.
    // Default pattern: 0xFF→0xFE→…→0x00→0x01→…→0xFF (50% duty cycle).
    // Custom patterns ("viznut waveforms") are created by toggling the
    // enable bit with cycle-exact timing at maximum shift rate.
    uint8_t  shift_reg[VIC_NUM_VOICES];

    // Current digital output for each voice (bit 0 of shift register)
    uint8_t  output[VIC_NUM_VOICES];

    // Noise LFSR (Fibonacci, 16-bit, left-shifting)
    uint16_t noise_lfsr;
    uint8_t  noise_lfsr0_old;          // Previous bit 0 for edge detection

    // Downsampling accumulator ----------------------------------------
    // Accumulates non-linear mix table values between output-sample boundaries
    uint32_t sample_accum;           // Sum of per-cycle table lookups (unsigned)
    uint32_t sample_tick_count;      // Cycles accumulated so far

    // Fixed-point step: how many chip cycles per output sample (16.16)
    uint32_t cycles_per_sample_fp;

    // Fractional cycle accumulator for sample timing (16.16 fixed point)
    uint32_t sample_frac;

    // Analog output stage filter state (first-order IIR, per output sample)
    // Models the VIC-20 hardware RC output circuit:
    //   Lowpass:  1kΩ series + 100nF to ground  → fc ≈ 1592 Hz
    //   Highpass: 1µF coupling capacitor + 1kΩ  → fc ≈  159 Hz
    float lowpass_buf;               // Lowpass filter accumulator
    float highpass_buf;              // Highpass filter accumulator
    float lowpass_alpha;             // Lowpass coefficient  = dt / (dt + RC)
    float highpass_alpha;            // Highpass coefficient = dt / (dt + RC)
    float output_gain;               // Maps filter output to uint8 range

    // Output ring buffer (mono, unsigned 8-bit, centre = 128) ----------
    uint8_t  buffer[VIC_AUDIO_BUFFER_SIZE];
    uint32_t write_pos;              // Next write index (wraps)
    uint32_t read_pos;               // Next read  index (wraps)
} vic_audio_state_t;

// VIC chip structure (common base — inherits ChipBase for GUI integration)
typedef struct vic_base_s : public ChipBase {
    void* bus = nullptr;

    // Registers
    uint8_t registers[16] = {};

    // Timing
    uint16_t raster_counter = 0;
    uint32_t current_cycle = 0;
    uint32_t cycles_per_line = 0;
    uint32_t total_lines = 0;
    uint32_t clock_frequency = 0;

    // Video state
    uint8_t current_line[40] = {};
    uint8_t color_ram[1024] = {};
    uint32_t pixel_line_buffer[284] = {};  // Max line width for rendering
    int pixel_line_index = 0;

    // Framebuffer
    uint32_t* framebuffer = nullptr;
    int framebuffer_width = 0;
    int framebuffer_height = 0;

    // Configuration
    bool is_pal = false;
    const vic_chip_config_t* config = nullptr;

    // Memory access callbacks
    vic_mem_read_fn_t mem_read = nullptr;
    void* mem_user_data = nullptr;
    vic_mem_read_fn_t color_read = nullptr;
    void* color_user_data = nullptr;

    // Video generation state (updated every tick, not just register copies)
    bool in_display_area = false;
    uint16_t matrix_index = 0;
    uint8_t matrix_video_byte = 0;
    uint8_t matrix_color_byte = 0;
    uint8_t matrix_char_data = 0;

    // Audio generation state
    vic_audio_state_t audio = {};

    // --- Public methods ---
    virtual void reset();
    bus_state_t tick(bus_state_t bus_state);
    void set_framebuffer(uint32_t* framebuffer, int width, int height);
    void set_memory_callbacks(vic_mem_read_fn_t mem_read, void* mem_user_data,
                              vic_mem_read_fn_t color_read, void* color_user_data);
    virtual bus_state_t registers_read(bus_state_t bus_state);
    virtual bus_state_t registers_write(bus_state_t bus_state);
    static uint32_t* get_default_palette();

    // Audio API
    void audio_reset(uint32_t chip_clock_hz, uint32_t sample_rate_hz);
    void audio_tick();       // Called once per chip cycle from tick
    uint32_t audio_available() const;
    uint32_t audio_read(uint8_t* dest, uint32_t max_samples);

    // --- ChipBase interface ---
    ChipIdentity chip_identity() const override;
    bool has_debug_content()    const override;
    bool has_settings_content() const override;
    bool has_layout_content()   const override;
    void render_debug_content()    override;
    void render_settings_content() override;
    void render_layout_content()   override;

private:
    void emit_pixel(uint8_t color_index);
    void flush_pixel_line(int raster_line);
} vic_base_t;