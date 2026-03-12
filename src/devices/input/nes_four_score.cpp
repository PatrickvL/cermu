/**
 * nes_four_score.cpp — NES Four Score (NES-034) Implementation
 *
 * The Four Score is a 4-player adapter for the NES.  Two units are used,
 * one plugged into each controller port.  Each unit provides two controller
 * slots connected to the NES via a 24-bit serial shift protocol:
 *
 *   Bits  1–8:  Primary player buttons   (Player 1 or 2)
 *   Bits  9–16: Secondary player buttons  (Player 3 or 4)
 *   Bits 17–24: Signature byte            ($10 for port 1, $08 for port 2)
 *
 * Games detect the Four Score by checking the signature bytes after
 * reading 24 bits from each port.
 */

#include "devices/input/nes_four_score.hpp"
#include "core/device_registry.hpp"

#include <cstdio>
#include <SDL_events.h>

using ConnectorSignals::NESControllerBit;

// ============================================================================
// CONSTRUCTION / RESET
// ============================================================================

NesFourScore::NesFourScore() {
    primary_   = std::make_unique<NesStandardController>();
    secondary_ = std::make_unique<NesStandardController>();

    // Default binding: keyboard
    binding_.type  = HostInputType::KEYBOARD;
    binding_.label = "Keyboard";

    // Default keymaps: primary gets WASD, secondary gets IJKL
    primary_->apply_keymap_preset(0);    // WASD
    secondary_->apply_keymap_preset(1);  // IJKL
}

