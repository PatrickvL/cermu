#pragma once
/*
 * ay_3_8910.h — General Instrument AY-3-8910 Programmable Sound Generator
 *
 * The AY-3-8910 (1978) is a 3-channel square wave + noise + envelope
 * sound generator with two 8-bit I/O ports.  One of the most widely-used
 * sound chips of the 8-bit era.
 *
 * Variants:
 *   AY-3-8910: 3 channels, 2 I/O ports (40-pin DIP)
 *   AY-3-8912: 3 channels, 1 I/O port  (28-pin DIP)
 *   AY-3-8913: 3 channels, no I/O ports (24-pin DIP)
 *   YM2149:    Yamaha-licensed clone with half-step envelope precision
 *
 * Used in: ZX Spectrum 128K, Amstrad CPC (via PPI), MSX, Atari ST (YM2149),
 *          Intellivision, many arcade machines (Bomb Jack, etc.)
 *
 * Bus interface:
 *   BDIR + BC1 select the bus operation:
 *     00 = Inactive
 *     01 = Read register
 *     10 = Write register
 *     11 = Latch address
 */

#include "chip/sound/sound_chip_base.hpp"
#include "core/system_lines.hpp"
#include <cstdint>
#include <cstring>

// ============================================================================
// AY-3-8910 Variant Configuration
// ============================================================================

enum class AYVariant : uint8_t {
    AY_3_8910,   // Original GI, 2 I/O ports
    AY_3_8912,   // 1 I/O port
    AY_3_8913,   // No I/O ports
    YM2149,      // Yamaha clone (half-step envelope)
};

struct AYVariantTraits {
    const char* part_number;
    const char* manufacturer;
    uint8_t     io_port_count;   // 0, 1, or 2
    bool        half_step_env;   // YM2149 envelope precision
};

inline constexpr AYVariantTraits ay_variant_traits[] = {
    { "AY-3-8910", "General Instrument", 2, false },
    { "AY-3-8912", "General Instrument", 1, false },
    { "AY-3-8913", "General Instrument", 0, false },
    { "YM2149",    "Yamaha",             2, true  },
};

// ============================================================================
// AY-3-8910 REGISTER TABLE — single source of truth
// ============================================================================

// DECL(REG, FLD, CMP) — 16 registers, 18 fields (MIXER/AMP/ENV_SHAPE bits)
#define AY_DECL(REG, FLD, CMP) \
    REG(0x00, TONE_A_FINE,   "Ch A tone period fine")                            \
    REG(0x01, TONE_A_COARSE, "Ch A tone period coarse")                          \
    REG(0x02, TONE_B_FINE,   "Ch B tone period fine")                            \
    REG(0x03, TONE_B_COARSE, "Ch B tone period coarse")                          \
    REG(0x04, TONE_C_FINE,   "Ch C tone period fine")                            \
    REG(0x05, TONE_C_COARSE, "Ch C tone period coarse")                          \
    REG(0x06, NOISE_PERIOD,  "Noise generator period")                           \
    REG(0x07, MIXER,         "Tone/noise mixer control")                         \
      FLD(MIXER, IOB_DIR,   7:7, "I/O port B dir (1=out)",   Flag, 0, 0)        \
      FLD(MIXER, IOA_DIR,   6:6, "I/O port A dir (1=out)",   Flag, 0, 0)        \
      FLD(MIXER, NOISE_C,   5:5, "Noise C disable",          Flag, 0, 0)        \
      FLD(MIXER, NOISE_B,   4:4, "Noise B disable",          Flag, 0, 0)        \
      FLD(MIXER, NOISE_A,   3:3, "Noise A disable",          Flag, 0, 0)        \
      FLD(MIXER, TONE_C,    2:2, "Tone C disable",           Flag, 0, 0)        \
      FLD(MIXER, TONE_B,    1:1, "Tone B disable",           Flag, 0, 0)        \
      FLD(MIXER, TONE_A,    0:0, "Tone A disable",           Flag, 0, 0)        \
    REG(0x08, AMP_A,         "Ch A amplitude")                                   \
      FLD(AMP_A, ENV_A,     4:4, "Envelope mode",            Flag, 0, 0)        \
      FLD(AMP_A, VOL_A,     3:0, "Amplitude",                Value, 0, 0)       \
    REG(0x09, AMP_B,         "Ch B amplitude")                                   \
      FLD(AMP_B, ENV_B,     4:4, "Envelope mode",            Flag, 0, 0)        \
      FLD(AMP_B, VOL_B,     3:0, "Amplitude",                Value, 0, 0)       \
    REG(0x0A, AMP_C,         "Ch C amplitude")                                   \
      FLD(AMP_C, ENV_C,     4:4, "Envelope mode",            Flag, 0, 0)        \
      FLD(AMP_C, VOL_C,     3:0, "Amplitude",                Value, 0, 0)       \
    REG(0x0B, ENV_FINE,      "Envelope period fine")                             \
    REG(0x0C, ENV_COARSE,    "Envelope period coarse")                           \
    REG(0x0D, ENV_SHAPE,     "Envelope shape/cycle")                             \
      FLD(ENV_SHAPE, CONT,  3:3, "Continue",                 Flag, 0, 0)        \
      FLD(ENV_SHAPE, ATT,   2:2, "Attack",                   Flag, 0, 0)        \
      FLD(ENV_SHAPE, ALT,   1:1, "Alternate",                Flag, 0, 0)        \
      FLD(ENV_SHAPE, HOLD,  0:0, "Hold",                     Flag, 0, 0)        \
    REG(0x0E, IO_PORT_A,     "I/O port A data")                                  \
    REG(0x0F, IO_PORT_B,     "I/O port B data")

