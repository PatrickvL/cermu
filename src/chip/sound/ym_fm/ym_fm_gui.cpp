/*
 * ym_fm_gui.cpp — Yamaha FM chip family Debug/Layout GUI
 *
 * Hardware-accurate pinouts for all supported YM FM variants:
 *   YM2151 (OPM):   24-pin DIP
 *   YM2203 (OPN):   40-pin DIP
 *   YM2413 (OPLL):  18-pin DIP
 *   YM2608 (OPNA):  64-pin QFP
 *   YM2610 (OPNB):  64-pin QFP
 *   YM2612 (OPN2):  24-pin DIP
 *   YM3438 (OPN2C): 24-pin DIP
 *   YM3526 (OPL):   24-pin DIP
 *   YM3812 (OPL2):  24-pin DIP
 *
 * SHORTCOMINGS:
 *   - QFP64 pinout (YM2608, YM2610) is a simplified placeholder; the real
 *     64-pin package has pins on all four sides with complex signal grouping.
 *   - Debug fields show per-channel F-Num/Block from live state but do not
 *     expose per-operator envelope level, phase, or output — the most useful
 *     debug data for FM synthesis work.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 * Debug field registration compiled when CERMU_HAS_CHIP_DEBUG is defined.
 */

// Include all variant headers (for explicit template instantiation)
#include "chip/sound/ym_fm/ym2151.hpp"
#include "chip/sound/ym_fm/ym2203.hpp"
#include "chip/sound/ym_fm/ym2413.hpp"
#include "chip/sound/ym_fm/ym2608.hpp"
#include "chip/sound/ym_fm/ym2610.hpp"
#include "chip/sound/ym_fm/ym2612.hpp"
#include "chip/sound/ym_fm/ym3438.hpp"
#include "chip/sound/ym_fm/ym3526.hpp"
#include "chip/sound/ym_fm/ym3812.hpp"

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
template <const YMTraits& Traits>
void ym_fm_t<Traits>::register_debug_fields() {
    using S = const ym_fm_t<Traits>;
    auto& r = debug_registry_;
    wire_debug_registers(YM_FM_REG_INFO);
    r.set_decl_entries(YM_FM_DECL_ENTRIES.data(), YM_FM_DECL_ENTRIES.size());

    // --- Status ---
    r.category("Status");
    r.value("Status Reg", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->status_;
    }, 8);

    // --- Timers ---
    r.category("Timers");
    r.value("Timer A", +[](const ChipBase* c) -> uint32_t {
        auto* s = static_cast<S*>(c);
        uint16_t ta = (static_cast<uint16_t>(s->regs_[ym_fm::reg::TIMER_A_H_REG]) << 2)
                    | (s->regs_[ym_fm::reg::TIMER_A_L_REG] & 0x03);
        return ta;
    }, 10);
    r.value("Timer B", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->regs_[ym_fm::reg::TIMER_B_REG];
    }, 8);

    // --- FM Channels (summary: F-Num/Block from registers) ---
    // Per-channel frequency/algorithm are in the hardware register file,
    // visible through the DECL walk.  Add combined values here.
    r.category("FM Ch 1");
    r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[0].fnum;
    }, 11);
    r.value("Block", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[0].block;
    }, 3);

    r.category("FM Ch 2");
    r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[1].fnum;
    }, 11);
    r.value("Block", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->channel_[1].block;
    }, 3);

    if constexpr (Traits.fm_channels >= 3) {
        r.category("FM Ch 3");
        r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[2].fnum;
        }, 11);
        r.value("Block", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[2].block;
        }, 3);
    }

    if constexpr (Traits.fm_channels >= 4) {
        r.category("FM Ch 4");
        r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[3].fnum;
        }, 11);
        r.value("Block", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[3].block;
        }, 3);
    }

    if constexpr (Traits.fm_channels >= 6) {
        r.category("FM Ch 5");
        r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[4].fnum;
        }, 11);
        r.value("Block", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[4].block;
        }, 3);

        r.category("FM Ch 6");
        r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[5].fnum;
        }, 11);
        r.value("Block", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[5].block;
        }, 3);
    }

    if constexpr (Traits.fm_channels >= 8) {
        r.category("FM Ch 7");
        r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[6].fnum;
        }, 11);

        r.category("FM Ch 8");
        r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[7].fnum;
        }, 11);
    }

    if constexpr (Traits.fm_channels >= 9) {
        r.category("FM Ch 9");
        r.value("F-Num", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->channel_[8].fnum;
        }, 11);
    }

    // --- DAC (OPN2 only) ---
    if constexpr (Traits.has_dac) {
        r.category("DAC");
        r.value("DAC Value", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->regs_[ym_fm::reg::DAC_DATA_REG];
        }, 8);
        r.flag("DAC Enabled", +[](const ChipBase* c) -> uint32_t {
            return (static_cast<S*>(c)->regs_[ym_fm::reg::DAC_EN_REG] & 0x80) ? 1 : 0;
        });
    }

    // --- LFO (if present) ---
    if constexpr (Traits.has_lfo) {
        r.category("LFO");
        r.value("LFO Rate", +[](const ChipBase* c) -> uint32_t {
            return static_cast<S*>(c)->regs_[ym_fm::reg::LFO_REG] & 0x07;
        }, 3);
        r.flag("LFO Enable", +[](const ChipBase* c) -> uint32_t {
            return (static_cast<S*>(c)->regs_[ym_fm::reg::LFO_REG] & 0x08) ? 1 : 0;
        });
    }
}

