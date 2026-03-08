#pragma once
/*
 * tia.h — Atari TIA (Television Interface Adapter) — CO10444
 *
 * The TIA is the custom video and audio chip of the Atari 2600.
 * It generates:
 *   - 228 color clocks per scanline (68 blank + 160 visible)
 *   - 262 scanlines per frame (NTSC) / 312 (PAL)
 *   - 128-color palette (NTSC) / 104 (PAL)
 *   - 5 graphics objects: 2 players, 2 missiles, 1 ball, plus playfield
 *   - 2 audio channels with 4-bit frequency divider, 4-bit volume, 4-bit waveform
 *   - Collision detection between all object pairs
 *   - CPU synchronization via WSYNC (halt CPU until end of scanline)
 *
 * Address space (active when A12=0, A7=0):
 *   Write registers: $00-$2C (active on A6=0)
 *   Read registers:  $00-$0D (active on A6=0, active on A1-A3)
 *
 * Display timing (NTSC):
 *   TIA clock = 3× CPU clock = 3.579545 MHz
 *   228 TIA clocks/line = 76 CPU cycles/line
 *   68 clocks HBLANK + 160 clocks visible
 *   262 lines/frame @ ~60 Hz
 */

#include "../../core/chip.h"
#include "../video_pixel_unit.h"
#include <cstdint>

// ============================================================================
// TIA WRITE REGISTERS ($00-$2C)
// ============================================================================

static constexpr uint8_t TIA_VSYNC   = 0x00;  // Vertical sync
static constexpr uint8_t TIA_VBLANK  = 0x01;  // Vertical blank
static constexpr uint8_t TIA_WSYNC   = 0x02;  // Wait for horizontal sync (halt CPU)
static constexpr uint8_t TIA_RSYNC   = 0x03;  // Reset horizontal sync counter
static constexpr uint8_t TIA_NUSIZ0  = 0x04;  // Number-size player/missile 0
static constexpr uint8_t TIA_NUSIZ1  = 0x05;  // Number-size player/missile 1
static constexpr uint8_t TIA_COLUP0  = 0x06;  // Color-luminance player 0
static constexpr uint8_t TIA_COLUP1  = 0x07;  // Color-luminance player 1
static constexpr uint8_t TIA_COLUPF  = 0x08;  // Color-luminance playfield
static constexpr uint8_t TIA_COLUBK  = 0x09;  // Color-luminance background
static constexpr uint8_t TIA_CTRLPF  = 0x0A;  // Control playfield, ball size
static constexpr uint8_t TIA_REFP0   = 0x0B;  // Reflect player 0
static constexpr uint8_t TIA_REFP1   = 0x0C;  // Reflect player 1
static constexpr uint8_t TIA_PF0     = 0x0D;  // Playfield register 0 (bits 4-7)
static constexpr uint8_t TIA_PF1     = 0x0E;  // Playfield register 1 (bits 0-7)
static constexpr uint8_t TIA_PF2     = 0x0F;  // Playfield register 2 (bits 0-7)
static constexpr uint8_t TIA_RESP0   = 0x10;  // Reset player 0 position
static constexpr uint8_t TIA_RESP1   = 0x11;  // Reset player 1 position
static constexpr uint8_t TIA_RESM0   = 0x12;  // Reset missile 0 position
static constexpr uint8_t TIA_RESM1   = 0x13;  // Reset missile 1 position
static constexpr uint8_t TIA_RESBL   = 0x14;  // Reset ball position
static constexpr uint8_t TIA_AUDC0   = 0x15;  // Audio control 0
static constexpr uint8_t TIA_AUDC1   = 0x16;  // Audio control 1
static constexpr uint8_t TIA_AUDF0   = 0x17;  // Audio frequency 0
static constexpr uint8_t TIA_AUDF1   = 0x18;  // Audio frequency 1
static constexpr uint8_t TIA_AUDV0   = 0x19;  // Audio volume 0
static constexpr uint8_t TIA_AUDV1   = 0x1A;  // Audio volume 1
static constexpr uint8_t TIA_GRP0    = 0x1B;  // Graphics player 0
static constexpr uint8_t TIA_GRP1    = 0x1C;  // Graphics player 1
static constexpr uint8_t TIA_ENAM0   = 0x1D;  // Enable missile 0
static constexpr uint8_t TIA_ENAM1   = 0x1E;  // Enable missile 1
static constexpr uint8_t TIA_ENABL   = 0x1F;  // Enable ball
static constexpr uint8_t TIA_HMP0    = 0x20;  // Horizontal motion player 0
static constexpr uint8_t TIA_HMP1    = 0x21;  // Horizontal motion player 1
static constexpr uint8_t TIA_HMM0    = 0x22;  // Horizontal motion missile 0
static constexpr uint8_t TIA_HMM1    = 0x23;  // Horizontal motion missile 1
static constexpr uint8_t TIA_HMBL    = 0x24;  // Horizontal motion ball
static constexpr uint8_t TIA_VDELP0  = 0x25;  // Vertical delay player 0
static constexpr uint8_t TIA_VDELP1  = 0x26;  // Vertical delay player 1
static constexpr uint8_t TIA_VDELBL  = 0x27;  // Vertical delay ball
static constexpr uint8_t TIA_RESMP0  = 0x28;  // Reset missile 0 to player 0
static constexpr uint8_t TIA_RESMP1  = 0x29;  // Reset missile 1 to player 1
static constexpr uint8_t TIA_HMOVE   = 0x2A;  // Apply horizontal motion
static constexpr uint8_t TIA_HMCLR   = 0x2B;  // Clear horizontal motion registers
static constexpr uint8_t TIA_CXCLR   = 0x2C;  // Clear collision latches

