#pragma once

// =============================================================================
// launcher_panel.hpp — Unified launcher UI for Cermu
// =============================================================================
//
// Replaces SystemSelectionDialog with the full launcher layout:
//   Left panel:  system list with type tabs, maker filter, search
//   Right panel: Zone A (system header + config strip)
//                Zone B (file browser / title browser — stub for now)
//                Zone C (probe result bar — stub for now)
//
// =============================================================================

#ifdef CERMU_HAS_GUI

#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>

#include "core/system_registry.hpp"
#include "core/hardware_traits.hpp"
#include "core/formats/format_handler.hpp"
#include "gui/launcher_theme.hpp"
#include "gui/shared_config_store.hpp"
#include "gui/file_browser.hpp"

/// Probe state machine for file-first launch (§7.2)
enum class ProbeState { None, Probing, SingleMatch, Ambiguous, NoMatch };

/**
 * LauncherPanel — unified launcher UI replacing SystemSelectionDialog.
 *
 * Rendered as a full-window panel when no system is loaded, or when the
 * user invokes File → Switch System / Browse.
 */
class LauncherPanel {
public:
    LauncherPanel();

    /// Open the launcher panel (pauses emulation when a system is active)
    void open();

    /// Close the launcher panel
    void close();

    /// Is the launcher currently visible?
    bool is_open() const { return is_open_; }

    /// Render the full launcher UI. Call every frame.
    /// @param allow_cancel  true when a system is already running (show close button)
    void render(bool allow_cancel = true);

    /// Was a system launch confirmed this frame?
    bool selection_confirmed() const { return selection_confirmed_; }

    /// Get selected system short_name (nullptr if none)
    const char* get_selected_system() const { return selected_system_name_; }

    /// Get selected memory option index (-1 = default)
    int get_selected_memory_option() const { return selected_memory_option_; }

    /// Get selected region option index (-1 = default)
    int get_selected_region_option() const { return selected_region_option_; }

    /// Get selected peripherals map
    const std::map<std::string, bool>& get_selected_peripherals() const { return selected_peripherals_; }

    /// Get selected custom settings
    const std::map<std::string, std::string>& get_selected_custom_settings() const { return selected_custom_settings_; }

    /// Get the file path to load on launch (from file browser selection)
    const std::string& get_pending_file_path() const { return pending_file_path_; }

    /// Handle a file dropped onto the launcher (navigate + probe)
    void handle_drop(const std::string& path);

    /// Reset selection state after processing
    void reset();

private:
    // =========================================================================
    // State
    // =========================================================================
    bool is_open_ = false;
    bool selection_confirmed_ = false;
    const char* selected_system_name_ = nullptr;
    int selected_system_index_ = -1;
    int selected_memory_option_ = -1;
    int selected_region_option_ = -1;
    std::map<std::string, bool> selected_peripherals_;
    std::map<std::string, std::string> selected_custom_settings_;

    // Focus tracking
    enum class FocusPanel { SystemList, FileBrowser };
    FocusPanel focus_panel_ = FocusPanel::SystemList;

    // Filtering
    char search_filter_[256] = {};
    SystemType type_filter_ = SystemType::Other;  // Special: 255 = All (we'll use a sentinel)
    bool type_filter_all_ = true;
    int maker_filter_index_ = 0;   // 0 = All makers
    std::vector<std::string> maker_list_;  // Populated on open

    // Config strip state (Zone A)
    bool config_dirty_ = false;

    // File path to pass along on launch
    std::string pending_file_path_;

    // Shared config memory
    SharedConfigStore config_store_;

    // File browser (Zone B)
    FileBrowser file_browser_;

    // Probe state
    ProbeState probe_state_ = ProbeState::None;
    std::string probe_system_name_;       // System name from probe
    float probe_confidence_ = 0.0f;
    SystemConfiguration probe_config_;    // Config from probe

    // Cached sorted system list
    struct SystemEntry {
        int registry_index;
        const SystemDescriptor* descriptor;
        SystemFactory factory;
    };
    std::vector<SystemEntry> sorted_systems_;
    std::vector<SystemEntry> filtered_systems_;

