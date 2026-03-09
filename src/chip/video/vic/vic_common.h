#pragma once

#include <cstdint>

#include "../video_chip_base.h"
#include "../../core/system_lines.h"
#include "../video_pixel_unit.h"

// ============================================================================
// VIC 6560/6561 REGISTER TABLE — single source of truth
// ============================================================================

// DECL(REG, FLD, CMP) — 16 registers, 21 fields
#define VIC_DECL(REG, FLD, CMP) \
    REG(0x00, CONTROL1,     "Horiz origin/interlace")                              \
      FLD(CONTROL1, INTERLACE,   7:7, "Interlace",           Flag,  0, 0)          \
    REG(0x01, CONTROL2,     "Vert origin")                                         \
    REG(0x02, VIDEO_MATRIX, "Video base/columns")                                  \
      FLD(VIDEO_MATRIX, VID_B9,  7:7, "Video base bit 9",    Flag,  0, 0)          \
      FLD(VIDEO_MATRIX, COLUMNS, 6:0, "Display columns",     Value, 0, 0)          \
    REG(0x03, ROWS,         "Raster b0/rows/2x-H")                                \
      FLD(ROWS, RASTER_B0,      7:7, "Raster counter bit 0", Flag,  0, 0)          \
      FLD(ROWS, ROW_COUNT,      6:1, "Display rows",         Value, 0, 0)          \
      FLD(ROWS, DBL_H,          0:0, "Double height chars",  Flag,  0, 0)          \
    REG(0x04, RASTER,       "Raster counter hi")                                   \
    REG(0x05, CHAR_BASE,    "Video/char base addr")                                \
      FLD(CHAR_BASE, VID_BASE,  7:4, "Video base address",   Value, 0, 0)          \
      FLD(CHAR_BASE, CHR_BASE,  3:0, "Char base address",    Value, 0, 0)          \
    REG(0x06, LIGHTPEN_X,   "Light pen X")                                         \
    REG(0x07, LIGHTPEN_Y,   "Light pen Y")                                         \
    REG(0x08, PADDLE_X,     "Paddle X")                                            \
    REG(0x09, PADDLE_Y,     "Paddle Y")                                            \
    REG(0x0A, BASS_FREQ,    "Voice 1 bass freq")                                   \
      FLD(BASS_FREQ, BASS_EN,   7:7, "Bass enable",          Flag,  0, 0)          \
      FLD(BASS_FREQ, BASS_F,    6:0, "Bass frequency",       Value, 0, 0)          \
    REG(0x0B, ALTO_FREQ,    "Voice 2 alto freq")                                   \
      FLD(ALTO_FREQ, ALTO_EN,   7:7, "Alto enable",          Flag,  0, 0)          \
      FLD(ALTO_FREQ, ALTO_F,    6:0, "Alto frequency",       Value, 0, 0)          \
    REG(0x0C, SOPRANO_FREQ, "Voice 3 soprano freq")                                \
      FLD(SOPRANO_FREQ, SOP_EN, 7:7, "Soprano enable",       Flag,  0, 0)          \
      FLD(SOPRANO_FREQ, SOP_F,  6:0, "Soprano frequency",    Value, 0, 0)          \
    REG(0x0D, NOISE_FREQ,   "Voice 4 noise freq")                                  \
      FLD(NOISE_FREQ, NOISE_EN, 7:7, "Noise enable",         Flag,  0, 0)          \
      FLD(NOISE_FREQ, NOISE_F,  6:0, "Noise frequency",      Value, 0, 0)          \
    REG(0x0E, AUX_COLOR,    "Aux color / volume")                                  \
      FLD(AUX_COLOR, AUX_COL,   7:4, "Auxiliary color",      Value, 0, 0)          \
      FLD(AUX_COLOR, VOLUME,    3:0, "Volume",               Value, 0, 0)          \
    REG(0x0F, BACKGROUND,   "BG/reverse/border")                                   \
      FLD(BACKGROUND, BG_COL,   7:4, "Background color",     Value, 0, 0)          \
      FLD(BACKGROUND, REVERSE,  3:3, "Reverse screen",       Flag,  0, 0)          \
      FLD(BACKGROUND, BORDER,   2:0, "Border color",         Value, 0, 0)

