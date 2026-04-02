#pragma once
// ============================================================================
// ls139.hpp — 74LS139 Dual 2-to-4 Line Decoder/Demultiplexer
// ============================================================================
//
// The 74LS139 contains two independent 1-of-4 decoders in a 16-pin DIP.
//
// Each half has:
//   - 1 active-low enable input (/G)
//   - 2 binary select inputs (A, B)
//   - 4 active-low outputs (Y0–Y3)
//
// When /G is LOW (enabled), the selected output goes LOW; all others HIGH.
// When /G is HIGH (disabled), all outputs are HIGH (inactive).
//
// Truth table (one half, when /G = LOW):
//   B A | Active output
//   0 0 | Y0 = LOW
//   0 1 | Y1 = LOW
//   1 0 | Y2 = LOW
//   1 1 | Y3 = LOW
//
// Purely combinational — no clock, no state beyond current input levels.
// Used for I/O sub-page decoding in the C264 series (Plus/4, C16, C116),
// C64, and many other 6502/Z80 systems.
//
// Header-only — stateless combinational logic, suitable for inline use.
// ============================================================================

#include "chip/logic/logic_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>

// ── DECL register map ───────────────────────────────────────────────────
// Four registers: inputs and outputs for each half (a and b).
//
// Each half's input byte:
//   bit 0: A   (select 0)
//   bit 1: B   (select 1)
//   bit 2: _G  (enable, active-low)
//
// Each half's output byte:
//   bits 3:0 = _Y3.._Y0 (active-low)
#define LS139_DECL(REG, FLD, CMP) \
    REG(0x00, INPUTS_A,  "Half-A input pins")                                  \
      FLD(INPUTS_A, A_A,  0:0, "Select A (1A pin 2)",  Flag, 0, 0)             \
      FLD(INPUTS_A, B_A,  1:1, "Select B (1B pin 3)",  Flag, 0, 0)             \
      FLD(INPUTS_A, _G_A, 2:2, "_G enable (1G pin 1)", Flag, 0, 0)             \
    REG(0x01, OUTPUTS_A, "Half-A outputs _Y3.._Y0")                            \
      FLD(OUTPUTS_A, _Y0_A, 0:0, "1Y0 (pin 4)",  Flag, 0, 0)                   \
      FLD(OUTPUTS_A, _Y1_A, 1:1, "1Y1 (pin 5)",  Flag, 0, 0)                   \
      FLD(OUTPUTS_A, _Y2_A, 2:2, "1Y2 (pin 6)",  Flag, 0, 0)                   \
      FLD(OUTPUTS_A, _Y3_A, 3:3, "1Y3 (pin 7)",  Flag, 0, 0)                   \
    REG(0x02, INPUTS_B,  "Half-B input pins")                                  \
      FLD(INPUTS_B, A_B,  0:0, "Select A (2A pin 14)", Flag, 0, 0)             \
      FLD(INPUTS_B, B_B,  1:1, "Select B (2B pin 13)", Flag, 0, 0)             \
      FLD(INPUTS_B, _G_B, 2:2, "_G enable (2G pin 15)",Flag, 0, 0)             \
    REG(0x03, OUTPUTS_B, "Half-B outputs _Y3.._Y0")                            \
      FLD(OUTPUTS_B, _Y0_B, 0:0, "2Y0 (pin 12)", Flag, 0, 0)                   \
      FLD(OUTPUTS_B, _Y1_B, 1:1, "2Y1 (pin 11)", Flag, 0, 0)                   \
      FLD(OUTPUTS_B, _Y2_B, 2:2, "2Y2 (pin 10)", Flag, 0, 0)                   \
      FLD(OUTPUTS_B, _Y3_B, 3:3, "2Y3 (pin 9)",  Flag, 0, 0)

namespace ls139 { namespace reg {
LS139_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} } // namespace ls139::reg

DECL_EXTRACT(LS139, LS139_DECL)

class LS139 : public LogicChipBase {
public:
    LS139()
        : LogicChipBase(ChipInfo{"74LS139", "Texas Instruments",
                                 "74LS139 Dual 2-to-4 Line Decoder/Demultiplexer"}) {
        init_regs(LS139_NUM_REGS);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(LS139_REG_INFO);
        register_debug_fields();
#endif
    }
    ~LS139() override;