    // =========================================================================
    // Rendering
    // =========================================================================
    void render_top_bar(bool allow_cancel);
    void render_left_panel();
    void render_right_panel();
    void render_system_header();    // Zone A
    void render_config_strip();     // Zone A config controls
    void render_file_browser();     // Zone B
    void render_probe_bar();        // Zone C
    void render_status_bar();
    void handle_keyboard();         // Global keyboard shortcuts

    // =========================================================================
    // Helpers
    // =========================================================================
    void rebuild_system_list();
    void apply_filters();
    void select_system(int filtered_index);
    void select_system_by_name(const char* short_name);
    void launch_selected_system();
    void trigger_probe(const std::string& file_path);
    bool matches_search(const SystemDescriptor& desc) const;
};

// =============================================================================
// Implementation
// =============================================================================

inline LauncherPanel::LauncherPanel() {
    rebuild_system_list();
}

inline void LauncherPanel::open() {
    is_open_ = true;
    selection_confirmed_ = false;
    rebuild_system_list();
    apply_filters();
}

inline void LauncherPanel::close() {
    is_open_ = false;
}

inline void LauncherPanel::reset() {
    selection_confirmed_ = false;
    selected_system_name_ = nullptr;
}

inline void LauncherPanel::rebuild_system_list() {
    const auto& systems = SystemRegistry::instance().get_systems();
    sorted_systems_.clear();
    sorted_systems_.reserve(systems.size());

    std::map<std::string, bool> maker_seen;
    maker_list_.clear();
    maker_list_.push_back("All");

    for (int i = 0; i < static_cast<int>(systems.size()); ++i) {
        sorted_systems_.push_back({i, &systems[i].first, systems[i].second});

        // Collect unique makers
        if (systems[i].first.maker) {
            std::string m(systems[i].first.maker);
            if (!maker_seen.count(m)) {
                maker_seen[m] = true;
                maker_list_.push_back(m);
            }
        }
    }

    // Sort alphabetically by name
    std::sort(sorted_systems_.begin(), sorted_systems_.end(),
        [](const SystemEntry& a, const SystemEntry& b) {
            return strcmp(a.descriptor->name, b.descriptor->name) < 0;
        });

    // Sort makers alphabetically (skip "All" at index 0)
    std::sort(maker_list_.begin() + 1, maker_list_.end());
}

inline void LauncherPanel::apply_filters() {
    filtered_systems_.clear();
    for (const auto& entry : sorted_systems_) {
        // Type filter
        if (!type_filter_all_ && entry.descriptor->type != type_filter_)
            continue;

        // Maker filter
        if (maker_filter_index_ > 0) {
            const char* maker = entry.descriptor->maker;
            if (!maker || maker_list_[maker_filter_index_] != maker)
                continue;
        }

        // Search filter
        if (search_filter_[0] != '\0' && !matches_search(*entry.descriptor))
            continue;

        filtered_systems_.push_back(entry);
    }
}

inline bool LauncherPanel::matches_search(const SystemDescriptor& desc) const {
    // Case-insensitive substring search across name, short_name, description, maker, cpu_summary, year
    auto contains = [](const char* haystack, const char* needle) -> bool {
        if (!haystack || !needle) return false;
        std::string h(haystack), n(needle);
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        return h.find(n) != std::string::npos;
    };

    const char* q = search_filter_;
    if (contains(desc.name, q)) return true;
    if (contains(desc.short_name, q)) return true;
    if (contains(desc.description, q)) return true;
    if (contains(desc.maker, q)) return true;
    if (contains(desc.cpu_summary, q)) return true;

    // Year as string
    if (desc.year > 0) {
        char year_str[8];
        snprintf(year_str, sizeof(year_str), "%d", desc.year);
        if (contains(year_str, q)) return true;
    }

    return false;
}

