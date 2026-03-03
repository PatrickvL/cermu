/*
 * nes_ppu_gui.cpp — Ricoh 2C02 PPU Debug/Settings GUI Windows
 *
 * Hardware-accurate 40-pin DIP pinout based on the Ricoh 2C02 datasheet.
 * The PPU (Picture Processing Unit) generates the NES video signal,
 * rendering backgrounds from name tables and sprites from OAM.
 *
 * Pinout reference: Ricoh RP2C02 Datasheet
 */

#include "nes_ppu.h"
#include "../nes_system.h"
#include "../../../core/chip_layout.h"
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../../gui/chip_visualization.h"
#include "../../../gui/global_chip_style.h"
#endif
#include <cstdio>

// ============================================================================
// RICOH 2C02 PPU LAYOUT (40-pin DIP)
// ============================================================================

inline ChipLayout create_ppu_layout(bool pal) {
    ChipLayout layout = create_dip40_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    layout.markings = {
        pal ? "RP2C07" : "RP2C02",
        "Ricoh",
        {}, {}, {}, {},
        true, true, false, false
    };

    // Hardware-accurate Ricoh 2C02 PPU pinout (40-pin DIP)
    PIN_LR(layout,  1, RW,      VDD, 40)       // R/W / +5V
    PIN_LR(layout,  2, D0,      ALE, 39)       // data lo / addr latch en
    PIN_LR(layout,  3, D1,      MA0, 38)       // / PPU mux addr lo
    PIN_LR(layout,  4, D2,      MA1, 37)
    PIN_LR(layout,  5, D3,      MA2, 36)
    PIN_LR(layout,  6, D4,      MA3, 35)
    PIN_LR(layout,  7, D5,      MA4, 34)
    PIN_LR(layout,  8, D6,      MA5, 33)
    PIN_LR(layout,  9, D7,      MA6, 32)       // data hi
    PIN_LR(layout, 10, A2,      MA7, 31)       // CPU addr / mux addr hi
    PIN_LR(layout, 11, A1,      A8, 30)        // / PPU addr hi
    PIN_LR(layout, 12, A0,      A9, 29)        // CPU addr lo
    PIN_LR(layout, 13, _CS,     A10, 28)       // chip sel / PPU addr
    PIN_LR(layout, 14, EXT0,    A11, 27)       // ext port lo
    PIN_LR(layout, 15, EXT1,    A12, 26)
    PIN_LR(layout, 16, EXT2,    A13, 25)       // ext port hi / PPU addr hi
    PIN_LR(layout, 17, EXT3,    _RD, 24)       // / VRAM read strobe
    PIN_LR(layout, 18, CLK,     _WE, 23)       // master clock / VRAM write
    PIN_LR(layout, 19, _NMI,    _RES, 22)      // interrupt out / reset
    PIN_LR(layout, 20, VSS,     VOUT, 21)      // gnd / composite video

    return layout;
}

// ============================================================================
// PPU PIN STATES
// ============================================================================

