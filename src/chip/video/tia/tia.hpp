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

#include "chip/video/video_chip_base.hpp"
#include "core/signal/composite_video_out.hpp"
#include "core/signal/audio_port.hpp"
#include "utils/ring_buffer.hpp"
#include <cstdint>

// ============================================================================
// TIA WRITE REGISTER TABLE ($00-$2C) — single source of truth
// ============================================================================

// TIA_WRITE_DECL(REG, FLD, CMP) — 45 write registers
// Strobe registers (RESxx, HMOVE, HMCLR, CXCLR, WSYNC, RSYNC) trigger
// side-effects on write; stored value is informational only.
#define TIA_WRITE_DECL(REG, FLD, CMP) \
    REG(0x00, VSYNC,    "Vertical sync set/clear")                          \
      FLD(VSYNC,  VSYNC_EN,   1:1, "VSYNC enable",         Flag, 0, 0)      \
    REG(0x01, VBLANK,   "Vertical blank / input control")                   \
      FLD(VBLANK, VBLANK_EN,  1:1, "VBLANK enable",        Flag, 0, 0)      \
      FLD(VBLANK, INP_LATCH,  6:6, "Latch input ports",    Flag, 0, 0)      \
      FLD(VBLANK, INP_DUMP,   7:7, "Dump paddle caps",     Flag, 0, 0)      \
    REG(0x02, WSYNC,    "Wait for horiz sync (strobe)")                     \
    REG(0x03, RSYNC,    "Reset horiz sync counter (strobe)")                \
    REG(0x04, NUSIZ0,   "Number-size player/missile 0")                     \
      FLD(NUSIZ0, PM_MODE0,   2:0, "Player/missile mode",  Value, 0, 0)     \
      FLD(NUSIZ0, MISSIL_SZ0, 5:4, "Missile width (2^n)",  Value, 0, 0)     \
    REG(0x05, NUSIZ1,   "Number-size player/missile 1")                     \
      FLD(NUSIZ1, PM_MODE1,   2:0, "Player/missile mode",  Value, 0, 0)     \
      FLD(NUSIZ1, MISSIL_SZ1, 5:4, "Missile width (2^n)",  Value, 0, 0)     \
    REG(0x06, COLUP0,   "Color-luminance player 0")                         \
      FLD(COLUP0, COL_P0,     7:1, "Color-lum index",      Color, 0, 0)     \
    REG(0x07, COLUP1,   "Color-luminance player 1")                         \
      FLD(COLUP1, COL_P1,     7:1, "Color-lum index",      Color, 0, 0)     \
    REG(0x08, COLUPF,   "Color-luminance playfield")                        \
      FLD(COLUPF, COL_PF,     7:1, "Color-lum index",      Color, 0, 0)     \
    REG(0x09, COLUBK,   "Color-luminance background")                       \
      FLD(COLUBK, COL_BK,     7:1, "Color-lum index",      Color, 0, 0)     \
    REG(0x0A, CTRLPF,   "Playfield control / ball size")                    \
      FLD(CTRLPF, PF_REFLECT, 0:0, "Playfield reflect",    Flag, 0, 0)      \
      FLD(CTRLPF, PF_SCORE,   1:1, "Score mode coloring",  Flag, 0, 0)      \
      FLD(CTRLPF, PF_PRIO,    2:2, "PF/BL priority",       Flag, 0, 0)      \
      FLD(CTRLPF, BALL_SZ,    5:4, "Ball width (2^n)",     Value, 0, 0)     \
    REG(0x0B, REFP0,    "Reflect player 0")                                 \
      FLD(REFP0,  REFLECT_P0, 3:3, "Reflect",              Flag, 0, 0)      \
    REG(0x0C, REFP1,    "Reflect player 1")                                 \
      FLD(REFP1,  REFLECT_P1, 3:3, "Reflect",              Flag, 0, 0)      \
    REG(0x0D, PF0,      "Playfield bits 4-7")                               \
      FLD(PF0,    PF0_BITS,   7:4, "PF bits 0-3 (reversed)",Value, 0, 0)    \
    REG(0x0E, PF1,      "Playfield bits 0-7")                               \
    REG(0x0F, PF2,      "Playfield bits 0-7 (reversed)")                    \
    REG(0x10, RESP0,    "Reset player 0 position (strobe)")                 \
    REG(0x11, RESP1,    "Reset player 1 position (strobe)")                 \
    REG(0x12, RESM0,    "Reset missile 0 position (strobe)")                \
    REG(0x13, RESM1,    "Reset missile 1 position (strobe)")                \
    REG(0x14, RESBL,    "Reset ball position (strobe)")                     \
    REG(0x15, AUDC0,    "Audio control 0")                                  \
      FLD(AUDC0,  AUD_MODE0,  3:0, "Control mode",         Value, 0, 0)     \
    REG(0x16, AUDC1,    "Audio control 1")                                  \
      FLD(AUDC1,  AUD_MODE1,  3:0, "Control mode",         Value, 0, 0)     \
    REG(0x17, AUDF0,    "Audio frequency divider 0")                        \
      FLD(AUDF0,  AUD_FREQ0,  4:0, "Frequency divider",    Value, 0, 0)     \
    REG(0x18, AUDF1,    "Audio frequency divider 1")                        \
      FLD(AUDF1,  AUD_FREQ1,  4:0, "Frequency divider",    Value, 0, 0)     \
    REG(0x19, AUDV0,    "Audio volume 0")                                   \
      FLD(AUDV0,  AUD_VOL0,   3:0, "Volume",               Value, 0, 0)     \
    REG(0x1A, AUDV1,    "Audio volume 1")                                   \
      FLD(AUDV1,  AUD_VOL1,   3:0, "Volume",               Value, 0, 0)     \
    REG(0x1B, GRP0,     "Graphics player 0")                                \
    REG(0x1C, GRP1,     "Graphics player 1")                                \
    REG(0x1D, ENAM0,    "Enable missile 0")                                 \
      FLD(ENAM0,  EN_M0,      1:1, "Missile 0 enable",     Flag, 0, 0)      \
    REG(0x1E, ENAM1,    "Enable missile 1")                                 \
      FLD(ENAM1,  EN_M1,      1:1, "Missile 1 enable",     Flag, 0, 0)      \
    REG(0x1F, ENABL,    "Enable ball")                                      \
      FLD(ENABL,  EN_BL,      1:1, "Ball enable",          Flag, 0, 0)      \
    REG(0x20, HMP0,     "Horiz motion player 0")                            \
      FLD(HMP0,   HM_P0,      7:4, "Motion value (signed)",Value, 0, 0)     \
    REG(0x21, HMP1,     "Horiz motion player 1")                            \
      FLD(HMP1,   HM_P1,      7:4, "Motion value (signed)",Value, 0, 0)     \
    REG(0x22, HMM0,     "Horiz motion missile 0")                           \
      FLD(HMM0,   HM_M0,      7:4, "Motion value (signed)",Value, 0, 0)     \
    REG(0x23, HMM1,     "Horiz motion missile 1")                           \
      FLD(HMM1,   HM_M1,      7:4, "Motion value (signed)",Value, 0, 0)     \
    REG(0x24, HMBL,     "Horiz motion ball")                                \
      FLD(HMBL,   HM_BL,      7:4, "Motion value (signed)",Value, 0, 0)     \
    REG(0x25, VDELP0,   "Vertical delay player 0")                          \
      FLD(VDELP0, VD_P0,      0:0, "Delay P0",             Flag, 0, 0)      \
    REG(0x26, VDELP1,   "Vertical delay player 1")                          \
      FLD(VDELP1, VD_P1,      0:0, "Delay P1",             Flag, 0, 0)      \
    REG(0x27, VDELBL,   "Vertical delay ball")                              \
      FLD(VDELBL, VD_BL,      0:0, "Delay ball",           Flag, 0, 0)      \
    REG(0x28, RESMP0,   "Reset missile 0 to player 0")                      \
      FLD(RESMP0, RST_M0,     1:1, "Lock M0 to P0",        Flag, 0, 0)      \
    REG(0x29, RESMP1,   "Reset missile 1 to player 1")                      \
      FLD(RESMP1, RST_M1,     1:1, "Lock M1 to P1",        Flag, 0, 0)      \
    REG(0x2A, HMOVE,    "Apply horiz motion (strobe)")                      \
    REG(0x2B, HMCLR,    "Clear horiz motion regs (strobe)")                 \
    REG(0x2C, CXCLR,    "Clear collision latches (strobe)")

