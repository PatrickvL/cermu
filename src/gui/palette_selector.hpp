#pragma once
/*
 * palette_selector.hpp — Generic palette combo for any VideoChipBase subclass
 *
 * Systems include this from their render_configuration_ui() under
 * #ifdef CERMU_HAS_GUI.  The function renders an ImGui combo populated
 * from the chip's named palette registry and applies the selection via
 * the system's configuration path.
 *
 * Returns true if the user changed the selection (caller should persist).
 */

#include "chip/video/video_chip_base.hpp"
#include <imgui.h>
#include <string>
#include <map>

namespace palette_selector {

// Render a palette combo for a video chip that has named palettes.
// - chip:     the video chip with named palette registry
// - settings: mutable reference to config_.custom_settings
// - key:      settings key (default "display_palette")
//
// Returns true if the user changed the selection.  The caller is
// responsible for calling set_configuration() / apply_configuration().
inline bool render(VideoChipBase& chip,
                   std::map<std::string, std::string>& settings,
                   const char* key = "display_palette") {
    const NamedPalette* palettes = chip.named_palettes();
    const int count = chip.named_palette_count();
    if (count < 2) return false;  // Nothing to select

    auto it = settings.find(key);
    const char* current_id = (it != settings.end()) ? it->second.c_str()
                                                    : palettes[0].id;
    int selected = 0;
    for (int i = 0; i < count; ++i) {
        if (std::strcmp(current_id, palettes[i].id) == 0) { selected = i; break; }
    }

    bool changed = ImGui::Combo("Display Palette", &selected,
        [](void* data, int idx) -> const char* {
            return static_cast<const NamedPalette*>(data)[idx].name;
        }, const_cast<NamedPalette*>(palettes), count);

    if (changed) {
        settings[key] = palettes[selected].id;
    }
    return changed;
}

} // namespace palette_selector