inline void LauncherPanel::select_system(int filtered_index) {
    if (filtered_index < 0 || filtered_index >= static_cast<int>(filtered_systems_.size()))
        return;

    selected_system_index_ = filtered_index;
    const auto* desc = filtered_systems_[filtered_index].descriptor;
    selected_system_name_ = desc->short_name;

    // Build config from shared store
    auto cfg = config_store_.build_config(desc->short_name, desc->hardware_traits);
    selected_memory_option_ = cfg.memory_option_index;
    selected_region_option_ = cfg.region_option_index;
    selected_peripherals_ = cfg.enabled_peripherals;
    selected_custom_settings_ = cfg.custom_settings;
    config_dirty_ = false;

    // Clear probe state when system manually selected
    probe_state_ = ProbeState::None;
    probe_system_name_.clear();

    // Configure file browser format filter to this system's formats
    file_browser_.set_formats(desc->supported_formats);

    // Navigate to this system's data folder (only if browser is at default / hasn't been navigated)
    if (desc->data_folder) {
        // Build null-terminated alias array for path discovery
        std::vector<const char*> names;
        names.push_back(desc->data_folder);
        for (const auto* a : desc->aliases)
            names.push_back(a);
        names.push_back(nullptr);
        file_browser_.navigate_to_system_data(desc->data_folder, names.data());
    }
}

inline void LauncherPanel::select_system_by_name(const char* short_name) {
    if (!short_name) return;
    for (int i = 0; i < static_cast<int>(filtered_systems_.size()); ++i) {
        if (strcmp(filtered_systems_[i].descriptor->short_name, short_name) == 0) {
            select_system(i);
            return;
        }
    }
    // Not found in filtered list — try sorted list
    for (int i = 0; i < static_cast<int>(sorted_systems_.size()); ++i) {
        if (strcmp(sorted_systems_[i].descriptor->short_name, short_name) == 0) {
            // Clear filters to show this system
            type_filter_all_ = true;
            maker_filter_index_ = 0;
            search_filter_[0] = '\0';
            apply_filters();
            // Now find in filtered
            for (int j = 0; j < static_cast<int>(filtered_systems_.size()); ++j) {
                if (strcmp(filtered_systems_[j].descriptor->short_name, short_name) == 0) {
                    select_system(j);
                    return;
                }
            }
            break;
        }
    }
}

inline void LauncherPanel::trigger_probe(const std::string& file_path) {
    if (file_path.empty()) {
        probe_state_ = ProbeState::None;
        return;
    }

    // Read the file via VFS (handles archives, containers, real filesystem)
    size_t file_size = 0;
    uint8_t* data = format_read_entire_file(file_path.c_str(), &file_size);
    if (!data) {
        probe_state_ = ProbeState::NoMatch;
        return;
    }

    // Call the registry probe
    auto match = SystemRegistry::instance().identify_system(
        file_path.c_str(), data, file_size);
    free(data);

    static constexpr float kSingleMatchThreshold = 0.50f;
    static constexpr float kNoMatchThreshold = 0.30f;

    if (match.confidence >= kSingleMatchThreshold && !match.system_name.empty()) {
        probe_state_ = ProbeState::SingleMatch;
        probe_system_name_ = match.system_name;
        probe_confidence_ = match.confidence;
        probe_config_ = match.configuration;

        // Auto-populate Zone A from probe result
        // Find and select the matched system so config strip appears
        select_system_by_name(match.system_name.c_str());

        // Apply probe config overrides
        selected_memory_option_ = match.configuration.memory_option_index;
        selected_region_option_ = match.configuration.region_option_index;
        selected_peripherals_ = match.configuration.enabled_peripherals;
        selected_custom_settings_ = match.configuration.custom_settings;
    } else if (match.confidence >= kNoMatchThreshold && !match.system_name.empty()) {
        probe_state_ = ProbeState::Ambiguous;
        probe_system_name_ = match.system_name;
        probe_confidence_ = match.confidence;
        probe_config_ = match.configuration;
    } else {
        probe_state_ = ProbeState::NoMatch;
        probe_system_name_.clear();
        probe_confidence_ = 0.0f;
    }
}

inline void LauncherPanel::launch_selected_system() {
    if (!selected_system_name_) return;

    // Record config to shared store
    if (selected_system_index_ >= 0 && selected_system_index_ < static_cast<int>(filtered_systems_.size())) {
        const auto* desc = filtered_systems_[selected_system_index_].descriptor;
        SystemConfiguration cfg;
        cfg.memory_option_index = selected_memory_option_ >= 0 ? selected_memory_option_ : 0;
        cfg.region_option_index = selected_region_option_ >= 0 ? selected_region_option_ : 0;
        cfg.enabled_peripherals = selected_peripherals_;
        cfg.custom_settings = selected_custom_settings_;
        config_store_.record_config(desc->short_name, desc->hardware_traits, cfg);
    }

    selection_confirmed_ = true;
}

