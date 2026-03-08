#pragma once
/*
 * cd4021.h — CD4021 8-Bit Static Shift Register (PISO)
 *
 * Cross-system chip: used in NES/Famicom controllers, SNES controllers,
 * and various arcade boards.
 *
 * The CD4021 is a CMOS parallel-in/serial-out shift register.
 * When LATCH (pin 9, labeled P/S̅) goes HIGH, all 8 parallel input
 * pins (P0-P7) are latched into the shift register.  When LATCH
 * returns LOW, each rising edge of CLK (pin 10) shifts the register
 * one bit toward the serial output Q7 (pin 11).  Bits come out MSB
 * (P7) first.
 *
 * Pinout (16-pin DIP, per TI CD4021B datasheet SCHS054E):
 *   Pin 1:  P0   (parallel in 0)     Pin 16: VDD
 *   Pin 2:  Q6   (stage 6 out)       Pin 15: Q̅7   (complement out)
 *   Pin 3:  P4   (parallel in 4)     Pin 14: DS   (serial in)
 *   Pin 4:  P3   (parallel in 3)     Pin 13: P5   (parallel in 5)
 *   Pin 5:  P6   (parallel in 6)     Pin 12: P7   (parallel in 7)
 *   Pin 6:  P2   (parallel in 2)     Pin 11: Q7   (serial out)
 *   Pin 7:  P1   (parallel in 1)     Pin 10: CLK
 *   Pin 8:  VSS  (GND)               Pin  9: P/S̅  (LATCH)
 *
 * Header-only — no side effects, suitable for inline use.
 */

#include "../../core/chip.h"

#include <cstdint>
#include <cstring>

class CD4021 : public ChipBase {
public:
    CD4021()
        : ChipBase(ChipInfo{"CD4021", "8-Bit Static Shift Register", "Texas Instruments"}) {
        reset();
#ifdef CERMU_HAS_GUI
        register_debug_fields();
#endif
    }

    // ---------------------------------------------------------------
    // Parallel load: sample all 8 input pins into the shift register.
    // Called when LATCH (P/S̅) transitions HIGH.
    // ---------------------------------------------------------------
    void latch(uint8_t parallel_data) {
        shift_register_ = parallel_data;
    }

    // ---------------------------------------------------------------
    // Serial read: return the MSB (Q7) and shift left by one.
    // Called on each rising edge of CLK while LATCH is LOW.
    // The serial input (DS, pin 14) is tied LOW in NES controllers,
    // so shifted-in bits are always 0.
    // ---------------------------------------------------------------
    uint8_t shift_out() {
        uint8_t bit = (shift_register_ & 0x80) >> 7;
        shift_register_ <<= 1;
        // DS (serial in) is grounded → shifted-in bit is 0
        return bit;
    }

    // ---------------------------------------------------------------
    // Direct register access (for debug/save-state)
    // ---------------------------------------------------------------
    uint8_t get_shift_register() const { return shift_register_; }
    void    set_shift_register(uint8_t val) { shift_register_ = val; }

    // ---------------------------------------------------------------
    // ChipBase overrides
    // ---------------------------------------------------------------
    void reset() {
        shift_register_ = 0;
    }

    // GUI virtuals are in cd4021_gui.cpp (#ifdef CERMU_HAS_GUI)
    bool has_layout_content() const override;
    void render_layout_content() override;

private:
    uint8_t shift_register_ = 0;

    void register_debug_fields() {
        using CD = const CD4021;
        static const char* const bit_labels[] = {
            "P7", "P6", "P5", "P4", "P3", "P2", "P1", "P0"
        };
        debug_registry_
            .category("CD4021B Shift Register")
            .value("Shift Register", +[](const ChipBase* c) -> uint32_t { return static_cast<CD*>(c)->shift_register_; })
            .bitfield("Bits", +[](const ChipBase* c) -> uint32_t { return static_cast<CD*>(c)->shift_register_; }, 8, bit_labels);
    }
};