// Backward compat: old REG_TABLE is just the REG rows from the DECL
#define VIC_REG_TABLE(X) VIC_DECL(X, DECL_FLD_NOP, DECL_CMP_NOP)

// Audio / waveform registers ($900A-$900E)
// The MOS 6560/6561 contains three square-wave tone generators and one
// white-noise generator.  Each voice register has a 7-bit frequency value
// (bits 0-6) and an enable bit (bit 7).  Output frequency formulas:
//   Bass    freq = Phi2 / (128 * (128 - N))   (lowest octave)
//   Alto    freq = Phi2 / (64  * (64  - N))   (middle octave)
//   Soprano freq = Phi2 / (32  * (32  - N))   (highest octave)
//   Noise   freq = Phi2 / (32  * (32  - N))   (LFSR clocked at same rate as soprano)
// where Phi2 = chip master clock and N = 7-bit register value.

// --- Extract address constants (prefix VIC_REG_ added by macro) ---
#define VIC_X_CONST_(a, s, l) static constexpr uint8_t VIC_REG_##s = a;
VIC_REG_TABLE(VIC_X_CONST_)
#undef VIC_X_CONST_

// --- Extract register info array ---
#define VIC_X_INFO_(a, s, l) { #s, l },
static constexpr RegEntry VIC_REG_INFO[] = { VIC_REG_TABLE(VIC_X_INFO_) };
#undef VIC_X_INFO_

static constexpr uint8_t VIC_NUM_REGS = 16;

