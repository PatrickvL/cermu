#pragma once

#include <cstdint>

#include "chip/video/vic/vic_traits.hpp"
#include "chip/video/video_chip_base.hpp"
#include "core/signal/composite_video_out.hpp"
#include "core/system_lines.hpp"

struct AudioPort;

// ============================================================================
// VIC 6560/6561 REGISTER TABLE — single source of truth
// ============================================================================

// DECL(REG, FLD, CMP) — 16 registers, 21 fields
#define VIC_DECL(REG, FLD, CMP) \
    REG(0x00, CONTROL1,     "Horiz origin/interlace")                      \
      FLD(CONTROL1, INTERLACE,   7:7, "Interlace",           Flag,  0, 0)  \
    REG(0x01, CONTROL2,     "Vert origin")                                 \
    REG(0x02, VIDEO_MATRIX, "Video base/columns")                          \
      FLD(VIDEO_MATRIX, VID_B9,  7:7, "Video base bit 9",    Flag,  0, 0)  \
      FLD(VIDEO_MATRIX, COLUMNS, 6:0, "Display columns",     Value, 0, 0)  \
    REG(0x03, ROWS,         "Raster b0/rows/2x-H")                        \
      FLD(ROWS, RASTER_B0,      7:7, "Raster counter bit 0", Flag,  0, 0)  \
      FLD(ROWS, ROW_COUNT,      6:1, "Display rows",         Value, 0, 0)  \
      FLD(ROWS, DBL_H,          0:0, "Double height chars",  Flag,  0, 0)  \
    REG(0x04, RASTER,       "Raster counter hi")                           \
    REG(0x05, CHAR_BASE,    "Video/char base addr")                        \
      FLD(CHAR_BASE, VID_BASE,  7:4, "Video base address",   Value, 0, 0)  \
      FLD(CHAR_BASE, CHR_BASE,  3:0, "Char base address",    Value, 0, 0)  \
    REG(0x06, LIGHTPEN_X,   "Light pen X")                                 \
    REG(0x07, LIGHTPEN_Y,   "Light pen Y")                                 \
    REG(0x08, PADDLE_X,     "Paddle X")                                    \
    REG(0x09, PADDLE_Y,     "Paddle Y")                                    \
    REG(0x0A, BASS_FREQ,    "Voice 1 bass freq")                           \
      FLD(BASS_FREQ, BASS_EN,   7:7, "Bass enable",          Flag,  0, 0)  \
      FLD(BASS_FREQ, BASS_F,    6:0, "Bass frequency",       Value, 0, 0)  \
    REG(0x0B, ALTO_FREQ,    "Voice 2 alto freq")                           \
      FLD(ALTO_FREQ, ALTO_EN,   7:7, "Alto enable",          Flag,  0, 0)  \
      FLD(ALTO_FREQ, ALTO_F,    6:0, "Alto frequency",       Value, 0, 0)  \
    REG(0x0C, SOPRANO_FREQ, "Voice 3 soprano freq")                        \
      FLD(SOPRANO_FREQ, SOP_EN, 7:7, "Soprano enable",       Flag,  0, 0)  \
      FLD(SOPRANO_FREQ, SOP_F,  6:0, "Soprano frequency",    Value, 0, 0)  \
    REG(0x0D, NOISE_FREQ,   "Voice 4 noise freq")                          \
      FLD(NOISE_FREQ, NOISE_EN, 7:7, "Noise enable",         Flag,  0, 0)  \
      FLD(NOISE_FREQ, NOISE_F,  6:0, "Noise frequency",      Value, 0, 0)  \
    REG(0x0E, AUX_COLOR,    "Aux color / volume")                          \
      FLD(AUX_COLOR, AUX_COL,   7:4, "Auxiliary color",      Color, 0, 0)  \
      FLD(AUX_COLOR, VOLUME,    3:0, "Volume",               Value, 0, 0)  \
    REG(0x0F, BACKGROUND,   "BG/reverse/border")                           \
      FLD(BACKGROUND, BG_COL,   7:4, "Background color",     Color, 0, 0)  \
      FLD(BACKGROUND, REVERSE,  3:3, "Reverse screen",       Flag,  0, 0)  \
      FLD(BACKGROUND, BORDER,   2:0, "Border color",         Color, 0, 0)

// Audio / waveform registers ($900A-$900E)
// The MOS 6560/6561 contains three square-wave tone generators and one
// white-noise generator.  Each voice register has a 7-bit frequency value
// (bits 0-6) and an enable bit (bit 7).  Output frequency formulas:
//   Bass    freq = Phi2 / (128 * (128 - N))   (lowest octave)
//   Alto    freq = Phi2 / (64  * (64  - N))   (middle octave)
//   Soprano freq = Phi2 / (32  * (32  - N))   (highest octave)
//   Noise   freq = Phi2 / (32  * (32  - N))   (LFSR clocked at same rate as soprano)
// where Phi2 = chip master clock and N = 7-bit register value.

// --- Extract address constants ---
namespace vic {
namespace reg {
    VIC_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} // namespace reg
} // namespace vic

DECL_EXTRACT(VIC, VIC_DECL)