// ============================================================================
// TIA READ REGISTERS ($00-$0D)
// ============================================================================

static constexpr uint8_t TIA_CXM0P   = 0x00;  // Collision M0-P1, M0-P0
static constexpr uint8_t TIA_CXM1P   = 0x01;  // Collision M1-P0, M1-P1
static constexpr uint8_t TIA_CXP0FB  = 0x02;  // Collision P0-PF, P0-BL
static constexpr uint8_t TIA_CXP1FB  = 0x03;  // Collision P1-PF, P1-BL
static constexpr uint8_t TIA_CXM0FB  = 0x04;  // Collision M0-PF, M0-BL
static constexpr uint8_t TIA_CXM1FB  = 0x05;  // Collision M1-PF, M1-BL
static constexpr uint8_t TIA_CXBLPF  = 0x06;  // Collision BL-PF
static constexpr uint8_t TIA_CXPPMM  = 0x07;  // Collision P0-P1, M0-M1
static constexpr uint8_t TIA_INPT0   = 0x08;  // Pot port 0 (paddle)
static constexpr uint8_t TIA_INPT1   = 0x09;  // Pot port 1
static constexpr uint8_t TIA_INPT2   = 0x0A;  // Pot port 2
static constexpr uint8_t TIA_INPT3   = 0x0B;  // Pot port 3
static constexpr uint8_t TIA_INPT4   = 0x0C;  // Input port 4 (joystick fire P0)
static constexpr uint8_t TIA_INPT5   = 0x0D;  // Input port 5 (joystick fire P1)

// ============================================================================
// TIA CONSTANTS
// ============================================================================

namespace tia_constants {
    inline constexpr int CLOCKS_PER_LINE     = 228;
    inline constexpr int HBLANK_CLOCKS       = 68;
    inline constexpr int VISIBLE_CLOCKS      = 160;
    inline constexpr int LINES_PER_FRAME_NTSC = 262;
    inline constexpr int LINES_PER_FRAME_PAL  = 312;

    inline constexpr int DISPLAY_WIDTH       = 160;
    inline constexpr int DISPLAY_HEIGHT      = 192;    // Typical visible area (varies by game)

    // Color clock frequency
    inline constexpr uint32_t TIA_FREQ_NTSC  = 3579545;
    inline constexpr uint32_t CPU_FREQ_NTSC  = TIA_FREQ_NTSC / 3;  // ~1.19 MHz
    inline constexpr uint32_t CPU_FREQ_PAL   = 3546894 / 3;        // ~1.18 MHz