// =============================================================================
// Rendering
// =============================================================================

inline void LauncherPanel::render(bool allow_cancel) {
    if (!is_open_) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 display_size = io.DisplaySize;

    // Full-window panel
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(display_size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, launcher_theme::kWindowBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##Launcher", nullptr, flags)) {
        float top_bar_height = 36.0f;
        float status_bar_height = 24.0f;
        float content_height = display_size.y - top_bar_height - status_bar_height;

        // Top bar
        ImGui::SetCursorPos(ImVec2(0, 0));
        render_top_bar(allow_cancel);

        // Content area
        ImGui::SetCursorPos(ImVec2(0, top_bar_height));

        // Left panel (system list)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kLeftPanelBg);
        ImGui::BeginChild("##LeftPanel", ImVec2(launcher_theme::kLeftPanelWidth, content_height), true);
        render_left_panel();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Right panel (content)
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("##RightPanel", ImVec2(0, content_height), false);
        render_right_panel();
        ImGui::EndChild();

        // Status bar
        ImGui::SetCursorPos(ImVec2(0, display_size.y - status_bar_height));
        render_status_bar();

        // Keyboard navigation (processed after rendering so focus state is current)
        handle_keyboard();
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

inline void LauncherPanel::render_top_bar(bool allow_cancel) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kBarBg);
    ImGui::BeginChild("##TopBar", ImVec2(0, 36.0f), false);

    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kAccentTeal);
    ImGui::Text("\xe2\x97\x88 CERMU");  // ◈ CERMU
    ImGui::PopStyleColor();

    // Type filter tabs
    ImGui::SameLine(140);
    ImGui::SetCursorPosY(6);

    auto tab_button = [&](const char* label, bool active) -> bool {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, launcher_theme::kSystemRowSelected);
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
        }
        bool clicked = ImGui::SmallButton(label);
        ImGui::PopStyleColor(2);
        return clicked;
    };

    if (tab_button("All", type_filter_all_)) {
        type_filter_all_ = true;
        apply_filters();
    }
    ImGui::SameLine();
    if (tab_button("Home", !type_filter_all_ && type_filter_ == SystemType::Home)) {
        type_filter_all_ = false; type_filter_ = SystemType::Home; apply_filters();
    }
    ImGui::SameLine();
    if (tab_button("Console", !type_filter_all_ && type_filter_ == SystemType::Console)) {
        type_filter_all_ = false; type_filter_ = SystemType::Console; apply_filters();
    }
    ImGui::SameLine();
    if (tab_button("Arcade", !type_filter_all_ && type_filter_ == SystemType::Arcade)) {
        type_filter_all_ = false; type_filter_ = SystemType::Arcade; apply_filters();
    }
    ImGui::SameLine();
    if (tab_button("Other", !type_filter_all_ && type_filter_ == SystemType::Other)) {
        type_filter_all_ = false; type_filter_ = SystemType::Other; apply_filters();
    }

    // Maker filter combo (right side)
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::SetCursorPosY(6);
    ImGui::PushItemWidth(100);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kSearchInputBg);
    if (ImGui::BeginCombo("##Maker", maker_list_[maker_filter_index_].c_str(), ImGuiComboFlags_NoArrowButton)) {
        for (int i = 0; i < static_cast<int>(maker_list_.size()); ++i) {
            bool selected = (i == maker_filter_index_);
            if (ImGui::Selectable(maker_list_[i].c_str(), selected)) {
                maker_filter_index_ = i;
                apply_filters();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor();
    ImGui::PopItemWidth();

    // System count
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
    ImGui::Text("%d/%d", static_cast<int>(filtered_systems_.size()),
                static_cast<int>(sorted_systems_.size()));
    ImGui::PopStyleColor();

    // Close button (if running system)
    if (allow_cancel) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 40);
        ImGui::SetCursorPosY(6);
        if (ImGui::SmallButton("X")) {
            close();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

inline void LauncherPanel::render_left_panel() {
    // Search input
    ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kSearchInputBg);
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##SystemSearch", search_filter_, sizeof(search_filter_),
                         ImGuiInputTextFlags_AutoSelectAll)) {
        apply_filters();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // System list
    for (int i = 0; i < static_cast<int>(filtered_systems_.size()); ++i) {
        const auto& entry = filtered_systems_[i];
        const auto* desc = entry.descriptor;

        bool is_selected = (i == selected_system_index_);

        ImGui::PushID(i);

        // Row background
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Header, launcher_theme::kSystemRowSelected);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, launcher_theme::kSystemRowSelected);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, launcher_theme::kFileRowHover);
        }

        if (ImGui::Selectable("##row", is_selected, ImGuiSelectableFlags_AllowDoubleClick,
                              ImVec2(0, 34))) {
            select_system(i);

            if (ImGui::IsMouseDoubleClicked(0)) {
                // Double-click: select and move focus to file browser
                focus_panel_ = FocusPanel::FileBrowser;
            }
        }

        ImGui::PopStyleColor(2);

        // Overlay text on the selectable
        ImVec2 row_min = ImGui::GetItemRectMin();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // System name
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
        draw_list->AddText(ImVec2(row_min.x + 8, row_min.y + 2), ImGui::GetColorU32(launcher_theme::kTextPrimary), desc->name);
        ImGui::PopStyleColor();

        // Year + maker (second line)
        if (desc->maker || desc->year > 0) {
            char meta[128] = {};
            if (desc->year > 0 && desc->maker)
                snprintf(meta, sizeof(meta), "%d \xc2\xb7 %s", desc->year, desc->maker);
            else if (desc->year > 0)
                snprintf(meta, sizeof(meta), "%d", desc->year);
            else if (desc->maker)
                snprintf(meta, sizeof(meta), "%s", desc->maker);

            draw_list->AddText(ImVec2(row_min.x + 8, row_min.y + 18),
                               ImGui::GetColorU32(launcher_theme::kTextMuted), meta);
        }

        ImGui::PopID();
    }
}