namespace ay_regs {
    AY_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    constexpr uint8_t REG_COUNT = 16;
} // namespace ay_regs

DECL_EXTRACT(AY, AY_DECL)

// ============================================================================
// AY-3-8910 Sound Chip
// ============================================================================

class ay_3_8910_t : public SoundChipBase {
public:
    explicit ay_3_8910_t(AYVariant variant = AYVariant::AY_3_8910)
        : SoundChipBase(ChipInfo(
              ay_variant_traits[static_cast<int>(variant)].part_number,
              ay_variant_traits[static_cast<int>(variant)].manufacturer))
        , variant_(variant)
    {
        init_regs(ay_regs::REG_COUNT);
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void init() {
        std::memset(regs_, 0, num_regs_);
        latch_addr_ = 0;
        for (auto& ch : tone_counter_) ch = 0;
        for (auto& ch : tone_output_)  ch = 0;
        noise_counter_ = 0;
        noise_shift_ = 1;  // LFSR seed
        env_counter_ = 0;
        env_step_ = 0;
        env_holding_ = false;
        io_port_a_ = 0xFF;
        io_port_b_ = 0xFF;
    }

    void reset() { init(); }

    // === Register access (address latch + read/write) ===

    void latch_address(uint8_t addr) {
        latch_addr_ = addr & 0x0F;
    }

    void write_register(uint8_t data) {
        regs_[latch_addr_] = data;
        if (latch_addr_ == ay_regs::ENV_SHAPE) {
            // Writing envelope shape resets the envelope counter
            env_step_ = 0;
            env_counter_ = 0;
            env_holding_ = false;
        }
    }

    uint8_t read_register() const {
        if (latch_addr_ == ay_regs::IO_PORT_A) return io_port_a_;
        if (latch_addr_ == ay_regs::IO_PORT_B) return io_port_b_;
        return regs_[latch_addr_];
    }

    // === I/O port access (directly from system, not through register bus) ===

    void set_io_port_a(uint8_t data) { io_port_a_ = data; }
    void set_io_port_b(uint8_t data) { io_port_b_ = data; }
    uint8_t get_io_port_a() const { return io_port_a_; }
    uint8_t get_io_port_b() const { return io_port_b_; }

    // === Audio generation ===

    /// Tick at the AY clock rate (typically 1.7734 MHz or CPU_CLK/2).
    /// Generates one sample step.  Call at the PSG clock rate.
    void tick() {
        // TODO: Full audio generation implementation
        // Tone counters, noise LFSR, envelope generator, mixer, DAC
    }

    /// Get mixed mono sample (float, -1.0 to +1.0)
    float get_sample() const {
        // TODO: DAC table lookup + channel mixing
        return 0.0f;
    }

    AYVariant variant() const { return variant_; }

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    AYVariant variant_;

    uint8_t   latch_addr_ = 0;

    // Tone generators (3 channels)
    uint16_t  tone_counter_[3]{};
    uint8_t   tone_output_[3]{};

    // Noise generator (17-bit LFSR)
    uint16_t  noise_counter_ = 0;
    uint32_t  noise_shift_ = 1;

    // Envelope generator
    uint16_t  env_counter_ = 0;
    uint8_t   env_step_ = 0;
    bool      env_holding_ = false;

    // I/O ports
    uint8_t   io_port_a_ = 0xFF;
    uint8_t   io_port_b_ = 0xFF;

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};