// --- Extract write register constants ---
namespace tia_w {
namespace reg {
    TIA_WRITE_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} // namespace reg
} // namespace tia_w

DECL_EXTRACT(TIA_W, TIA_WRITE_DECL)

namespace tia_w { namespace fld {
#define TIA_W_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
TIA_WRITE_DECL(DECL_REG_NOP, TIA_W_X_FLD_NS_, DECL_CMP_NOP)
#undef TIA_W_X_FLD_NS_
} } // namespace tia_w::fld

// ============================================================================
// TIA READ REGISTER TABLE ($00-$0D) — single source of truth
// ============================================================================

// TIA_READ_DECL(REG, FLD, CMP) — 14 read registers
// Collision latches in bits 7:6, input ports in bit 7.
#define TIA_READ_DECL(REG, FLD, CMP) \
    REG(0x00, CXM0P,   "Collision M0-P1, M0-P0")                           \
      FLD(CXM0P,  CX_M0P1,    7:7, "M0-P1 collision",     Flag, 0, 0)      \
      FLD(CXM0P,  CX_M0P0,    6:6, "M0-P0 collision",     Flag, 0, 0)      \
    REG(0x01, CXM1P,   "Collision M1-P0, M1-P1")                           \
      FLD(CXM1P,  CX_M1P0,    7:7, "M1-P0 collision",     Flag, 0, 0)      \
      FLD(CXM1P,  CX_M1P1,    6:6, "M1-P1 collision",     Flag, 0, 0)      \
    REG(0x02, CXP0FB,  "Collision P0-PF, P0-BL")                           \
      FLD(CXP0FB, CX_P0PF,    7:7, "P0-PF collision",     Flag, 0, 0)      \
      FLD(CXP0FB, CX_P0BL,    6:6, "P0-BL collision",     Flag, 0, 0)      \
    REG(0x03, CXP1FB,  "Collision P1-PF, P1-BL")                           \
      FLD(CXP1FB, CX_P1PF,    7:7, "P1-PF collision",     Flag, 0, 0)      \
      FLD(CXP1FB, CX_P1BL,    6:6, "P1-BL collision",     Flag, 0, 0)      \
    REG(0x04, CXM0FB,  "Collision M0-PF, M0-BL")                           \
      FLD(CXM0FB, CX_M0PF,    7:7, "M0-PF collision",     Flag, 0, 0)      \
      FLD(CXM0FB, CX_M0BL,    6:6, "M0-BL collision",     Flag, 0, 0)      \
    REG(0x05, CXM1FB,  "Collision M1-PF, M1-BL")                           \
      FLD(CXM1FB, CX_M1PF,    7:7, "M1-PF collision",     Flag, 0, 0)      \
      FLD(CXM1FB, CX_M1BL,    6:6, "M1-BL collision",     Flag, 0, 0)      \
    REG(0x06, CXBLPF,  "Collision BL-PF")                                  \
      FLD(CXBLPF, CX_BLPF,    7:7, "BL-PF collision",     Flag, 0, 0)      \
    REG(0x07, CXPPMM,  "Collision P0-P1, M0-M1")                           \
      FLD(CXPPMM, CX_P0P1,    7:7, "P0-P1 collision",     Flag, 0, 0)      \
      FLD(CXPPMM, CX_M0M1,    6:6, "M0-M1 collision",     Flag, 0, 0)      \
    REG(0x08, INPT0,   "Pot port 0 (paddle)")                              \
      FLD(INPT0,  POT0,       7:7, "Pot 0 input",          Flag, 0, 0)     \
    REG(0x09, INPT1,   "Pot port 1")                                       \
      FLD(INPT1,  POT1,       7:7, "Pot 1 input",          Flag, 0, 0)     \
    REG(0x0A, INPT2,   "Pot port 2")                                       \
      FLD(INPT2,  POT2,       7:7, "Pot 2 input",          Flag, 0, 0)     \
    REG(0x0B, INPT3,   "Pot port 3")                                       \
      FLD(INPT3,  POT3,       7:7, "Pot 3 input",          Flag, 0, 0)     \
    REG(0x0C, INPT4,   "Joystick fire P0")                                 \
      FLD(INPT4,  FIRE_P0,    7:7, "P0 fire button",       Flag, 0, 0)     \
    REG(0x0D, INPT5,   "Joystick fire P1")                                 \
      FLD(INPT5,  FIRE_P1,    7:7, "P1 fire button",       Flag, 0, 0)

