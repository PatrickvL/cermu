/*
 * nes_ppu_gui.cpp — Ricoh 2C02 PPU Debug/Settings GUI Windows
 *
 * Hardware-accurate 40-pin DIP pinout based on the Ricoh 2C02 datasheet.
 * The PPU (Picture Processing Unit) generates the NES video signal,
 * rendering backgrounds from name tables and sprites from OAM.
 *
 * Pinout reference: Ricoh RP2C02 Datasheet
 */

#include "chip/video/nes_ppu/nes_ppu.hpp"
#include "systems/nes/nes_system.hpp"
#include "core/chip_layout.hpp"
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "gui/chip_visualization.hpp"
#include "gui/global_chip_style.hpp"
#endif
#include <cstdio>

// ============================================================================
// RICOH 2C02 PPU LAYOUT (40-pin DIP)
// ============================================================================

#ifdef CERMU_HAS_GUI

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
// ChipBase interface implementation
// ============================================================================

bool nes_system::PPU::has_settings_content() const { return true; }

ChipLayout* nes_system::PPU::create_chip_layout() const {
    static ChipLayout layout_ntsc = create_ppu_layout(false);
    static ChipLayout layout_pal  = create_ppu_layout(true);
    return is_pal ? &layout_pal : &layout_ntsc;
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

std::vector<PinSignalState> nes_system::PPU::get_layout_pin_states(ChipLayout& layout) {
    return get_ppu_pin_states(this, &layout, bus_snapshot_);
}

const char* nes_system::PPU::get_layout_chip_name() const {
    return is_pal ? "RP2C07" : "RP2C02";
}

// ============================================================================
// NES PPU GUI SETTINGS
// ============================================================================

void nes_system::PPU::render_settings_content() {
    auto* ppu = this;

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
}
#endif // CERMU_HAS_GUI