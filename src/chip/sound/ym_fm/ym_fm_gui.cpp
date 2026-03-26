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

            // YM2151 / YM2612 / YM3526 / YM3812 24-pin DIP pinout
            // Pin assignments vary slightly per chip but share the same
            // structural pattern.  Using the YM2612 pinout as reference:
            //
            //           ┌──── YM2612 ────┐
            //   GND  1  │•              │ 24  VCC
            //   D0   2  │               │ 23  D1
            //   D2   3  │               │ 22  D3
            //   D4   4  │               │ 21  D5
            //   D6   5  │               │ 20  D7
            //  /RD   6  │               │ 19  /WR
            //   A0   7  │               │ 18  A1
            //  /CS   8  │               │ 17  /IC (reset)
            //  PHI_M 9  │               │ 16  /IRQ
            //   SH1 10  │               │ 15  SH2
            //   MO  11  │               │ 14  NC
            //   GND 12  │               │ 13  NC
            //           └───────────────┘
            //
            //                  LEFT                          RIGHT
            PIN_LR(layout,  1, VSS,        VCC,          24);
            PIN_LR(layout,  2, D0,         D1,           23);
            PIN_LR(layout,  3, D2,         D3,           22);
            PIN_LR(layout,  4, D4,         D5,           21);
            PIN_LR(layout,  5, D6,         D7,           20);
            PIN_LR(layout,  6, _RD,        _WR,          19);
            PIN_LR(layout,  7, A0,         A1,           18);
            PIN_LR(layout,  8, _CS,        _RES,         17);  // /IC = reset
            PIN_LR(layout,  9, CLK,        _IRQ,         16);  // PHI_M
            PIN_LR(layout, 10, NC,         NC,           15);  // SH1 / SH2
            PIN_LR(layout, 11, AUDIO_OUT,  NC,           14);  // MO (mixed output)
            PIN_LR(layout, 12, VSS,        NC,           13);

            return layout;
        }();
        return &layout;
    }

    // ---------- 40-pin DIP (YM2203 OPN) ----------
    if constexpr (Traits.pin_count == 40) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip40_layout();

            // YM2203 40-pin DIP pinout (Yamaha datasheet)
            //
            //           ┌──── YM2203 ────┐
            //   GND  1  │•              │ 40  VCC
            //   D0   2  │               │ 39  /RES
            //   D1   3  │               │ 38  PHI_M
            //   D2   4  │               │ 37  PHI_S (SSG clock)
            //   D3   5  │               │ 36  /IRQ
            //   D4   6  │               │ 35  NC
            //   D5   7  │               │ 34  IOB7
            //   D6   8  │               │ 33  IOB6
            //   D7   9  │               │ 32  IOB5
            //  /CS  10  │               │ 31  IOB4
            //  /RD  11  │               │ 30  IOB3
            //  /WR  12  │               │ 29  IOB2
            //   A0  13  │               │ 28  IOB1
            //   A1  14  │               │ 27  IOB0
            //   SH1 15  │               │ 26  IOA7
            //   SH2 16  │               │ 25  IOA6
            //   MO  17  │               │ 24  IOA5
            //   CH3 18  │               │ 23  IOA4
            //   SSG 19  │               │ 22  IOA3
            //   GND 20  │               │ 21  IOA2/1/0
            //           └───────────────┘
            //
            //                  LEFT                          RIGHT
            PIN_LR(layout,  1, VSS,        VCC,          40);
            PIN_LR(layout,  2, D0,         _RES,         39);
            PIN_LR(layout,  3, D1,         CLK,          38);  // PHI_M
            PIN_LR(layout,  4, D2,         NC,           37);  // PHI_S
            PIN_LR(layout,  5, D3,         _IRQ,         36);
            PIN_LR(layout,  6, D4,         NC,           35);
            PIN_LR(layout,  7, D5,         PB7,          34);  // IOB7
            PIN_LR(layout,  8, D6,         PB6,          33);
            PIN_LR(layout,  9, D7,         PB5,          32);
            PIN_LR(layout, 10, _CS,        PB4,          31);
            PIN_LR(layout, 11, _RD,        PB3,          30);
            PIN_LR(layout, 12, _WR,        PB2,          29);
            PIN_LR(layout, 13, A0,         PB1,          28);
            PIN_LR(layout, 14, A1,         PB0,          27);
            PIN_LR(layout, 15, NC,         PA7,          26);  // SH1
            PIN_LR(layout, 16, NC,         PA6,          25);  // SH2
            PIN_LR(layout, 17, AUDIO_OUT,  PA5,          24);  // MO
            PIN_LR(layout, 18, NC,         PA4,          23);  // CH3 out
            PIN_LR(layout, 19, CHANNEL_A,  PA3,          22);  // SSG out
            PIN_LR(layout, 20, VSS,        PA2,          21);

            return layout;
        }();
        return &layout;
    }

    // ---------- 18-pin DIP (YM2413 OPLL) ----------
    if constexpr (Traits.pin_count == 18) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip18_layout();

            // YM2413 18-pin DIP pinout (Yamaha datasheet)
            //
            //           ┌──── YM2413 ────┐
            //   D0   1  │•              │ 18  VCC
            //   D1   2  │               │ 17  D2
            //   D3   3  │               │ 16  D4
            //   D5   4  │               │ 15  D6
            //   D7   5  │               │ 14  /RES
            //  /CS   6  │               │ 13  A0
            //  /WR   7  │               │ 12  PHI_M
            //   MO   8  │               │ 11  /IC
            //   GND  9  │               │ 10  RO
            //           └───────────────┘
            //
            //                  LEFT                          RIGHT
            PIN_LR(layout,  1, D0,         VCC,          18);
            PIN_LR(layout,  2, D1,         D2,           17);
            PIN_LR(layout,  3, D3,         D4,           16);
            PIN_LR(layout,  4, D5,         D6,           15);
            PIN_LR(layout,  5, D7,         _RES,         14);
            PIN_LR(layout,  6, _CS,        A0,           13);
            PIN_LR(layout,  7, _WR,        CLK,          12);  // PHI_M
            PIN_LR(layout,  8, AUDIO_OUT,  NC,           11);  // MO / /IC
            PIN_LR(layout,  9, VSS,        NC,           10);  // RO (rhythm out)

            return layout;
        }();
        return &layout;
    }

    // ---------- 64-pin QFP (YM2608, YM2610) ----------
    if constexpr (Traits.pin_count == 64) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_qfp64_layout();

            // QFP64 — simplified pinout.  The full 64-pin QFP has pins on
            // all four sides.  We map the key signals for visualization;
            // the exact assignment varies between YM2608 and YM2610.
            //
            // Left side (pins 1-16):  D0-D7, control
            layout.left_pins.push_back(ChipPin PIN( 1, D0));
            layout.left_pins.push_back(ChipPin PIN( 2, D1));
            layout.left_pins.push_back(ChipPin PIN( 3, D2));
            layout.left_pins.push_back(ChipPin PIN( 4, D3));
            layout.left_pins.push_back(ChipPin PIN( 5, D4));
            layout.left_pins.push_back(ChipPin PIN( 6, D5));
            layout.left_pins.push_back(ChipPin PIN( 7, D6));
            layout.left_pins.push_back(ChipPin PIN( 8, D7));
            layout.left_pins.push_back(ChipPin PIN( 9, _CS));
            layout.left_pins.push_back(ChipPin PIN(10, _RD));
            layout.left_pins.push_back(ChipPin PIN(11, _WR));
            layout.left_pins.push_back(ChipPin PIN(12, A0));
            layout.left_pins.push_back(ChipPin PIN(13, A1));
            layout.left_pins.push_back(ChipPin PIN(14, _RES));
            layout.left_pins.push_back(ChipPin PIN(15, _IRQ));
            layout.left_pins.push_back(ChipPin PIN(16, CLK));

            // Bottom side (pins 17-32): I/O ports, SSG output
            for (uint8_t i = 17; i <= 24; i++)
                layout.bottom_pins.push_back(ChipPin PIN(i, NC));
            layout.bottom_pins.push_back(ChipPin PIN(25, CHANNEL_A));  // SSG out
            for (uint8_t i = 26; i <= 32; i++)
                layout.bottom_pins.push_back(ChipPin PIN(i, NC));

            // Right side (pins 33-48): audio outputs, ADPCM memory bus
            for (uint8_t i = 33; i <= 44; i++)
                layout.right_pins.push_back(ChipPin PIN(i, NC));
            layout.right_pins.push_back(ChipPin PIN(45, AUDIO_OUT));  // MO
            for (uint8_t i = 46; i <= 48; i++)
                layout.right_pins.push_back(ChipPin PIN(i, NC));

            // Top side (pins 49-64): power, ADPCM address bus
            layout.top_pins.push_back(ChipPin PIN(49, VCC));
            for (uint8_t i = 50; i <= 63; i++)
                layout.top_pins.push_back(ChipPin PIN(i, NC));
            layout.top_pins.push_back(ChipPin PIN(64, VSS));

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
