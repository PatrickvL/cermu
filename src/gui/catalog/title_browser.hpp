#pragma once
// ============================================================================
// TitleBrowser — card grid view for the ROM catalog (§6)
// ============================================================================
//
// Displays titles from the CatalogStore as a responsive card grid.
// Cards show: title name, system badges, variant count, format.
// Supports filtering by text search and system selection.
// Shows scan progress during active pipeline runs.
//
// Integrated into LauncherPanel as an alternative to the file browser
// (Zone B), toggled via a view mode switch in the top bar.
// ============================================================================

#ifndef CERMU_NO_SQLITE

#include "gui/catalog/catalog_store.hpp"
#include "gui/catalog/catalog_pipeline.hpp"
#include "gui/launcher_theme.hpp"
#include "core/system_registry.hpp"

#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

namespace catalog {

class TitleBrowser {
public:
    TitleBrowser() = default;

    /// Refresh the internal card list from the store.
    /// Call when catalog changes (after pipeline batch, or on open).
    void refresh(CatalogStore& store) {
        groups_ = store.get_title_groups();

        // Sort alphabetically by title
        std::sort(groups_.begin(), groups_.end(),
                  [](const TitleGroup& a, const TitleGroup& b) {
                      return a.title < b.title;
                  });

        apply_filter();
    }

    /// Render the title browser into the current ImGui region.
    void render(CatalogStore& store, CatalogPipeline& pipeline) {
        auto progress = pipeline.get_progress();

        // Scan progress bar
        if (progress.running) {
            render_scan_progress(progress);
        }

        // Empty state
        if (groups_.empty() && !progress.running) {
            render_empty_state();
            return;
        }

        // Search bar
        render_search_bar();

        // Card grid
        render_card_grid(store);
    }

    /// Returns true if a title was activated (double-click or Enter).
    bool title_activated() const { return activated_; }

    /// Get the activated group's entries for launching.
    const std::vector<CatalogEntry>& activated_entries() const { return activated_entries_; }

    /// Clear the activation flag after handling.
    void clear_activation() { activated_ = false; activated_entries_.clear(); }

    /// Filter by system (empty = all systems).
    void set_system_filter(const std::string& system_id) {
        system_filter_ = system_id;
        apply_filter();
    }

private:
    // Data
    std::vector<TitleGroup>    groups_;
    std::vector<TitleGroup*>   filtered_;
    std::vector<CatalogEntry>  activated_entries_;

    // Filter state
    char search_buf_[256] = {};
    std::string system_filter_;

    // Selection
    int  selected_index_ = -1;
    bool activated_ = false;

    // Variant picker
    int  expanded_group_ = -1;  // group_id of expanded variant picker, -1 = none

    // ---- Filtering --------------------------------------------------------

    void apply_filter() {
        filtered_.clear();

        std::string search_lower;
        if (search_buf_[0]) {
            search_lower = search_buf_;
            for (auto& c : search_lower)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        for (auto& g : groups_) {
            // System filter
            if (!system_filter_.empty() && g.system_id != system_filter_)
                continue;

            // Text search filter
            if (!search_lower.empty()) {
                std::string title_lower = g.title;
                for (auto& c : title_lower)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                if (title_lower.find(search_lower) == std::string::npos)
                    continue;
            }

            filtered_.push_back(&g);
        }
    }

    // ---- Rendering --------------------------------------------------------

    void render_scan_progress(const PipelineProgress& prog) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.12f, 0.16f, 1.0f));
        ImGui::BeginChild("##ScanProgress", ImVec2(0, 32), false);

