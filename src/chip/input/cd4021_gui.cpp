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
    PIN_LR(layout,  2, P6,         P_S,         15);  // P/S̅ (latch control)
    PIN_LR(layout,  3, Q7,         CLK,         14);  // serial out / clock
    PIN_LR(layout,  4, P1,         P4,          13);
    PIN_LR(layout,  5, P0,         P3,          12);
    PIN_LR(layout,  6, P7,         DS,          11);  // serial data in
    PIN_LR(layout,  7, _Q7,        P2,          10);  // Q̅7 (complement out)
    PIN_LR(layout,  8, VSS,        Q6,           9);

    return layout;
}

static ChipLayout& get_cd4021_layout() {
    static ChipLayout layout = create_cd4021_layout();
    return layout;
}

// Helper: derive CD4021 pin states from shift register internals.
// The CD4021 is not on the main system bus — it lives in the controller.
// bus_state_t is unused; all signals come from the shift register state.
static std::vector<PinSignalState> get_cd4021_pin_states(
        CD4021* chip, const ChipLayout* layout) {
    if (!chip || !layout) return {};

    // Start with generic handler (sets POWER, CLOCK, NC correctly)
    auto ps = populate_pin_states_from_bus(*layout, 0);

    uint8_t sr = chip->get_shift_register();

    // --- Parallel inputs P0-P7 (directly from shift register latched state) ---
    // P5 (pin 1, idx 0)
    ps[0].signal_level = (sr >> 5) & 1; ps[0].drive_direction = false;
    ps[0].high_impedance = false;       ps[0].signal_valid = true;
    // P6 (pin 2, idx 1)
    ps[1].signal_level = (sr >> 6) & 1; ps[1].drive_direction = false;
    ps[1].high_impedance = false;       ps[1].signal_valid = true;
    // P1 (pin 4, idx 3)
    ps[3].signal_level = (sr >> 1) & 1; ps[3].drive_direction = false;
    ps[3].high_impedance = false;       ps[3].signal_valid = true;
    // P0 (pin 5, idx 4)
    ps[4].signal_level = (sr >> 0) & 1; ps[4].drive_direction = false;
    ps[4].high_impedance = false;       ps[4].signal_valid = true;
    // P7 (pin 6, idx 5)
    ps[5].signal_level = (sr >> 7) & 1; ps[5].drive_direction = false;
    ps[5].high_impedance = false;       ps[5].signal_valid = true;
    // P4 (pin 13, idx 12)
    ps[12].signal_level = (sr >> 4) & 1; ps[12].drive_direction = false;
    ps[12].high_impedance = false;       ps[12].signal_valid = true;
    // P3 (pin 12, idx 11)
    ps[11].signal_level = (sr >> 3) & 1; ps[11].drive_direction = false;
    ps[11].high_impedance = false;       ps[11].signal_valid = true;
    // P2 (pin 10, idx 9)
    ps[9].signal_level = (sr >> 2) & 1; ps[9].drive_direction = false;
    ps[9].high_impedance = false;       ps[9].signal_valid = true;

    // --- Q7 serial output (pin 3, idx 2) — MSB of shift register ---
    ps[2].signal_level    = (sr >> 7) & 1;
    ps[2].drive_direction = true;
    ps[2].high_impedance  = false;
    ps[2].signal_valid    = true;

    // --- /Q7 complement output (pin 7, idx 6) ---
    ps[6].signal_level    = !((sr >> 7) & 1);
    ps[6].drive_direction = true;
    ps[6].high_impedance  = false;
    ps[6].signal_valid    = true;

    // --- Q6 stage 6 output (pin 9, idx 8) ---
    ps[8].signal_level    = (sr >> 6) & 1;
    ps[8].drive_direction = true;
    ps[8].high_impedance  = false;
    ps[8].signal_valid    = true;

    // --- DS serial data input (pin 11, idx 10) — grounded in NES ---
    ps[10].signal_level    = false;
    ps[10].drive_direction = false;
    ps[10].high_impedance  = false;
    ps[10].signal_valid    = true;

    // --- P/S latch control (pin 15, idx 14) — input ---
    ps[14].signal_level    = false; // LOW = serial mode (default readout state)
    ps[14].drive_direction = false;
    ps[14].high_impedance  = false;
    ps[14].signal_valid    = true;

    return ps;
}

// ============================================================================
// ChipBase GUI Overrides
// ============================================================================

bool CD4021::has_layout_content() const { return true; }
bool CD4021::has_debug_content()  const { return true; }

void CD4021::render_layout_content() {
#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_cd4021_layout();
    std::vector<PinSignalState> pin_states = get_cd4021_pin_states(this, &layout);
    render_chip_layout(layout, pin_states, "CD4021");
#endif
}

void CD4021::render_debug_content() {
#ifdef CERMU_HAS_GUI
    // Two-column layout: chip visualization on left, debugging info on right
    ImVec2 window_size = ImGui::GetContentRegionAvail();

    // Left column: Chip Visualization (fixed width ~250px)
    ImVec2 chip_viz_size = ImVec2(250.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Chip Visualization");
        ImGui::Separator();

        ImVec2 chip_center = ImGui::GetCursorScreenPos();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        chip_center.x += content_region.x * 0.5f;
        chip_center.y += 200.0f;

        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_cd4021_layout();

        std::vector<PinSignalState> pin_states = get_cd4021_pin_states(this, &layout);
        renderer.render(layout, chip_center, pin_states, "CD4021");
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 5.0f);

    // Right column: All debugging information
    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0);
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("CD4021B Shift Register");
        ImGui::Separator();

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
    }
    ImGui::EndChild();
#endif
}