// Explicit template instantiation — debug fields
template void ym_fm_t<YM2151_Traits>::register_debug_fields();
template void ym_fm_t<YM2203_Traits>::register_debug_fields();
template void ym_fm_t<YM2413_Traits>::register_debug_fields();
template void ym_fm_t<YM2608_Traits>::register_debug_fields();
template void ym_fm_t<YM2610_Traits>::register_debug_fields();
template void ym_fm_t<YM2612_Traits>::register_debug_fields();
template void ym_fm_t<YM3438_Traits>::register_debug_fields();
template void ym_fm_t<YM3526_Traits>::register_debug_fields();
template void ym_fm_t<YM3812_Traits>::register_debug_fields();
#endif // CERMU_HAS_CHIP_DEBUG

// ============================================================================
// Chip layout and signal mapping
// ============================================================================

#ifdef CERMU_HAS_GUI

// --- 24-pin DIP layout (YM2151, YM2612, YM3438, YM3526, YM3812) ---

template <const YMTraits& Traits>
ChipLayout* ym_fm_t<Traits>::create_chip_layout() const {
    // ---------- 24-pin DIP (OPM, OPN2, OPL family) ----------
    if constexpr (Traits.pin_count == 24) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip24_layout();

            PIN_LR(layout,  1, GND,        VCC,          24);
            PIN_LR(layout,  2, D0,         D1,           23);
            PIN_LR(layout,  3, D2,         D3,           22);
            PIN_LR(layout,  4, D4,         D5,           21);
            PIN_LR(layout,  5, D6,         D7,           20);
            PIN_LR(layout,  6, _RD,        _WR,          19);
            PIN_LR(layout,  7, A0,         A1,           18);
            PIN_LR(layout,  8, _CS,        _IC,          17);
            PIN_LR(layout,  9, PHI_M,      _IRQ,         16);
            PIN_LR(layout, 10, SH1,        SH2,          15);
            PIN_LR(layout, 11, MO,         NC,           14);
            PIN_LR(layout, 12, GND,        NC,           13);

            return layout;
        }();
        return &layout;
    }

    // ---------- 40-pin DIP (YM2203 OPN) ----------
    if constexpr (Traits.pin_count == 40) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip40_layout();

            PIN_LR(layout,  1, GND,        VCC,          40);
            PIN_LR(layout,  2, D0,         _RES,         39);
            PIN_LR(layout,  3, D1,         PHI_M,        38);
            PIN_LR(layout,  4, D2,         PHI_S,        37);
            PIN_LR(layout,  5, D3,         _IRQ,         36);
            PIN_LR(layout,  6, D4,         NC,           35);
            PIN_LR(layout,  7, D5,         PB7,          34);
            PIN_LR(layout,  8, D6,         PB6,          33);
            PIN_LR(layout,  9, D7,         PB5,          32);
            PIN_LR(layout, 10, _CS,        PB4,          31);
            PIN_LR(layout, 11, _RD,        PB3,          30);
            PIN_LR(layout, 12, _WR,        PB2,          29);
            PIN_LR(layout, 13, A0,         PB1,          28);
            PIN_LR(layout, 14, A1,         PB0,          27);
            PIN_LR(layout, 15, SH1,        PA7,          26);
            PIN_LR(layout, 16, SH2,        PA6,          25);
            PIN_LR(layout, 17, MO,         PA5,          24);
            PIN_LR(layout, 18, CH3_OUT,    PA4,          23);
            PIN_LR(layout, 19, SSG_OUT,    PA3,          22);
            PIN_LR(layout, 20, GND,        PA2,          21);

            return layout;
        }();
        return &layout;
    }

    // ---------- 18-pin DIP (YM2413 OPLL) ----------
    if constexpr (Traits.pin_count == 18) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip18_layout();

            PIN_LR(layout,  1, D0,         VCC,          18);
            PIN_LR(layout,  2, D1,         D2,           17);
            PIN_LR(layout,  3, D3,         D4,           16);
            PIN_LR(layout,  4, D5,         D6,           15);
            PIN_LR(layout,  5, D7,         _RES,         14);
            PIN_LR(layout,  6, _CS,        A0,           13);
            PIN_LR(layout,  7, _WR,        PHI_M,        12);
            PIN_LR(layout,  8, MO,         _IC,          11);
            PIN_LR(layout,  9, GND,        RO,           10);

            return layout;
        }();
        return &layout;
    }

    // ---------- 64-pin QFP (YM2608, YM2610) ----------
    if constexpr (Traits.pin_count == 64) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_qfp64_layout();

            // QFP64 — simplified pinout; exact assignment
            // varies between YM2608 and YM2610.
            layout.left_pins.push_back(ChipPin CHIP_PIN( 1, D0));
            layout.left_pins.push_back(ChipPin CHIP_PIN( 2, D1));
            layout.left_pins.push_back(ChipPin CHIP_PIN( 3, D2));
            layout.left_pins.push_back(ChipPin CHIP_PIN( 4, D3));
            layout.left_pins.push_back(ChipPin CHIP_PIN( 5, D4));
            layout.left_pins.push_back(ChipPin CHIP_PIN( 6, D5));
            layout.left_pins.push_back(ChipPin CHIP_PIN( 7, D6));
            layout.left_pins.push_back(ChipPin CHIP_PIN( 8, D7));
            layout.left_pins.push_back(ChipPin CHIP_PIN( 9, _CS));
            layout.left_pins.push_back(ChipPin CHIP_PIN(10, _RD));
            layout.left_pins.push_back(ChipPin CHIP_PIN(11, _WR));
            layout.left_pins.push_back(ChipPin CHIP_PIN(12, A0));
            layout.left_pins.push_back(ChipPin CHIP_PIN(13, A1));
            layout.left_pins.push_back(ChipPin CHIP_PIN(14, _RES));
            layout.left_pins.push_back(ChipPin CHIP_PIN(15, _IRQ));
            layout.left_pins.push_back(ChipPin CHIP_PIN(16, PHI_M));

            // Bottom side (pins 17-32): I/O ports, SSG output
            for (uint8_t i = 17; i <= 24; i++)
                layout.bottom_pins.push_back(ChipPin CHIP_PIN(i, NC));
            layout.bottom_pins.push_back(ChipPin CHIP_PIN(25, SSG_OUT));
            for (uint8_t i = 26; i <= 32; i++)
                layout.bottom_pins.push_back(ChipPin CHIP_PIN(i, NC));

            // Right side (pins 33-48): audio outputs, ADPCM memory bus
            for (uint8_t i = 33; i <= 44; i++)
                layout.right_pins.push_back(ChipPin CHIP_PIN(i, NC));
            layout.right_pins.push_back(ChipPin CHIP_PIN(45, MO));
            for (uint8_t i = 46; i <= 48; i++)
                layout.right_pins.push_back(ChipPin CHIP_PIN(i, NC));

            // Top side (pins 49-64): power, ADPCM address bus
            layout.top_pins.push_back(ChipPin CHIP_PIN(49, VCC));
            for (uint8_t i = 50; i <= 63; i++)
                layout.top_pins.push_back(ChipPin CHIP_PIN(i, NC));
            layout.top_pins.push_back(ChipPin CHIP_PIN(64, VSS));

            return layout;
        }();
        return &layout;
    }

    return nullptr;
}

