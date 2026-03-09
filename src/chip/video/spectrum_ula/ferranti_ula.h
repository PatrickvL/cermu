#pragma once
/*
 * ferranti_ula.h — Ferranti ULA (Uncommitted Logic Array) for ZX Spectrum
 *
 * The ULA is a custom gate array that handles video generation, keyboard
 * scanning, tape I/O, and memory contention for the ZX Spectrum.  It is the
 * single most important chip (besides the Z80A) in the Spectrum architecture.
 *
 * ZX Spectrum 48K ULA (Ferranti 6C001E-6 / 6C001E-7):
 *   - 256×192 pixel bitmap display with 32×24 attribute cells (8×8 blocks)
 *   - 8 colors × 2 brightness levels = 15 unique colors + black
 *   - Border color register
 *   - Keyboard scanning (active-low, directly decoded into half-row port reads)
 *   - Tape input/output (MIC/EAR signals)
 *   - Memory contention: ULA stalls CPU when accessing lower 16KB during screen fetch
 *
 * I/O port $FE (active-low accent on A0):
 *   Write: border color (bits 2-0), MIC (bit 3), EAR/speaker (bit 4)
 *   Read:  keyboard half-row (bits 4-0), EAR input (bit 6)
 *
 * Video timing (48K, NTSC not applicable — Spectrum is PAL/50Hz only):
 *   312 scanlines × 448 T-states per line = 69888 T-states per frame
 *   224 T-states per scanline visible (32 char columns × 8 pixels × 2 T-states)
 *
 * This is a system-specific chip living in src/chip/video/ because the ULA
 * is the defining component of multiple Spectrum variants (48K, 128K, +2, +3).
 */

#include "../video_chip_base.h"
#include "../../core/system_lines.h"
#include <cstdint>
#include <cstring>

// ============================================================================
// Spectrum ULA UNIFIED DECLARATION TABLE — single source of truth
// ============================================================================
//
//   REG(offset, symbol, description)
//   FLD(reg_sym, field_sym, hi:lo, description, kind, display_shift, display_scale)
//   CMP(symbol, description, kind, total_bits, display_shift, display_scale,
//       reg1, hilo1, dst1, reg2, hilo2, dst2)

#define SPECTRUM_ULA_DECL(REG, FLD, CMP) \
    REG(0x00, PORT_FE, "Border/speaker/mic I/O")                               \
      FLD(PORT_FE, BORDER, 2:0, "Border color",     Color, 0, 0)              \
      FLD(PORT_FE, MIC,    3:3, "MIC output",       Flag, 0, 0)               \
      FLD(PORT_FE, EAR,    4:4, "EAR/speaker out",  Flag, 0, 0)

namespace spectrum_ula {

    // Register constants from DECL
    SPECTRUM_ULA_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
    inline constexpr uint8_t REG_COUNT = 1;

    // Display dimensions
    inline constexpr int SCREEN_WIDTH      = 256;
    inline constexpr int SCREEN_HEIGHT     = 192;
    inline constexpr int BORDER_LEFT       = 48;
    inline constexpr int BORDER_RIGHT      = 48;
    inline constexpr int BORDER_TOP        = 48;
    inline constexpr int BORDER_BOTTOM     = 56;
    inline constexpr int TOTAL_WIDTH       = BORDER_LEFT + SCREEN_WIDTH + BORDER_RIGHT;   // 352
    inline constexpr int TOTAL_HEIGHT      = BORDER_TOP + SCREEN_HEIGHT + BORDER_BOTTOM;  // 296

    // Timing (T-states)
    inline constexpr int TSTATES_PER_LINE  = 224;
    inline constexpr int LINES_PER_FRAME   = 312;
    inline constexpr int TSTATES_PER_FRAME = TSTATES_PER_LINE * LINES_PER_FRAME;  // 69888

    // Color palette (GRB → RGB conversion, 15 colors + black)
    // 8 normal + 8 bright, with bright black = normal black
    inline constexpr uint32_t PALETTE[16] = {
        0xFF000000,  // Black (normal)
        0xFF0000CD,  // Blue
        0xFFCD0000,  // Red
        0xFFCD00CD,  // Magenta
        0xFF00CD00,  // Green
        0xFF00CDCD,  // Cyan
        0xFFCDCD00,  // Yellow
        0xFFCDCDCD,  // White (normal)
        0xFF000000,  // Black (bright) — same as normal
        0xFF0000FF,  // Bright Blue
        0xFFFF0000,  // Bright Red
        0xFFFF00FF,  // Bright Magenta
        0xFF00FF00,  // Bright Green
        0xFF00FFFF,  // Bright Cyan
        0xFFFFFF00,  // Bright Yellow
        0xFFFFFFFF,  // Bright White
    };

    // I/O port $FE bit positions
    inline constexpr uint8_t BORDER_MASK   = 0x07;  // Bits 2-0: border color
    inline constexpr uint8_t MIC_BIT       = 0x08;  // Bit 3: MIC output
    inline constexpr uint8_t EAR_BIT       = 0x10;  // Bit 4: EAR/speaker output
    inline constexpr uint8_t EAR_IN_BIT    = 0x40;  // Bit 6: EAR input (read)

} // namespace spectrum_ula

DECL_EXTRACT_ALL(SPECTRUM_ULA, SPECTRUM_ULA_DECL)

// ============================================================================
// Ferranti ULA Chip
// ============================================================================