// --- Bitfield accessors ---
namespace vic { namespace fld {
#define VIC_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
VIC_DECL(DECL_REG_NOP, VIC_X_FLD_NS_, DECL_CMP_NOP)
#undef VIC_X_FLD_NS_
} } // namespace vic::fld

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
    float lowpass_alpha;             // Lowpass coefficient  = dt / (dt + RC)  (host-rate, legacy path)
    float highpass_alpha;            // Highpass coefficient = dt / (dt + RC)  (host-rate, legacy path)
    float output_gain;               // Maps filter output to uint8 range

    // Per-cycle IIR coefficients (chip-rate, for AudioPort path)
    float lp_alpha_chip;             // Lowpass at chip clock rate
    float hp_alpha_chip;             // Highpass at chip clock rate
    float lp_buf_chip;               // Lowpass state (chip-rate path)
    float hp_buf_chip;               // Highpass state (chip-rate path)

    // Output ring buffer (mono, unsigned 8-bit, centre = 128) ----------
    uint8_t  buffer[VIC_AUDIO_BUFFER_SIZE];
    uint32_t write_pos;              // Next write index (wraps)
    uint32_t read_pos;               // Next read  index (wraps)
};

// VIC chip structure (common base — inherits ChipBase for GUI integration)
struct vic_base_t : public VideoChipBase {
    vic_base_t() {
        init_regs(16);  // VIC has 16 registers
        system_palette_ = get_default_palette();
        palette_size_   = 16;
    }

    void* bus = nullptr;

    // Timing
    uint16_t raster_counter = 0;
    uint32_t current_cycle = 0;
    uint32_t cycles_per_line = 0;
    uint32_t total_lines = 0;
    uint32_t clock_frequency = 0;

    // Video state
    uint8_t current_line[40] = {};
    uint8_t color_ram[1024] = {};

    // Video output (non-owning pointer, set by system/board)
    CompositeVideoOut* video_out_ = nullptr;
    void set_video_out(CompositeVideoOut* s) { video_out_ = s; }

    // Maintained analog signal flags — adjusted at cycle boundaries,
    // used directly in drive() calls (HSync on cycle 0, VSync during vblank).
    SyncFlag drive_flags_ = SyncFlag::None;

    // Frame-end one-shot — set when raster wraps to 0, consumed on first
    // drive() of the new frame.
    bool frame_wrapped_ = false;
    uint16_t signal_frame_start_raster_ = 0;  // Raster for FrameEnd (vertical centering)

    // Audio port output (non-owning pointer, set by system/board)
    // When set, audio_tick() drives the port instead of the internal uint8_t ring buffer.
    AudioPort* audio_port_ = nullptr;
    void set_audio_port(AudioPort* p) { audio_port_ = p; }

    // Configuration
    bool is_pal = false;
    const VicTraits* traits_ = nullptr;

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
    uint16_t cached_columns = 0;           // VIDEO_MATRIX & VIC_VM_COLUMNS_MASK
    uint8_t  cached_border_color = 0;      // BACKGROUND & VIC_BG_BORDER_MASK
    uint8_t  cached_background_color = 0;  // (BACKGROUND >> 4) & 0x0F
    bool     cached_reversed = true;       // !(BACKGROUND & VIC_BG_REVERSE)  (0 = reversed)
    uint8_t  cached_char_height = 8;       // 8 or 16 from ROWS bit 0
    uint8_t  cached_volume = 0;            // AUX_COLOR & VIC_AUX_VOLUME_MASK
    uint8_t  cached_auxiliary_color = 0;   // (AUX_COLOR >> 4) & 0x0F
    uint16_t cached_screen_origin_x = 0;   // CONTROL1 & VIC_C1_SCREEN_ORIGIN_X_MASK
    uint16_t cached_screen_origin_y = 0;   // CONTROL2 << 1
    uint16_t cached_num_rows = 0;          // (ROWS >> 1) & 0x3F
    uint16_t cached_base_video = 0;        // Combined from REG_VIDEO_MATRIX + REG_CHAR_BASE
    uint16_t cached_base_char = 0;         // (REG_CHAR_BASE & 0x0F) << 10

    // Audio generation state
    vic_audio_state_t audio = {};

    // --- Public methods ---
    void init_base(const VicTraits& traits);   // Trait-driven initialization
    virtual void reset();
    bus_state_t tick(bus_state_t bus_state);
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

    // --- ChipBase MMIO interface ---
    bool has_mmio() const override { return true; }
    bus_state_t on_bus_read(bus_state_t bus) noexcept override  { return registers_read(bus); }
    bus_state_t on_bus_write(bus_state_t bus) noexcept override { return registers_write(bus); }

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
};

// ============================================================================
// vic_t<VicTraits> — NTTP-driven VIC template
// ============================================================================
//
// Thin wrapper that inherits all state and implementation from vic_base_t.
// The template parameter bakes variant-specific constants into the type,
// enabling type-safe PAL/NTSC distinction and compile-time identity.
//
// Usage:
//   using mos6561_t = vic_t<MOS6561_traits>;   // PAL
//   using mos6560_t = vic_t<MOS6560_traits>;   // NTSC
//
template<const VicTraits& Traits>
struct vic_t : public vic_base_t {
    static constexpr const VicTraits& traits = Traits;

    // Initialize with Traits-derived config
    void init() {
        init_base(Traits);
    }
};