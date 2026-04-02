/*
 * pla_gui.cpp — MOS 906114-01 PLA Debug/Layout GUI
 *
 * 28-pin DIP pinout based on the C64 PLA dissection document
 * (http://skoe.de/docs/c64-dissected/pla/c64_pla_dissected_a4ss.pdf).
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/logic/pla.hpp"
#include "core/chip_layout.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase layout virtuals
// ============================================================================

ChipLayout* PLA906114::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip28_layout();

        layout.markings.custom_text  = "Programmable Logic Array";

        // Hardware-accurate PLA pinout (28-pin DIP)
        // I0-I15 = inputs, F0-F7 = outputs
        PIN_LR(layout,  1, NC,        VCC, 28);       // prog / +5V
        PIN_LR(layout,  2, A13,       A12, 27);       // I7 / I8
        PIN_LR(layout,  3, A14,       BA, 26);        // I6 / I9
        PIN_LR(layout,  4, A15,       _AEC, 25);      // I5 / I10
        PIN_LR(layout,  5, _VA14,     RW, 24);        // I4 / I11 (R/W)
        PIN_LR(layout,  6, _CHAREN,   _EXROM, 23);    // I3 / I12
        PIN_LR(layout,  7, _HIRAM,    _GAME, 22);     // I2 / I13
        PIN_LR(layout,  8, _LORAM,    VA13, 21);      // I1 / I14
        PIN_LR(layout,  9, _CAS,      VA12, 20);      // I0 / I15
        PIN_LR(layout, 10, _ROMH,     _CS, 19);       // F7 / chip enable
        PIN_LR(layout, 11, _ROML,     _CASRAM_PLA, 18); // F6 / F0
        PIN_LR(layout, 12, _IO,       _BASIC, 17);    // F5 / F1
        PIN_LR(layout, 13, GRW,       _KERNAL, 16);   // F4 (GR/W) / F2
        PIN_LR(layout, 14, VSS,       _CHAROM, 15);   // gnd / F3

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> PLA906114::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, bus_snapshot_);

    auto overlay = [&](const ChipPin& pin) {
        if (pin.pin_number == 0 || pin.pin_number > ps.size()) return;
        PinSignalState& s = ps[pin.pin_number - 1];

        switch (pin.label) {
            // Banking inputs — positive logic in PLA struct
            case PinLabel::_CHAREN:     s.signal_level = inputs_.n_charen; s.high_impedance = false; break;
            case PinLabel::_HIRAM:      s.signal_level = inputs_.n_hiram;  s.high_impedance = false; break;
            case PinLabel::_LORAM:      s.signal_level = inputs_.n_loram;  s.high_impedance = false; break;
            case PinLabel::_GAME:       s.signal_level = inputs_.n_game;   s.high_impedance = false; break;
            case PinLabel::_EXROM:      s.signal_level = inputs_.n_exrom;  s.high_impedance = false; break;

            // Other inputs
            case PinLabel::_VA14:       s.signal_level = !inputs_.n_va14;  s.high_impedance = false; break;
            case PinLabel::_CAS:        s.signal_level = !inputs_.n_cas;   s.high_impedance = false; break;
            case PinLabel::VA12:        s.signal_level = inputs_.va12;     s.high_impedance = false; break;
            case PinLabel::VA13:        s.signal_level = inputs_.va13;     s.high_impedance = false; break;
            case PinLabel::_CS:         s.signal_level = true;             s.high_impedance = false; break;

            // Output pins — active-low
            case PinLabel::_ROMH:       s.signal_level = !outputs_.n_romh;     s.drive_direction = true; s.high_impedance = false; break;
            case PinLabel::_ROML:       s.signal_level = !outputs_.n_roml;     s.drive_direction = true; s.high_impedance = false; break;
            case PinLabel::_IO:         s.signal_level = !outputs_.n_io;       s.drive_direction = true; s.high_impedance = false; break;
            case PinLabel::GRW:         s.signal_level = !outputs_.n_grw;      s.drive_direction = true; s.high_impedance = false; break;
            case PinLabel::_CHAROM:     s.signal_level = !outputs_.n_charrom;  s.drive_direction = true; s.high_impedance = false; break;
            case PinLabel::_KERNAL:     s.signal_level = !outputs_.n_kernal;   s.drive_direction = true; s.high_impedance = false; break;
            case PinLabel::_BASIC:      s.signal_level = !outputs_.n_basic;    s.drive_direction = true; s.high_impedance = false; break;
            case PinLabel::_CASRAM_PLA: s.signal_level = !outputs_.n_casram;   s.drive_direction = true; s.high_impedance = false; break;

            default: return;
        }
        s.signal_value = s.signal_level ? 1 : 0;
    };

    for (const auto& pin : layout.left_pins)  overlay(pin);
    for (const auto& pin : layout.right_pins) overlay(pin);

    return ps;
}

#endif // CERMU_HAS_GUI
