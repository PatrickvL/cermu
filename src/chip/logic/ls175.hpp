#pragma once
// ============================================================================
// ls175.hpp — 74LS175 Quad D-Type Flip-Flop with Clear
// ============================================================================
//
// The 74LS175 contains four edge-triggered D flip-flops with a common
// clock (CLK) and common active-low master reset (/MR), in a 16-pin DIP.
//
// Each flip-flop has:
//   - D input (data)
//   - Q output (true)
//   - /Q output (complement)
//
// On the rising edge of CLK, each Q latches its corresponding D input.
// When /MR is LOW, all Q outputs are forced LOW regardless of CLK.
//
// Pinout (16-pin DIP):
//   Pin  1: /MR (master reset, active-low)
//   Pin  2: Q0
//   Pin  3: /Q0
//   Pin  4: D0
//   Pin  5: D1
//   Pin  6: /Q1
//   Pin  7: Q1
//   Pin  8: GND
//   Pin  9: CLK
//   Pin 10: Q2
//   Pin 11: /Q2
//   Pin 12: D2
//   Pin 13: D3
//   Pin 14: /Q3
//   Pin 15: Q3
//   Pin 16: VCC
//
// In the C264 series, a 74LS175 at U21 latches ROM bank-select signals
// from address bits when writing to $FDD0–$FDDF.  The Q outputs drive
// ROM chip-select lines (C1LO, C1HI, C2LO, C2HI).
//
// Header-only — trivial latch logic, suitable for inline use.
// ============================================================================

#include "chip/logic/logic_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>

// ── DECL register map ───────────────────────────────────────────────────
// Two registers: D inputs and Q outputs (snapshot of last latch).
//
// D inputs byte:
//   bit 0: D0
//   bit 1: D1
//   bit 2: D2
//   bit 3: D3
//   bit 4: _MR (master reset, active-low)
//   bit 5: CLK (clock — edge-triggered)
//
// Q outputs byte:
//   bits 3:0 = Q3..Q0
//   bits 7:4 = /Q3../Q0
#define LS175_DECL(REG, FLD, CMP) \
    REG(0x00, INPUTS,  "Input pins")                                           \
      FLD(INPUTS, D0,    0:0, "D0 input (pin 4)",      Flag, 0, 0)             \
      FLD(INPUTS, D1,    1:1, "D1 input (pin 5)",      Flag, 0, 0)             \
      FLD(INPUTS, D2,    2:2, "D2 input (pin 12)",     Flag, 0, 0)             \
      FLD(INPUTS, D3,    3:3, "D3 input (pin 13)",     Flag, 0, 0)             \
      FLD(INPUTS, _MR,   4:4, "_MR reset (pin 1)",     Flag, 0, 0)             \
      FLD(INPUTS, CLK,   5:5, "CLK clock (pin 9)",     Flag, 0, 0)             \
    REG(0x01, OUTPUTS, "Output pins Q3..Q0 and /Q3../Q0")                      \
      FLD(OUTPUTS, Q0,   0:0, "Q0 output (pin 2)",     Flag, 0, 0)             \
      FLD(OUTPUTS, Q1,   1:1, "Q1 output (pin 7)",     Flag, 0, 0)             \
      FLD(OUTPUTS, Q2,   2:2, "Q2 output (pin 10)",    Flag, 0, 0)             \
      FLD(OUTPUTS, Q3,   3:3, "Q3 output (pin 15)",    Flag, 0, 0)             \
      FLD(OUTPUTS, _Q0,  4:4, "_Q0 complement (pin 3)",  Flag, 0, 0)           \
      FLD(OUTPUTS, _Q1,  5:5, "_Q1 complement (pin 6)",  Flag, 0, 0)           \
      FLD(OUTPUTS, _Q2,  6:6, "_Q2 complement (pin 11)", Flag, 0, 0)           \
      FLD(OUTPUTS, _Q3,  7:7, "_Q3 complement (pin 14)", Flag, 0, 0)

namespace ls175 { namespace reg {
LS175_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} } // namespace ls175::reg

DECL_EXTRACT(LS175, LS175_DECL)

class LS175 : public LogicChipBase {
public:
    LS175()
        : LogicChipBase(ChipInfo{"74LS175", "Texas Instruments",
                                 "74LS175 Quad D-Type Flip-Flop with Clear"}) {
        init_regs(LS175_NUM_REGS);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(LS175_REG_INFO);
        register_debug_fields();
#endif
    }
    ~LS175() override;

    // ── Core evaluation ─────────────────────────────────────────────────
    //
    // clock_pulse() latches D3..D0 into Q3..Q0 on the rising edge.
    // The caller is responsible for edge detection — call this once per
    // rising edge, not continuously.
    //
    // @param d_inputs  4-bit value (bits 3:0 = D3..D0)
    // @return          Q outputs (bits 3:0 = Q3..Q0)

    uint8_t clock_pulse(uint8_t d_inputs) noexcept {
        uint8_t d = d_inputs & 0x0F;
        regs_[ls175::reg::INPUTS]  = d | (1 << 4);  // _MR=1 (not asserted)
        regs_[ls175::reg::OUTPUTS] = d | ((~d & 0x0F) << 4);
        return d;
    }

    // ── Master reset (asynchronous, active-low) ─────────────────────────
    //
    // clear() forces all Q outputs LOW, regardless of clock state.

    void clear() noexcept {
        regs_[ls175::reg::INPUTS]  = (regs_[ls175::reg::INPUTS] & 0x0F);  // _MR=0 (asserted)
        regs_[ls175::reg::OUTPUTS] = 0xF0;  // Q all LOW, /Q all HIGH
    }

    // ── Output queries ──────────────────────────────────────────────────

    /// Return Q3..Q0 as a 4-bit value.
    uint8_t q_all()           const { return regs_[ls175::reg::OUTPUTS] & 0x0F; }

    /// Return individual Q output (0–3).
    bool    q(uint8_t n)      const { return (regs_[ls175::reg::OUTPUTS] >> (n & 3)) & 1; }

    /// Return complement /Q output (0–3).
    bool    q_not(uint8_t n)  const { return !q(n); }

    /// Return D3..D0 as last seen.
    uint8_t d_all()           const { return regs_[ls175::reg::INPUTS] & 0x0F; }

    // ── ChipBase overrides ──────────────────────────────────────────────

    void reset() override {
        if (num_regs_ >= LS175_NUM_REGS) {
            regs_[ls175::reg::INPUTS]  = 0;
            regs_[ls175::reg::OUTPUTS] = 0xF0;  // /Q all HIGH when Q all LOW
        }
    }

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        // All input/output flag fields are rendered by the DECL walk.
        debug_registry_
            .set_decl_entries(LS175_DECL_ENTRIES.data(), LS175_DECL_ENTRIES.size());
    }
#endif
};