template <const YMTraits& Traits>
std::vector<PinSignalState> ym_fm_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, bus_snapshot_);

    // Find audio output pin and mark it as driven
    auto mark_audio = [&](size_t side_offset, size_t pin_idx) {
        size_t idx = side_offset + pin_idx;
        if (idx < ps.size()) {
            ps[idx].signal_level    = true;
            ps[idx].drive_direction = true;
            ps[idx].high_impedance  = false;
            ps[idx].signal_valid    = true;
        }
    };

    if constexpr (Traits.pin_count == 24) {
        // Audio out is pin 11 (left side idx 10)
        mark_audio(0, 10);
    } else if constexpr (Traits.pin_count == 40) {
        // Audio out is pin 17 (left side idx 16)
        mark_audio(0, 16);
    } else if constexpr (Traits.pin_count == 18) {
        // Audio out is pin 8 (left side idx 7)
        mark_audio(0, 7);
    }
    // QFP64: audio pin position varies — skip for now

    return ps;
}

// Explicit template instantiation — GUI methods
template ChipLayout* ym_fm_t<YM2151_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM2151_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ym_fm_t<YM2203_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM2203_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ym_fm_t<YM2413_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM2413_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ym_fm_t<YM2608_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM2608_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ym_fm_t<YM2610_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM2610_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ym_fm_t<YM2612_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM2612_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ym_fm_t<YM3438_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM3438_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ym_fm_t<YM3526_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM3526_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ym_fm_t<YM3812_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ym_fm_t<YM3812_Traits>::get_layout_pin_states(ChipLayout&);

#endif // CERMU_HAS_GUI
