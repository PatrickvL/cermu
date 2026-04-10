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

#include "core/cermu.hpp"
#include <imgui.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstring>

#include "core/system_registry.hpp"
#include "core/hardware_traits.hpp"
#include "core/formats/format_handler.hpp"
#include "gui/launcher_theme.hpp"
#include "gui/shared_config_store.hpp"
#include "gui/file_browser.hpp"

#ifndef CERMU_NO_SQLITE
#include "gui/catalog/catalog_store.hpp"
#include "gui/catalog/catalog_pipeline.hpp"
#include "gui/catalog/title_browser.hpp"
#endif

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

    /// Current UI scale factor
    float get_ui_scale() const { return ui_scale_; }

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

    // Favourites (in-session; persisted via SharedConfigStore if desired)
    std::set<std::string> favourites_;

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

    // UI zoom (proportional rendering)
    static constexpr float kDefaultUiScale = 1.5f;
    float ui_scale_ = kDefaultUiScale;
    static constexpr float kMinUiScale  = 0.5f;
    static constexpr float kMaxUiScale  = 3.0f;
    static constexpr float kUiScaleStep = 0.1f;

    // View mode — file browser vs. title browser
    enum class ViewMode { FileBrowser, TitleBrowser };
    ViewMode view_mode_ = ViewMode::FileBrowser;

#ifndef CERMU_NO_SQLITE
    // Catalog infrastructure (§13)
    catalog::CatalogStore   catalog_store_;
    catalog::CatalogPipeline catalog_pipeline_;
    catalog::TitleBrowser   title_browser_;
    bool catalog_opened_ = false;

    void ensure_catalog_open();
    void render_title_browser();
#endif

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

    // Apply proportional zoom — scale all ImGui rendering while the launcher is active
    float saved_scale = ImGui::GetIO().FontGlobalScale;
    ImGui::GetIO().FontGlobalScale = saved_scale * ui_scale_;

    if (ImGui::Begin("##Launcher", nullptr, flags)) {
        float top_bar_height = 36.0f;
        float status_bar_height = 24.0f;
        float content_height = display_size.y - top_bar_height - status_bar_height;

        // Top bar
        ImGui::SetCursorPos(ImVec2(0, 0));
        render_top_bar(allow_cancel);

        // Content area
        ImGui::SetCursorPos(ImVec2(0, top_bar_height));

        // Left panel (system list) — width scales gently with zoom (square root)
        float left_w = launcher_theme::kLeftPanelWidth * sqrtf(ui_scale_);
        bool left_focused = (focus_panel_ == FocusPanel::SystemList);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kLeftPanelBg);
        ImGui::PushStyleColor(ImGuiCol_Border,
            left_focused ? launcher_theme::kPanelBorderFocus : launcher_theme::kPanelBorder);
        ImGui::BeginChild("##LeftPanel", ImVec2(left_w, content_height), true);
        render_left_panel();
        ImGui::EndChild();
        ImGui::PopStyleColor(2);

        // Right panel (content)
        ImGui::SameLine(0, 0);
        bool right_focused = (focus_panel_ == FocusPanel::FileBrowser);
        ImGui::PushStyleColor(ImGuiCol_Border,
            right_focused ? launcher_theme::kPanelBorderFocus : launcher_theme::kPanelBorder);
        ImGui::BeginChild("##RightPanel", ImVec2(0, content_height), false);
        render_right_panel();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Status bar
        ImGui::SetCursorPos(ImVec2(0, display_size.y - status_bar_height));
        render_status_bar();

        // Keyboard navigation (processed after rendering so focus state is current)
        handle_keyboard();
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();

    // Restore global scale
    ImGui::GetIO().FontGlobalScale = saved_scale;
}

