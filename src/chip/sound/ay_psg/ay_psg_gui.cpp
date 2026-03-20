/*
 * ay_psg_gui.cpp — AY/YM PSG family Debug/Layout GUI
 *
 * Hardware-accurate pinouts for all AY variants:
 *   AY-3-8910: 40-pin DIP (GI datasheet)
 *   AY-3-8912: 28-pin DIP
 *   AY-3-8913: 24-pin DIP
 *   YM2149:    40-pin DIP (Yamaha, pin-compatible with 8910)
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 * Debug field registration compiled when CERMU_HAS_CHIP_DEBUG is defined.
 */

// Include all variant headers used by systems (for explicit instantiation)
#include "chip/sound/ay_psg/ay_3_8910.hpp"
#include "chip/sound/ay_psg/ay_3_8912.hpp"
#include "chip/sound/ay_psg/ay_3_8913.hpp"
#include "chip/sound/ay_psg/ay_3_8914.hpp"
#include "chip/sound/ay_psg/ym2149.hpp"
#include "chip/sound/ay_psg/ym3439.hpp"
#include "chip/sound/ay_psg/ay8930.hpp"

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
template <const AYTraits& Traits>
void ay_psg_t<Traits>::register_debug_fields() {
    using S = const ay_psg_t<Traits>;
    auto& r = debug_registry_;
    r.set_registers(regs_, ay_regs::REG_COUNT, AY_REG_INFO);
    r.set_decl_entries(AY_DECL_ENTRIES.data(), AY_DECL_ENTRIES.size());

    // Mixer enables, amplitude/env-mode bits, and envelope shape are in the DECL walk.
    // Combined multi-register values (tone periods, envelope period) remain.

    r.category("Channel A");
    r.value("Tone Period", +[](const ChipBase* c) -> uint32_t {
        auto* s = static_cast<S*>(c);
        return s->regs_[ay_regs::TONE_A_FINE] | (uint16_t(s->regs_[ay_regs::TONE_A_COARSE] & 0x0F) << 8);
    }, 12);

    r.category("Channel B");
    r.value("Tone Period", +[](const ChipBase* c) -> uint32_t {
        auto* s = static_cast<S*>(c);
        return s->regs_[ay_regs::TONE_B_FINE] | (uint16_t(s->regs_[ay_regs::TONE_B_COARSE] & 0x0F) << 8);
    }, 12);

    r.category("Channel C");
    r.value("Tone Period", +[](const ChipBase* c) -> uint32_t {
        auto* s = static_cast<S*>(c);
        return s->regs_[ay_regs::TONE_C_FINE] | (uint16_t(s->regs_[ay_regs::TONE_C_COARSE] & 0x0F) << 8);
    }, 12);

    r.category("Envelope");
    r.value("Env Period", +[](const ChipBase* c) -> uint32_t {
        auto* s = static_cast<S*>(c);
        return s->regs_[ay_regs::ENV_FINE] | (uint16_t(s->regs_[ay_regs::ENV_COARSE]) << 8);
    }, 16);
}

// Explicit template instantiation — debug fields
template void ay_psg_t<AY_3_8910_Traits>::register_debug_fields();
template void ay_psg_t<AY_3_8912_Traits>::register_debug_fields();
template void ay_psg_t<AY_3_8913_Traits>::register_debug_fields();
template void ay_psg_t<AY_3_8914_Traits>::register_debug_fields();
template void ay_psg_t<YM2149_Traits>::register_debug_fields();
template void ay_psg_t<YM3439_Traits>::register_debug_fields();
template void ay_psg_t<AY8930_Traits>::register_debug_fields();
#endif // CERMU_HAS_CHIP_DEBUG

// ============================================================================
// Chip layout and signal mapping
// ============================================================================

#ifdef CERMU_HAS_GUI

