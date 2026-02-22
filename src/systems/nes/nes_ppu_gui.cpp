/*
 * nes_ppu_gui.cpp — Ricoh 2C02 PPU Debug/Settings GUI Windows
 *
 * Hardware-accurate 40-pin DIP pinout based on the Ricoh 2C02 datasheet.
 * The PPU (Picture Processing Unit) generates the NES video signal,
 * rendering backgrounds from name tables and sprites from OAM.
 *
 * Pinout reference: Ricoh RP2C02 Datasheet
 *
 *           ╔═══════════╗
 *    R/W ──┤ 1      40 ├── VCC
 *    D0  ──┤ 2      39 ├── ALE
 *    D1  ──┤ 3      38 ├── AD0
 *    D2  ──┤ 4      37 ├── AD1
 *    D3  ──┤ 5      36 ├── AD2
 *    D4  ──┤ 6      35 ├── AD3
 *    D5  ──┤ 7      34 ├── AD4
 *    D6  ──┤ 8      33 ├── AD5
 *    D7  ──┤ 9      32 ├── AD6
 *    A2  ──┤10      31 ├── AD7
 *    A1  ──┤11      30 ├── A8
 *    A0  ──┤12      29 ├── A9
 *   /CS  ──┤13      28 ├── A10
 *  EXT0  ──┤14      27 ├── A11
 *  EXT1  ──┤15      26 ├── A12
 *  EXT2  ──┤16      25 ├── A13
 *  EXT3  ──┤17      24 ├── /RD
 *   CLK  ──┤18      23 ├── /WR
 *  /INT  ──┤19      22 ├── /RST
 *   GND  ──┤20      21 ├── VOUT
 *           ╚═══════════╝
 */

#include "nes_system.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#ifdef IMGUI_VERSION
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <stdio.h>
#include <memory>

// ============================================================================
// RICOH 2C02 PPU LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_ricoh_2c02_layout() {
    ChipLayout layout = create_dip40_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        "RP2C02",
        "Ricoh",
        nullptr, nullptr, nullptr, nullptr,
        true, true, false, false
    };

    // Hardware-accurate Ricoh 2C02 PPU pinout (40-pin DIP)
    PIN_LR(layout,  1, RW,      VDD, 40)       // Read/Write / +5V
    PIN_LR(layout,  2, D0,      UNKNOWN, 39)   // Data 0 / ALE (Address Latch Enable)
    PIN_LR(layout,  3, D1,      MA0, 38)       // Data 1 / AD0 (Addr/Data mux)
    PIN_LR(layout,  4, D2,      MA1, 37)       // Data 2 / AD1
    PIN_LR(layout,  5, D3,      MA2, 36)       // Data 3 / AD2
    PIN_LR(layout,  6, D4,      MA3, 35)       // Data 4 / AD3
    PIN_LR(layout,  7, D5,      MA4, 34)       // Data 5 / AD4
    PIN_LR(layout,  8, D6,      MA5, 33)       // Data 6 / AD5
    PIN_LR(layout,  9, D7,      MA6, 32)       // Data 7 / AD6
    PIN_LR(layout, 10, A2,      MA7, 31)       // CPU Address 2 / AD7
    PIN_LR(layout, 11, A1,      A8, 30)        // CPU Address 1 / PPU Address 8
    PIN_LR(layout, 12, A0,      A9, 29)        // CPU Address 0 / PPU Address 9
    PIN_LR(layout, 13, CS,      A10, 28)       // /Chip Select / PPU Address 10
    PIN_LR(layout, 14, UNKNOWN, A11, 27)       // EXT0 / PPU Address 11
    PIN_LR(layout, 15, UNKNOWN, A12, 26)       // EXT1 / PPU Address 12
    PIN_LR(layout, 16, UNKNOWN, A13, 25)       // EXT2 / PPU Address 13
    PIN_LR(layout, 17, UNKNOWN, UNKNOWN, 24)   // EXT3 / /RD
    PIN_LR(layout, 18, UNKNOWN, WE, 23)        // CLK / /WR
    PIN_LR(layout, 19, NMI,     RES, 22)       // /INT (NMI output) / /RST
    PIN_LR(layout, 20, VSS,     UNKNOWN, 21)   // Ground / VOUT (composite video)

    return layout;
}

// ============================================================================
// PPU PIN STATES
// ============================================================================