class ferranti_ula_t : public VideoChipBase {
public:
    ferranti_ula_t()
        : VideoChipBase(ChipInfo("6C001E-7", "Ferranti"))
    {
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

    void init() {
        border_color_ = 7;  // White border at startup
        flash_state_ = false;
        flash_counter_ = 0;
        frame_counter_ = 0;
        t_state_ = 0;
        scanline_ = 0;
        ear_output_ = false;
        mic_output_ = false;
        ear_input_ = false;
        frame_int_pending_ = false;
        std::memset(keyboard_state_, 0xFF, sizeof(keyboard_state_));
        regs_[spectrum_ula::PORT_FE] = 0x07;  // White border
    }

    void reset() { init(); }

    // === I/O port access ===

    /// Write to ULA I/O port ($FE)
    void write_port_fe(uint8_t data) {
        regs_[spectrum_ula::PORT_FE] = data;
        border_color_ = data & spectrum_ula::BORDER_MASK;
        mic_output_   = (data & spectrum_ula::MIC_BIT) != 0;
        ear_output_   = (data & spectrum_ula::EAR_BIT) != 0;
    }

    /// Read from ULA I/O port ($FE), given the high byte of the address for row select
    uint8_t read_port_fe(uint8_t addr_high) const {
        // Each bit in addr_high selects a keyboard half-row (active-low)
        uint8_t result = 0xFF;
        for (int row = 0; row < 8; ++row) {
            if (!(addr_high & (1 << row))) {
                result &= keyboard_state_[row];
            }
        }
        // Bit 6: EAR input
        if (ear_input_) result |= spectrum_ula::EAR_IN_BIT;
        else            result &= ~spectrum_ula::EAR_IN_BIT;
        // Bits 7 and 5 float high
        return (result & 0x5F) | 0xA0;
    }

    // === Keyboard ===

    /// Set keyboard half-row state (8 rows × 5 bits, active-low)
    void set_keyboard_row(int row, uint8_t state) {
        keyboard_state_[row & 7] = state;
    }

    // === Video generation ===

    /// Tick one T-state.  Advances video timing and memory contention state.
    void tick() {
        t_state_++;
        if (t_state_ >= spectrum_ula::TSTATES_PER_LINE) {
            t_state_ = 0;
            scanline_++;
            if (scanline_ >= spectrum_ula::LINES_PER_FRAME) {
                scanline_ = 0;
                frame_counter_++;
                frame_int_pending_ = true;
                // Flash toggle every 16 frames
                flash_counter_++;
                if (flash_counter_ >= 16) {
                    flash_counter_ = 0;
                    flash_state_ = !flash_state_;
                }
            }
        }
    }

    /// Is the ULA currently contending the bus? (CPU must wait)
    /// Contention occurs when ULA is fetching screen data from lower 16KB.
    bool is_contended() const {
        // Contention pattern: during active display area, specific T-state phases
        if (scanline_ < spectrum_ula::BORDER_TOP ||
            scanline_ >= spectrum_ula::BORDER_TOP + spectrum_ula::SCREEN_HEIGHT)
            return false;
        // TODO: Exact contention pattern (depends on 48K vs 128K)
        return false;
    }

    // === Tape I/O ===

    void set_ear_input(bool state) { ear_input_ = state; }
    bool get_ear_output() const { return ear_output_; }
    bool get_mic_output() const { return mic_output_; }

    // === Frame interrupt ===

    bool check_frame_interrupt() {
        bool pending = frame_int_pending_;
        frame_int_pending_ = false;
        return pending;
    }

    // === State queries ===

    uint8_t  border_color()  const { return border_color_; }
    bool     flash_state()   const { return flash_state_; }
    uint32_t frame_counter() const { return frame_counter_; }
    int      scanline()      const { return scanline_; }
    int      t_state_pos()   const { return t_state_; }

    // === ChipBase GUI virtuals ===
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    uint8_t   border_color_ = 7;
    bool      flash_state_ = false;
    uint8_t   flash_counter_ = 0;
    uint32_t  frame_counter_ = 0;
    int       t_state_ = 0;
    int       scanline_ = 0;

    // Audio I/O
    bool      ear_output_ = false;
    bool      mic_output_ = false;
    bool      ear_input_ = false;

    // Frame interrupt
    bool      frame_int_pending_ = false;

    // Keyboard matrix (8 half-rows × 5 keys, active-low)
    uint8_t   keyboard_state_[8]{};

    // Register mirror
    uint8_t   regs_[spectrum_ula::REG_COUNT]{};

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        using S = const ferranti_ula_t;
        auto& r = debug_registry_;
        r.set_registers(regs_, spectrum_ula::REG_COUNT, SPECTRUM_ULA_REG_INFO);
        r.set_decl_order(SPECTRUM_ULA_DECL_ORDER.data(), SPECTRUM_ULA_DECL_ORDER.size(),
                         SPECTRUM_ULA_FLD_INFO, SPECTRUM_ULA_NUM_FIELDS,
                         nullptr, 0, nullptr);
        r.set_palette(spectrum_ula::PALETTE, 16);

        r.category("Port $FE Input");
        r.flag("EAR In", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->ear_input_;
        });

        r.category("Video Timing");
        r.value("Scanline", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->scanline_;
        }, 9);
        r.value("T-state", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->t_state_;
        }, 8);
        r.value("Frame", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->frame_counter_;
        }, 32);
        r.flag("Flash", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->flash_state_;
        });
        r.flag("INT Pending", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->frame_int_pending_;
        });
    }
#endif
};