    // ── Core combinational evaluation ───────────────────────────────────
    //
    // Each decode method accepts a 3-bit input byte:
    //   bit 0: A   (select 0)
    //   bit 1: B   (select 1)
    //   bit 2: _G  (enable, active-low: 0 = enabled)
    //
    // Returns 4-bit output (_Y3.._Y0, active-low).
    // All bits HIGH (0x0F) when disabled; exactly one bit LOW when enabled.

    /// Decode half A.
    uint8_t decode_a(uint8_t inputs) noexcept {
        regs_[ls139::reg::INPUTS_A] = inputs & 0x07;

        if (inputs & 0x04) {  // _G HIGH → disabled
            regs_[ls139::reg::OUTPUTS_A] = 0x0F;
        } else {
            regs_[ls139::reg::OUTPUTS_A] = ~(1u << (inputs & 0x03)) & 0x0F;
        }
        return regs_[ls139::reg::OUTPUTS_A];
    }

    /// Decode half B.
    uint8_t decode_b(uint8_t inputs) noexcept {
        regs_[ls139::reg::INPUTS_B] = inputs & 0x07;

        if (inputs & 0x04) {  // _G HIGH → disabled
            regs_[ls139::reg::OUTPUTS_B] = 0x0F;
        } else {
            regs_[ls139::reg::OUTPUTS_B] = ~(1u << (inputs & 0x03)) & 0x0F;
        }
        return regs_[ls139::reg::OUTPUTS_B];
    }

    /// Decode half A and return selected output index (0–3), or -1 if disabled.
    int decode_select_a(uint8_t inputs) noexcept {
        decode_a(inputs);
        return (regs_[ls139::reg::OUTPUTS_A] != 0x0F) ? (inputs & 0x03) : -1;
    }

    /// Decode half B and return selected output index (0–3), or -1 if disabled.
    int decode_select_b(uint8_t inputs) noexcept {
        decode_b(inputs);
        return (regs_[ls139::reg::OUTPUTS_B] != 0x0F) ? (inputs & 0x03) : -1;
    }

    // ── Output queries ──────────────────────────────────────────────────

    uint8_t ya_all()              const { return regs_[ls139::reg::OUTPUTS_A]; }
    uint8_t yb_all()              const { return regs_[ls139::reg::OUTPUTS_B]; }
    bool    ya_active(uint8_t n)  const { return !(regs_[ls139::reg::OUTPUTS_A] & (1u << (n & 3))); }
    bool    yb_active(uint8_t n)  const { return !(regs_[ls139::reg::OUTPUTS_B] & (1u << (n & 3))); }
    int     active_output_a()     const { return (regs_[ls139::reg::OUTPUTS_A] != 0x0F) ? (regs_[ls139::reg::INPUTS_A] & 0x03) : -1; }
    int     active_output_b()     const { return (regs_[ls139::reg::OUTPUTS_B] != 0x0F) ? (regs_[ls139::reg::INPUTS_B] & 0x03) : -1; }

    // ── ChipBase overrides ──────────────────────────────────────────────

    void reset() override {
        if (num_regs_ >= LS139_NUM_REGS) {
            regs_[ls139::reg::INPUTS_A]  = 0x04;  // _G = 1 → disabled at power-on
            regs_[ls139::reg::INPUTS_B]  = 0x04;
            regs_[ls139::reg::OUTPUTS_A] = 0x0F;  // all inactive
            regs_[ls139::reg::OUTPUTS_B] = 0x0F;
        }
    }

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        // Input/output flag fields are rendered by the DECL walk.
        debug_registry_
            .set_decl_entries(LS139_DECL_ENTRIES.data(), LS139_DECL_ENTRIES.size());

        debug_registry_
            .category("74LS139 — Derived")
            .value("Active Output A", +[](const ChipBase* c) -> uint32_t {
                int out = static_cast<const LS139*>(c)->active_output_a();
                return out < 0 ? 0xFF : uint32_t(out);
            })
            .value("Active Output B", +[](const ChipBase* c) -> uint32_t {
                int out = static_cast<const LS139*>(c)->active_output_b();
                return out < 0 ? 0xFF : uint32_t(out);
            });
    }
#endif
};