static std::vector<PinSignalState> get_ppu_pin_states(nes_system::PPU* ppu, const ChipLayout* layout) {
    std::vector<PinSignalState> pin_states;
    if (!ppu || !layout) return pin_states;

    int total_pins = layout->get_total_pins();
    pin_states.resize(total_pins);

    for (int i = 0; i < total_pins; i++) {
        pin_states[i] = PinSignalState{
            .pin_number = static_cast<uint8_t>(i + 1),
            .signal_level = false,
            .drive_direction = false,
            .signal_value = 0,
            .high_impedance = true,
            .has_pullup = false,
            .has_pulldown = false,
            .signal_valid = true,
            .analog_voltage = 0.0f,
            .is_pwm = false,
            .pwm_duty_cycle = 0.0f
        };
    }

    // Power pins
    pin_states[19].signal_level = false; // VSS (Ground, pin 20)
    pin_states[19].high_impedance = false;
    pin_states[39].signal_level = true;  // VCC (+5V, pin 40)
    pin_states[39].high_impedance = false;

    // /INT (NMI) pin (pin 19, index 18) — active low
    pin_states[18].signal_level = !ppu->nmi;
    pin_states[18].drive_direction = true;
    pin_states[18].high_impedance = false;

    // VOUT (pin 21, index 20) — composite video, always driven
    pin_states[20].signal_level = true;
    pin_states[20].drive_direction = true;
    pin_states[20].high_impedance = false;

    return pin_states;
}

static ChipLayout& get_ppu_layout() {
    static ChipLayout layout = create_ricoh_2c02_layout();
    return layout;
}

// ============================================================================
// ChipBase interface implementation
// ============================================================================

ChipIdentity nes_system::PPU::chip_identity() const {
    return {"RP2C02", "Ricoh"};
}

bool nes_system::PPU::has_debug_content()    const { return true; }
bool nes_system::PPU::has_settings_content() const { return true; }
bool nes_system::PPU::has_layout_content()   const { return true; }

// ============================================================================
// NES PPU GUI DEBUG WINDOW
// ============================================================================