// --- Extract read register constants ---
namespace tia_r {
namespace reg {
    TIA_READ_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} // namespace reg
} // namespace tia_r

DECL_EXTRACT(TIA_R, TIA_READ_DECL)

namespace tia_r { namespace fld {
#define TIA_R_X_FLD_NS_(reg, fld, hilo, desc, kind, ds, dm) \
    inline constexpr uint32_t reg##_##fld   = BF_MASK(hilo); \
    inline constexpr uint8_t  reg##_##fld##_S = BF_LO(hilo);
TIA_READ_DECL(DECL_REG_NOP, TIA_R_X_FLD_NS_, DECL_CMP_NOP)
#undef TIA_R_X_FLD_NS_
} } // namespace tia_r::fld

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
    // Internal state only — register values live in ChipBase::regs_[]
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

struct tia_t : public VideoChipBase {
    tia_t() : VideoChipBase(ChipInfo{"TIA", "Atari"}) {
        init_split_regs(WRITE_REG_COUNT, READ_REG_COUNT);
    }

    // --- ChipBase bus interface (MMIO) ---
    bool has_mmio() const override { return true; }
    bus_state_t on_bus_read(bus_state_t bus) noexcept override {
        BUS_SET_DATA(bus, read(BUS_GET_ADDR(bus)));
        return bus;
    }
    bus_state_t on_bus_write(bus_state_t bus) noexcept override {
        write(BUS_GET_ADDR(bus), BUS_GET_DATA(bus));
        return bus;
    }

