/*
 * nes_apu_gui.cpp — NES APU (Ricoh 2A03 built-in) Debug/Settings GUI Windows
 *
 * Hardware-accurate 40-pin DIP pinout based on the Ricoh RP2A03 datasheet.
 * The APU (Audio Processing Unit) is integrated into the 2A03 CPU package
 * and provides 5 audio channels: 2 pulse, 1 triangle, 1 noise, 1 DMC.
 *
 * Pinout reference: Ricoh RP2A03 Datasheet
 */

#include "nes_apu.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include "../../gui/chip_visualization.h"
#include "../../gui/global_chip_style.h"
#endif
#include <cstdio>
#include <memory>

#ifdef CERMU_HAS_GUI

// ============================================================================
// ChipBase interface implementation
// ============================================================================

bool nes6502_apu::APU::has_settings_content() const { return true; }

ChipLayout* nes6502_apu::APU::create_chip_layout() const {
    static ChipLayout layout = [] {
        auto layout = create_dip40_layout();
        layout.markings.manufacturer = "Ricoh";
        layout.markings.part_number  = "RP2A03";
        layout.markings.custom_text  = "CPU + APU";

        // Left column  (pins 1-20, top to bottom)
        // Right column (pins 40-21, top to bottom)
        PIN_LR(layout,  1, AD1,    VCC,   40); // audio delta-sigma 1 / +5V
        PIN_LR(layout,  2, AD2,    CLK,   39); // audio delta-sigma 2 / master clock in
        PIN_LR(layout,  3, _RES,   _NMI,  38); // reset / NMI
        PIN_LR(layout,  4, A0,     _IRQ,  37); // addr lo / interrupt
        PIN_LR(layout,  5, A1,     M2,    36); // / CPU clock out
        PIN_LR(layout,  6, A2,     SND1,  35); // / sound output 1
        PIN_LR(layout,  7, A3,     SND2,  34); // / sound output 2
        PIN_LR(layout,  8, A4,     IN0,   33); // / controller 1
        PIN_LR(layout,  9, A5,     IN1,   32); // / controller 2
        PIN_LR(layout, 10, A6,     D0,    31); // / data lo
        PIN_LR(layout, 11, A7,     D1,    30);
        PIN_LR(layout, 12, A8,     D2,    29);
        PIN_LR(layout, 13, A9,     D3,    28);
        PIN_LR(layout, 14, A10,    D4,    27);
        PIN_LR(layout, 15, A11,    D5,    26);
        PIN_LR(layout, 16, A12,    D6,    25);
        PIN_LR(layout, 17, A13,    D7,    24); // addr hi / data hi
        PIN_LR(layout, 18, A14,    OUT0,  23); // / ctrl strobe 0
        PIN_LR(layout, 19, RW,     OUT1,  22); // R/W / ctrl strobe 1
        PIN_LR(layout, 20, VSS,    OUT2,  21); // gnd / ctrl strobe 2

        return layout;
    }();
    return &layout;
}

// ============================================================================
// PIN SIGNAL STATES
// ============================================================================

static std::vector<PinSignalState> get_apu_pin_states(
        nes6502_apu::APU* apu, const ChipLayout* layout, bus_state_t bus_state) {
    if (!apu || !layout) return {};

    // Generic bus-derived pin states (address, data, power, clock, control)
    auto states = populate_pin_states_from_bus(*layout, bus_state);

    // APU specific: SND1 (pin 35, index 34) — pulse + triangle mix (analog, show PWM)
    uint8_t p1   = apu->pulse1.output();
    uint8_t p2   = apu->pulse2.output();
    uint8_t tri  = apu->triangle.output();
    float pulse_sum = (float)(p1 + p2);
    float snd1_mix  = (pulse_sum > 0) ? (95.88f / ((8128.0f / pulse_sum) + 100.0f)) : 0.0f;
    float tri_mix   = (tri > 0) ? (tri / 8227.0f) : 0.0f;
    states[34].signal_level = (snd1_mix + tri_mix) > 0.01f;
    states[34].drive_direction = true;
    states[34].high_impedance = false;
    states[34].is_pwm = true;
    states[34].pwm_duty_cycle = snd1_mix + tri_mix * 0.5f;

    // SND2 (pin 34, index 33) — noise + DMC mix (analog, show PWM)
    uint8_t noi  = apu->noise.output();
    uint8_t dmc  = apu->dmc.output();
    float noi_f  = (float)noi / 12241.0f;
    float dmc_f  = (float)dmc / 22638.0f;
    states[33].signal_level = (noi_f + dmc_f) > 0.001f;
    states[33].drive_direction = true;
    states[33].high_impedance = false;
    states[33].is_pwm = true;
    states[33].pwm_duty_cycle = noi_f + dmc_f;

    // IRQ (pin 37, index 36) — APU can assert IRQ via frame counter or DMC
    states[36].signal_level = !apu->irq(); // active-low
    states[36].drive_direction = true;
    states[36].high_impedance = false;

    return states;
}

std::vector<PinSignalState> nes6502_apu::APU::get_layout_pin_states(ChipLayout& layout) {
    return get_apu_pin_states(this, &layout, bus_snapshot_);
}

// ============================================================================
// SETTINGS
// ============================================================================

void nes6502_apu::APU::render_settings_content() {
    auto* apu = this;

    // Channel output summary
    if (ImGui::CollapsingHeader("Channel Outputs",
            ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(3, "apu_ch_out");
        ImGui::Text("Channel"); ImGui::NextColumn();
        ImGui::Text("Output");  ImGui::NextColumn();
        ImGui::Text("Active");  ImGui::NextColumn();
        ImGui::Separator();

        auto row = [](const char* name, uint8_t out, bool active) {
            ImGui::Text("%s", name); ImGui::NextColumn();
            ImGui::Text("%3d",  out); ImGui::NextColumn();
            ImGui::TextColored(
                active ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1),
                "%s", active ? "Yes" : "No");
            ImGui::NextColumn();
        };
        row("Pulse 1",  apu->pulse1.output(),   apu->pulse1.length.active());
        row("Pulse 2",  apu->pulse2.output(),   apu->pulse2.length.active());
        row("Triangle", apu->triangle.output(),  apu->triangle.length.active());
        row("Noise",    apu->noise.output(),     apu->noise.length.active());
        row("DMC",      apu->dmc.output(),       apu->dmc.active());
        ImGui::Columns(1);
    }

    // Register-level view (MMIO addresses)
    if (ImGui::CollapsingHeader("APU Register Map")) {
        ImGui::Text("$4000-$4003 : Pulse 1");
        ImGui::Text("$4004-$4007 : Pulse 2");
        ImGui::Text("$4008-$400B : Triangle");
        ImGui::Text("$400C-$400F : Noise");
        ImGui::Text("$4010-$4013 : DMC");
        ImGui::Text("$4015       : Status");
        ImGui::Text("$4017       : Frame Counter");
    }
}

#endif // CERMU_HAS_GUI
