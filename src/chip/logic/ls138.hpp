#pragma once
// ============================================================================
// ls138.hpp — 74LS138 3-to-8 Line Decoder/Demultiplexer
// ============================================================================
//
// The 74LS138 is a high-speed 1-of-8 decoder/demultiplexer in a 16-pin DIP.
//
// Three binary select inputs (A, B, C) choose one of eight active-low
// outputs (Y0–Y7).  Three enable inputs (/E1, /E2, E3) must all be active
// for decoding: /E1=LOW, /E2=LOW, E3=HIGH.  When disabled, all outputs
// are HIGH (inactive).
//
// Truth table (when enabled):
//   C B A | Active output
//   0 0 0 | Y0 = LOW
//   0 0 1 | Y1 = LOW
//   0 1 0 | Y2 = LOW
//   0 1 1 | Y3 = LOW
//   1 0 0 | Y4 = LOW
//   1 0 1 | Y5 = LOW
//   1 1 0 | Y6 = LOW
//   1 1 1 | Y7 = LOW
//
// Purely combinational — no clock, no state beyond current input levels.
// Commonly used for address decoding in 6502/Z80 systems (VIC-20, BBC
// Micro, many arcade boards).
//
// Header-only — stateless combinational logic, suitable for inline use.
// ============================================================================

#include "chip/logic/logic_chip_base.hpp"
#include "core/chip_debug_registry.hpp"
#include <cstdint>

// ── DECL register map ───────────────────────────────────────────────────
// Two registers: input pins and output pins (snapshot of last evaluation).
#define LS138_DECL(REG, FLD, CMP) \
    REG(0x00, INPUTS,  "Input pins")                                            \
      FLD(INPUTS, A,     0:0, "Select A (pin 1)",       Flag, 0, 0)             \
      FLD(INPUTS, B,     1:1, "Select B (pin 2)",       Flag, 0, 0)             \
      FLD(INPUTS, C,     2:2, "Select C (pin 3)",       Flag, 0, 0)             \
      FLD(INPUTS, _E1,   3:3, "_E1 enable (pin 4)",     Flag, 0, 0)             \
      FLD(INPUTS, _E2,   4:4, "_E2 enable (pin 5)",     Flag, 0, 0)             \
      FLD(INPUTS, E3,    5:5, "E3 enable (pin 6)",      Flag, 0, 0)             \
    REG(0x01, OUTPUTS, "Output pins _Y0.._Y7")                                  \
      FLD(OUTPUTS, _Y0,  0:0, "_Y0 output (pin 15)",    Flag, 0, 0)             \
      FLD(OUTPUTS, _Y1,  1:1, "_Y1 output (pin 14)",    Flag, 0, 0)             \
      FLD(OUTPUTS, _Y2,  2:2, "_Y2 output (pin 13)",    Flag, 0, 0)             \
      FLD(OUTPUTS, _Y3,  3:3, "_Y3 output (pin 12)",    Flag, 0, 0)             \
      FLD(OUTPUTS, _Y4,  4:4, "_Y4 output (pin 11)",    Flag, 0, 0)             \
      FLD(OUTPUTS, _Y5,  5:5, "_Y5 output (pin 10)",    Flag, 0, 0)             \
      FLD(OUTPUTS, _Y6,  6:6, "_Y6 output (pin 9)",     Flag, 0, 0)             \
      FLD(OUTPUTS, _Y7,  7:7, "_Y7 output (pin 7)",     Flag, 0, 0)

namespace ls138 { namespace reg {
LS138_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} } // namespace ls138::reg

DECL_EXTRACT(LS138, LS138_DECL)

class LS138 : public LogicChipBase {
public:
    LS138()
        : LogicChipBase(ChipInfo{"74LS138", "Texas Instruments",
                                 "74LS138 3-to-8 Line Decoder/Demultiplexer"}) {
        init_regs(LS138_NUM_REGS);
        reset();
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(LS138_REG_INFO);
        register_debug_fields();
#endif
    }
    ~LS138() override;