template <const AYTraits& Traits>
ChipLayout* ay_psg_t<Traits>::create_chip_layout() const {
    // 40-pin DIP: AY-3-8910, AY-3-8914, YM2149, YM3439, AY8930
    if constexpr (Traits.io_port_count == 2) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip40_layout();
            layout.markings.part_number  = Traits.chip_id;
            layout.markings.manufacturer = Traits.vendor;

            // AY-3-8910 40-pin DIP pinout (GI datasheet)
            //                  LEFT                          RIGHT
            PIN_LR(layout,  1, VSS,        VCC,          40);
            PIN_LR(layout,  2, NC,         NC,           39);  // TEST2
            PIN_LR(layout,  3, CHANNEL_B,  NC,           38);  // TEST1
            PIN_LR(layout,  4, CHANNEL_A,  CHANNEL_C,    37);
            PIN_LR(layout,  5, NC,         D7,           36);
            PIN_LR(layout,  6, PB7,        D6,           35);
            PIN_LR(layout,  7, PB6,        D5,           34);
            PIN_LR(layout,  8, PB5,        D4,           33);
            PIN_LR(layout,  9, PB4,        D3,           32);
            PIN_LR(layout, 10, PB3,        D2,           31);
            PIN_LR(layout, 11, PB2,        D1,           30);
            PIN_LR(layout, 12, PB1,        D0,           29);
            PIN_LR(layout, 13, PB0,        BDIR,         28);
            PIN_LR(layout, 14, PA7,        BC2,          27);
            PIN_LR(layout, 15, PA6,        BC1,          26);
            PIN_LR(layout, 16, PA5,        _CS,          25);  // /A9
            PIN_LR(layout, 17, PA4,        A8,           24);
            PIN_LR(layout, 18, PA3,        _RES,         23);
            PIN_LR(layout, 19, PA2,        CLK,          22);
            PIN_LR(layout, 20, PA1,        PA0,          21);

            return layout;
        }();
        return &layout;
    }

    // 28-pin DIP: AY-3-8912 (1 I/O port, no port B)
    if constexpr (Traits.io_port_count == 1) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip28_layout();
            layout.markings.part_number  = Traits.chip_id;
            layout.markings.manufacturer = Traits.vendor;

            //                  LEFT                          RIGHT
            PIN_LR(layout,  1, VSS,        VCC,          28);
            PIN_LR(layout,  2, NC,         CHANNEL_C,    27);
            PIN_LR(layout,  3, CHANNEL_B,  D7,           26);
            PIN_LR(layout,  4, CHANNEL_A,  D6,           25);
            PIN_LR(layout,  5, PA7,        D5,           24);
            PIN_LR(layout,  6, PA6,        D4,           23);
            PIN_LR(layout,  7, PA5,        D3,           22);
            PIN_LR(layout,  8, PA4,        D2,           21);
            PIN_LR(layout,  9, PA3,        D1,           20);
            PIN_LR(layout, 10, PA2,        D0,           19);
            PIN_LR(layout, 11, PA1,        BDIR,         18);
            PIN_LR(layout, 12, PA0,        BC2,          17);
            PIN_LR(layout, 13, A8,         BC1,          16);
            PIN_LR(layout, 14, _CS,        _RES,         15);  // /A9 / /RESET

            return layout;
        }();
        return &layout;
    }

    // 24-pin DIP: AY-3-8913 (no I/O ports)
    if constexpr (Traits.io_port_count == 0) {
        static ChipLayout layout = [] {
            ChipLayout layout = create_dip24_layout();
            layout.markings.part_number  = Traits.chip_id;
            layout.markings.manufacturer = Traits.vendor;

            //                  LEFT                          RIGHT
            PIN_LR(layout,  1, VSS,        VCC,          24);
            PIN_LR(layout,  2, NC,         CHANNEL_C,    23);
            PIN_LR(layout,  3, CHANNEL_B,  D7,           22);
            PIN_LR(layout,  4, CHANNEL_A,  D6,           21);
            PIN_LR(layout,  5, NC,         D5,           20);
            PIN_LR(layout,  6, NC,         D4,           19);
            PIN_LR(layout,  7, NC,         D3,           18);
            PIN_LR(layout,  8, NC,         D2,           17);
            PIN_LR(layout,  9, A8,         D1,           16);
            PIN_LR(layout, 10, _CS,        D0,           15);  // /A9
            PIN_LR(layout, 11, _RES,       BDIR,         14);
            PIN_LR(layout, 12, CLK,        BC1,          13);

            return layout;
        }();
        return &layout;
    }
}

template <const AYTraits& Traits>
std::vector<PinSignalState> ay_psg_t<Traits>::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, 0);
    // AY-3-8910 is not on main system bus — signals come from BDIR/BC1/BC2
    return ps;
}

// Explicit template instantiation — GUI methods
template ChipLayout* ay_psg_t<AY_3_8910_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ay_psg_t<AY_3_8910_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ay_psg_t<AY_3_8912_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ay_psg_t<AY_3_8912_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ay_psg_t<AY_3_8913_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ay_psg_t<AY_3_8913_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ay_psg_t<AY_3_8914_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ay_psg_t<AY_3_8914_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ay_psg_t<YM2149_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ay_psg_t<YM2149_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ay_psg_t<YM3439_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ay_psg_t<YM3439_Traits>::get_layout_pin_states(ChipLayout&);

template ChipLayout* ay_psg_t<AY8930_Traits>::create_chip_layout() const;
template std::vector<PinSignalState> ay_psg_t<AY8930_Traits>::get_layout_pin_states(ChipLayout&);

#endif // CERMU_HAS_GUI
