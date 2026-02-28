/*
 * cd4021_gui.cpp — CD4021 Shift Register Debug/Layout GUI
 *
 * 16-pin DIP pinout based on the Texas Instruments CD4021B datasheet.
 * The CD4021 is a CMOS 8-stage static shift register used as a
 * parallel-in/serial-out (PISO) interface in NES/SNES controllers.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "cd4021.h"
#include "../../core/chip_layout.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif

// ============================================================================
// 16-pin DIP Layout
// ============================================================================
//
//   Pin 1:  P5   (parallel in 5)     Pin 16: VDD
//   Pin 2:  P6   (parallel in 6)     Pin 15: P/S̅  (LATCH)
//   Pin 3:  Q7   (serial out)        Pin 14: CLK
//   Pin 4:  P1   (parallel in 1)     Pin 13: P4
//   Pin 5:  P0   (parallel in 0)     Pin 12: P3
//   Pin 6:  P7   (parallel in 7)     Pin 11: DS   (serial in)
//   Pin 7:  Q̅7   (complement out)    Pin 10: P2
//   Pin 8:  VSS  (GND)               Pin  9: Q6   (stage 6 out)
//

static ChipLayout create_cd4021_layout() {
    ChipLayout layout = create_dip16_layout();

    layout.markings.part_number  = "CD4021B";
    layout.markings.manufacturer = "Texas Instruments";
    layout.markings.custom_text  = "CMOS Shift Register";

    //                  LEFT                        RIGHT
    PIN_LR(layout,  1, P5,         VDD,         16);
    PIN_LR(layout,  2, P6,         CLK,         15);  // P/S̅ labeled as CLK for layout
    PIN_LR(layout,  3, Q7,         CLK,         14);
    PIN_LR(layout,  4, P1,         P4,          13);
    PIN_LR(layout,  5, P0,         P3,          12);
    PIN_LR(layout,  6, P7,         NC,          11);  // DS (serial in)
    PIN_LR(layout,  7, Q7,         P2,          10);  // Q̅7 (complement)
    PIN_LR(layout,  8, VSS,        Q6,           9);

    return layout;
}

static ChipLayout& get_cd4021_layout() {
    static ChipLayout layout = create_cd4021_layout();
    return layout;
}

// ============================================================================
// ChipBase GUI Overrides
// ============================================================================

bool CD4021::has_layout_content() const { return true; }
bool CD4021::has_debug_content()  const { return true; }

void CD4021::render_layout_content() {
#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_cd4021_layout();
    std::vector<PinSignalState> pin_states; // Empty — no live signal mapping yet
    render_chip_layout(layout, pin_states, "CD4021");
#endif
}

void CD4021::render_debug_content() {
#ifdef CERMU_HAS_GUI
    uint8_t sr = get_shift_register();

    ImGui::Text("Shift Register: $%02X (%d)", sr, sr);
    ImGui::Separator();

    // Show individual bits as a visual bit display
    ImGui::Text("Bits: ");
    ImGui::SameLine();
    for (int i = 7; i >= 0; i--) {
        bool bit = (sr >> i) & 1;
        if (bit)
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%d", bit);
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%d", bit);
        if (i > 0) ImGui::SameLine();
    }

    ImGui::Text("       P7 P6 P5 P4 P3 P2 P1 P0");
#endif
}
