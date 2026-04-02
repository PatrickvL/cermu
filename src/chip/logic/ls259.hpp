#pragma once
// ============================================================================
// ls259.hpp — 74LS259 8-Bit Addressable Latch
// ============================================================================
//
// The 74LS259 is a bipolar addressable latch in a 16-pin DIP.
//
// Address inputs A0-A2 select one of eight Q outputs (Q0-Q7).
// When /G (enable) is LOW and /CLR is HIGH, the selected Q is set to D.
// When /G is HIGH, the outputs are latched (transparent while /G is LOW,
// latched on /G rising edge — in arcade systems the CPU write strobe
// provides the /G pulse, so each write latches immediately).
// When /CLR (master reset) is LOW, all Q outputs are forced LOW.
//
// Used in many Atari arcade boards:
//   Battlezone / Red Baron : $1000-$1007 (Q5 = NMI enable)
//   Asteroids Deluxe       : $3C00-$3C07 (Q4 = NMI enable)
//
// Header-only — no side effects, suitable for inline use.
// ============================================================================

#include "chip/logic/logic_chip_base.hpp"
#include "core/chip_debug_registry.hpp"

#include <cstdint>

// ── DECL register map ───────────────────────────────────────────────────────
// One 8-bit register holding all Q outputs; individual bits are fields.
#define LS259_DECL(REG, FLD, CMP) \
    REG(0x00, Q_OUTPUTS, "Q7..Q0 latch outputs")                              \
    FLD(Q_OUTPUTS, Q0, 0:0, "Q0 output", Flag, 0, 0)                          \
    FLD(Q_OUTPUTS, Q1, 1:1, "Q1 output", Flag, 0, 0)                          \
    FLD(Q_OUTPUTS, Q2, 2:2, "Q2 output", Flag, 0, 0)                          \
    FLD(Q_OUTPUTS, Q3, 3:3, "Q3 output", Flag, 0, 0)                          \
    FLD(Q_OUTPUTS, Q4, 4:4, "Q4 output", Flag, 0, 0)                          \
    FLD(Q_OUTPUTS, Q5, 5:5, "Q5 output", Flag, 0, 0)                          \
    FLD(Q_OUTPUTS, Q6, 6:6, "Q6 output", Flag, 0, 0)                          \
    FLD(Q_OUTPUTS, Q7, 7:7, "Q7 output", Flag, 0, 0)

namespace ls259 { namespace reg {
LS259_DECL(DECL_X_CONST_, DECL_FLD_NOP, DECL_CMP_NOP)
} } // namespace ls259::reg

DECL_EXTRACT(LS259, LS259_DECL)

class LS259 : public LogicChipBase {
public:
    LS259()
        : LogicChipBase(ChipInfo{"74LS259", "Texas Instruments", "74LS259 8-Bit Addressable Latch"}) {
        init_regs(LS259_NUM_REGS);
#ifdef CERMU_HAS_CHIP_DEBUG
        wire_debug_registers(LS259_REG_INFO);
        register_debug_fields();
#endif
    }

    // ── Core interface ──────────────────────────────────────────────────
    //
    // write() models a CPU write strobe: the address low 3 bits select
    // which Q output to set, and D0 provides the value.
    //
    void write(uint16_t addr, uint8_t data) {
        uint8_t q_sel = addr & 0x07;
        uint8_t bit   = data & 0x01;
        if (bit)
            q_outputs_ |= (1u << q_sel);
        else
            q_outputs_ &= ~(1u << q_sel);
        regs_[ls259::reg::Q_OUTPUTS] = q_outputs_;
    }

    // ── Output queries ──────────────────────────────────────────────────

    /// Return the full Q7..Q0 byte.
    uint8_t q_all()               const { return q_outputs_; }

    /// Return a single Q output (0-7).
    bool    q(uint8_t n)          const { return (q_outputs_ >> (n & 7)) & 1; }

    // ── ChipBase overrides ──────────────────────────────────────────────

    void reset() override {
        q_outputs_ = 0x00;   // /CLR asserted: all Q outputs LOW
        regs_[ls259::reg::Q_OUTPUTS] = 0;
    }

    // Layout virtuals — defined in ls259_gui.cpp (GUI builds only)
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    uint8_t q_outputs_ = 0x00;

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields() {
        // All output flag fields are rendered by the DECL walk.
        debug_registry_
            .set_decl_entries(LS259_DECL_ENTRIES.data(), LS259_DECL_ENTRIES.size());
    }
#endif
};
