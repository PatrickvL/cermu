/*
 * pia6820_gui.cpp — Motorola 6820 PIA Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the Motorola MC6820 / MC6821 datasheet.
 * The 6820 provides two 8-bit bidirectional I/O ports with handshake
 * control lines.  Used in the Apple 1, various 6800/6502 systems.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "pia6820.h"
#include "../../core/chip_layout.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif

// ============================================================================
// 40-pin DIP Layout — Motorola 6820 PIA
// ============================================================================
//
//   Pin  1: VSS           Pin 40: CA1
//   Pin  2: PA0           Pin 39: CA2
//   Pin  3: PA1           Pin 38: /IRQA
//   Pin  4: PA2           Pin 37: /IRQB
//   Pin  5: PA3           Pin 36: RS0
//   Pin  6: PA4           Pin 35: RS1
//   Pin  7: PA5           Pin 34: /RES
//   Pin  8: PA6           Pin 33: D0
//   Pin  9: PA7           Pin 32: D1
//   Pin 10: PB0           Pin 31: D2
//   Pin 11: PB1           Pin 30: D3
//   Pin 12: PB2           Pin 29: D4
//   Pin 13: PB3           Pin 28: D5
//   Pin 14: PB4           Pin 27: D6
//   Pin 15: PB5           Pin 26: D7
//   Pin 16: PB6           Pin 25: E (Enable)
//   Pin 17: PB7           Pin 24: CS1
//   Pin 18: CB1           Pin 23: /CS2
//   Pin 19: CB2           Pin 22: CS0
//   Pin 20: VCC           Pin 21: R/W
//

static ChipLayout create_pia6820_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.markings.part_number  = "MC6821";
    layout.markings.manufacturer = "Motorola";
    layout.markings.custom_text  = "PIA";

    //                  LEFT                         RIGHT
    PIN_LR(layout,  1, VSS,         CA1,         40);
    PIN_LR(layout,  2, PA0,         CA2,         39);
    PIN_LR(layout,  3, PA1,         _IRQA,       38);
    PIN_LR(layout,  4, PA2,         _IRQB,       37);
    PIN_LR(layout,  5, PA3,         RS0,         36);
    PIN_LR(layout,  6, PA4,         RS1,         35);
    PIN_LR(layout,  7, PA5,         _RES,        34);
    PIN_LR(layout,  8, PA6,         D0,          33);
    PIN_LR(layout,  9, PA7,         D1,          32);
    PIN_LR(layout, 10, PB0,         D2,          31);
    PIN_LR(layout, 11, PB1,         D3,          30);
    PIN_LR(layout, 12, PB2,         D4,          29);
    PIN_LR(layout, 13, PB3,         D5,          28);
    PIN_LR(layout, 14, PB4,         D6,          27);
    PIN_LR(layout, 15, PB5,         D7,          26);
    PIN_LR(layout, 16, PB6,         ENABLE,      25);
    PIN_LR(layout, 17, PB7,         CS1,         24);
    PIN_LR(layout, 18, CB1,         _CS2,        23);
    PIN_LR(layout, 19, CB2,         CS0,         22);
    PIN_LR(layout, 20, VCC,         RW,          21);

    return layout;
}

static ChipLayout& get_pia6820_layout() {
    static ChipLayout layout = create_pia6820_layout();
    return layout;
}

// ============================================================================
// ChipBase GUI Overrides
// ============================================================================

bool pia6820_t::has_layout_content() const { return true; }
bool pia6820_t::has_debug_content()  const { return true; }

void pia6820_t::render_layout_content() {
#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_pia6820_layout();
    std::vector<PinSignalState> pin_states;
    render_chip_layout(layout, pin_states, "PIA");
#endif
}

void pia6820_t::render_debug_content() {
#ifdef CERMU_HAS_GUI
    // --- Port A ---
    if (ImGui::CollapsingHeader("Port A", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Data:    $%02X", port_a_data);
        ImGui::Text("DDR:     $%02X", port_a_direction);
        ImGui::Text("Control: $%02X", port_a_control);
        ImGui::Text("CA1: %d  CA2: %d", ca1_state ? 1 : 0, ca2_state ? 1 : 0);
        ImGui::Text("IRQ A1: %s  A2: %s",
                     irq_a1 ? "SET" : "clr", irq_a2 ? "SET" : "clr");
    }

    // --- Port B ---
    if (ImGui::CollapsingHeader("Port B", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Data:    $%02X", port_b_data);
        ImGui::Text("DDR:     $%02X", port_b_direction);
        ImGui::Text("Control: $%02X", port_b_control);
        ImGui::Text("CB1: %d  CB2: %d", cb1_state ? 1 : 0, cb2_state ? 1 : 0);
        ImGui::Text("IRQ B1: %s  B2: %s",
                     irq_b1 ? "SET" : "clr", irq_b2 ? "SET" : "clr");
    }
#endif
}