    // Cycles per frame
    inline constexpr uint32_t CYCLES_PER_FRAME_NTSC = 262 * 76;    // 19912
    inline constexpr uint32_t CYCLES_PER_FRAME_PAL  = 312 * 76;    // 23712

    inline constexpr int AUDIO_SAMPLE_RATE = 44100;
}

// ============================================================================
// TIA AUDIO CHANNEL
// ============================================================================

struct tia_audio_channel_t {
    // Internal state only — register values live in tia_t::write_regs[]
    uint8_t  div_counter = 0; // Frequency divider counter
    uint8_t  poly4  = 0x0F;   // 4-bit polynomial counter (LFSR)
    uint8_t  poly5  = 0x1F;   // 5-bit polynomial counter
    uint8_t  poly9  = 0;      // 9-bit polynomial counter (low byte)
    uint8_t  poly9_hi = 0x01; // 9-bit polynomial counter (high bit)
    bool     output = false;  // Current audio output state
};

// ============================================================================
// TIA CHIP STRUCTURE
// ============================================================================

struct tia_t : public ChipBase {
    tia_t() : ChipBase(ChipInfo{"TIA", "Atari"}) {}

    // --- ChipBase GUI interface ---
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ========================================================================
    // REGISTER ARRAYS
    // ========================================================================

    // Write registers ($00-$2C) — raw byte as written by CPU.
    // Strobe registers (RESPx, HMOVE, HMCLR, CXCLR, WSYNC, RSYNC) store
    // the data written, though only the side-effect matters.
    static constexpr int WRITE_REG_COUNT = 0x2D;  // 45 registers
    uint8_t write_regs[WRITE_REG_COUNT] = {};

    // Read registers ($00-$0D) — collision + input ports.
    // Updated lazily in read().
    static constexpr int READ_REG_COUNT = 0x0E;   // 14 registers
    uint8_t read_regs[READ_REG_COUNT] = {};

    // ========================================================================
    // DISPLAY / TIMING STATE
    // ========================================================================

    // Horizontal position (0-227, in color clocks)
    uint16_t h_counter = 0;

    // Vertical position (scanline number, managed by system — TIA doesn't count them)
    uint16_t scanline = 0;

    // WSYNC — halt CPU until end of scanline
    bool wsync_pending = false;

    // HMOVE blanking — first 8 visible pixels blanked after HMOVE strobe during HBLANK
    bool hmove_blank_active = false;

    // Visible row tracking for framebuffer mapping.
    // Increments only during non-VBLANK scanlines, so the first visible
    // line maps to framebuffer row 0 regardless of VBLANK duration.
    int visible_row = -1;        // -1 = not yet in visible area
    bool prev_vblank = false;    // Edge detection for VBLANK→visible transition

    // ========================================================================
    // GRAPHICS INTERNAL STATE (not directly register values)
    // ========================================================================

    // Latched graphics copies (vertical delay mechanism)
    uint8_t grp0_old = 0;  // Previous GRP0 (for vertical delay)
    uint8_t grp1_old = 0;  // Previous GRP1 (for vertical delay)
    bool    enabl_old = false; // Previous ENABL (for vertical delay)

    // Object positions (reset by RESPx strobes, adjusted by HMxx via HMOVE)
    uint8_t pos_p0 = 0;
    uint8_t pos_p1 = 0;
    uint8_t pos_m0 = 0;
    uint8_t pos_m1 = 0;
    uint8_t pos_bl = 0;

    // ========================================================================
    // COLLISION
    // ========================================================================

    // Per-object pixel bitmask constants.
    // Pixel test functions return these directly (or 0), so the caller
    // can OR return values together into a single pixel_bits byte.
    // Bit positions are chosen so that collision read registers for the
    // four "FB" pairs (P0-PF/BL, P1-PF/BL, M0-PF/BL, M1-PF/BL) can be
    // extracted with a single (cx[i] & 0x30) << 2, and CXM0P uses
    // (cx[0] & 0x0C) << 4.
    static constexpr uint8_t PX_M0 = 1 << 0; // Missile 0
    static constexpr uint8_t PX_M1 = 1 << 1; // Missile 1
    static constexpr uint8_t PX_P0 = 1 << 2; // Player 0
    static constexpr uint8_t PX_P1 = 1 << 3; // Player 1
    static constexpr uint8_t PX_BL = 1 << 4; // Ball
    static constexpr uint8_t PX_PF = 1 << 5; // Playfield

