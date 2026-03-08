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
#ifdef CERMU_HAS_CHIP_DEBUG
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

    // Layout virtuals — defined in cd4021_gui.cpp (GUI builds only)
#ifdef CERMU_HAS_GUI
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
    uint8_t shift_register_ = 0;

#ifdef CERMU_HAS_CHIP_DEBUG
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
#endif
};
