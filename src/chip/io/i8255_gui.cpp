/*
 * i8255_gui.cpp — Intel 8255 PPI Debug/Layout GUI
 *
 * Hardware-accurate 40-pin DIP pinout per Intel 8255A datasheet.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "i8255.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif

// ============================================================================
// Debug field registration
// ============================================================================

#ifdef CERMU_HAS_CHIP_DEBUG
void i8255_t::register_debug_fields() {
    using S = const i8255_t;
    auto& r = debug_registry_;
    r.set_registers(regs_, i8255_regs::REG_COUNT, I8255_REG_INFO);

    r.category("Port A");
    r.value("Output Latch", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->regs_[i8255_regs::PORT_A];
    }, 8);
    r.value("Input", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_a_in_;
    }, 8);
    r.flag("Direction (In)", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_a_input();
    });

    r.category("Port B");
    r.value("Output Latch", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->regs_[i8255_regs::PORT_B];
    }, 8);
    r.value("Input", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_b_in_;
    }, 8);
    r.flag("Direction (In)", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_b_input();
    });

    r.category("Port C");
    r.value("Output Latch", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->regs_[i8255_regs::PORT_C];
    }, 8);
    r.value("Input", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_c_in_;
    }, 8);
    r.flag("Upper (In)", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_c_upper_input();
    });
    r.flag("Lower (In)", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->port_c_lower_input();
    });

    r.category("Control");
    r.value("Control Reg", +[](const ChipBase* c) -> uint32_t {
        return static_cast<S*>(c)->regs_[i8255_regs::CONTROL];
    }, 8);
    r.value("Group A Mode", +[](const ChipBase* c) -> uint32_t {
        return (static_cast<S*>(c)->regs_[i8255_regs::CONTROL] >> 5) & 0x03;
    }, 2);
    r.value("Group B Mode", +[](const ChipBase* c) -> uint32_t {
        return (static_cast<S*>(c)->regs_[i8255_regs::CONTROL] >> 2) & 0x01;
    }, 1);
}
#endif

// ============================================================================
// i8255 40-pin DIP layout
// ============================================================================

#ifdef CERMU_HAS_GUI

ChipLayout* i8255_t::create_chip_layout() const {
    // Intel 8255A — 40-pin DIP (per datasheet)
    static ChipLayout layout = [] {
        ChipLayout layout = create_dip40_layout();
        layout.left_pins.clear();
        layout.right_pins.clear();

        layout.markings = {
            "8255A",                     // part_number
            "Intel",                     // manufacturer
            {},                          // package_variant
            {},                          // date_code
            {},                          // lot_number
            {},                          // custom_text
            true,                        // show_part_number
            true,                        // show_manufacturer
            false,                       // show_package_variant
            false                        // show_date_code
        };

        //           Left side                Right side
        PIN_LR(layout,  1,  PA3,   PA4,   40)
        PIN_LR(layout,  2,  PA2,   PA5,   39)
        PIN_LR(layout,  3,  PA1,   PA6,   38)
        PIN_LR(layout,  4,  PA0,   PA7,   37)
        PIN_LR(layout,  5,  _RD,   _WR,   36)
        PIN_LR(layout,  6,  _CS,   _RES,  35)
        PIN_LR(layout,  7,  VSS,   D0,    34)
        PIN_LR(layout,  8,  A1,    D1,    33)
        PIN_LR(layout,  9,  A0,    D2,    32)
        PIN_LR(layout, 10,  PC7,   D3,    31)
        PIN_LR(layout, 11,  PC6,   D4,    30)
        PIN_LR(layout, 12,  PC5,   D5,    29)
        PIN_LR(layout, 13,  PC4,   D6,    28)
        PIN_LR(layout, 14,  PC0,   D7,    27)
        PIN_LR(layout, 15,  PC1,   VDD,   26)
        PIN_LR(layout, 16,  PC2,   PB7,   25)
        PIN_LR(layout, 17,  PC3,   PB6,   24)
        PIN_LR(layout, 18,  PB0,   PB5,   23)
        PIN_LR(layout, 19,  PB1,   PB4,   22)
        PIN_LR(layout, 20,  PB2,   PB3,   21)

        return layout;
    }();
    return &layout;
}

std::vector<PinSignalState> i8255_t::get_layout_pin_states(ChipLayout& layout) {
    auto ps = populate_pin_states_from_bus(layout, bus_snapshot_);
    int total = static_cast<int>(ps.size());

    // Port A pins: PA3(1), PA2(2), PA1(3), PA0(4), PA4(40), PA5(39), PA6(38), PA7(37)
    // Map output latch / input based on direction
    auto set_port_pin = [&](int pin_idx, bool level, bool is_output) {
        if (pin_idx >= 0 && pin_idx < total) {
            ps[pin_idx].signal_level = level;
            ps[pin_idx].drive_direction = is_output;
            ps[pin_idx].high_impedance = !is_output;
        }
    };

    bool pa_out = !port_a_input();
    uint8_t pa_val = pa_out ? port_a_out_ : port_a_in_;
    // Left side: PA3=pin1(idx0), PA2=pin2(idx1), PA1=pin3(idx2), PA0=pin4(idx3)
    set_port_pin(0, (pa_val >> 3) & 1, pa_out);
    set_port_pin(1, (pa_val >> 2) & 1, pa_out);
    set_port_pin(2, (pa_val >> 1) & 1, pa_out);
    set_port_pin(3, (pa_val >> 0) & 1, pa_out);
    // Right side: PA4=pin40(idx39), PA5=pin39(idx38), PA6=pin38(idx37), PA7=pin37(idx36)
    set_port_pin(39, (pa_val >> 4) & 1, pa_out);
    set_port_pin(38, (pa_val >> 5) & 1, pa_out);
    set_port_pin(37, (pa_val >> 6) & 1, pa_out);
    set_port_pin(36, (pa_val >> 7) & 1, pa_out);

    // Port B pins: PB0=pin18(idx17)..PB2=pin20(idx19), PB3=pin21(idx20)..PB7=pin25(idx24)
    bool pb_out = !port_b_input();
    uint8_t pb_val = pb_out ? port_b_out_ : port_b_in_;
    // Left: PB0(17), PB1(18), PB2(19)
    set_port_pin(17, (pb_val >> 0) & 1, pb_out);
    set_port_pin(18, (pb_val >> 1) & 1, pb_out);
    set_port_pin(19, (pb_val >> 2) & 1, pb_out);
    // Right: PB3(20), PB4(21), PB5(22), PB6(23), PB7(24)
    set_port_pin(20, (pb_val >> 3) & 1, pb_out);
    set_port_pin(21, (pb_val >> 4) & 1, pb_out);
    set_port_pin(22, (pb_val >> 5) & 1, pb_out);
    set_port_pin(23, (pb_val >> 6) & 1, pb_out);
    set_port_pin(24, (pb_val >> 7) & 1, pb_out);

    // Port C pins (split direction: upper C4-C7, lower C0-C3)
    bool pc_upper_out = !port_c_upper_input();
    bool pc_lower_out = !port_c_lower_input();
    uint8_t pc_val = read_port_c();
    // Left: PC7=pin10(idx9), PC6=pin11(idx10), PC5=pin12(idx11), PC4=pin13(idx12)
    set_port_pin(9,  (pc_val >> 7) & 1, pc_upper_out);
    set_port_pin(10, (pc_val >> 6) & 1, pc_upper_out);
    set_port_pin(11, (pc_val >> 5) & 1, pc_upper_out);
    set_port_pin(12, (pc_val >> 4) & 1, pc_upper_out);
    // Left: PC0=pin14(idx13), PC1=pin15(idx14), PC2=pin16(idx15), PC3=pin17(idx16)
    set_port_pin(13, (pc_val >> 0) & 1, pc_lower_out);
    set_port_pin(14, (pc_val >> 1) & 1, pc_lower_out);
    set_port_pin(15, (pc_val >> 2) & 1, pc_lower_out);
    set_port_pin(16, (pc_val >> 3) & 1, pc_lower_out);

    return ps;
}

#endif // CERMU_HAS_GUI