inline void LauncherPanel::render_right_panel() {
    // Zone A: system header + config strip (visible when system selected)
    if (selected_system_name_) {
        render_system_header();
        render_config_strip();
        ImGui::Separator();
    }

    // Zone B: file browser stub
    render_file_browser();

    // Zone C: probe result bar (visible only after probe)
    render_probe_bar();
}

inline void LauncherPanel::render_system_header() {
    if (selected_system_index_ < 0 || selected_system_index_ >= static_cast<int>(filtered_systems_.size()))
        return;

    const auto* desc = filtered_systems_[selected_system_index_].descriptor;
    ImVec4 accent = launcher_theme::accent_for_maker(desc->maker);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(accent.x * 0.1f, accent.y * 0.1f, accent.z * 0.1f, 1.0f));
    ImGui::BeginChild("##ZoneA_Header", ImVec2(0, 48), false);

    ImGui::SetCursorPos(ImVec2(12, 6));

    // System name
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
    ImGui::Text("%s", desc->name);
    ImGui::PopStyleColor();

    // Metadata badges
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
    if (desc->year > 0)
        ImGui::Text("(%d)", desc->year);
    ImGui::PopStyleColor();

    if (desc->cpu_summary) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextSecondary);
        ImGui::Text("\xc2\xb7 %s", desc->cpu_summary);  // · cpu_summary
        ImGui::PopStyleColor();
    }

    if (desc->maker) {
        ImGui::SameLine();
        // Maker badge with accent color
        ImGui::PushStyleColor(ImGuiCol_Text, accent);
        ImGui::Text("\xc2\xb7 %s", desc->maker);  // · maker
        ImGui::PopStyleColor();
    }

    // Type badge
    {
        ImGui::SameLine();
        const char* type_str = "Other";
        switch (desc->type) {
            case SystemType::Home:    type_str = "Home"; break;
            case SystemType::Console: type_str = "Console"; break;
            case SystemType::Arcade:  type_str = "Arcade"; break;
            default: break;
        }
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
        ImGui::Text("[%s]", type_str);
        ImGui::PopStyleColor();
    }

    // Launch button (right-aligned)
    float launch_width = 80.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - launch_width - 12);
    ImGui::SetCursorPosY(10);
    ImGui::PushStyleColor(ImGuiCol_Button, accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accent.x * 1.2f, accent.y * 1.2f, accent.z * 1.2f, 1.0f));
    if (ImGui::Button("\xe2\x96\xb6 Launch", ImVec2(launch_width, 28))) {  // ▶ Launch
        launch_selected_system();
    }
    ImGui::PopStyleColor(2);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

