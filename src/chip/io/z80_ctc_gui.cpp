/*
 * z80_ctc_gui.cpp — Z80 CTC Debug/Layout GUI
 *
 * Hardware-accurate 28-pin DIP pinout per Zilog Z8430 datasheet.
 * Also applies to DDR clone U857 (VEB MME Erfurt).
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/io/z80_ctc.hpp"
#include "core/chip_layout.hpp"
#include "core/pin_macros.hpp"
#include <cstdio>

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

// ============================================================================
// Debug field registration
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void z80_ctc_t::register_debug_fields() {
    using S = const z80_ctc_t;
    auto& r = debug_registry_;
    r.set_registers(regs_, z80_ctc_regs::REG_COUNT, Z80_CTC_REG_INFO);
    r.set_decl_order(Z80_CTC_DECL_ORDER.data(), Z80_CTC_DECL_ORDER.size(),
                     nullptr, 0,
                     nullptr, 0, nullptr);

    // Control word and time constant values are in the DECL walk.
    // Live counters and internal state flags remain as builder chains.
    r.category("Channel 0");
    r.counter("Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->ch_[0].counter;
    }, 256);
    r.flag("Counter Mode", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[0].counter_mode; });
    r.flag("Running", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[0].running; });
    r.flag("Int Enabled", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[0].int_enabled; });
    r.flag("Int Pending", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[0].int_pending; });

    r.category("Channel 1");
    r.counter("Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->ch_[1].counter;
    }, 256);
    r.flag("Counter Mode", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[1].counter_mode; });
    r.flag("Running", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[1].running; });
    r.flag("Int Enabled", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[1].int_enabled; });
    r.flag("Int Pending", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[1].int_pending; });

    r.category("Channel 2");
    r.counter("Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->ch_[2].counter;
    }, 256);
    r.flag("Counter Mode", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[2].counter_mode; });
    r.flag("Running", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[2].running; });
    r.flag("Int Enabled", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[2].int_enabled; });
    r.flag("Int Pending", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[2].int_pending; });

    r.category("Channel 3");
    r.counter("Counter", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->ch_[3].counter;
    }, 256);
    r.flag("Counter Mode", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[3].counter_mode; });
    r.flag("Running", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[3].running; });
    r.flag("Int Enabled", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[3].int_enabled; });
    r.flag("Int Pending", +[](const ChipBase* c) -> uint32_t { return static_cast<S*>(c)->ch_[3].int_pending; });
}
#endif

// ============================================================================
// Z80 CTC 28-pin DIP layout (Z8430 datasheet)
// ============================================================================

#ifdef CERMU_HAS_GUI

ChipLayout* z80_ctc_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip28_layout();
        layout.left_pins.clear();
        layout.right_pins.clear();

        layout.markings = {
            "Z80 CTC",                   // part_number
            "Zilog",                     // manufacturer
            {},                          // package_variant
            {},                          // date_code
            {},                          // lot_number
            {},                          // custom_text
            true,                        // show_part_number
            true,                        // show_manufacturer
            false,                       // show_package_variant
            false                        // show_date_code
        };

        // Z80 CTC (Z8430) — 28-pin DIP
        //           Left side                      Right side
        PIN_LR(layout,  1,  D4,       D3,       28)
        PIN_LR(layout,  2,  D5,       D2,       27)
        PIN_LR(layout,  3,  D6,       D1,       26)
        PIN_LR(layout,  4,  D7,       D0,       25)
        PIN_LR(layout,  5,  VSS,      VDD,      24)
        PIN_LR(layout,  6,  _RD,      CLK_TRG3, 23)
        PIN_LR(layout,  7,  ZC_TO2,   CLK_TRG2, 22)
        PIN_LR(layout,  8,  ZC_TO1,   CLK_TRG1, 21)
        PIN_LR(layout,  9,  ZC_TO0,   CLK_TRG0, 20)
        PIN_LR(layout, 10,  CLK,      CS1,      19)
        PIN_LR(layout, 11,  _INT,     CS0,      18)
        PIN_LR(layout, 12,  IEI,      _IORQ,    17)
        PIN_LR(layout, 13,  IEO,      _CE,      16)
        PIN_LR(layout, 14,  _M1,      _RES,     15)

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> z80_ctc_t::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, bus_snapshot_);
    int total = static_cast<int>(ps.size());

    auto set_pin = [&](int pin_idx, bool level, bool is_output) {
        if (pin_idx >= 0 && pin_idx < total) {
            ps[pin_idx].signal_level = level;
            ps[pin_idx].drive_direction = is_output;
            ps[pin_idx].high_impedance = !is_output;
        }
    };

    // ZC/TO outputs: ZC/TO0=pin9(idx8), ZC/TO1=pin8(idx7), ZC/TO2=pin7(idx6)
    set_pin(8, ch_[0].zero_count, true);
    set_pin(7, ch_[1].zero_count, true);
    set_pin(6, ch_[2].zero_count, true);
    // Channel 3 has no ZC/TO output

    // INT (pin11, idx10) — active-low, open-drain output
    bool any_int = false;
    for (const auto& ch : ch_) {
        if (ch.int_pending) { any_int = true; break; }
    }
    set_pin(10, !any_int, true);

    return ps;
}

#endif // CERMU_HAS_GUI
