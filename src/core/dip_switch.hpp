#pragma once
/*
 * dip_switch.hpp — Generic DIP switch model for arcade and other systems
 *
 * Models physical DIP switch banks found on arcade PCBs. Each bank has
 * multiple switches, each controlling one or more bits of a byte register.
 * Switches can be single-bit toggles or multi-bit selectors.
 *
 * Usage:
 *   1. Define static DipSetting arrays for each switch's options.
 *   2. Define a static DipSwitch array for the bank.
 *   3. Define a static DipSwitchBankDescriptor tying them together.
 *   4. Create a DipSwitchBank runtime instance and call init().
 *   5. Read bank.value in I/O handlers (the composed byte).
 *   6. Call render_dip_switch_bank() in your config UI.
 *
 * Designed per MAME's input port model — bit masks, default indices,
 * and named settings map directly to MAME PORT_DIPNAME/PORT_DIPSETTING.
 */

#include <cstdint>
#include <cstring>

// ── Compile-time descriptors (immutable, shared across instances) ────────────

/// One named setting for a DIP switch (e.g. "English" → 0x00)
struct DipSetting {
    const char* name;       // Display name
    uint8_t     value;      // Bit pattern within the switch's mask
};

/// One logical DIP switch (may span 1–8 bits)
struct DipSwitch {
    const char* name;               // Display name (e.g. "Language", "Lives")
    uint8_t     mask;               // Which bits this switch covers
    int         default_index;      // Factory default: index into settings[]
    const DipSetting* settings;     // Array of possible settings
    int         num_settings;       // Length of settings[]
};

/// A complete DIP switch bank (one physical DIP package on the PCB)
struct DipSwitchBankDescriptor {
    const char* name;               // Bank label (e.g. "DSW0", "IN4")
    const DipSwitch* switches;      // Array of switches in this bank
    int         num_switches;       // Length of switches[]
};

// ── Runtime state ────────────────────────────────────────────────────────────

/// Holds current selections for one DIP switch bank.
/// The composed byte value is kept in sync via recompute().
struct DipSwitchBank {
    static constexpr int MAX_SWITCHES = 16;

    const DipSwitchBankDescriptor* descriptor = nullptr;
    int     selections[MAX_SWITCHES] = {};   // Current setting index per switch
    uint8_t value = 0;                       // Composed byte (read by I/O handlers)

    /// Bind to a descriptor and reset all switches to factory defaults.
    void init(const DipSwitchBankDescriptor* desc) {
        descriptor = desc;
        reset_to_defaults();
    }

    /// Reset all switches to their per-switch default_index.
    void reset_to_defaults() {
        if (!descriptor) return;
        for (int i = 0; i < descriptor->num_switches && i < MAX_SWITCHES; i++) {
            selections[i] = descriptor->switches[i].default_index;
        }
        recompute();
    }

    /// Change one switch's selection and recompute the byte.
    void set_selection(int switch_idx, int setting_idx) {
        if (!descriptor) return;
        if (switch_idx < 0 || switch_idx >= descriptor->num_switches) return;
        const auto& sw = descriptor->switches[switch_idx];
        if (setting_idx < 0 || setting_idx >= sw.num_settings) return;
        selections[switch_idx] = setting_idx;
        recompute();
    }

    /// Recompute the composed byte from current selections.
    void recompute() {
        value = 0;
        if (!descriptor) return;
        for (int i = 0; i < descriptor->num_switches && i < MAX_SWITCHES; i++) {
            const auto& sw = descriptor->switches[i];
            int sel = selections[i];
            if (sel >= 0 && sel < sw.num_settings) {
                value = (value & ~sw.mask) | (sw.settings[sel].value & sw.mask);
            }
        }
    }

    /// Find a switch by name. Returns index or -1 if not found.
    int find_switch(const char* name) const {
        if (!descriptor) return -1;
        for (int i = 0; i < descriptor->num_switches; i++) {
            if (std::strcmp(descriptor->switches[i].name, name) == 0)
                return i;
        }
        return -1;
    }

    /// Find a setting by name within a switch. Returns index or -1.
    static int find_setting(const DipSwitch& sw, const char* name) {
        for (int i = 0; i < sw.num_settings; i++) {
            if (std::strcmp(sw.settings[i].name, name) == 0)
                return i;
        }
        return -1;
    }
};

// ── DipSwitchBankComponent — ComponentBase wrapper ───────────────────────────
//
// Promotes DipSwitchBank to a first-class board component so it can
// participate in TypedManifest type lists alongside chips and ports.
// Used only by arcade systems (Atari Vector, Bomb Jack, Namco, …).

#include "core/component_base.hpp"

struct DipSwitchBankComponent : ComponentBase {
    DipSwitchBank bank;

    const char* name() const override {
        return bank.descriptor ? bank.descriptor->name : "DIP Switch";
    }
    void reset() override { bank.reset_to_defaults(); }
};

// Type trait for compile-time dispatch in make_manifest / bind_all.
template<typename T> struct is_dip_switch_component : std::false_type {};
template<> struct is_dip_switch_component<DipSwitchBankComponent> : std::true_type {};
template<typename T> inline constexpr bool is_dip_switch_component_v =
    is_dip_switch_component<T>::value;

// ── GUI rendering (ImGui, compiled only in GUI builds) ───────────────────────

#ifdef CERMU_HAS_GUI
#include <imgui.h>

/// Render combo boxes for all switches in a bank.
/// Returns true if any selection changed.
inline bool render_dip_switch_bank(DipSwitchBank& bank) {
    if (!bank.descriptor) return false;

    bool changed = false;
    const auto* desc = bank.descriptor;

    ImGui::Text("%s", desc->name);
    ImGui::Separator();

    for (int i = 0; i < desc->num_switches; i++) {
        const auto& sw = desc->switches[i];
        int sel = bank.selections[i];

        // Build combo items string (null-separated, double-null terminated)
        char items[512] = {};
        char* p = items;
        for (int j = 0; j < sw.num_settings; j++) {
            size_t len = std::strlen(sw.settings[j].name);
            if (p + len + 2 < items + sizeof(items)) {
                std::memcpy(p, sw.settings[j].name, len);
                p += len;
                *p++ = '\0';
            }
        }
        *p = '\0';  // Double null terminator

        ImGui::PushID(i);
        if (ImGui::Combo(sw.name, &sel, items)) {
            bank.set_selection(i, sel);
            changed = true;
        }
        ImGui::PopID();
    }

    return changed;
}

#endif // CERMU_HAS_GUI