static std::vector<PinSignalState> get_ppu_pin_states(nes_system::PPU* ppu, const ChipLayout* layout, bus_state_t bus_state) {
    if (!ppu || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto pin_states = populate_pin_states_from_bus(*layout, bus_state);

    // PPU specific: /INT (NMI) pin (pin 19, index 18) — PPU drives NMI
    // Read directly from the PPU bus snapshot (active-low: bit HIGH = not asserted)
    pin_states[18].signal_level = PPU_BUS_GET_BIT(ppu->bus_snapshot_, BUS_NMI_BIT);
    pin_states[18].drive_direction = true;
    pin_states[18].high_impedance = false;

    // VOUT (pin 21, index 20) — composite video, always driven
    pin_states[20].signal_level = true;
    pin_states[20].drive_direction = true;
    pin_states[20].high_impedance = false;

    return pin_states;
}

static ChipLayout& get_ppu_layout(bool pal) {
    static bool cached_pal = pal;
    static ChipLayout layout = create_ppu_layout(pal);
    if (cached_pal != pal) {
        cached_pal = pal;
        layout = create_ppu_layout(pal);
    }
    return layout;
}

// ============================================================================
// ChipBase interface implementation
// ============================================================================

bool nes_system::PPU::has_debug_content()    const { return true; }
bool nes_system::PPU::has_settings_content() const { return true; }
bool nes_system::PPU::has_layout_content()   const { return true; }

// ============================================================================
// NES PPU GUI DEBUG WINDOW
// ============================================================================

void nes_system::PPU::render_debug_content() {
    auto* ppu = this;

#ifdef CERMU_HAS_GUI
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
        ChipLayout& layout = get_ppu_layout(ppu->is_pal);
        std::vector<PinSignalState> pin_states = get_ppu_pin_states(ppu, &layout, ppu->bus_snapshot_);
        renderer.render(layout, chip_center, pin_states, ppu->is_pal ? "RP2C07" : "RP2C02");
    }
    ImGui::EndChild();

    ImGui::SameLine(0, 5.0f);

    ImVec2 right_column_size = ImVec2(window_size.x - 270.0f, 0);
    if (ImGui::BeginChild("DebugInfo", right_column_size, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::Text("Ricoh %s PPU (Picture Processing Unit)", ppu->is_pal ? "2C07" : "2C02");
        ImGui::Separator();

        // Timing
        if (ImGui::CollapsingHeader("Timing", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Scanline:     %d", ppu->scanline);
            ImGui::Text("Cycle:        %d / %u", ppu->cycle, nes_constants::DOTS_PER_SCANLINE);
            ImGui::Text("Frame:        %llu", (unsigned long long)ppu->frame_count);
            ImGui::Text("Region:       %s", ppu->is_pal ? "PAL" : "NTSC");
            ImGui::Text("Frame Done:   %s", ppu->frame_complete ? "YES" : "NO");
            ImGui::Text("NMI Internal: %s", ppu->vbl_flag_internal_ ? "YES" : "NO");

            // Scanline progress
            int total_scanlines = ppu->is_pal ? nes_constants::TOTAL_SCANLINES_PAL : nes_constants::TOTAL_SCANLINES_NTSC;
            float scanline_progress = (float)(ppu->scanline + 1) / (float)total_scanlines;
            ImGui::ProgressBar(scanline_progress, ImVec2(-1, 0), NULL);
        }

        // Registers
        if (ImGui::CollapsingHeader("Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("PPUCTRL   ($2000): $%02X", ppu->regs[PPUCTRL]);
            ImGui::Indent(20.0f);
            ImGui::Text("NMI Enable:       %d", (ppu->regs[PPUCTRL] >> 7) & 1);
            ImGui::Text("Master/Slave:     %d", (ppu->regs[PPUCTRL] >> 6) & 1);
            ImGui::Text("Sprite Size:      %s", (ppu->regs[PPUCTRL] & 0x20) ? "8x16" : "8x8");
            ImGui::Text("BG Pattern Base:  $%04X", (ppu->regs[PPUCTRL] & 0x10) ? 0x1000 : 0x0000);
            ImGui::Text("SPR Pattern Base: $%04X", (ppu->regs[PPUCTRL] & 0x08) ? 0x1000 : 0x0000);
            ImGui::Text("VRAM Increment:   %d", (ppu->regs[PPUCTRL] & 0x04) ? 32 : 1);
            ImGui::Text("Base Nametable:   $%04X", 0x2000 + (ppu->regs[PPUCTRL] & 0x03) * 0x400);
            ImGui::Unindent(20.0f);

            ImGui::Text("PPUMASK   ($2001): $%02X", ppu->regs[PPUMASK]);
            ImGui::Indent(20.0f);
            ImGui::Text("Emph Blue:        %d", (ppu->regs[PPUMASK] >> 7) & 1);
            ImGui::Text("Emph Green:       %d", (ppu->regs[PPUMASK] >> 6) & 1);
            ImGui::Text("Emph Red:         %d", (ppu->regs[PPUMASK] >> 5) & 1);
            ImGui::Text("Show Sprites:     %d", (ppu->regs[PPUMASK] >> 4) & 1);
            ImGui::Text("Show Background:  %d", (ppu->regs[PPUMASK] >> 3) & 1);
            ImGui::Text("Show Left SPR:    %d", (ppu->regs[PPUMASK] >> 2) & 1);
            ImGui::Text("Show Left BG:     %d", (ppu->regs[PPUMASK] >> 1) & 1);
            ImGui::Text("Greyscale:        %d", ppu->regs[PPUMASK] & 1);
            ImGui::Unindent(20.0f);

            ImGui::Text("PPUSTATUS ($2002): $%02X", ppu->regs[PPUSTATUS]);
            ImGui::Indent(20.0f);
            ImGui::Text("VBlank:           %d", (ppu->regs[PPUSTATUS] >> 7) & 1);
            ImGui::Text("Sprite 0 Hit:     %d", (ppu->regs[PPUSTATUS] >> 6) & 1);
            ImGui::Text("Sprite Overflow:  %d", (ppu->regs[PPUSTATUS] >> 5) & 1);
            ImGui::Unindent(20.0f);

            ImGui::Text("OAM Addr  ($2003): $%02X", ppu->regs[OAMADDR]);
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

#ifdef CERMU_HAS_GUI

    ImGui::Text("Ricoh %s PPU Configuration", ppu->is_pal ? "2C07" : "2C02");
    ImGui::Separator();
    ImGui::Text("Chip Type: Ricoh %s (40-pin DIP)", ppu->is_pal ? "RP2C07" : "RP2C02");
    ImGui::Text("Region: %s", ppu->is_pal ? "PAL (2C07)" : "NTSC (2C02)");
    ImGui::Text("PPU Clock: %s", ppu->is_pal ? "5.32 MHz" : "5.37 MHz");

    ImGui::Separator();

    // Memory summary
    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("VRAM:    %zu bytes", PPU::CIRAM_SIZE);
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

#ifdef CERMU_HAS_GUI
    ChipLayout& layout = get_ppu_layout(ppu->is_pal);
    std::vector<PinSignalState> pin_states = get_ppu_pin_states(ppu, &layout, ppu->bus_snapshot_);
    render_chip_layout(layout, pin_states, ppu->is_pal ? "RP2C07" : "RP2C02");
#endif
}