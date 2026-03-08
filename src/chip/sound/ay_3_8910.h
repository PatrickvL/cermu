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
 * Register map ($00-$0F):
 *   $00-$01: Channel A tone period (12-bit)
 *   $02-$03: Channel B tone period (12-bit)
 *   $04-$05: Channel C tone period (12-bit)
 *   $06:     Noise period (5-bit)
 *   $07:     Mixer control (tone/noise enable per channel, I/O direction)
 *   $08-$0A: Channel A/B/C amplitude (4-bit + envelope mode bit)
 *   $0B-$0C: Envelope period (16-bit)
 *   $0D:     Envelope shape
 *   $0E:     I/O Port A data
 *   $0F:     I/O Port B data
 *
 * Bus interface:
 *   BDIR + BC1 select the bus operation:
 *     00 = Inactive
 *     01 = Read register
 *     10 = Write register
 *     11 = Latch address
 */

#include "../../core/chip.h"
#include "../../core/system_lines.h"
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
// AY-3-8910 Register Addresses
// ============================================================================

namespace ay_regs {
    constexpr uint8_t TONE_A_FINE    = 0x00;
    constexpr uint8_t TONE_A_COARSE  = 0x01;
    constexpr uint8_t TONE_B_FINE    = 0x02;
    constexpr uint8_t TONE_B_COARSE  = 0x03;
    constexpr uint8_t TONE_C_FINE    = 0x04;
    constexpr uint8_t TONE_C_COARSE  = 0x05;
    constexpr uint8_t NOISE_PERIOD   = 0x06;
    constexpr uint8_t MIXER          = 0x07;
    constexpr uint8_t AMP_A          = 0x08;
    constexpr uint8_t AMP_B          = 0x09;
    constexpr uint8_t AMP_C          = 0x0A;
    constexpr uint8_t ENV_FINE       = 0x0B;
    constexpr uint8_t ENV_COARSE     = 0x0C;
    constexpr uint8_t ENV_SHAPE      = 0x0D;
    constexpr uint8_t IO_PORT_A      = 0x0E;
    constexpr uint8_t IO_PORT_B      = 0x0F;
    constexpr uint8_t REG_COUNT      = 16;
} // namespace ay_regs

// ============================================================================
// AY-3-8910 Sound Chip
// ============================================================================

class ay_3_8910_t : public ChipBase {
public:
    explicit ay_3_8910_t(AYVariant variant = AYVariant::AY_3_8910)
        : ChipBase(ChipInfo(
              ay_variant_traits[static_cast<int>(variant)].part_number,
              ay_variant_traits[static_cast<int>(variant)].manufacturer))
        , variant_(variant)
    {
        category_ = "Sound";
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void init() {
        std::memset(regs_, 0, sizeof(regs_));
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
    uint8_t   regs_[ay_regs::REG_COUNT]{};
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