    // ── Core combinational evaluation ───────────────────────────────────
    //
    // decode() accepts a single byte matching the INPUTS register layout:
    //   bit 0: A   (select 0)
    //   bit 1: B   (select 1)
    //   bit 2: C   (select 2)
    //   bit 3: _E1 (enable, active-low)
    //   bit 4: _E2 (enable, active-low)
    //   bit 5: E3  (enable, active-high)
    //
    // Returns the 8-bit OUTPUTS register (_Y7.._Y0, active-low).
    // All bits HIGH (0xFF) when disabled; exactly one bit LOW when enabled.

    uint8_t decode(uint8_t inputs) noexcept {
        inputs_ = inputs & 0x3F;
        regs_[ls138::reg::INPUTS] = inputs_;

        bool enabled = !(inputs & 0x08) && !(inputs & 0x10) && (inputs & 0x20);
        if (!enabled) {
            outputs_ = 0xFF;  // all outputs inactive (HIGH)
        } else {
            outputs_ = ~(1u << (inputs & 0x07));  // selected output LOW
        }
        regs_[ls138::reg::OUTPUTS] = outputs_;
        return outputs_;
    }

    // ── Output queries ──────────────────────────────────────────────────

    /// Decode and return selected output index (0–7), or -1 if disabled.
    /// Same as decode() but returns the select value directly.
    int decode_select(uint8_t inputs) noexcept {
        decode(inputs);
        return (outputs_ != 0xFF) ? (inputs & 0x07) : -1;
    }

    /// Return the full _Y7.._Y0 byte (active-low).
    uint8_t y_all()               const { return outputs_; }

    /// Return true if output Yn is active (LOW).
    bool    y_active(uint8_t n)   const { return !(outputs_ & (1u << (n & 7))); }

    /// Return the index of the single active output (0–7), or -1 if disabled.
    int     active_output()       const {
        if (outputs_ == 0xFF) return -1;
        // Exactly one bit is clear — find it
        for (int i = 0; i < 8; ++i)
            if (!(outputs_ & (1u << i))) return i;
        return -1;
    }

    // ── ChipBase overrides ──────────────────────────────────────────────

    void reset() override {
        inputs_  = 0;
        outputs_ = 0xFF;  // all outputs inactive
        if (num_regs_ >= LS138_NUM_REGS) {
            regs_[ls138::reg::INPUTS]  = inputs_;
            regs_[ls138::reg::OUTPUTS] = outputs_;
        }
    }

#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    uint8_t inputs_  = 0;
    uint8_t outputs_ = 0xFF;

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        debug_registry_
            .category("74LS138 — Inputs")
            .flag("A (Select 0)", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const LS138*>(c)->inputs_ & 0x01;
            })
            .flag("B (Select 1)", +[](const ChipBase* c) -> uint32_t {
                return (static_cast<const LS138*>(c)->inputs_ >> 1) & 1;
            })
            .flag("C (Select 2)", +[](const ChipBase* c) -> uint32_t {
                return (static_cast<const LS138*>(c)->inputs_ >> 2) & 1;
            })
            .flag("_E1", +[](const ChipBase* c) -> uint32_t {
                return (static_cast<const LS138*>(c)->inputs_ >> 3) & 1;
            })
            .flag("_E2", +[](const ChipBase* c) -> uint32_t {
                return (static_cast<const LS138*>(c)->inputs_ >> 4) & 1;
            })
            .flag("E3", +[](const ChipBase* c) -> uint32_t {
                return (static_cast<const LS138*>(c)->inputs_ >> 5) & 1;
            })

            .category("74LS138 — Outputs (active-low)")
            .value("Active Output", +[](const ChipBase* c) -> uint32_t {
                int out = static_cast<const LS138*>(c)->active_output();
                return out < 0 ? 0xFF : uint32_t(out);
            })
            .value("_Y7.._Y0", +[](const ChipBase* c) -> uint32_t {
                return static_cast<const LS138*>(c)->outputs_;
            });
    }
#endif
};