inline void LauncherPanel::render_config_strip() {
    if (selected_system_index_ < 0 || selected_system_index_ >= static_cast<int>(filtered_systems_.size()))
        return;

    const auto* desc = filtered_systems_[selected_system_index_].descriptor;
    const auto& traits = desc->hardware_traits;

    // Only show if there are configurable options
    bool has_options = !traits.video_standard_configs.empty() ||
                       !traits.memory_options.empty() ||
                       !traits.peripheral_options.empty() ||
                       !traits.custom_options.empty();
    if (!has_options) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kCfgBgDefault);
    ImGui::BeginChild("##ConfigStrip", ImVec2(0, launcher_theme::kConfigStripHeight), false);
    ImGui::SetCursorPos(ImVec2(12, 6));

    // Region combo
    if (!traits.video_standard_configs.empty()) {
        ImGui::PushItemWidth(90);
        int region_idx = selected_region_option_ >= 0 ? selected_region_option_ : 0;
        const char* region_preview = (region_idx < static_cast<int>(traits.video_standard_configs.size()))
            ? traits.video_standard_configs[region_idx].name : "?";

        ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kCfgBgDefault);
        ImGui::PushStyleColor(ImGuiCol_Border, launcher_theme::kCfgBorderDefault);
        if (ImGui::BeginCombo("##Region", region_preview, ImGuiComboFlags_NoArrowButton)) {
            for (int i = 0; i < static_cast<int>(traits.video_standard_configs.size()); ++i) {
                bool is_sel = (i == region_idx);
                if (ImGui::Selectable(traits.video_standard_configs[i].name, is_sel)) {
                    selected_region_option_ = i;
                    config_dirty_ = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopItemWidth();
        ImGui::SameLine();
    }

    // Memory combo
    if (!traits.memory_options.empty()) {
        ImGui::PushItemWidth(90);
        int mem_idx = selected_memory_option_ >= 0 ? selected_memory_option_ : 0;
        const char* mem_preview = (mem_idx < static_cast<int>(traits.memory_options.size()))
            ? traits.memory_options[mem_idx].name : "?";

        ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kCfgBgDefault);
        ImGui::PushStyleColor(ImGuiCol_Border, launcher_theme::kCfgBorderDefault);
        if (ImGui::BeginCombo("##Memory", mem_preview, ImGuiComboFlags_NoArrowButton)) {
            for (int i = 0; i < static_cast<int>(traits.memory_options.size()); ++i) {
                bool is_sel = (i == mem_idx);
                if (ImGui::Selectable(traits.memory_options[i].name, is_sel)) {
                    selected_memory_option_ = i;
                    config_dirty_ = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopItemWidth();
        ImGui::SameLine();
    }

    // Peripheral toggles
    for (const auto& po : traits.peripheral_options) {
        bool enabled = false;
        auto it = selected_peripherals_.find(po.id);
        if (it != selected_peripherals_.end())
            enabled = it->second;
        else
            enabled = po.enabled_by_default;

        if (ImGui::Checkbox(po.name, &enabled)) {
            selected_peripherals_[po.id] = enabled;
            config_dirty_ = true;
        }
        ImGui::SameLine();
    }

    // Custom option combos
    for (const auto& co : traits.custom_options) {
        if (co.choices.empty()) continue;

        ImGui::PushItemWidth(100);
        std::string current;
        auto it = selected_custom_settings_.find(co.id);
        if (it != selected_custom_settings_.end())
            current = it->second;
        else if (co.default_index >= 0 && co.default_index < static_cast<int>(co.choices.size()))
            current = co.choices[co.default_index];

        ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kCfgBgDefault);
        ImGui::PushStyleColor(ImGuiCol_Border, launcher_theme::kCfgBorderDefault);
        std::string combo_id = "##Custom_" + std::string(co.id);
        if (ImGui::BeginCombo(combo_id.c_str(), current.c_str(), ImGuiComboFlags_NoArrowButton)) {
            for (int i = 0; i < static_cast<int>(co.choices.size()); ++i) {
                bool is_sel = (current == co.choices[i]);
                if (ImGui::Selectable(co.choices[i], is_sel)) {
                    selected_custom_settings_[co.id] = co.choices[i];
                    config_dirty_ = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopStyleColor(2);
        ImGui::PopItemWidth();
        ImGui::SameLine();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

inline void LauncherPanel::render_file_browser() {
    // Zone B — file browser occupies remaining space above probe bar
    float probe_height = (probe_state_ != ProbeState::None) ? launcher_theme::kProbeBarHeight : 0.0f;
    ImGui::BeginChild("##ZoneB", ImVec2(0, -probe_height), false);

    file_browser_.render();

    // Handle file selection: trigger probe when no system is selected
    if (file_browser_.file_selection_changed() && !selected_system_name_) {
        trigger_probe(file_browser_.selected_file());
    }

    // Handle file activation: launch if system is selected or probe matched
    if (file_browser_.file_activated()) {
        file_browser_.clear_activation();
        if (selected_system_name_) {
            // System selected — launch with this file
            pending_file_path_ = file_browser_.selected_file();
            launch_selected_system();
        } else if (probe_state_ == ProbeState::SingleMatch) {
            // No system selected but probe matched — auto-select and launch
            select_system_by_name(probe_system_name_.c_str());
            if (selected_system_name_) {
                pending_file_path_ = file_browser_.selected_file();
                launch_selected_system();
            }
        }
    }

    ImGui::EndChild();
}

inline void LauncherPanel::render_probe_bar() {
    if (probe_state_ == ProbeState::None) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kProbeBarBg);
    ImGui::BeginChild("##ProbeBar", ImVec2(0, launcher_theme::kProbeBarHeight), true);

    ImGui::SetCursorPos(ImVec2(12, 12));

    switch (probe_state_) {
        case ProbeState::SingleMatch: {
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kProbeMatch);
            ImGui::Text("\xe2\x9c\x93");  // ✓
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
            ImGui::Text("%s", probe_system_name_.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
            ImGui::Text("(%.0f%%)", probe_confidence_ * 100.0f);
            ImGui::PopStyleColor();
            break;
        }
        case ProbeState::Ambiguous: {
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kProbeAmbiguous);
            ImGui::Text("\xe2\x9a\xa0");  // ⚠
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextSecondary);
            ImGui::Text("Multiple systems match \xe2\x80\x94 %s (%.0f%%)",
                        probe_system_name_.c_str(), probe_confidence_ * 100.0f);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::SmallButton("Choose...")) {
                // Select the best match for now
                select_system_by_name(probe_system_name_.c_str());
                probe_state_ = ProbeState::SingleMatch;
            }
            break;
        }
        case ProbeState::NoMatch: {
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kProbeNoMatch);
            ImGui::Text("\xe2\x9c\x97  Format not recognised");  // ✗
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
            if (ImGui::SmallButton("Choose system manually...")) {
                // Focus left panel — just clear probe
                probe_state_ = ProbeState::None;
            }
            ImGui::PopStyleColor();
            break;
        }
        default:
            break;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

inline void LauncherPanel::render_status_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kBarBg);
    ImGui::BeginChild("##StatusBar", ImVec2(0, 24.0f), false);

    ImGui::SetCursorPos(ImVec2(12, 4));
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);

    // Context-sensitive hints (§15.5)
    if (focus_panel_ == FocusPanel::SystemList) {
        ImGui::Text("\xe2\x86\x91\xe2\x86\x93 Navigate  \xc2\xb7  \xe2\x86\xb5 Open files  \xc2\xb7  Tab Switch panel  \xc2\xb7  / Search");
    } else if (focus_panel_ == FocusPanel::FileBrowser) {
        if (probe_state_ == ProbeState::SingleMatch) {
            ImGui::Text("\xe2\x86\xb5 Launch  \xc2\xb7  Esc Clear  \xc2\xb7  Tab Switch panel");
        } else if (probe_state_ == ProbeState::Ambiguous) {
            ImGui::Text("\xe2\x86\xb5 Choose\xe2\x80\xa6  \xc2\xb7  Esc Clear");
        } else if (selected_system_name_) {
            ImGui::Text("\xe2\x86\x91\xe2\x86\x93 Navigate  \xc2\xb7  \xe2\x86\xb5 Launch  \xc2\xb7  / Search  \xc2\xb7  Backspace Up");
        } else {
            ImGui::Text("\xe2\x86\x91\xe2\x86\x93 Navigate  \xc2\xb7  \xe2\x86\xb5 Select & probe  \xc2\xb7  / Search  \xc2\xb7  Backspace Up");
        }
    }

    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

inline void LauncherPanel::handle_keyboard() {
    // Don't process keyboard when a text input is active
    if (ImGui::IsAnyItemActive()) return;

    // Tab: toggle focus between system list and file browser
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        focus_panel_ = (focus_panel_ == FocusPanel::SystemList)
                     ? FocusPanel::FileBrowser
                     : FocusPanel::SystemList;
    }

    // Slash: focus search input of active panel
    if (ImGui::IsKeyPressed(ImGuiKey_Slash)) {
        if (focus_panel_ == FocusPanel::SystemList) {
            ImGui::SetKeyboardFocusHere(-1);  // Will be handled by next frame
        }
    }

    // Escape
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (focus_panel_ == FocusPanel::FileBrowser) {
            // Clear probe result, return focus to system list
            probe_state_ = ProbeState::None;
            probe_system_name_.clear();
            file_browser_.clear_selection();
            focus_panel_ = FocusPanel::SystemList;
        }
    }

    if (focus_panel_ == FocusPanel::SystemList) {
        // Arrow keys: navigate system list
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            int next = selected_system_index_ + 1;
            if (next < static_cast<int>(filtered_systems_.size()))
                select_system(next);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            int prev = selected_system_index_ - 1;
            if (prev >= 0)
                select_system(prev);
        }

        // Enter: move focus to file browser
        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            if (selected_system_name_) {
                focus_panel_ = FocusPanel::FileBrowser;
            }
        }
    } else if (focus_panel_ == FocusPanel::FileBrowser) {
        // Enter in file browser
        if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            if (selected_system_name_ && !file_browser_.selected_file().empty()) {
                // Launch with selected file
                pending_file_path_ = file_browser_.selected_file();
                launch_selected_system();
            } else if (probe_state_ == ProbeState::SingleMatch) {
                // Auto-select probed system and launch
                select_system_by_name(probe_system_name_.c_str());
                if (selected_system_name_ && !file_browser_.selected_file().empty()) {
                    pending_file_path_ = file_browser_.selected_file();
                    launch_selected_system();
                }
            }
        }
    }
}

inline void LauncherPanel::handle_drop(const std::string& path) {
    if (path.empty()) return;

    // Determine if the dropped path is a directory or file
    namespace fs = std::filesystem;
    std::error_code ec;

    bool is_dir = fs::is_directory(path, ec);
    bool is_archive = VfsFileSystem::has_archive_extension(path);

    if (is_dir) {
        // Navigate file browser to that directory
        file_browser_.navigate_to(path);
        focus_panel_ = FocusPanel::FileBrowser;
    } else if (is_archive) {
        // Navigate into the archive
        file_browser_.navigate_to(path + "!/");
        focus_panel_ = FocusPanel::FileBrowser;
    } else {
        // File — get parent dir, navigate there, and trigger probe
        fs::path p(path);
        auto parent = p.parent_path();
        if (!parent.empty()) {
            file_browser_.navigate_to(parent.string());
        }
        // Select and probe the file
        trigger_probe(path);
        focus_panel_ = FocusPanel::FileBrowser;
    }
}

#endif // CERMU_HAS_GUI
