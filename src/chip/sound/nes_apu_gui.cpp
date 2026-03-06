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

// ============================================================================
// RICOH 2A03 LAYOUT (40-pin DIP) — APU-focused view
// ============================================================================
// Same physical package as the CPU; we re-render it here with emphasis
// on the audio output pins SND1 (35) and SND2 (34).

inline ChipLayout create_ricoh_2a03_apu_layout() {
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
}

// ============================================================================
// ChipBase interface implementation
// ============================================================================

bool nes6502_apu::APU::has_debug_content()    const { return true; }
bool nes6502_apu::APU::has_settings_content() const { return true; }
bool nes6502_apu::APU::has_layout_content()   const { return true; }

#ifdef CERMU_HAS_GUI

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

// ============================================================================
// DEBUG WINDOW
// ============================================================================

void nes6502_apu::APU::render_debug_content() {
    auto* apu = this;

    float avail_w = ImGui::GetContentRegionAvail().x;
    float chip_w  = 250.0f;
    float info_w  = avail_w - chip_w - 8.0f;

    // --- Chip visualization (left) ---
    ImGui::BeginChild("##apu_chip_viz", ImVec2(chip_w, 0), true);
    {
        static ChipLayout layout = create_ricoh_2a03_apu_layout();
        auto pin_states = get_apu_pin_states(apu, &layout, apu->bus_snapshot_);

        ImVec2 region = ImGui::GetContentRegionAvail();
        ImVec2 center(
            ImGui::GetCursorScreenPos().x + region.x * 0.5f,
            ImGui::GetCursorScreenPos().y + region.y * 0.5f);

        GetGlobalChipRenderer().render(layout, center, pin_states, "RP2A03");
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // --- Debug info (right) ---
    ImGui::BeginChild("##apu_debug_info", ImVec2(info_w, 0), true);
    {
        // ---- Status register ($4015) ----
        if (ImGui::CollapsingHeader("Status ($4015)",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            bool p1_active  = apu->pulse1.length.active();
            bool p2_active  = apu->pulse2.length.active();
            bool tri_active = apu->triangle.length.active();
            bool noi_active = apu->noise.length.active();
            bool dmc_active = apu->dmc.active();
            bool frame_irq  = apu->frame.irq_flag;
            bool dmc_irq    = apu->dmc.irq_flag;

            ImGui::Text("Channel Enable:");
            ImGui::SameLine();
            ImGui::TextColored(p1_active ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "P1");
            ImGui::SameLine();
            ImGui::TextColored(p2_active ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "P2");
            ImGui::SameLine();
            ImGui::TextColored(tri_active ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "TRI");
            ImGui::SameLine();
            ImGui::TextColored(noi_active ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "NOI");
            ImGui::SameLine();
            ImGui::TextColored(dmc_active ? ImVec4(0,1,0,1) : ImVec4(0.5f,0.5f,0.5f,1), "DMC");

            ImGui::Text("IRQ: Frame=%s  DMC=%s",
                frame_irq ? "SET" : "clr",
                dmc_irq   ? "SET" : "clr");
        }

        ImGui::Separator();

        // ---- Pulse 1 ($4000-$4003) ----
        if (ImGui::CollapsingHeader("Pulse 1 ($4000-$4003)",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& p = apu->pulse1;
            ImGui::Text("Duty:   %d (%.1f%%)",
                p.duty, p.duty == 0 ? 12.5f : p.duty == 1 ? 25.0f : p.duty == 2 ? 50.0f : 75.0f);
            ImGui::Text("Timer:  %d  (period $%03X)",
                p.output(), p.timer_period);
            ImGui::Text("Length: %s  counter=%d",
                p.length.active() ? "active" : "off",
                p.length.value());

            // Envelope
            ImGui::Text("Envelope: const=%s  vol=%d  loop=%s",
                p.envelope.constant_volume ? "Y" : "N",
                p.envelope.volume(),
                p.envelope.loop ? "Y" : "N");

            // Sweep
            ImGui::Text("Sweep: en=%s  period=%d  negate=%s  shift=%d",
                p.sweep.enabled ? "Y" : "N",
                p.sweep.period,
                p.sweep.negate ? "Y" : "N",
                p.sweep.shift);
            ImGui::Text("  muting=%s  target=%d",
                p.sweep.is_muting(p.timer_period) ? "Y" : "N",
                p.sweep.calculate_target(p.timer_period));
        }

        // ---- Pulse 2 ($4004-$4007) ----
        if (ImGui::CollapsingHeader("Pulse 2 ($4004-$4007)",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& p = apu->pulse2;
            ImGui::Text("Duty:   %d (%.1f%%)",
                p.duty, p.duty == 0 ? 12.5f : p.duty == 1 ? 25.0f : p.duty == 2 ? 50.0f : 75.0f);
            ImGui::Text("Timer:  %d  (period $%03X)",
                p.output(), p.timer_period);
            ImGui::Text("Length: %s  counter=%d",
                p.length.active() ? "active" : "off",
                p.length.value());
            ImGui::Text("Envelope: const=%s  vol=%d  loop=%s",
                p.envelope.constant_volume ? "Y" : "N",
                p.envelope.volume(),
                p.envelope.loop ? "Y" : "N");
            ImGui::Text("Sweep: en=%s  period=%d  negate=%s  shift=%d",
                p.sweep.enabled ? "Y" : "N",
                p.sweep.period,
                p.sweep.negate ? "Y" : "N",
                p.sweep.shift);
            ImGui::Text("  muting=%s  target=%d",
                p.sweep.is_muting(p.timer_period) ? "Y" : "N",
                p.sweep.calculate_target(p.timer_period));
        }

        // ---- Triangle ($4008-$400B) ----
        if (ImGui::CollapsingHeader("Triangle ($4008-$400B)",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& t = apu->triangle;
            ImGui::Text("Timer period: $%03X", t.timer_period);
            ImGui::Text("Output: %d", t.output());
            ImGui::Text("Length: %s  counter=%d",
                t.length.active() ? "active" : "off",
                t.length.value());
            ImGui::Text("Linear counter load: %d  control=%s",
                t.linear_counter_load,
                t.control_flag ? "Y" : "N");
        }

        // ---- Noise ($400C-$400F) ----
        if (ImGui::CollapsingHeader("Noise ($400C-$400F)",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& n = apu->noise;
            ImGui::Text("Mode: %s  period_idx=%d",
                n.mode ? "6-bit" : "15-bit",
                n.period_index);
            ImGui::Text("Output: %d", n.output());
            ImGui::Text("Length: %s  counter=%d",
                n.length.active() ? "active" : "off",
                n.length.value());
            ImGui::Text("Envelope: const=%s  vol=%d  loop=%s",
                n.envelope.constant_volume ? "Y" : "N",
                n.envelope.volume(),
                n.envelope.loop ? "Y" : "N");
            ImGui::Text("Shift reg: $%04X", n.shift_register);
        }

        // ---- DMC ($4010-$4013) ----
        if (ImGui::CollapsingHeader("DMC ($4010-$4013)",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& d = apu->dmc;
            ImGui::Text("IRQ en: %s  loop: %s  rate_idx: %d",
                d.irq_enabled ? "Y" : "N",
                d.loop ? "Y" : "N",
                d.rate_index);
            ImGui::Text("Output level: %d / 127", d.output_level);
            ImGui::Text("Sample addr: $%04X  length: %d",
                d.sample_address, d.sample_length);
            ImGui::Text("Current addr: $%04X  remaining: %d",
                d.current_address, d.bytes_remaining);
            ImGui::Text("Active: %s  IRQ flag: %s  Needs sample: %s",
                d.active() ? "Y" : "N",
                d.irq_flag ? "Y" : "N",
                d.needs_sample ? "Y" : "N");
        }

        ImGui::Separator();

        // ---- Frame Counter ($4017) ----
        if (ImGui::CollapsingHeader("Frame Counter ($4017)",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& f = apu->frame;
            ImGui::Text("Mode: %s", f.mode ? "5-step" : "4-step");
            ImGui::Text("IRQ inhibit: %s  IRQ flag: %s",
                f.irq_inhibit ? "Y" : "N",
                f.irq_flag ? "Y" : "N");
        }

        // ---- Mixer Output ----
        if (ImGui::CollapsingHeader("Mixer Output")) {
            float sample = apu->sample();
            ImGui::Text("Mixed output: %.6f", sample);

            // Simple level bar
            float abs_sample = sample < 0 ? -sample : sample;
            ImGui::ProgressBar(abs_sample, ImVec2(-1, 0), "");
        }
    }
    ImGui::EndChild();
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

// ============================================================================
// NES APU LAYOUT (standalone pinout diagram)
// ============================================================================

void nes6502_apu::APU::render_layout_content() {
    auto* apu = this;

    static ChipLayout layout = create_ricoh_2a03_apu_layout();
    auto pin_states = get_apu_pin_states(apu, &layout, apu->bus_snapshot_);
    render_chip_layout(layout, pin_states, "RP2A03");
}

#else // !CERMU_HAS_GUI

void nes6502_apu::APU::render_debug_content() {}
void nes6502_apu::APU::render_settings_content() {}
void nes6502_apu::APU::render_layout_content() {}

#endif