inline void LauncherPanel::render_top_bar(bool allow_cancel) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kBarBg);
    ImGui::BeginChild("##TopBar", ImVec2(0, 36.0f), false);

    ImGui::SetCursorPos(ImVec2(12, 8));
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kAccentTeal);
    ImGui::Text("\xe2\x97\x88 CERMU");  // ◈ CERMU
    ImGui::PopStyleColor();

    // Vertical divider after logo
    ImGui::SameLine(0, 8);
    ImVec2 div_pos = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(div_pos.x, div_pos.y + 2),
        ImVec2(div_pos.x + 1, div_pos.y + 14),
        ImGui::GetColorU32(ImVec4(0.078f, 0.110f, 0.157f, 1.0f)));
    ImGui::Dummy(ImVec2(1, 0));

    // Type filter tabs
    ImGui::SameLine(140);
    ImGui::SetCursorPosY(6);

    auto tab_button = [&](const char* label, bool active) -> bool {
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, launcher_theme::kSystemRowSelected);
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kAccentBlue);
            ImGui::PushStyleColor(ImGuiCol_Border, launcher_theme::kFilterActiveBorder);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.067f, 0.110f, 0.157f, 1.0f));
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        bool clicked = ImGui::SmallButton(label);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
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

    // System count (right-aligned in top bar)
    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
    ImGui::SetCursorPosY(6);
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
    ImGui::Text("%d/%d", static_cast<int>(filtered_systems_.size()),
                static_cast<int>(sorted_systems_.size()));
    ImGui::PopStyleColor();

    // View mode toggle (File / Title)
    ImGui::SameLine();
    ImGui::SetCursorPosY(6);
    {
        bool is_file = (view_mode_ == ViewMode::FileBrowser);
        if (is_file) {
            ImGui::PushStyleColor(ImGuiCol_Button, launcher_theme::kSystemRowSelected);
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
        }
        if (ImGui::SmallButton("File")) view_mode_ = ViewMode::FileBrowser;
        ImGui::PopStyleColor(2);

        ImGui::SameLine();
        bool is_title = (view_mode_ == ViewMode::TitleBrowser);
        if (is_title) {
            ImGui::PushStyleColor(ImGuiCol_Button, launcher_theme::kSystemRowSelected);
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
        }
        if (ImGui::SmallButton("Title")) {
            view_mode_ = ViewMode::TitleBrowser;
#ifndef CERMU_NO_SQLITE
            ensure_catalog_open();
            title_browser_.refresh(catalog_store_);
#endif
        }
        ImGui::PopStyleColor(2);
    }

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
    // Search input with placeholder hint
    ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kSearchInputBg);
    ImGui::PushItemWidth(-1);
    if (ImGui::InputTextWithHint("##SystemSearch", "Name, CPU, year, type\xe2\x80\xa6",
                                 search_filter_, sizeof(search_filter_),
                                 ImGuiInputTextFlags_AutoSelectAll)) {
        apply_filters();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    // Maker filter — directly above the system list for proximity
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
    ImGui::TextUnformatted("Maker");
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 4);
    ImGui::PushItemWidth(-1);
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
                // Double-click: launch the selected system immediately
                launch_selected_system();
            }
        }

        ImGui::PopStyleColor(2);

        // Left accent bar (colored by maker)
        ImVec2 row_min = ImGui::GetItemRectMin();
        ImVec2 row_max = ImGui::GetItemRectMax();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (is_selected) {
            ImVec4 maker_accent = launcher_theme::accent_for_maker(desc->maker);
            draw_list->AddRectFilled(
                ImVec2(row_min.x, row_min.y),
                ImVec2(row_min.x + 3.0f, row_max.y),
                ImGui::GetColorU32(maker_accent));
        }

        // System name
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
        draw_list->AddText(ImVec2(row_min.x + 8, row_min.y + 2), ImGui::GetColorU32(launcher_theme::kTextPrimary), desc->name);
        ImGui::PopStyleColor();

        // Year + maker badge (second line)
        if (desc->maker || desc->year > 0) {
            float meta_x = row_min.x + 8;
            float meta_y = row_min.y + 18;

            // Year
            if (desc->year > 0) {
                char year_str[8];
                snprintf(year_str, sizeof(year_str), "%d", desc->year);
                draw_list->AddText(ImVec2(meta_x, meta_y),
                                   ImGui::GetColorU32(launcher_theme::kTextMuted), year_str);
                meta_x += ImGui::CalcTextSize(year_str).x + 4;
            }

            // Maker as pill badge
            if (desc->maker) {
                ImVec4 maker_color = launcher_theme::accent_for_maker(desc->maker);
                ImVec2 text_size = ImGui::CalcTextSize(desc->maker);
                float pill_h = text_size.y + 2;
                float pill_w = text_size.x + 8;
                ImVec4 pill_bg = ImVec4(maker_color.x, maker_color.y, maker_color.z, 0.15f);
                draw_list->AddRectFilled(
                    ImVec2(meta_x, meta_y - 1),
                    ImVec2(meta_x + pill_w, meta_y + pill_h),
                    ImGui::GetColorU32(pill_bg),
                    3.0f);
                draw_list->AddText(ImVec2(meta_x + 4, meta_y),
                                   ImGui::GetColorU32(maker_color), desc->maker);
            }
        }

        // Favourite star (right-aligned)
        if (desc->short_name && favourites_.count(desc->short_name)) {
            const char* star = "\xe2\x98\x85";  // ★
            ImVec2 star_size = ImGui::CalcTextSize(star);
            draw_list->AddText(
                ImVec2(row_max.x - star_size.x - 6, row_min.y + 2),
                ImGui::GetColorU32(ImVec4(0.667f, 0.533f, 0.133f, 1.0f)),
                star);
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

    // Zone B: file browser or title browser based on view mode
    if (view_mode_ == ViewMode::FileBrowser) {
        render_file_browser();
    }
#ifndef CERMU_NO_SQLITE
    else {
        render_title_browser();
    }
#endif

    // Zone C: probe result bar (visible only after probe)
    render_probe_bar();
}

