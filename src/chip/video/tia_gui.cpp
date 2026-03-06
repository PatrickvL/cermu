/*
 * tia_gui.cpp — Atari TIA (CO10444) Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the Atari CO10444 (TIA) datasheet.
 * The TIA generates video, audio, and handles player input for the
 * Atari 2600 VCS.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "tia.h"
#include "../../core/chip_layout.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif

// ============================================================================
// 40-pin DIP Layout — Atari TIA (CO10444)
// ============================================================================
//
//   Pin  1: VSS           Pin 40: Vtia
//   Pin  2: COLOR CLK     Pin 39: Φ2
//   Pin  3: /CS1          Pin 38: RDY
//   Pin  4: CS0           Pin 37: DUMP
//   Pin  5: CS3           Pin 36: INPT0
//   Pin  6: R/W           Pin 35: INPT1
//   Pin  7: Φ0            Pin 34: INPT2
//   Pin  8: D0            Pin 33: INPT3
//   Pin  9: D1            Pin 32: INPT4
//   Pin 10: D2            Pin 31: INPT5
//   Pin 11: D3            Pin 30: OSC IN
//   Pin 12: D4            Pin 29: OSC OUT
//   Pin 13: D5            Pin 28: A0
//   Pin 14: D6            Pin 27: A1
//   Pin 15: D7            Pin 26: A2
//   Pin 16: AUD0          Pin 25: A3
//   Pin 17: AUD1          Pin 24: A4
//   Pin 18: COLU          Pin 23: A5
//   Pin 19: LUMA          Pin 22: VCC
//   Pin 20: BLK           Pin 21: CSYNC
//

static ChipLayout create_tia_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.markings.part_number  = "CO10444";
    layout.markings.manufacturer = "Atari";
    layout.markings.custom_text  = "TIA";

    //                  LEFT                         RIGHT
    PIN_LR(layout,  1, VSS,         VTIA,        40);
    PIN_LR(layout,  2, COLOR_CLK,   PHI2,        39);
    PIN_LR(layout,  3, _CS1,        RDY,         38);
    PIN_LR(layout,  4, CS0,         DUMP,        37);
    PIN_LR(layout,  5, CS3,         INPT0,       36);
    PIN_LR(layout,  6, RW,          INPT1,       35);
    PIN_LR(layout,  7, PHI0,        INPT2,       34);
    PIN_LR(layout,  8, D0,          INPT3,       33);
    PIN_LR(layout,  9, D1,          INPT4,       32);
    PIN_LR(layout, 10, D2,          INPT5,       31);
    PIN_LR(layout, 11, D3,          OSC_IN,      30);
    PIN_LR(layout, 12, D4,          OSC_OUT,     29);
    PIN_LR(layout, 13, D5,          A0,          28);
    PIN_LR(layout, 14, D6,          A1,          27);
    PIN_LR(layout, 15, D7,          A2,          26);
    PIN_LR(layout, 16, AUD0,        A3,          25);
    PIN_LR(layout, 17, AUD1,        A4,          24);
    PIN_LR(layout, 18, COLU,        A5,          23);
    PIN_LR(layout, 19, LUMA,        VCC,         22);
    PIN_LR(layout, 20, COMP_BLK,    CSYNC,       21);

    return layout;
}

static ChipLayout& get_tia_layout() {
    static ChipLayout layout = create_tia_layout();
    return layout;
}

// ============================================================================
// ChipBase GUI Overrides
// ============================================================================

bool tia_t::has_layout_content() const { return true; }
bool tia_t::has_debug_content()  const { return true; }

void tia_t::render_layout_content() {
#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_tia_layout();
    std::vector<PinSignalState> pin_states;
    render_chip_layout(layout, pin_states, "TIA");
#endif
}

void tia_t::render_debug_content() {
#ifdef CERMU_HAS_GUI
    // --- Timing ---
    if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("H Counter:  %3d / 227", h_counter);
        ImGui::Text("Scanline:   %3d", scanline);
        ImGui::Text("Visible Row: %d", visible_row);
        ImGui::Text("VSYNC: %s  VBLANK: %s  WSYNC: %s",
                     vsync_active ? "ON" : "off",
                     vblank_active ? "ON" : "off",
                     wsync_pending ? "HALT" : "ok");
    }

    // --- Colors ---
    if (ImGui::CollapsingHeader("Colors")) {
        ImGui::Text("COLUP0: $%02X  COLUP1: $%02X", colup0, colup1);
        ImGui::Text("COLUPF: $%02X  COLUBK: $%02X", colupf, colubk);
    }

    // --- Players ---
    if (ImGui::CollapsingHeader("Players")) {
        ImGui::Text("GRP0: $%02X  GRP1: $%02X", grp0, grp1);
        ImGui::Text("NUSIZ0: $%02X  NUSIZ1: $%02X", nusiz0, nusiz1);
        ImGui::Text("POS P0: %3d  P1: %3d", pos_p0, pos_p1);
        ImGui::Text("HM  P0: %+2d  P1: %+2d", (int)hm_p0, (int)hm_p1);
        ImGui::Text("REF P0: %s  P1: %s",
                     refp0 ? "yes" : "no", refp1 ? "yes" : "no");
    }

    // --- Missiles & Ball ---
    if (ImGui::CollapsingHeader("Missiles & Ball")) {
        ImGui::Text("M0: %s pos=%3d  M1: %s pos=%3d",
                     enam0 ? "ON" : "off", pos_m0,
                     enam1 ? "ON" : "off", pos_m1);
        ImGui::Text("Ball: %s pos=%3d", enabl ? "ON" : "off", pos_bl);
        ImGui::Text("HM  M0: %+2d  M1: %+2d  BL: %+2d",
                     (int)hm_m0, (int)hm_m1, (int)hm_bl);
    }

    // --- Playfield ---
    if (ImGui::CollapsingHeader("Playfield")) {
        ImGui::Text("PF0: $%02X  PF1: $%02X  PF2: $%02X", pf0, pf1, pf2);
        ImGui::Text("CTRLPF: $%02X (REF=%d SCORE=%d PRI=%d)",
                     ctrlpf,
                     ctrlpf & 1, (ctrlpf >> 1) & 1, (ctrlpf >> 2) & 1);
    }

    // --- Collision ---
    if (ImGui::CollapsingHeader("Collision")) {
        ImGui::Text("Collision: $%04X", collision);
    }

    // --- Audio ---
    if (ImGui::CollapsingHeader("Audio")) {
        for (int ch = 0; ch < 2; ch++) {
            ImGui::Text("CH%d: CTRL=$%X FREQ=$%02X VOL=$%X",
                         ch, audio[ch].control, audio[ch].frequency, audio[ch].volume);
        }
    }

    // --- Input ---
    if (ImGui::CollapsingHeader("Input Ports")) {
        ImGui::Text("INPT4: %d  INPT5: %d", inpt4 ? 1 : 0, inpt5 ? 1 : 0);
        ImGui::Text("INPT0: %d  INPT1: %d  INPT2: %d  INPT3: %d",
                     inpt0 ? 1 : 0, inpt1 ? 1 : 0,
                     inpt2 ? 1 : 0, inpt3 ? 1 : 0);
        ImGui::Text("Latch: %s", input_latch_enabled ? "ON" : "off");
    }
#endif
}
