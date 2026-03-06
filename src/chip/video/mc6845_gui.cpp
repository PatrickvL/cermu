/*
 * mc6845_gui.cpp — Motorola MC6845 CRT Controller Debug/Layout GUI
 *
 * 40-pin DIP pinout based on the Motorola MC6845 datasheet.
 * The MC6845 generates timing and addressing for raster-scanned CRT
 * displays.  Used in Commodore PET/CBM, BBC Micro, Amstrad CPC, etc.
 *
 * Compiled only when CERMU_HAS_GUI is defined.
 */

#include "mc6845.h"
#include "../../core/chip_layout.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif

// ============================================================================
// 40-pin DIP Layout — Motorola MC6845
// ============================================================================
//
//   Pin  1: VSS           Pin 40: VCC
//   Pin  2: /RES          Pin 39: CLK
//   Pin  3: LPSTB         Pin 38: /CS
//   Pin  4: MA0           Pin 37: RS
//   Pin  5: MA1           Pin 36: E (Enable)
//   Pin  6: MA2           Pin 35: R/W
//   Pin  7: MA3           Pin 34: VSYNC
//   Pin  8: MA4           Pin 33: HSYNC
//   Pin  9: MA5           Pin 32: DE
//   Pin 10: MA6           Pin 31: CURSOR
//   Pin 11: MA7           Pin 30: D7
//   Pin 12: MA8           Pin 29: D6
//   Pin 13: MA9           Pin 28: D5
//   Pin 14: MA10          Pin 27: D4
//   Pin 15: MA11          Pin 26: D3
//   Pin 16: MA12          Pin 25: D2
//   Pin 17: MA13          Pin 24: D1
//   Pin 18: RA0           Pin 23: D0
//   Pin 19: RA1           Pin 22: RA4
//   Pin 20: RA2           Pin 21: RA3
//

static ChipLayout create_mc6845_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.markings.part_number  = "MC6845";
    layout.markings.manufacturer = "Motorola";
    layout.markings.custom_text  = "CRT Controller";

    //                  LEFT                         RIGHT
    PIN_LR(layout,  1, VSS,         VCC,         40);
    PIN_LR(layout,  2, _RES,        CLK,         39);
    PIN_LR(layout,  3, LPSTB,       _CS,         38);
    PIN_LR(layout,  4, MA0,         RS,          37);
    PIN_LR(layout,  5, MA1,         ENABLE,      36);
    PIN_LR(layout,  6, MA2,         RW,          35);
    PIN_LR(layout,  7, MA3,         VSYNC,       34);
    PIN_LR(layout,  8, MA4,         HSYNC,       33);
    PIN_LR(layout,  9, MA5,         DE,          32);
    PIN_LR(layout, 10, MA6,         CURSOR,      31);
    PIN_LR(layout, 11, MA7,         D7,          30);
    PIN_LR(layout, 12, MA8,         D6,          29);
    PIN_LR(layout, 13, MA9,         D5,          28);
    PIN_LR(layout, 14, MA10,        D4,          27);
    PIN_LR(layout, 15, MA11,        D3,          26);
    PIN_LR(layout, 16, MA12,        D2,          25);
    PIN_LR(layout, 17, MA13,        D1,          24);
    PIN_LR(layout, 18, RA0,         D0,          23);
    PIN_LR(layout, 19, RA1,         RA4,         22);
    PIN_LR(layout, 20, RA2,         RA3,         21);

    return layout;
}

static ChipLayout& get_mc6845_layout() {
    static ChipLayout layout = create_mc6845_layout();
    return layout;
}

// ============================================================================
// Register names for debug display
// ============================================================================

static const char* mc6845_reg_names[MC6845_NUM_REGISTERS] = {
    "R0  H Total",        "R1  H Displayed",
    "R2  H Sync Pos",     "R3  Sync Widths",
    "R4  V Total",        "R5  V Adjust",
    "R6  V Displayed",    "R7  V Sync Pos",
    "R8  Mode Ctrl",      "R9  Max Scanline",
    "R10 Cursor Start",   "R11 Cursor End",
    "R12 Start Addr Hi",  "R13 Start Addr Lo",
    "R14 Cursor Hi",      "R15 Cursor Lo",
    "R16 LPen Hi (RO)",   "R17 LPen Lo (RO)",
};

// ============================================================================
// ChipBase GUI Overrides
// ============================================================================

bool mc6845_t::has_layout_content() const { return true; }
bool mc6845_t::has_debug_content()  const { return true; }

void mc6845_t::render_layout_content() {
#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_mc6845_layout();
    std::vector<PinSignalState> pin_states;
    render_chip_layout(layout, pin_states, "MC6845");
#endif
}

void mc6845_t::render_debug_content() {
#ifdef CERMU_HAS_GUI
    // --- Registers ---
    if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Addr Reg: R%d", address_register);
        ImGui::Separator();
        for (int i = 0; i < MC6845_NUM_REGISTERS; i++) {
            ImGui::Text("%-20s = $%02X (%3d)", mc6845_reg_names[i], regs[i], regs[i]);
        }
    }

    // --- Counters ---
    if (ImGui::CollapsingHeader("Counters")) {
        ImGui::Text("H Char Counter:   %3d / %3d", h_char_counter, chars_per_line() - 1);
        ImGui::Text("V Row Counter:    %3d / %3d", v_row_counter, rows_per_frame() - 1);
        ImGui::Text("V Scanline:       %3d / %3d", v_scanline_counter, scanlines_per_row() - 1);
        ImGui::Text("V Adjust Counter: %3d", v_adjust_counter);
    }

    // --- Sync & Display ---
    if (ImGui::CollapsingHeader("Sync & Display")) {
        ImGui::Text("HSYNC: %s   VSYNC: %s",
                     h_sync_active ? "ACTIVE" : "off",
                     v_sync_active ? "ACTIVE" : "off");
        ImGui::Text("H Display: %s  V Display: %s",
                     h_display_active ? "ON" : "off",
                     v_display_active ? "ON" : "off");
        ImGui::Text("In Adjust: %s", in_adjust ? "yes" : "no");
    }

    // --- Address ---
    if (ImGui::CollapsingHeader("Address")) {
        ImGui::Text("Linear Addr:  $%04X", linear_address);
        ImGui::Text("Row Start:    $%04X", row_start_address);
        ImGui::Text("Start Addr:   $%04X", start_address());
        ImGui::Text("Cursor Addr:  $%04X", cursor_address());
        ImGui::Text("Cursor Vis:   %s", cursor_visible ? "yes" : "no");
    }

    // --- Light Pen ---
    if (ImGui::CollapsingHeader("Light Pen")) {
        ImGui::Text("Latched: %s  Addr: $%04X",
                     light_pen_latched ? "yes" : "no", light_pen_address);
    }

    // --- Frame ---
    ImGui::Text("Frame: %u", frame_count);
#endif
}