        ImGui::SetCursorPos(ImVec2(12, 8));
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextSecondary);

        const char* phase_name = "Scanning";
        switch (prog.phase) {
            case PipelinePhase::Discovery: phase_name = "Discovering files"; break;
            case PipelinePhase::Probing:   phase_name = "Probing files"; break;
            case PipelinePhase::Grouping:  phase_name = "Grouping titles"; break;
            default: break;
        }

        if (prog.files_total > 0) {
            ImGui::Text("%s...  %d / %d files", phase_name, prog.files_probed, prog.files_total);
        } else {
            ImGui::Text("%s...  %d files found", phase_name, prog.files_found);
        }

        ImGui::PopStyleColor();

        // Progress bar
        if (prog.files_total > 0) {
            float frac = static_cast<float>(prog.files_probed) /
                         static_cast<float>(prog.files_total);
            ImGui::SameLine(ImGui::GetWindowWidth() - 220);
            ImGui::SetCursorPosY(10);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, launcher_theme::kAccentTeal);
            ImGui::ProgressBar(frac, ImVec2(200, 12), "");
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void render_empty_state() {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 text_size = ImGui::CalcTextSize("No library configured");
        ImGui::SetCursorPos(ImVec2(
            (avail.x - text_size.x) * 0.5f,
            avail.y * 0.35f));

        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
        ImGui::Text("No library configured");
        ImGui::PopStyleColor();

        ImVec2 btn_size = ImGui::CalcTextSize("Set up scan roots...");
        btn_size.x += 24;
        btn_size.y += 12;
        ImGui::SetCursorPosX((avail.x - btn_size.x) * 0.5f);

        if (ImGui::Button("Set up scan roots...", btn_size)) {
            show_setup_requested_ = true;
        }
    }

    void render_search_bar() {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kSearchInputBg);
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 90);
        bool changed = ImGui::InputTextWithHint("##TitleSearch", "Search titles...",
                                                 search_buf_, sizeof(search_buf_));
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();

        if (changed) apply_filter();

        // Scan roots / Rescan buttons
        ImGui::SameLine();
        if (ImGui::SmallButton("Scan roots")) {
            show_setup_requested_ = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Rescan")) {
            rescan_requested_ = true;
        }
    }

    void render_card_grid(CatalogStore& store) {
        ImGui::BeginChild("##TitleGrid", ImVec2(0, 0), false);

        if (filtered_.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
            ImGui::SetCursorPosY(ImGui::GetContentRegionAvail().y * 0.3f);
            ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x * 0.3f);
            ImGui::Text("No matching titles");
            ImGui::PopStyleColor();
            ImGui::EndChild();
            return;
        }

        // Compute card layout
        float avail_width = ImGui::GetContentRegionAvail().x;
        constexpr float kCardWidth  = 240.0f;
        constexpr float kCardHeight = 72.0f;
        constexpr float kCardPadding = 8.0f;

        int cols = std::max(1, static_cast<int>((avail_width + kCardPadding) /
                                                 (kCardWidth + kCardPadding)));

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kCardPadding, kCardPadding));

        for (int i = 0; i < static_cast<int>(filtered_.size()); ++i) {
            auto* group = filtered_[i];
            int col = i % cols;

            if (col > 0) ImGui::SameLine();

            render_card(i, *group, kCardWidth, kCardHeight, store);
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    void render_card(int index, const TitleGroup& group,
                     float width, float height, CatalogStore& store) {
        bool selected = (index == selected_index_);
        ImVec4 bg = selected ? launcher_theme::kSystemRowSelected
                             : launcher_theme::kCardBgDefault;

        ImGui::PushID(group.group_id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
        ImGui::BeginChild("##Card", ImVec2(width, height), true,
                          ImGuiWindowFlags_NoScrollbar);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 card_min = ImGui::GetWindowPos();

        // Left accent bar (maker color from system_id)
        ImVec4 sys_accent = launcher_theme::kAccentTeal;
        if (!group.system_id.empty()) {
            // Look up maker from system registry for accent color
            const auto& systems = SystemRegistry::instance().get_systems();
            for (const auto& [desc, factory] : systems) {
                if (desc.short_name && group.system_id == desc.short_name) {
                    sys_accent = launcher_theme::accent_for_maker(desc.maker);
                    break;
                }
            }
        }
        dl->AddRectFilled(card_min, ImVec2(card_min.x + 3, card_min.y + height),
                          ImGui::GetColorU32(sys_accent));

        // Title
        ImGui::SetCursorPos(ImVec2(10, 6));
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 16);
        ImGui::TextWrapped("%s", group.title.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        // Bottom row: system badge + variant count
        ImGui::SetCursorPos(ImVec2(10, height - 22));

        // System badge as colored pill
        if (!group.system_id.empty()) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 tsz = ImGui::CalcTextSize(group.system_id.c_str());
            float pw = tsz.x + 8;
            ImVec4 pill_bg = ImVec4(sys_accent.x, sys_accent.y, sys_accent.z, 0.15f);
            dl->AddRectFilled(pos, ImVec2(pos.x + pw, pos.y + tsz.y + 2),
                              ImGui::GetColorU32(pill_bg), 3.0f);
            dl->AddText(ImVec2(pos.x + 4, pos.y + 1),
                        ImGui::GetColorU32(sys_accent),
                        group.system_id.c_str());
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pw + 4);
        }

        // Variant count badge
        if (group.entry_count > 1) {
            ImGui::SameLine(width - 40);
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
            ImGui::Text("%d\xc3\x97", group.entry_count);  // Nx
            ImGui::PopStyleColor();
        }

        // Interaction
        // Use invisible button overlay for click detection
        ImGui::SetCursorPos(ImVec2(0, 0));
        if (ImGui::InvisibleButton("##CardBtn", ImVec2(width, height))) {
            if (selected && group.entry_count == 1) {
                // Single-click on already selected single-variant → activate
                activate_group(group, store);
            } else if (selected && group.entry_count > 1) {
                // Toggle variant picker
                expanded_group_ = (expanded_group_ == group.group_id) ? -1 : group.group_id;
            } else {
                selected_index_ = index;
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            selected_index_ = index;
            if (group.entry_count == 1) {
                activate_group(group, store);
            } else {
                expanded_group_ = group.group_id;
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Variant picker (expanded inline below the card)
        if (expanded_group_ == group.group_id) {
            render_variant_picker(group, width, store);
        }

        ImGui::PopID();
    }

    void render_variant_picker(const TitleGroup& group, float width,
                               CatalogStore& store) {
        auto entries = store.get_group_entries(group.group_id);
        if (entries.empty()) return;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 1.0f));
        float picker_height = std::min(static_cast<float>(entries.size()) * 24.0f + 8.0f,
                                        120.0f);
        ImGui::BeginChild("##Variants", ImVec2(width, picker_height), true);

        for (auto& entry : entries) {
            ImGui::PushID(static_cast<int>(entry.id));

            // Build variant label: format + filename
            char label[256];
            snprintf(label, sizeof(label), "[%s] %s",
                     entry.format.c_str(), entry.filename.c_str());

            if (ImGui::Selectable(label)) {
                activated_ = true;
                activated_entries_.clear();
                activated_entries_.push_back(entry);
                expanded_group_ = -1;
            }

            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void activate_group(const TitleGroup& group, CatalogStore& store) {
        activated_ = true;
        activated_entries_ = store.get_group_entries(group.group_id);
    }

    // ---- Public flags for parent to check ---------------------------------
public:
    bool show_setup_requested_ = false;  // "Set up scan roots" / "Scan roots" was clicked
    bool rescan_requested_ = false;      // "Rescan" was clicked
};

} // namespace catalog

#endif // CERMU_NO_SQLITE