void NesFourScore::reset() {
    shift_register_ = 0;
    shift_count_ = 0;
    latch_was_high_ = false;
    clk_was_high_ = false;
    output_signals_ = 0xFFFFFFFF;
    primary_->reset();
    secondary_->reset();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void NesFourScore::on_attach(ConnectorPort* port) {
    PeripheralDevice::on_attach(port);

    // Auto-detect signature from port index:
    //   Port index 1 → Port 1 side → signature $10
    //   Port index 2 → Port 2 side → signature $08
    if (port) {
        int idx = port->get_port_index();
        signature_ = (idx == 2) ? SIGNATURE_PORT2 : SIGNATURE_PORT1;
        printf("Four Score: Attached to port %d (signature $%02X)\n", idx, signature_);
    }
}

// ============================================================================
// HOST INPUT
// ============================================================================

void NesFourScore::set_host_input_binding(const HostInputBinding& binding) {
    on_input_source_will_change();
    binding_ = binding;
    // Forward binding type to sub-controllers
    primary_->set_host_input_binding(binding);
    secondary_->set_host_input_binding(binding);
}

void NesFourScore::on_input_source_will_change() {
    output_signals_ = 0xFFFFFFFF;
}

bool NesFourScore::process_sdl_event(const SDL_Event& event) {
    // Forward to both sub-controllers — each only consumes events
    // matching its own keymap, so there's no conflict
    bool consumed = false;
    consumed |= primary_->process_sdl_event(event);
    consumed |= secondary_->process_sdl_event(event);
    return consumed;
}

// ============================================================================
// SIGNAL PROTOCOL — 24-BIT SHIFT REGISTER
// ============================================================================

void NesFourScore::on_signal_change(uint32_t signal_state) {
    bool latch_now = (signal_state & (1u << NESControllerBit::NES_LATCH)) != 0;
    bool clk_now   = (signal_state & (1u << NESControllerBit::NES_CLK))   != 0;

    // --- LATCH rising edge: capture button states and build 24-bit word ---
    if (latch_now && !latch_was_high_) {
        uint8_t primary_buttons   = primary_->get_button_state();
        uint8_t secondary_buttons = secondary_->get_button_state();

        // Build 24-bit shift register: [primary][secondary][signature]
        // MSB of primary_buttons is shifted out first
        shift_register_ = (static_cast<uint32_t>(primary_buttons)   << 16)
                        | (static_cast<uint32_t>(secondary_buttons) << 8)
                        | static_cast<uint32_t>(signature_);
        shift_count_ = 0;
    }

    latch_was_high_ = latch_now;

    // --- CLK rising edge (while LATCH is low): shift out next bit ---
    if (clk_now && !clk_was_high_ && !latch_now) {
        if (shift_count_ < 24) {
            shift_register_ <<= 1;
            shift_count_++;
        }
    }

    clk_was_high_ = clk_now;

    // --- Drive D0 from current MSB of shift register (active-low) ---
    uint8_t current_bit = 0;
    if (shift_count_ < 24) {
        current_bit = (shift_register_ >> 23) & 1;
    }
    // else: after 24 bits, D0 = released (high on bus)

    if (current_bit) {
        output_signals_ &= ~(1u << NESControllerBit::NES_D0);
    } else {
        output_signals_ |= (1u << NESControllerBit::NES_D0);
    }

    if (port_) port_->notify_device_output_changed(output_signals_);
}

// ============================================================================
// KEYMAP PRESETS (delegates to sub-controllers)
// ============================================================================

// The Four Score doesn't have its own keymap presets — each sub-controller
// manages its own.  We expose the primary controller's presets at the
// Four Score level for the generic UI, but render_input_source_settings_ui()
// provides per-controller preset selection.

const ControllerKeyMapPreset* NesFourScore::get_keymap_presets_table() const {
    // Not used, we override the public API directly
    return nullptr;
}

int NesFourScore::get_keymap_presets_table_size() const {
    return 0;
}

void NesFourScore::apply_keymap_preset(int index) {
    primary_->apply_keymap_preset(index);
}

int NesFourScore::get_active_keymap_preset() const {
    return primary_->get_active_keymap_preset();
}

int NesFourScore::get_keymap_preset_count() const {
    return primary_->get_keymap_preset_count();
}

const ControllerKeyMapPreset& NesFourScore::get_keymap_preset(int index) const {
    return primary_->get_keymap_preset(index);
}

// ============================================================================
// GUI
// ============================================================================

#ifdef CERMU_HAS_GUI
void NesFourScore::render_device_ui() {
    const char* port_label = (signature_ == SIGNATURE_PORT2) ? "Port 2" : "Port 1";
    ImGui::Text("Four Score (%s)  Sig: $%02X", port_label, signature_);

    // Primary controller state
    uint8_t p = primary_->get_button_state();
    ImGui::Text("  Primary:   %s %s %s %s  %s %s  %s %s",
                (p & NesStandardController::UP)     ? "U" : ".",
                (p & NesStandardController::DOWN)   ? "D" : ".",
                (p & NesStandardController::LEFT)   ? "L" : ".",
                (p & NesStandardController::RIGHT)  ? "R" : ".",
                (p & NesStandardController::SELECT) ? "Se" : "..",
                (p & NesStandardController::START)  ? "St" : "..",
                (p & NesStandardController::B)      ? "B" : ".",
                (p & NesStandardController::A)      ? "A" : ".");

    // Secondary controller state
    uint8_t s = secondary_->get_button_state();
    ImGui::Text("  Secondary: %s %s %s %s  %s %s  %s %s",
                (s & NesStandardController::UP)     ? "U" : ".",
                (s & NesStandardController::DOWN)   ? "D" : ".",
                (s & NesStandardController::LEFT)   ? "L" : ".",
                (s & NesStandardController::RIGHT)  ? "R" : ".",
                (s & NesStandardController::SELECT) ? "Se" : "..",
                (s & NesStandardController::START)  ? "St" : "..",
                (s & NesStandardController::B)      ? "B" : ".",
                (s & NesStandardController::A)      ? "A" : ".");

    ImGui::Text("  Shift: %d/24 bits  Reg: $%06X", shift_count_, shift_register_);
    render_input_source_badge();
}

void NesFourScore::render_input_source_settings_ui() {
    // Per-sub-controller keymap preset selection
    if (binding_.type != HostInputType::KEYBOARD) return;

    auto render_preset_combo = [](const char* label, NesStandardController* ctrl) {
        if (!ctrl) return;
        int active = ctrl->get_active_keymap_preset();
        int count  = ctrl->get_keymap_preset_count();
        if (count == 0) return;

        const char* preview = (active >= 0) ? ctrl->get_keymap_preset(active).name : "Custom";
        if (ImGui::BeginCombo(label, preview)) {
            for (int i = 0; i < count; i++) {
                const auto& preset = ctrl->get_keymap_preset(i);
                bool selected = (i == active);
                if (ImGui::Selectable(preset.name, selected)) {
                    ctrl->apply_keymap_preset(i);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    };

    render_preset_combo("Primary Keys", primary_.get());
    render_preset_combo("Secondary Keys", secondary_.get());
}
#endif

// ============================================================================
// SELF-REGISTRATION
// ============================================================================

static const DeviceDescriptor nes_four_score_descriptor = {
    "nes_four_score",
    "NES Four Score",
    "NES-034 Four Score 4-player adapter (provides 2 controller slots per port)",
    ConnectorType::CONTROLLER_NES,
    false
};

REGISTER_DEVICE(nes_four_score_descriptor, []() {
    return std::make_unique<NesFourScore>();
})