inline void LauncherPanel::render_system_header() {
    if (selected_system_index_ < 0 || selected_system_index_ >= static_cast<int>(filtered_systems_.size()))
        return;

    const auto* desc = filtered_systems_[selected_system_index_].descriptor;
    ImVec4 accent = launcher_theme::accent_for_maker(desc->maker);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(accent.x * 0.1f, accent.y * 0.1f, accent.z * 0.1f, 1.0f));
    ImGui::BeginChild("##ZoneA_Header", ImVec2(0, 64), false);

    // Procedural mini-CRT thumbnail
    {
        ImVec2 crt_pos = ImVec2(ImGui::GetCursorScreenPos().x + 12,
                                ImGui::GetCursorScreenPos().y + 8);
        float crt_w = 60.0f, crt_h = 48.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // CRT background (dark, tinted by accent)
        ImVec4 crt_bg = ImVec4(accent.x * 0.04f, accent.y * 0.04f, accent.z * 0.04f, 1.0f);
        dl->AddRectFilled(crt_pos, ImVec2(crt_pos.x + crt_w, crt_pos.y + crt_h),
                          ImGui::GetColorU32(crt_bg), 4.0f);

        // Determine phosphor tint from system characteristics
        // Apple/PET → green, Atari 2600/Robotron → amber, others → blue/cyan
        ImU32 line_color;
        if (desc->maker) {
            std::string_view m(desc->maker);
            if (m.find("Apple") != std::string_view::npos ||
                (m.find("Commodore") != std::string_view::npos
                 && desc->name && std::string_view(desc->name).find("PET") != std::string_view::npos))
                line_color = ImGui::GetColorU32(ImVec4(0.12f, 0.93f, 0.31f, 0.7f));  // green
            else if (m.find("Robotron") != std::string_view::npos || m.find("Atari") != std::string_view::npos)
                line_color = ImGui::GetColorU32(ImVec4(1.0f, 0.67f, 0.2f, 0.7f));    // amber
            else
                line_color = ImGui::GetColorU32(ImVec4(0.35f, 0.68f, 0.91f, 0.7f));  // blue/cyan
        } else {
            line_color = ImGui::GetColorU32(ImVec4(0.35f, 0.68f, 0.91f, 0.7f));
        }

        // Procedural "code" lines — deterministic from system name hash
        uint32_t h = 5381;
        if (desc->name) {
            for (const char* p = desc->name; *p; ++p)
                h = ((h << 5) + h) + static_cast<uint8_t>(*p);
        }
        int num_lines = 8;
        for (int i = 0; i < num_lines; ++i) {
            uint32_t r = ((h * 2654435761u) >> (i * 3)) & 0xFFFF;
            float lw = 8.0f + (r % static_cast<uint32_t>(crt_w - 20));
            float indent = (r >> 8) % 8;
            float y = crt_pos.y + 4 + i * (crt_h - 10) / num_lines;
            dl->AddRectFilled(
                ImVec2(crt_pos.x + 3 + indent, y),
                ImVec2(crt_pos.x + 3 + indent + lw, y + 2),
                line_color, 1.0f);
        }

        // Cursor blink
        float cursor_y = crt_pos.y + crt_h - 8;
        dl->AddRectFilled(
            ImVec2(crt_pos.x + 3, cursor_y),
            ImVec2(crt_pos.x + 8, cursor_y + 3),
            line_color);

        // Scanline overlay (subtle dark horizontal lines)
        for (float y = crt_pos.y; y < crt_pos.y + crt_h; y += 3.0f) {
            dl->AddLine(
                ImVec2(crt_pos.x, y),
                ImVec2(crt_pos.x + crt_w, y),
                ImGui::GetColorU32(ImVec4(0, 0, 0, 0.16f)));
        }

        // CRT bezel border
        dl->AddRect(crt_pos, ImVec2(crt_pos.x + crt_w, crt_pos.y + crt_h),
                    ImGui::GetColorU32(ImVec4(0.08f, 0.10f, 0.16f, 1.0f)), 4.0f);
    }

    // System info — positioned right of the CRT thumbnail
    ImGui::SetCursorPos(ImVec2(84, 8));

    // System name
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextPrimary);
    ImGui::Text("%s", desc->name);
    ImGui::PopStyleColor();

    // Metadata badges — rendered as pills with background
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
    if (desc->year > 0)
        ImGui::Text("%d", desc->year);
    ImGui::PopStyleColor();

    if (desc->cpu_summary) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextSecondary);
        ImGui::Text("\xc2\xb7 %s", desc->cpu_summary);  // · cpu_summary
        ImGui::PopStyleColor();
    }

    if (desc->maker) {
        ImGui::SameLine();
        // Maker as a pill badge with colored background
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 text_size = ImGui::CalcTextSize(desc->maker);
        float pad_x = 6.0f, pad_y = 1.0f;
        ImVec4 pill_bg = ImVec4(accent.x, accent.y, accent.z, 0.15f);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(
            ImVec2(cursor.x, cursor.y + pad_y),
            ImVec2(cursor.x + text_size.x + pad_x * 2, cursor.y + text_size.y + pad_y),
            ImGui::GetColorU32(pill_bg),
            4.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad_x);
        ImGui::PushStyleColor(ImGuiCol_Text, accent);
        ImGui::TextUnformatted(desc->maker);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, pad_x);
    }

    // Type badge as pill
    {
        const char* type_str = "Other";
        switch (desc->type) {
            case SystemType::Home:    type_str = "home"; break;
            case SystemType::Console: type_str = "console"; break;
            case SystemType::Arcade:  type_str = "arcade"; break;
            default: type_str = "other"; break;
        }
        ImGui::SameLine();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 text_size = ImGui::CalcTextSize(type_str);
        float pad_x = 5.0f, pad_y = 1.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(
            ImVec2(cursor.x, cursor.y + pad_y),
            ImVec2(cursor.x + text_size.x + pad_x * 2, cursor.y + text_size.y + pad_y),
            ImGui::GetColorU32(ImVec4(0.055f, 0.094f, 0.157f, 1.0f)),
            4.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad_x);
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
        ImGui::TextUnformatted(type_str);
        ImGui::PopStyleColor();
    }

    // Action buttons: favourite star + settings + launch (right-aligned)
    float launch_width = ImGui::CalcTextSize("\xe2\x96\xb6 Launch").x + 20.0f;
    if (launch_width < 80.0f) launch_width = 80.0f;
    float button_group_width = 28.0f + 4.0f + 28.0f + 4.0f + launch_width;  // star + gap + settings + gap + launch
    float content_w = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    float group_x = content_w - button_group_width - 8.0f;
    if (group_x < 84.0f) group_x = 84.0f;  // don't overlap CRT/info area
    ImGui::SameLine(group_x);
    ImGui::SetCursorPosY(18);

    // Favourite toggle
    {
        bool is_fav = desc->short_name && favourites_.count(desc->short_name);
        ImGui::PushStyleColor(ImGuiCol_Button, launcher_theme::kCfgBgDefault);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, launcher_theme::kCfgBgChanged);
        ImGui::PushStyleColor(ImGuiCol_Text,
            is_fav ? ImVec4(0.800f, 0.600f, 0.133f, 1.0f) : launcher_theme::kTextMuted);
        if (ImGui::Button("\xe2\x98\x85##fav", ImVec2(28, 28))) {  // ★
            if (desc->short_name) {
                if (is_fav) favourites_.erase(desc->short_name);
                else favourites_.insert(desc->short_name);
            }
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::SameLine(0, 4);

    // Settings placeholder
    ImGui::PushStyleColor(ImGuiCol_Button, launcher_theme::kCfgBgDefault);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, launcher_theme::kCfgBgChanged);
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
    ImGui::Button("\xe2\x9a\x99##settings", ImVec2(28, 28));  // ⚙
    ImGui::PopStyleColor(3);
    ImGui::SameLine(0, 4);

    // Launch button
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

    // Config strip — two-row height to accommodate wrapping options
    float strip_height = ImGui::GetFrameHeight() * 2 + 16.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kCfgBgDefault);
    ImGui::BeginChild("##ConfigStrip", ImVec2(0, strip_height), false);
    ImGui::SetCursorPos(ImVec2(12, 6));

    // Helper: render a labeled config combo as a pill
    auto config_pill = [&](const char* label, const char* combo_id, const char* preview,
                           bool is_default, auto render_items) {
        // Label
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kCfgTextDefault);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);

        // Combo value
        ImVec4 border_col = is_default ? launcher_theme::kCfgBorderDefault : launcher_theme::kCfgBorderChanged;
        ImVec4 bg_col = is_default ? launcher_theme::kCfgBgDefault : launcher_theme::kCfgBgChanged;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, bg_col);
        ImGui::PushStyleColor(ImGuiCol_Border, border_col);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        ImGui::PushItemWidth(ImGui::CalcTextSize(preview).x + 24);
        if (ImGui::BeginCombo(combo_id, preview, ImGuiComboFlags_NoArrowButton)) {
            render_items();
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 10);
    };

    // Region combo
    if (!traits.video_standard_configs.empty()) {
        int region_idx = selected_region_option_ >= 0 ? selected_region_option_ : 0;
        const char* region_preview = (region_idx < static_cast<int>(traits.video_standard_configs.size()))
            ? traits.video_standard_configs[region_idx].name : "?";
        bool region_default = (selected_region_option_ <= 0);

        config_pill("REGION", "##Region", region_preview, region_default, [&]() {
            for (int i = 0; i < static_cast<int>(traits.video_standard_configs.size()); ++i) {
                bool is_sel = (i == region_idx);
                if (ImGui::Selectable(traits.video_standard_configs[i].name, is_sel)) {
                    selected_region_option_ = i;
                    config_dirty_ = true;
                }
            }
        });
    }

    // Memory combo
    if (!traits.memory_options.empty()) {
        int mem_idx = selected_memory_option_ >= 0 ? selected_memory_option_ : 0;
        const char* mem_preview = (mem_idx < static_cast<int>(traits.memory_options.size()))
            ? traits.memory_options[mem_idx].name : "?";
        bool mem_default = (selected_memory_option_ <= 0);

        config_pill("RAM EXP.", "##Memory", mem_preview, mem_default, [&]() {
            for (int i = 0; i < static_cast<int>(traits.memory_options.size()); ++i) {
                bool is_sel = (i == mem_idx);
                if (ImGui::Selectable(traits.memory_options[i].name, is_sel)) {
                    selected_memory_option_ = i;
                    config_dirty_ = true;
                }
            }
        });
    }

    // Peripheral toggles as labeled pills
    for (const auto& po : traits.peripheral_options) {
        bool enabled = false;
        auto it = selected_peripherals_.find(po.id);
        if (it != selected_peripherals_.end())
            enabled = it->second;
        else
            enabled = po.enabled_by_default;

        // Label
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kCfgTextDefault);
        ImGui::TextUnformatted(po.name);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);

        // Value as toggle text
        const char* val_str = enabled ? "On" : "None";
        ImVec4 bg_col = enabled ? launcher_theme::kCfgBgChanged : launcher_theme::kCfgBgDefault;
        ImGui::PushStyleColor(ImGuiCol_Button, bg_col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, launcher_theme::kCfgBgChanged);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        std::string btn_id = std::string(val_str) + "##periph_" + std::string(po.id);
        if (ImGui::SmallButton(btn_id.c_str())) {
            enabled = !enabled;
            selected_peripherals_[po.id] = enabled;
            config_dirty_ = true;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0, 10);
    }

    // Custom option combos with labels
    for (const auto& co : traits.custom_options) {
        if (co.choices.empty()) continue;

        std::string current;
        auto it = selected_custom_settings_.find(co.id);
        if (it != selected_custom_settings_.end())
            current = it->second;
        else if (co.default_index >= 0 && co.default_index < static_cast<int>(co.choices.size()))
            current = co.choices[co.default_index];

        bool is_default = (co.default_index >= 0 && co.default_index < static_cast<int>(co.choices.size())
                           && current == co.choices[co.default_index]);
        std::string combo_id = "##Custom_" + std::string(co.id);

        config_pill(co.name, combo_id.c_str(), current.c_str(), is_default, [&]() {
            for (int i = 0; i < static_cast<int>(co.choices.size()); ++i) {
                bool is_sel = (current == co.choices[i]);
                if (ImGui::Selectable(co.choices[i], is_sel)) {
                    selected_custom_settings_[co.id] = co.choices[i];
                    config_dirty_ = true;
                }
            }
        });
    }

    // Add bottom padding to the wrapping strip
    ImGui::Dummy(ImVec2(0, 4));

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

    // Helper: render a key badge + label pair
    auto kbd_hint = [](const char* key, const char* desc) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 key_size = ImGui::CalcTextSize(key);
        float pad_x = 4.0f, pad_y = 1.0f;
        float badge_w = key_size.x + pad_x * 2;
        float badge_h = key_size.y + pad_y * 2;

        // Badge background
        dl->AddRectFilled(
            ImVec2(pos.x, pos.y),
            ImVec2(pos.x + badge_w, pos.y + badge_h),
            ImGui::GetColorU32(ImVec4(0.039f, 0.051f, 0.110f, 1.0f)),
            3.0f);
        // Badge border
        dl->AddRect(
            ImVec2(pos.x, pos.y),
            ImVec2(pos.x + badge_w, pos.y + badge_h),
            ImGui::GetColorU32(ImVec4(0.094f, 0.125f, 0.188f, 1.0f)),
            3.0f);
        // Key text
        dl->AddText(
            ImVec2(pos.x + pad_x, pos.y + pad_y),
            ImGui::GetColorU32(launcher_theme::kTextMuted),
            key);

        // Advance cursor past badge
        ImGui::Dummy(ImVec2(badge_w, badge_h));
        ImGui::SameLine(0, 4);

        // Description text
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
        ImGui::TextUnformatted(desc);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 10);
    };

    // Context-sensitive hints (§15.5)
    if (focus_panel_ == FocusPanel::SystemList) {
        kbd_hint("\xe2\x86\x91\xe2\x86\x93", "Navigate");
        kbd_hint("\xe2\x86\xb5", "Open ROMs");
        kbd_hint("Tab", "Switch panel");
        kbd_hint("/", "Search");
    } else if (focus_panel_ == FocusPanel::FileBrowser) {
        if (probe_state_ == ProbeState::SingleMatch) {
            kbd_hint("\xe2\x86\xb5", "Launch");
            kbd_hint("Esc", "Clear");
            kbd_hint("Tab", "Switch panel");
        } else if (probe_state_ == ProbeState::Ambiguous) {
            kbd_hint("\xe2\x86\xb5", "Choose\xe2\x80\xa6");
            kbd_hint("Esc", "Clear");
        } else if (selected_system_name_) {
            kbd_hint("\xe2\x86\x91\xe2\x86\x93", "Navigate");
            kbd_hint("\xe2\x86\xb5", "Launch");
            kbd_hint("/", "Search");
            kbd_hint("\xe2\x8c\xab", "Up");
        } else {
            kbd_hint("\xe2\x86\x91\xe2\x86\x93", "Navigate");
            kbd_hint("\xe2\x86\xb5", "Select & probe");
            kbd_hint("/", "Search");
            kbd_hint("\xe2\x8c\xab", "Up");
        }
    }

    // Right-aligned: mode indicator + zoom
    {
        const char* mode_str = (focus_panel_ == FocusPanel::SystemList) ? "SYSTEMS" : "FILES";
        char right_label[64];
        if (ui_scale_ < kDefaultUiScale - 0.01f || ui_scale_ > kDefaultUiScale + 0.01f)
            snprintf(right_label, sizeof(right_label), "%s \xc2\xb7 %d%%", mode_str,
                     static_cast<int>(ui_scale_ * 100.0f + 0.5f));
        else
            snprintf(right_label, sizeof(right_label), "%s \xc2\xb7 CERMU", mode_str);

        float text_w = ImGui::CalcTextSize(right_label).x;
        ImGui::SameLine(ImGui::GetWindowWidth() - text_w - 12);
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
        ImGui::Text("%s", right_label);
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

inline void LauncherPanel::handle_keyboard() {
    // Zoom shortcuts — processed even when a text input is active
    // Ctrl+= / Ctrl+Plus to zoom in, Ctrl+- to zoom out, Ctrl+0 to reset
    {
        bool ctrl = ImGui::GetIO().KeyCtrl;
#ifdef __APPLE__
        ctrl = ImGui::GetIO().KeySuper;  // Cmd on macOS
#endif
        if (ctrl) {
            // Zoom in: Ctrl+= (unshifted Plus key) or Ctrl+Keypad+
            if (ImGui::IsKeyPressed(ImGuiKey_Equal) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
                ui_scale_ = std::min(ui_scale_ + kUiScaleStep, kMaxUiScale);
            }
            // Zoom out: Ctrl+- or Ctrl+Keypad-
            if (ImGui::IsKeyPressed(ImGuiKey_Minus) ||
                ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
                ui_scale_ = std::max(ui_scale_ - kUiScaleStep, kMinUiScale);
            }
            // Reset: Ctrl+0 or Ctrl+Keypad0
            if (ImGui::IsKeyPressed(ImGuiKey_0) ||
                ImGui::IsKeyPressed(ImGuiKey_Keypad0)) {
                ui_scale_ = kDefaultUiScale;
            }
        }
    }

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

// =============================================================================
// Catalog integration (§13)
// =============================================================================

#ifndef CERMU_NO_SQLITE

inline void LauncherPanel::ensure_catalog_open() {
    if (catalog_opened_) return;

    // Open/create the catalog database alongside the scan roots config
    namespace fs = std::filesystem;
    std::string config_dir;
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    config_dir = appdata ? std::string(appdata) + "\\cermu" : ".";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg) config_dir = std::string(xdg) + "/cermu";
    else {
        const char* home = std::getenv("HOME");
        config_dir = home ? std::string(home) + "/.config/cermu" : ".";
    }
#endif
    std::error_code ec;
    fs::create_directories(config_dir, ec);

    std::string db_path = config_dir + "/catalog.db";
    if (catalog_store_.open(db_path)) {
        catalog_opened_ = true;
        log_info("Catalog database opened: %s\n", db_path.c_str());
    } else {
        log_info("Failed to open catalog database: %s\n", db_path.c_str());
    }
}

inline void LauncherPanel::render_title_browser() {
    if (!catalog_opened_) {
        ImGui::TextDisabled("Catalog not available.");
        return;
    }

    // Render the title browser (handles progress, empty state, and card grid)
    title_browser_.render(catalog_store_, catalog_pipeline_);
}

#endif // CERMU_NO_SQLITE

#endif // CERMU_HAS_GUI