// --- FieldEntry ---
#define VIC_X_FLD_INFO_(reg, fld, hilo, desc, kind, ds, dm) \
    { #fld, desc, VIC_REG_##reg, BF_LO(hilo), BF_WIDTH(hilo), DataKind::kind, (uint8_t)(ds), (uint16_t)(dm) },
static constexpr FieldEntry VIC_FLD_INFO[] = {
    VIC_DECL(DECL_REG_NOP, VIC_X_FLD_INFO_, DECL_CMP_NOP)
};
#undef VIC_X_FLD_INFO_
static constexpr size_t VIC_NUM_FIELDS = sizeof(VIC_FLD_INFO) / sizeof(VIC_FLD_INFO[0]);

// --- DeclOrder ---
#define VIC_X_ORD_REG_(a, s, l)                                              { DeclRowType::Reg, (uint16_t)(a) },
#define VIC_X_ORD_FLD_(r, f, hilo, d, k, ds, dm)                            { DeclRowType::Field, 0 },
#define VIC_X_ORD_CMP_(s, d, k, b, ds, dm, r1, h1, d1, r2, h2, d2)         { DeclRowType::Compound, 0 },
static constexpr DeclOrderEntry VIC_DECL_ORDER_RAW[] = {
    VIC_DECL(VIC_X_ORD_REG_, VIC_X_ORD_FLD_, VIC_X_ORD_CMP_)
};
#undef VIC_X_ORD_REG_
#undef VIC_X_ORD_FLD_
#undef VIC_X_ORD_CMP_
static constexpr auto VIC_DECL_ORDER = assign_decl_indices(VIC_DECL_ORDER_RAW);

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


// Noise LFSR — Fibonacci form, 16-bit, left-shifting
// Taps at bits 3, 12, 14, 15 (matching VICE's decapped-die analysis)
// The noise voice shift register is edge-triggered: it only shifts on
// a rising edge of the LFSR output (bit 0), not on every counter tick.
#define VIC_NOISE_LFSR_INIT  0x0000

// Auxiliary color register bit masks
#define VIC_AUX_COLOR_MASK 0xF0
#define VIC_AUX_COLOR_SHIFT 4
#define VIC_AUX_VOLUME_MASK 0x0F

// ============================================================================
// VIC HARDWARE CONSTANTS
// ============================================================================

// Audio defaults
#define VIC_DEFAULT_SAMPLE_RATE  22050     // Default output sample rate (Hz)
#define VIC_AUDIO_SILENCE        128       // DC center for unsigned 8-bit audio

// Memory sizes
#define VIC_MAX_LINE_WIDTH       284       // Maximum visible pixels per raster line

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
enum vic_color_t {
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
};

// VIC chip configuration
struct vic_chip_config_t {
    uint8_t cycles_per_line;
    uint16_t total_lines;
    uint32_t clock_frequency;
    const char* chip_name;
    bool is_pal;
};

// Memory read callback type for VIC to access system memory
typedef uint8_t (*vic_mem_read_fn_t)(void* user_data, uint16_t addr);

// ============================================================================
// Audio state for the four VIC voices
// ============================================================================
#define VIC_AUDIO_BUFFER_SIZE (1 << 11)                    // 2048 — ring buffer capacity (mono 8-bit samples)
#define VIC_AUDIO_BUFFER_MASK (VIC_AUDIO_BUFFER_SIZE - 1)   // 0x7FF — index wrap mask
#define VIC_NUM_VOICES        4      // 3 tones + 1 noise

struct vic_audio_state_t {
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
};

// VIC chip structure (common base — inherits ChipBase for GUI integration)
struct vic_base_t : public VideoChipBase {
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
    uint8_t color_line_buffer[VIC_MAX_LINE_WIDTH] = {};  // Per-pixel palette index buffer
    int pixel_line_index = 0;

    // Pixel output unit (framebuffer + shared flush)
    VideoPixelUnit pixel;

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

    // Cached register-derived values (decoded on register write, read in tick)
    uint16_t cached_columns = 0;           // VIC_REG_VIDEO_MATRIX & VIC_VM_COLUMNS_MASK
    uint8_t  cached_border_color = 0;      // VIC_REG_BACKGROUND & VIC_BG_BORDER_MASK
    uint8_t  cached_background_color = 0;  // (VIC_REG_BACKGROUND >> 4) & 0x0F
    bool     cached_reversed = true;       // !(VIC_REG_BACKGROUND & VIC_BG_REVERSE)  (0 = reversed)
    uint8_t  cached_char_height = 8;       // 8 or 16 from VIC_REG_ROWS bit 0
    uint8_t  cached_volume = 0;            // VIC_REG_AUX_COLOR & VIC_AUX_VOLUME_MASK
    uint8_t  cached_auxiliary_color = 0;   // (VIC_REG_AUX_COLOR >> 4) & 0x0F
    uint16_t cached_screen_origin_x = 0;   // VIC_REG_CONTROL1 & VIC_C1_SCREEN_ORIGIN_X_MASK
    uint16_t cached_screen_origin_y = 0;   // VIC_REG_CONTROL2 << 1
    uint16_t cached_num_rows = 0;          // (VIC_REG_ROWS >> 1) & 0x3F
    uint16_t cached_base_video = 0;        // Combined from REG_VIDEO_MATRIX + REG_CHAR_BASE
    uint16_t cached_base_char = 0;         // (REG_CHAR_BASE & 0x0F) << 10

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
    void decode_register(uint8_t reg_index);   // Update cached fields from register value
    void decode_all_registers();               // Decode all registers (after reset/bulk load)
    static uint32_t* get_default_palette();

    // Audio API
    void audio_reset(uint32_t chip_clock_hz, uint32_t sample_rate_hz);
    void audio_tick();       // Called once per chip cycle from tick
    uint32_t audio_available() const;
    uint32_t audio_read(uint8_t* dest, uint32_t max_samples);

    // --- ChipBase interface ---
#ifdef CERMU_HAS_GUI
    bool has_settings_content() const override;
    void render_settings_content() override;
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
    const char* get_layout_chip_name() const override;
#endif

protected:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif

private:
    void emit_pixel(uint8_t color_index);
    void flush_pixel_line(int raster_line);
};