void nes_system::PPU::render_debug_content() {
    auto* ppu = this;

#ifdef IMGUI_VERSION
    // Two-column layout
    ImVec2 window_size = ImGui::GetContentRegionAvail();

    ImVec2 chip_viz_size = ImVec2(250.0f, 0);
    if (ImGui::BeginChild("ChipVisualization", chip_viz_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Chip Visualization");
        ImGui::Separator();

        ImVec2 chip_center = ImGui::GetCursorScreenPos();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        chip_center.x += content_region.x * 0.5f;
        chip_center.y += 200.0f;

        ChipVisualization& renderer = GetGlobalChipRenderer();
        ChipLayout& layout = get_ppu_layout();
        std::vector<PinSignalState> pin_states = get_ppu_pin_states(ppu, &layout);
        renderer.render(layout, chip_center, pin_states, "RP2C02");
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 5.0f);

    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0);
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Ricoh 2C02 PPU (Picture Processing Unit)");
        ImGui::Separator();

        // Timing
        if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Scanline:     %d", ppu->scanline);
            ImGui::Text("Cycle:        %d / 341", ppu->cycle);
            ImGui::Text("Frame:        %llu", (unsigned long long)ppu->frame_count);
            ImGui::Text("Region:       %s", ppu->is_pal ? "PAL" : "NTSC");
            ImGui::Text("Frame Done:   %s", ppu->frame_complete ? "YES" : "NO");
            ImGui::Text("NMI Pending:  %s", ppu->nmi ? "YES" : "NO");

            // Scanline progress
            int total_scanlines = ppu->is_pal ? 312 : 262;
            float scanline_progress = (float)(ppu->scanline + 1) / (float)total_scanlines;
            ImGui::ProgressBar(scanline_progress, ImVec2(-1, 0), NULL);
        }

        // Registers
        if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("PPUCTRL   ($2000): $%02X", ppu->regs.ctrl);
            ImGui::Indent(20.0f);
            ImGui::Text("NMI Enable:       %d", (ppu->regs.ctrl >> 7) & 1);
            ImGui::Text("Master/Slave:     %d", (ppu->regs.ctrl >> 6) & 1);
            ImGui::Text("Sprite Size:      %s", (ppu->regs.ctrl & 0x20) ? "8x16" : "8x8");
            ImGui::Text("BG Pattern Base:  $%04X", (ppu->regs.ctrl & 0x10) ? 0x1000 : 0x0000);
            ImGui::Text("SPR Pattern Base: $%04X", (ppu->regs.ctrl & 0x08) ? 0x1000 : 0x0000);
            ImGui::Text("VRAM Increment:   %d", (ppu->regs.ctrl & 0x04) ? 32 : 1);
            ImGui::Text("Base Nametable:   $%04X", 0x2000 + (ppu->regs.ctrl & 0x03) * 0x400);
            ImGui::Unindent(20.0f);

            ImGui::Text("PPUMASK   ($2001): $%02X", ppu->regs.mask);
            ImGui::Indent(20.0f);
            ImGui::Text("Emph Blue:        %d", (ppu->regs.mask >> 7) & 1);
            ImGui::Text("Emph Green:       %d", (ppu->regs.mask >> 6) & 1);
            ImGui::Text("Emph Red:         %d", (ppu->regs.mask >> 5) & 1);
            ImGui::Text("Show Sprites:     %d", (ppu->regs.mask >> 4) & 1);
            ImGui::Text("Show Background:  %d", (ppu->regs.mask >> 3) & 1);
            ImGui::Text("Show Left SPR:    %d", (ppu->regs.mask >> 2) & 1);
            ImGui::Text("Show Left BG:     %d", (ppu->regs.mask >> 1) & 1);
            ImGui::Text("Greyscale:        %d", ppu->regs.mask & 1);
            ImGui::Unindent(20.0f);

            ImGui::Text("PPUSTATUS ($2002): $%02X", ppu->regs.status);
            ImGui::Indent(20.0f);
            ImGui::Text("VBlank:           %d", (ppu->regs.status >> 7) & 1);
            ImGui::Text("Sprite 0 Hit:     %d", (ppu->regs.status >> 6) & 1);
            ImGui::Text("Sprite Overflow:  %d", (ppu->regs.status >> 5) & 1);
            ImGui::Unindent(20.0f);

            ImGui::Text("OAM Addr  ($2003): $%02X", ppu->regs.oam_addr);
        }

        // Internal State
        if (ImGui::CollapsingHeader("Internal State")) {
            ImGui::Text("VRAM Addr (v):    $%04X", ppu->internal.v);
            ImGui::Text("Temp Addr (t):    $%04X", ppu->internal.t);
            ImGui::Text("Fine X Scroll:    %d", ppu->internal.x);
            ImGui::Text("Write Toggle (w): %d", ppu->internal.w ? 1 : 0);
            ImGui::Text("Fine Y:           %d", ppu->internal.fine_y);
        }

        // Palette
        if (ImGui::CollapsingHeader("Palette RAM")) {
            for (int i = 0; i < 32; i += 4) {
                ImGui::Text("$%02X: %02X %02X %02X %02X",
                            i, ppu->palette[i], ppu->palette[i+1],
                            ppu->palette[i+2], ppu->palette[i+3]);
            }
        }
    }
    ImGui::EndChild();
#endif
}

// ============================================================================
// NES PPU GUI SETTINGS
// ============================================================================

void nes_system::PPU::render_settings_content() {
    auto* ppu = this;

#ifdef IMGUI_VERSION

    ImGui::Text("Ricoh 2C02 PPU Configuration");
    ImGui::Separator();
    ImGui::Text("Chip Type: Ricoh RP2C02 (40-pin DIP)");
    ImGui::Text("Region: %s", ppu->is_pal ? "PAL (2C07)" : "NTSC (2C02)");
    ImGui::Text("PPU Clock: %s", ppu->is_pal ? "5.32 MHz" : "5.37 MHz");

    ImGui::Separator();

    // Memory summary
    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("VRAM:    %zu bytes", ppu->vram.size());
        ImGui::Text("OAM:     %zu bytes", ppu->oam.size());
        ImGui::Text("Palette: %zu bytes", ppu->palette.size());
    }
#endif
}

// ============================================================================
// NES PPU LAYOUT (standalone pinout diagram)
// ============================================================================

void nes_system::PPU::render_layout_content() {
    auto* ppu = this;

#ifdef IMGUI_VERSION
    ChipLayout& layout = get_ppu_layout();
    std::vector<PinSignalState> pin_states = get_ppu_pin_states(ppu, &layout);
    render_chip_layout(layout, pin_states, "RP2C02");
#endif
}