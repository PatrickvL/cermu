/*
 * pia6532_gui.cpp — MOS 6532 RIOT Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the MOS 6532 (RIOT) datasheet.
 * The 6532 combines 128 bytes RAM, two 8-bit I/O ports, and a
 * programmable interval timer.  Used in the Atari 2600 VCS.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "pia6532.h"
#include "../../core/chip_layout.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif

// ============================================================================
// 40-pin DIP Layout — MOS 6532 RIOT
// ============================================================================
//
//   Pin  1: VSS           Pin 40: VCC
//   Pin  2: A6            Pin 39: /CS2
//   Pin  3: Φ2            Pin 38: CS1
//   Pin  4: RS            Pin 37: PB7
//   Pin  5: R/W           Pin 36: PB6
//   Pin  6: /RES          Pin 35: PB5
//   Pin  7: /IRQ          Pin 34: PB4
//   Pin  8: D7            Pin 33: PB3
//   Pin  9: D6            Pin 32: PB2
//   Pin 10: D5            Pin 31: PB1
//   Pin 11: D4            Pin 30: PB0
//   Pin 12: D3            Pin 29: PA7
//   Pin 13: D2            Pin 28: PA6
//   Pin 14: D1            Pin 27: PA5
//   Pin 15: D0            Pin 26: PA4
//   Pin 16: A0            Pin 25: PA3
//   Pin 17: A1            Pin 24: PA2
//   Pin 18: A2            Pin 23: PA1
//   Pin 19: A3            Pin 22: PA0
//   Pin 20: A4            Pin 21: A5
//

static ChipLayout create_pia6532_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.markings.part_number  = "MOS 6532";
    layout.markings.manufacturer = "MOS Technology";
    layout.markings.custom_text  = "RIOT";

    //                  LEFT                         RIGHT
    PIN_LR(layout,  1, VSS,         VCC,         40);
    PIN_LR(layout,  2, A6,          _CS2,        39);
    PIN_LR(layout,  3, PHI2,        CS1,         38);
    PIN_LR(layout,  4, RS,          PB7,         37);
    PIN_LR(layout,  5, RW,          PB6,         36);
    PIN_LR(layout,  6, _RES,        PB5,         35);
    PIN_LR(layout,  7, _IRQ,        PB4,         34);
    PIN_LR(layout,  8, D7,          PB3,         33);
    PIN_LR(layout,  9, D6,          PB2,         32);
    PIN_LR(layout, 10, D5,          PB1,         31);
    PIN_LR(layout, 11, D4,          PB0,         30);
    PIN_LR(layout, 12, D3,          PA7,         29);
    PIN_LR(layout, 13, D2,          PA6,         28);
    PIN_LR(layout, 14, D1,          PA5,         27);
    PIN_LR(layout, 15, D0,          PA4,         26);
    PIN_LR(layout, 16, A0,          PA3,         25);
    PIN_LR(layout, 17, A1,          PA2,         24);
    PIN_LR(layout, 18, A2,          PA1,         23);
    PIN_LR(layout, 19, A3,          PA0,         22);
    PIN_LR(layout, 20, A4,          A5,          21);

    return layout;
}

static ChipLayout& get_pia6532_layout() {
    static ChipLayout layout = create_pia6532_layout();
    return layout;
}

// ============================================================================
// Timer divider labels
// ============================================================================

static const char* divider_label(uint16_t div) {
    switch (div) {
        case 1:    return "TIM1T (÷1)";
        case 8:    return "TIM8T (÷8)";
        case 64:   return "TIM64T (÷64)";
        case 1024: return "TIM1024T (÷1024)";
        default:   return "???";
    }
}

// ============================================================================
// ChipBase GUI Overrides
// ============================================================================

bool pia6532_t::has_layout_content() const { return true; }
bool pia6532_t::has_debug_content()  const { return true; }

void pia6532_t::render_layout_content() {
#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_pia6532_layout();
    std::vector<PinSignalState> pin_states;
    render_chip_layout(layout, pin_states, "6532");
#endif
}

void pia6532_t::render_debug_content() {
#ifdef CERMU_HAS_GUI
    // --- I/O Ports ---
    if (ImGui::CollapsingHeader("I/O Ports", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Port A: Data=$%02X  DDR=$%02X  Input=$%02X  Effective=$%02X",
                     port_a_data, port_a_ddr, port_a_input, read_port_a());
        ImGui::Text("Port B: Data=$%02X  DDR=$%02X  Input=$%02X  Effective=$%02X",
                     port_b_data, port_b_ddr, port_b_input, read_port_b());
    }

    // --- Timer ---
    if (ImGui::CollapsingHeader("Timer", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Value: $%02X (%3d)   %s",
                     timer_value, timer_value, divider_label(timer_divider));
        ImGui::Text("Sub-counter: %d / %d", timer_counter, timer_divider);
        ImGui::Text("Underflow: %s  IRQ Enable: %s",
                     timer_underflow ? "YES" : "no",
                     timer_interrupt_enabled ? "yes" : "no");
    }

    // --- RAM (first 16 bytes as hex dump) ---
    if (ImGui::CollapsingHeader("RAM (128 bytes)")) {
        for (int row = 0; row < 8; row++) {
            ImGui::Text("$%02X:", row * 16);
            ImGui::SameLine();
            for (int col = 0; col < 16; col++) {
                ImGui::SameLine();
                ImGui::Text("%02X", ram[row * 16 + col]);
            }
        }
    }
#endif
}