    // --- ChipBase GUI interface ---
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // ========================================================================
    // REGISTER ARRAYS (storage in ChipBase::regs_[])
    // ========================================================================

    // Write registers ($00-$2C) — raw byte as written by CPU.
    // Strobe registers (RESPx, HMOVE, HMCLR, CXCLR, WSYNC, RSYNC) store
    // the data written, though only the side-effect matters.
    // Accessed via regs_[0..WRITE_REG_COUNT-1].
    static constexpr int WRITE_REG_COUNT = 0x2D;  // 45 registers

    // Read registers ($00-$0D) — collision + input ports.
    // Updated lazily in read().
    // Accessed via read_regs_[0..READ_REG_COUNT-1].
    static constexpr int READ_REG_COUNT = 0x0E;   // 14 registers

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
    // Per-object collision accumulators (one per graphical object).
    // cx[i] records the OR of all pixel_bits values seen when object i was
    // active. Testing a collision pair (A, B) is: cx[cermu_ctz(A)] & B.
    // Replaces the old 15-bit collision word + 64-entry LUT.
    static constexpr int CX_M0 = 0; // Missile 0
    static constexpr int CX_M1 = 1; // Missile 1
    static constexpr int CX_P0 = 2; // Player 0
    static constexpr int CX_P1 = 3; // Player 1
    static constexpr int CX_BL = 4; // Ball
    static constexpr int CX_PF = 5; // Playfield
    uint8_t cx[6] = {};

    // Pixel test functions return these directly (or 0), so the caller
    // can OR return values together into a single pixel_bits byte.
    // Bit positions are chosen so that collision read registers for the
    // four "FB" pairs (P0-PF/BL, P1-PF/BL, M0-PF/BL, M1-PF/BL) can be
    // extracted with a single (cx[i] & 0x30) << 2, and CXM0P uses
    // (cx[0] & 0x0C) << 4.
    static constexpr uint8_t PX_M0 = 1 << CX_M0; // Missile 0 mask
    static constexpr uint8_t PX_M1 = 1 << CX_M1; // Missile 1 mask
    static constexpr uint8_t PX_P0 = 1 << CX_P0; // Player 0 mask
    static constexpr uint8_t PX_P1 = 1 << CX_P1; // Player 1 mask
    static constexpr uint8_t PX_BL = 1 << CX_BL; // Ball mask
    static constexpr uint8_t PX_PF = 1 << CX_PF; // Playfield mask

    /// Test whether two objects have collided.
    bool has_collision(uint8_t px_a, uint8_t px_b) const;

    // ========================================================================
    // AUDIO
    // ========================================================================

    tia_audio_channel_t audio[2];

    // Audio output ring buffer (float [-1, 1])
    AudioRingBuffer audio_buffer_{4096};
    uint32_t audio_cycle_counter = 0;
    uint32_t audio_cycles_per_sample = 0;     // CPU cycles per audio sample

    AudioPort* audio_port_ = nullptr;
    void set_audio_port(AudioPort* p) { audio_port_ = p; }

    // ========================================================================
    // VIDEO OUTPUT
    // ========================================================================

    CompositeVideoOut* video_out_ = nullptr;
    void set_video_out(CompositeVideoOut* s) { video_out_ = s; }
    bool prev_vsync_signal_ = false;  // Edge detection for FrameEnd emission

    // ========================================================================
    // TEST FRAMEBUFFER (optional, for harness pixel verification)
    // ========================================================================

    uint32_t* test_framebuffer_  = nullptr;  ///< Optional RGBA output for tests
    int       test_fb_width_     = 0;
    int       test_fb_height_    = 0;

    /// Set an RGBA framebuffer for test pixel readback.
    /// The TIA will blit each visible scanline into this buffer using the
    /// NTSC palette.  Pass nullptr to disable.
    void set_framebuffer(uint32_t* fb, int w, int h) {
        test_framebuffer_ = fb;
        test_fb_width_    = w;
        test_fb_height_   = h;
    }

    // ========================================================================
    // SCANLINE BUFFER
    // ========================================================================

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
