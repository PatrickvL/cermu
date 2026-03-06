/*
 * nes_cartridge_gui.cpp — NES Cartridge debug content
 *
 * Renders cartridge information in the Hardware menu: mapper ID, PRG/CHR
 * bank counts, memory sizes, mirroring mode, battery-backed status, and
 * IRQ state.
 *
 * No pin layout is provided because the cartridge is a PCB module with
 * an edge connector, not a standard IC in a DIP package.
 */

#include "nes_cartridge.h"

#ifdef CERMU_HAS_GUI
#include <imgui.h>
#include <cstdio>

namespace nes_system {

// ============================================================================
// Helpers
// ============================================================================

static const char* mirror_name(Mirror m) {
    switch (m) {
        case Mirror::HORIZONTAL:   return "Horizontal";
        case Mirror::VERTICAL:     return "Vertical";
        case Mirror::ONESCREEN_LO: return "One-Screen (Low)";
        case Mirror::ONESCREEN_HI: return "One-Screen (High)";
        case Mirror::FOUR_SCREEN:  return "Four-Screen";
    }
    return "Unknown";
}

static std::string format_size(size_t bytes) {
    if (bytes == 0) return "0";
    if (bytes >= 1024 * 1024)
        return std::to_string(bytes / (1024 * 1024)) + " MB";
    if (bytes >= 1024)
        return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes) + " B";
}

// ============================================================================
// Debug content
// ============================================================================

void Cartridge::render_debug_content() {
    // ---- Header / Identification ----
    if (ImGui::CollapsingHeader("Cartridge Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Mapper:        %u", mapper_id);
        ImGui::Text("PRG Banks:     %u  (%s)", prg_banks,
                     format_size(prg_banks * 16384u).c_str());
        ImGui::Text("CHR Banks:     %u  (%s)", chr_banks,
                     format_size(chr_banks * 8192u).c_str());
        ImGui::Text("Battery:       %s", battery_backed ? "Yes" : "No");
    }

    // ---- Memory ----
    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("PRG ROM:       %s", format_size(prg_memory.size()).c_str());

        bool chr_ram = chr_banks == 0 || (mapper && mapper->chr_is_ram());
        ImGui::Text("CHR %s:      %s",
                     chr_ram ? "RAM" : "ROM",
                     format_size(chr_memory.size()).c_str());

        if (!prg_ram.empty()) {
            ImGui::Text("PRG RAM:       %s%s",
                         format_size(prg_ram.size()).c_str(),
                         battery_backed ? " (battery)" : "");
        }
    }

    // ---- Mirroring ----
    if (ImGui::CollapsingHeader("Mirroring", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Mode:          %s", mirror_name(mirror_mode));

        // Visual nametable layout
        const auto& nt = MIRROR_NT_PAGES[static_cast<int>(mirror_mode)];
        ImGui::Text("$2000: page %u   $2400: page %u", nt[0], nt[1]);
        ImGui::Text("$2800: page %u   $2C00: page %u", nt[2], nt[3]);
    }

    // ---- Mapper State ----
    if (mapper) {
        if (ImGui::CollapsingHeader("Mapper State", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("IRQ Active:    %s", mapper->irq_state() ? "Yes" : "No");
            ImGui::Text("CHR Type:      %s", mapper->chr_is_ram() ? "RAM" : "ROM");

            Mirror m = mapper->mirror();
            if (m != mirror_mode) {
                ImGui::Text("Mapper Mirror: %s (overrides header)", mirror_name(m));
            }
        }
    }
}

} // namespace nes_system

#else // !CERMU_HAS_GUI

namespace nes_system {
void Cartridge::render_debug_content() {}
} // namespace nes_system

#endif