    // Per-object collision accumulators (one per PX_* bit position).
    // cx[i] records the OR of all pixel_bits values seen when object i was
    // active. Testing a collision pair (A, B) is: cx[bit_index(A)] & B.
    // Replaces the old 15-bit collision word + 64-entry LUT.
    static constexpr int CX_M0 = 0;
    static constexpr int CX_M1 = 1;
    static constexpr int CX_P0 = 2;
    static constexpr int CX_P1 = 3;
    static constexpr int CX_BL = 4;
    static constexpr int CX_PF = 5;
    uint8_t cx[6] = {};

    /// Test whether two objects have collided.
    bool has_collision(uint8_t px_a, uint8_t px_b) const;

    // ========================================================================
    // AUDIO
    // ========================================================================

    tia_audio_channel_t audio[2];

    // Audio output ring buffer (float [-1, 1])
    float    audio_ring_buffer[4096] = {};
    uint32_t audio_write_pos = 0;
    uint32_t audio_read_pos  = 0;
    uint32_t audio_cycle_counter = 0;
    uint32_t audio_cycles_per_sample = 0;     // CPU cycles per audio sample

    // ========================================================================
    // FRAMEBUFFER
    // ========================================================================

    VideoPixelUnit pixel;
    uint8_t color_line_buffer[tia_constants::DISPLAY_WIDTH] = {};

    // Pre-swizzled palette in ABGR format (GL_RGBA little-endian convention).
    // Built from ntsc_palette (ARGB) during init().
    uint32_t palette_rgba_[128] = {};

    // ========================================================================
    // NTSC PALETTE
    // ========================================================================

    static const uint32_t ntsc_palette[128];

    // ========================================================================
    // INTERFACE
    // ========================================================================

    void init();
    void reset();

    /// Advance TIA by one color clock (3 per CPU cycle).
    /// Call this 3 times per CPU cycle.
    void tick_color_clock();

    /// Convenience: tick 3 color clocks (one CPU cycle worth).
    void tick_cpu_cycle();

    /// Read a TIA register.
    uint8_t read(uint16_t addr);

    /// Write a TIA register.
    void write(uint16_t addr, uint8_t data);

    /// Set the framebuffer for rendering.
    void set_framebuffer(uint32_t* buf, int w, int h);

    /// Audio interface
    uint32_t audio_available() const;
    uint32_t audio_read(float* buffer, uint32_t max_samples);
    void set_audio_sample_rate(int sample_rate_hz);

    /// Is the CPU halted by WSYNC?
    bool is_cpu_halted() const { return wsync_pending; }

private:
    /// Render one pixel at the current h_counter position.
    void render_pixel();

    /// Tick one audio channel (0 or 1).
    void tick_audio_channel(int ch_idx);

    /// Get playfield pixel bitmask at the given pixel position (0-159).
    /// Returns PX_PF if playfield is set, 0 otherwise.
    uint8_t get_playfield_pixel(int x) const;

    /// Get player graphics pixel bitmask at the given pixel position.
    /// Returns px_bit (PX_P0 or PX_P1) if player is present, 0 otherwise.
    uint8_t get_player_pixel(int x, uint8_t grp, uint8_t pos, uint8_t nusiz, bool reflect, uint8_t px_bit) const;

    /// Get missile/ball pixel bitmask (single copy, used for ball).
    /// Returns px_bit if object is present, 0 otherwise.
    uint8_t get_missile_pixel(int x, uint8_t pos, uint8_t size_bits, bool enabled, uint8_t px_bit) const;

    /// Get missile pixel bitmask with copy positions from NUSIZ register.
    /// Returns px_bit if object is present, 0 otherwise.
    uint8_t get_missile_pixel(int x, uint8_t pos, uint8_t size_bits, bool enabled, uint8_t nusiz, uint8_t px_bit) const;

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};
