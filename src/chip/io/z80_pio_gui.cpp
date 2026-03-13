/*
 * z80_pio_gui.cpp — Z80 PIO Debug/Layout GUI
 *
 * Hardware-accurate 40-pin DIP pinout per Zilog Z8420 datasheet.
 * Also applies to DDR clone U855 (VEB MME Erfurt).
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "chip/io/z80_pio.hpp"
#include "core/chip_layout.hpp"
#include "core/pin_macros.hpp"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif

// ============================================================================
// Debug field registration
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void z80_pio_t::register_debug_fields() {
    using S = const z80_pio_t;
    auto& r = debug_registry_;
    r.set_registers(regs_, z80_pio_regs::REG_COUNT, Z80_PIO_REG_INFO);
    r.set_decl_entries(Z80_PIO_DECL_ENTRIES.data(), Z80_PIO_DECL_ENTRIES.size());

    // Register values (output latch, I/O select, int vector) are in the DECL walk.
    // Internal port state (input, mode, int flags, ready) remains as builder chains.
    r.category("Port A");
    r.value("Input", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_[0].input;
    }, 8);
    static const char* const pio_mode_names[] = {"Output", "Input", "Bidir", "Bit Ctrl"};
    r.state("Mode", +[](const ChipBase* c) -> uint32_t {
        return static_cast<uint32_t>(static_cast<S*>(c)->port_[0].mode);
    }, pio_mode_names, 4);
    r.flag("Int Enabled", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_[0].int_enabled;
    });
    r.flag("Int Pending", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_[0].int_pending;
    });
    r.flag("ARDY", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_[0].ready;
    });

    r.category("Port B");
    r.value("Input", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_[1].input;
    }, 8);
    r.state("Mode", +[](const ChipBase* c) -> uint32_t {
        return static_cast<uint32_t>(static_cast<S*>(c)->port_[1].mode);
    }, pio_mode_names, 4);
    r.flag("Int Enabled", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_[1].int_enabled;
    });
    r.flag("Int Pending", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_[1].int_pending;
    });
    r.flag("BRDY", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_[1].ready;
    });
}
#endif

// ============================================================================
// Z80 PIO 40-pin DIP layout (Z8420 datasheet)
// ============================================================================

#ifdef CERMU_HAS_GUI

ChipLayout* z80_pio_t::create_chip_layout() const {
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();
        layout.left_pins.clear();
        layout.right_pins.clear();

        layout.markings = {
            "Z80 PIO",                   // part_number
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

        // Z80 PIO (Z8420) — 40-pin DIP
        //           Left side                Right side
        PIN_LR(layout,  1,  D2,    D3,    40)
        PIN_LR(layout,  2,  D7,    D4,    39)
        PIN_LR(layout,  3,  D6,    D5,    38)
        PIN_LR(layout,  4,  _CE,   CLK,   37)
        PIN_LR(layout,  5,  C_DSEL, IEI,   36)  // C/D̅ select
        PIN_LR(layout,  6,  B_ASEL, _INT,  35)  // B/A̅ select
        PIN_LR(layout,  7,  PA7,   IEO,   34)
        PIN_LR(layout,  8,  PA6,   _M1,   33)
        PIN_LR(layout,  9,  PA5,   _IORQ, 32)
        PIN_LR(layout, 10,  PA4,   _RD,   31)
        PIN_LR(layout, 11,  VSS,   PB0,   30)
        PIN_LR(layout, 12,  PA3,   PB1,   29)
        PIN_LR(layout, 13,  PA2,   PB2,   28)
        PIN_LR(layout, 14,  PA1,   PB3,   27)
        PIN_LR(layout, 15,  PA0,   PB4,   26)
        PIN_LR(layout, 16,  ASTB,  PB5,   25)
        PIN_LR(layout, 17,  BSTB,  PB6,   24)
        PIN_LR(layout, 18,  ARDY,  PB7,   23)
        PIN_LR(layout, 19,  BRDY,  D0,    22)
        PIN_LR(layout, 20,  D1,    VDD,   21)

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> z80_pio_t::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, bus_snapshot_);
    int total = static_cast<int>(ps.size());

    auto set_pin = [&](int pin_idx, bool level, bool is_output) {
        if (pin_idx >= 0 && pin_idx < total) {
            ps[pin_idx].signal_level = level;
            ps[pin_idx].drive_direction = is_output;
            ps[pin_idx].high_impedance = !is_output;
        }
    };

    // Port A pins: PA7=pin7(idx6)..PA0=pin15(idx14)
    bool pa_output = (port_[0].mode == PIOMode::OUTPUT || port_[0].mode == PIOMode::BIDIRECTIONAL);
    uint8_t pa_val = pa_output ? port_[0].output : port_[0].input;
    for (int i = 0; i < 8; ++i) {
        // PA7 at idx 6, PA6 at 7, ..., PA0 at 14
        bool is_out = pa_output;
        if (port_[0].mode == PIOMode::BIT_CONTROL)
            is_out = !(port_[0].io_select & (1 << (7 - i)));
        int idx = 6 + (7 - i);  // PA7=idx6, PA6=idx7, ..., PA0=idx14
        uint8_t bit_val = (pa_val >> (7 - i)) & 1;
        set_pin(idx, bit_val, is_out);
    }

    // Port B pins: PB0=pin30(idx29)..PB7=pin23(idx22)
    bool pb_output = (port_[1].mode == PIOMode::OUTPUT);
    uint8_t pb_val = pb_output ? port_[1].output : port_[1].input;
    for (int i = 0; i < 8; ++i) {
        bool is_out = pb_output;
        if (port_[1].mode == PIOMode::BIT_CONTROL)
            is_out = !(port_[1].io_select & (1 << i));
        int idx = 29 - i;  // PB0=idx29, PB1=idx28, ..., PB7=idx22
        uint8_t bit_val = (pb_val >> i) & 1;
        set_pin(idx, bit_val, is_out);
    }

    // ARDY (pin18, idx17) — output from PIO
    set_pin(17, port_[0].ready, true);
    // BRDY (pin19, idx18) — output from PIO
    set_pin(18, port_[1].ready, true);

    // INT (pin35, idx34) — active-low, output
    bool any_int = port_[0].int_pending || port_[1].int_pending;
    set_pin(34, !any_int, true);

    return ps;
}

#endif // CERMU_HAS_GUI
