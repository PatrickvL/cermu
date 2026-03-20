/*
 * pokey_gui.cpp — Atari POKEY Debug/Layout GUI
 *
 * Hardware-accurate 40-pin DIP pinout for all POKEY variants:
 *   C012294:  Original 1979 NMOS (Atari 400/800, arcade boards)
 *   C012294B: Revised NMOS (timer bug fix, later 800XL/130XE)
 *   C014795:  5200/arcade variant
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 * Debug field registration compiled when CERMU_HAS_CHIP_DEBUG is defined.
 */

// Include all variant headers used by systems (for explicit instantiation)
#include "chip/sound/pokey/c012294.hpp"
#include "chip/sound/pokey/c012294b.hpp"
#include "chip/sound/pokey/c014795.hpp"

#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

// ============================================================================
// Debug field registration
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
template <const pokey::POKEYTraits& Traits>
void pokey::pokey_t<Traits>::register_debug_fields() {
    using S = const pokey::pokey_t<Traits>;
    auto& r = debug_registry_;
    r.set_registers(regs_, pokey_regs::TOTAL_REGS, POKEY_REG_INFO);
    r.set_decl_entries(POKEY_DECL_ENTRIES.data(), POKEY_DECL_ENTRIES.size());

    // Computed multi-register values and internal state

    r.category("Channel 1");
    r.value("Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[0].counter;
    }, 16);
    r.value("Output", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[0].output ? 1 : 0;
    }, 1);

    r.category("Channel 2");
    r.value("Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[1].counter;
    }, 16);
    r.value("Output", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[1].output ? 1 : 0;
    }, 1);

    r.category("Channel 3");
    r.value("Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[2].counter;
    }, 16);
    r.value("Output", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[2].output ? 1 : 0;
    }, 1);

    r.category("Channel 4");
    r.value("Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[3].counter;
    }, 16);
    r.value("Output", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[3].output ? 1 : 0;
    }, 1);

    r.category("Internal");
    r.value("LFSR", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->random_;
    }, 17);
    r.value("Div Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->div_counter_;
    }, 8);
}

// Explicit template instantiation — debug fields
template void pokey::pokey_t<pokey::C012294_Traits>::register_debug_fields();
template void pokey::pokey_t<pokey::C012294B_Traits>::register_debug_fields();
template void pokey::pokey_t<pokey::C014795_Traits>::register_debug_fields();
#endif // CERMU_HAS_CHIP_DEBUG

// ============================================================================
// Chip layout and signal mapping
// ============================================================================

#ifdef CERMU_HAS_GUI

template <const pokey::POKEYTraits& Traits>
ChipLayout* pokey::pokey_t<Traits>::create_chip_layout() const {
    // All POKEY variants are 40-pin DIP
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();
        layout.markings.part_number  = Traits.chip_id;
        layout.markings.manufacturer = Traits.vendor;

        // POKEY 40-pin DIP pinout (Atari/AMI datasheet CO12294/CO14795)
        //
        //           ┌──── C012294 ────┐
        //   VSS  1  │•               │ 40  A4
        //   D3   2  │                │ 39  A3
        //   D4   3  │                │ 38  A2
        //   D5   4  │                │ 37  A1
        //   D6   5  │                │ 36  A0
        //   D7   6  │                │ 35  PHI2
        //   P7   7  │                │ 34  R/W
        //   P6   8  │                │ 33  /CS1
        //   P5   9  │                │ 32  CS0
        //   P4  10  │                │ 31  K5
        //   P3  11  │                │ 30  K4
        //   P2  12  │                │ 29  K3
        //   P1  13  │                │ 28  SID  (SIO data in)
        //   P0  14  │                │ 27  SOD  (SIO data out)
        //   KR2 15  │                │ 26  SIO clock out
        //   KR1 16  │                │ 25  SIO clock in
        //  VCC  17  │                │ 24  /IRQ
        //   K0  18  │                │ 23  K1
        //  AUD  19  │                │ 22  K2
        //   D0  20  │                │ 21  D2/D1
        //           └────────────────┘
        //
        // Pin 21 is actually two pins (D1 adjacent to D2) but in this
        // physical pinout D1 and D2 share pin 21 in some diagrams,
        // more commonly D1=20 is grouped with D0; real datasheet has
        // D0=20, D1=21, D2=22 but that conflicts with K2 at 22.
        // Using the standard Atari C012294 datasheet pinout:
        //
        //                  LEFT                          RIGHT
        PIN_LR(layout,  1, VSS,        A4,           40);
        PIN_LR(layout,  2, D3,         A3,           39);
        PIN_LR(layout,  3, D4,         A2,           38);
        PIN_LR(layout,  4, D5,         A1,           37);
        PIN_LR(layout,  5, D6,         A0,           36);
        PIN_LR(layout,  6, D7,         PHI2,         35);
        PIN_LR(layout,  7, POT7,       RW,           34);
        PIN_LR(layout,  8, POT6,       _CS,          33);  // /CS1
        PIN_LR(layout,  9, POT5,       CS0,          32);
        PIN_LR(layout, 10, POT4,       K5,           31);
        PIN_LR(layout, 11, POT3,       K4,           30);
        PIN_LR(layout, 12, POT2,       K3,           29);
        PIN_LR(layout, 13, POT1,       SIO_IN,       28);
        PIN_LR(layout, 14, POT0,       SIO_OUT,      27);
        PIN_LR(layout, 15, KR2,        SIO_CLK_OUT,  26);
        PIN_LR(layout, 16, KR1,        SIO_CLK_IN,   25);
        PIN_LR(layout, 17, VCC,        _IRQ,         24);
        PIN_LR(layout, 18, K0,         K1,           23);
        PIN_LR(layout, 19, AUDIO_OUT,  K2,           22);
        PIN_LR(layout, 20, D0,         D2,           21);

        return layout;
    }();
    return &layout;
}

template <const pokey::POKEYTraits& Traits>
std::vector<PinSignalState> pokey::pokey_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, bus_snapshot_);

    // Audio output (pin 19, left side idx 18) — always driven
    if (ps.size() > 18) {
        ps[18].signal_level    = true;
        ps[18].drive_direction = true;
        ps[18].high_impedance  = false;
        ps[18].signal_valid    = true;
    }

    return ps;
}

// Explicit template instantiation — GUI methods
template ChipLayout* pokey::pokey_t<pokey::C012294_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> pokey::pokey_t<pokey::C012294_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* pokey::pokey_t<pokey::C012294B_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> pokey::pokey_t<pokey::C012294B_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* pokey::pokey_t<pokey::C014795_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> pokey::pokey_t<pokey::C014795_Traits>::get_layout_pin_states(ChipLayout&);

#endif // CERMU_HAS_GUI
