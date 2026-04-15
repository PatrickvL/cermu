#pragma once

// =============================================================================
// scan_root_manager.hpp — Scan root list with TOML persistence (§12)
// =============================================================================
//
// Manages a list of filesystem directories that the catalog pipeline scans for
// ROM/game files. Provides:
//   - Add / remove roots
//   - Persistence via a simple TOML config file
//   - Pre-population of candidate paths from host environment
//   - UI rendering for first-run setup and Library → Scan roots… dialog
//
// The config file format follows the design spec:
//
//   [scan_roots]
//   paths = [
//       "/home/user/roms",
//       "/mnt/nas/retro"
//   ]
//
// =============================================================================

#ifdef CERMU_HAS_GUI

#include "core/cermu.hpp"
#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef __has_include
#if __has_include("ImGuiFileDialog.h")
#include "gui/cermu_file_dialog.hpp"
#define SCANROOT_HAS_IGFD 1
#endif
#endif

namespace scan_roots {

// =============================================================================
// ScanRootEntry — one candidate or configured root
// =============================================================================
struct ScanRootEntry {
    std::string path;
    bool        selected  = false;   ///< Checked in the setup UI
    bool        exists    = false;   ///< Path exists on disk
    bool        has_files = false;   ///< Contains at least one file (shallow check)
};

// =============================================================================
// ScanRootManager
// =============================================================================
class ScanRootManager {
public:
    ScanRootManager() = default;

    // =========================================================================
    // Configuration file
    // =========================================================================

    /// Load roots from a TOML config file. Returns true if file existed.
    bool load(const std::string& path) {
        config_path_ = path;
        roots_.clear();

        FILE* f = fopen(path.c_str(), "r");
        if (!f) return false;

        // Simple TOML parser — only understands:
        //   [scan_roots]
        //   paths = [ "...", "..." ]
        // Handles multi-line arrays.
        char line[4096];
        bool in_section = false;
        bool in_array   = false;

        while (fgets(line, sizeof(line), f)) {
            std::string s(line);
            // Trim
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
                s.pop_back();

            // Skip empty / comment
            if (s.empty() || s[0] == '#') continue;

            // Section header
            if (s == "[scan_roots]") {
                in_section = true;
                continue;
            }
            if (s[0] == '[') {
                in_section = false;
                in_array = false;
                continue;
            }

            if (!in_section) continue;

            // Look for `paths = [` or continuation
            if (!in_array) {
                auto eq = s.find('=');
                if (eq == std::string::npos) continue;
                std::string key = s.substr(0, eq);
                // Trim key
                while (!key.empty() && key.back() == ' ') key.pop_back();
                while (!key.empty() && key.front() == ' ') key.erase(key.begin());
                if (key != "paths") continue;

                std::string val = s.substr(eq + 1);
                while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                if (!val.empty() && val.front() == '[') {
                    val.erase(val.begin());
                    in_array = true;
                    parse_array_entries(val);
                }
                continue;
            }

            // Inside array — look for entries and closing ]
            parse_array_entries(s);
            if (s.find(']') != std::string::npos) {
                in_array = false;
            }
        }
        fclose(f);

        // Validate existence
        for (auto& r : roots_)
            check_entry(r);

        return true;
    }

    /// Save current roots to the config file. Creates parent dirs if needed.
    bool save() const {
        if (config_path_.empty()) return false;

        // Create parent directory
        namespace fs = std::filesystem;
        std::error_code ec;
        auto parent = fs::path(config_path_).parent_path();
        if (!parent.empty())
            fs::create_directories(parent, ec);

        FILE* f = fopen(config_path_.c_str(), "w");
        if (!f) return false;

        fprintf(f, "[scan_roots]\npaths = [\n");
        for (size_t i = 0; i < roots_.size(); i++) {
            // Escape backslashes and quotes for TOML string
            std::string escaped;
            for (char c : roots_[i].path) {
                if (c == '\\') escaped += "\\\\";
                else if (c == '"') escaped += "\\\"";
                else escaped += c;
            }
            fprintf(f, "    \"%s\"%s\n", escaped.c_str(),
                    (i + 1 < roots_.size()) ? "," : "");
        }
        fprintf(f, "]\n");
        fclose(f);

        log_info("Scan roots saved to %s (%zu entries)\n",
               config_path_.c_str(), roots_.size());
        return true;
    }

    /// Save to a specific path (for first-time setup when config_path_ wasn't set)
    bool save(const std::string& path) {
        config_path_ = path;
        return save();
    }

    // =========================================================================
    // Root management
    // =========================================================================

    const std::vector<ScanRootEntry>& get_roots() const { return roots_; }
    bool empty() const { return roots_.empty(); }

    void add_root(const std::string& path) {
        // Avoid duplicates
        for (const auto& r : roots_)
            if (r.path == path) return;

        ScanRootEntry e;
        e.path = path;
        e.selected = true;
        check_entry(e);
        roots_.push_back(std::move(e));
        save();
    }

    void remove_root(int index) {
        if (index >= 0 && index < static_cast<int>(roots_.size())) {
            roots_.erase(roots_.begin() + index);
            save();
        }
    }

    void remove_root(const std::string& path) {
        roots_.erase(
            std::remove_if(roots_.begin(), roots_.end(),
                           [&](const ScanRootEntry& e) { return e.path == path; }),
            roots_.end());
        save();
    }

    // =========================================================================
    // Host environment pre-population (§12.2)
    // =========================================================================

    /// Discover candidate scan roots from the host environment.
    /// Returned entries have `selected = true` if they exist and contain files.
    std::vector<ScanRootEntry> discover_candidates() const {
        std::vector<ScanRootEntry> candidates;
        std::vector<std::string> tried;

        auto try_path = [&](const std::string& p) {
            if (p.empty()) return;
            // Normalize trailing slash
            std::string norm = p;
            while (norm.size() > 1 && norm.back() == '/') norm.pop_back();

            // Skip duplicates
            for (const auto& t : tried)
                if (t == norm) return;
            tried.push_back(norm);

            // Skip already-configured roots
            for (const auto& r : roots_)
                if (r.path == norm) return;

            ScanRootEntry e;
            e.path = norm;
            check_entry(e);
            e.selected = e.exists && e.has_files;
            candidates.push_back(std::move(e));
        };

#ifdef _WIN32
        // Windows paths
        const char* profile = std::getenv("USERPROFILE");
        if (profile) {
            try_path(std::string(profile) + "\\ROMs");
            try_path(std::string(profile) + "\\roms");
            try_path(std::string(profile) + "\\Documents\\ROMs");
            try_path(std::string(profile) + "\\Documents\\roms");
        }
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            try_path(std::string(appdata) + "\\RetroArch\\roms");
        }
#else
        // Linux / macOS paths
        const char* home = std::getenv("HOME");
        if (home) {
            try_path(std::string(home) + "/ROMs");
            try_path(std::string(home) + "/roms");
            try_path(std::string(home) + "/Emulation/roms");
            try_path(std::string(home) + "/Games/roms");
            try_path(std::string(home) + "/.local/share/retroarch/roms");
            try_path(std::string(home) + "/.vice/disks");
            try_path(std::string(home) + "/.mame/roms");

            // TOSEC collections: search ~/Downloads/tosec*/*/
            // TOSEC archives are typically structured as:
            //   <tosec_root>/<Manufacturer>/<System>/<Category>/<archive>.7z
            // We add the TOSEC root so the file browser can search
            // manufacturer/system subfolders for matching system names.
            discover_tosec_roots(std::string(home) + "/Downloads", candidates, tried);
        }
        const char* xdg = std::getenv("XDG_DATA_HOME");
        if (xdg) {
            try_path(std::string(xdg) + "/roms");
        }
#endif

        return candidates;
    }

    // =========================================================================
    // UI rendering
    // =========================================================================

    /// Render the scan roots configuration panel.
    /// Returns true if user clicked "Start scanning" (scan should begin).
    bool render_setup_panel() {
        bool start_scan = false;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.05f, 0.09f, 1.0f));
        ImGui::BeginChild("##ScanRootSetup", ImVec2(0, 0), true);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.95f, 1.0f, 1.0f));
        ImGui::Text("Set up your ROM library");
        ImGui::PopStyleColor();
        ImGui::TextWrapped("Cermu searches these folders for games. "
                           "Select the ones that apply:");

        ImGui::Spacing();

        // Existing configured roots
        if (!roots_.empty()) {
            ImGui::SeparatorText("Configured roots");
            for (int i = 0; i < static_cast<int>(roots_.size()); i++) {
                auto& r = roots_[i];
                ImGui::PushID(i);

                // Status indicator
                if (r.exists && r.has_files) {
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "\xe2\x9c\x93");  // ✓
                } else if (r.exists) {
                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "\xe2\x97\x8b");  // ○
                } else {
                    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "\xe2\x9c\x97");  // ✗
                }
                ImGui::SameLine();
                ImGui::Text("%s", r.path.c_str());
                ImGui::SameLine();

                // Status text
                if (!r.exists) {
                    ImGui::TextDisabled("(not found)");
                } else if (!r.has_files) {
                    ImGui::TextDisabled("(empty)");
                } else {
                    ImGui::TextDisabled("(contains files)");
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) {
                    remove_root(i);
                    ImGui::PopID();
                    break;  // List changed, restart on next frame
                }

                ImGui::PopID();
            }
        }

        // Discovered candidates (if any)
        if (!setup_candidates_.empty()) {
            ImGui::Spacing();
            ImGui::SeparatorText("Discovered paths");
            for (auto& c : setup_candidates_) {
                ImGui::Checkbox(c.path.c_str(), &c.selected);
                ImGui::SameLine();
                if (!c.exists) {
                    ImGui::TextDisabled("(not found)");
                } else if (!c.has_files) {
                    ImGui::TextDisabled("(empty)");
                } else {
                    ImGui::TextDisabled("(found, contains files)");
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Action buttons
        if (ImGui::Button("Add folder...")) {
            open_folder_picker();
        }

        // Text input fallback (always available for manual entry)
        ImGui::SameLine();
        ImGui::PushItemWidth(300);
        if (ImGui::InputText("##addpath", add_path_buf_, sizeof(add_path_buf_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (add_path_buf_[0] != '\0') {
                add_root(add_path_buf_);
                add_path_buf_[0] = '\0';
            }
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Skip for now")) {
            show_setup_ = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Start scanning")) {
            // Add selected candidates
            for (const auto& c : setup_candidates_) {
                if (c.selected) {
                    add_root(c.path);
                }
            }
            setup_candidates_.clear();
            show_setup_ = false;
            start_scan = true;
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        return start_scan;
    }

    /// Render the Library → Scan roots… dialog (modal-style).
    /// Returns true when dialog should close.
    bool render_manage_dialog() {
        bool should_close = false;

        ImGui::Text("Scan Roots");
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(roots_.size()); i++) {
            auto& r = roots_[i];
            ImGui::PushID(i);

            // Status icon
            if (r.exists) {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "\xe2\x9c\x93");  // ✓
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "\xe2\x9c\x97");  // ✗
            }
            ImGui::SameLine();
            ImGui::Text("%s", r.path.c_str());
            ImGui::SameLine();

            if (ImGui::SmallButton("Remove")) {
                remove_root(i);
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        if (roots_.empty()) {
            ImGui::TextDisabled("No scan roots configured.");
        }

        ImGui::Spacing();

        // Add folder (IGFD picker + text input)
        if (ImGui::Button("Add folder...")) {
            open_folder_picker();
        }
        ImGui::SameLine();
        ImGui::PushItemWidth(300);
        if (ImGui::InputText("##addroot", add_path_buf_, sizeof(add_path_buf_),
                             ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (add_path_buf_[0] != '\0') {
                add_root(add_path_buf_);
                add_path_buf_[0] = '\0';
            }
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Add")) {
            if (add_path_buf_[0] != '\0') {
                add_root(add_path_buf_);
                add_path_buf_[0] = '\0';
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Close")) {
            should_close = true;
        }

        return should_close;
    }

    /// Populate setup candidates (call once before showing setup panel)
    void prepare_setup() {
        show_setup_ = true;
        if (setup_candidates_.empty())
            setup_candidates_ = discover_candidates();
    }

    /// Whether the setup panel should be shown
    bool should_show_setup() const { return show_setup_; }

    /// Get the config file path
    const std::string& config_path() const { return config_path_; }

    /// Get default config path (~/.config/cermu/scan_roots.toml or platform equivalent)
    static std::string default_config_path() {
#ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        if (appdata) return std::string(appdata) + "\\cermu\\scan_roots.toml";
        return "scan_roots.toml";
#else
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg) return std::string(xdg) + "/cermu/scan_roots.toml";
        const char* home = std::getenv("HOME");
        if (home) return std::string(home) + "/.config/cermu/scan_roots.toml";
        return "scan_roots.toml";
#endif
    }

private:
    std::string config_path_;
    std::vector<ScanRootEntry>  roots_;

    // Setup UI state
    bool show_setup_ = false;
    std::vector<ScanRootEntry> setup_candidates_;
    char add_path_buf_[1024] = {};

    /// Open the IGFD folder picker for adding a scan root.
    void open_folder_picker() {
#ifdef SCANROOT_HAS_IGFD
        IGFD::FileDialogConfig config;
        // Start in home directory
        const char* home = std::getenv("HOME");
        if (home) config.path = home;
        config.flags = ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;
        cermu::FileDialogInstance()->OpenDialog(
            "ScanRootFolderPicker", "Select ROM folder",
            nullptr,   // nullptr filter = directory-only mode
            config);
#endif
        // Text input is always visible alongside the button as fallback
    }

public:
    /// Poll the IGFD folder picker for a result.  Call each frame from the
    /// host (SessionGUI) while the dialog is open.  Returns true if the
    /// picker just closed (so the host can stop rendering it).
    bool poll_folder_picker() {
#ifdef SCANROOT_HAS_IGFD
        auto* fd = cermu::FileDialogInstance();
        if (!fd->IsOpened("ScanRootFolderPicker")) return false;

        bool closed = false;
        ImGui::SetNextWindowSizeConstraints(ImVec2(600, 400), ImVec2(FLT_MAX, FLT_MAX));
        if (fd->Display("ScanRootFolderPicker",
                        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar,
                        ImVec2(600, 400))) {
            if (fd->IsOk()) {
                std::string selected = fd->GetCurrentPath();
                if (!selected.empty()) {
                    add_root(selected);
                }
            }
            fd->Close();
            closed = true;
        }
        return closed;
#else
        return false;
#endif
    }

private:

    void parse_array_entries(const std::string& s) {
        // Extract quoted strings from a TOML array fragment
        size_t pos = 0;
        while (pos < s.size()) {
            auto q1 = s.find('"', pos);
            if (q1 == std::string::npos) break;
            auto q2 = s.find('"', q1 + 1);
            if (q2 == std::string::npos) break;

            std::string entry = s.substr(q1 + 1, q2 - q1 - 1);
            // Unescape
            std::string unescaped;
            for (size_t i = 0; i < entry.size(); i++) {
                if (entry[i] == '\\' && i + 1 < entry.size()) {
                    char next = entry[i + 1];
                    if (next == '\\') { unescaped += '\\'; i++; }
                    else if (next == '"') { unescaped += '"'; i++; }
                    else unescaped += entry[i];
                } else {
                    unescaped += entry[i];
                }
            }

            if (!unescaped.empty()) {
                ScanRootEntry e;
                e.path = unescaped;
                e.selected = true;
                roots_.push_back(std::move(e));
            }

            pos = q2 + 1;
        }
    }

    static void check_entry(ScanRootEntry& e) {
        namespace fs = std::filesystem;
        std::error_code ec;
        e.exists = fs::is_directory(e.path, ec);
        e.has_files = false;
        if (e.exists) {
            // Shallow check — any regular file in the directory (non-recursive)?
            for (const auto& entry : fs::directory_iterator(e.path, ec)) {
                if (entry.is_regular_file(ec)) {
                    e.has_files = true;
                    break;
                }
            }
        }
    }

    /// Discover TOSEC collection roots inside a directory.
    /// Scans for directories matching "tosec*" (case-insensitive), then
    /// descends one level to find the versioned release folder (e.g.
    /// "tosec-full-2022-07-10").  Each such folder is added as a candidate.
    void discover_tosec_roots(const std::string& search_dir,
                              std::vector<ScanRootEntry>& candidates,
                              std::vector<std::string>& tried) const {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (!fs::is_directory(search_dir, ec)) return;

        auto try_candidate = [&](const std::string& p) {
            std::string norm = p;
            while (norm.size() > 1 && norm.back() == '/') norm.pop_back();
            for (const auto& t : tried)
                if (t == norm) return;
            tried.push_back(norm);
            for (const auto& r : roots_)
                if (r.path == norm) return;
            ScanRootEntry e;
            e.path = norm;
            check_entry(e);
            e.selected = e.exists && e.has_files;
            candidates.push_back(std::move(e));
        };

        try {
        for (const auto& entry : fs::directory_iterator(search_dir, ec)) {
            if (!entry.is_directory(ec)) continue;
            std::string name = entry.path().filename().string();
            std::string name_lower = name;
            std::transform(name_lower.begin(), name_lower.end(),
                           name_lower.begin(), ::tolower);

            if (name_lower.substr(0, 5) == "tosec") {
                // This could be the root itself (if it contains manufacturer folders)
                // or a container for versioned releases — check one level deeper.
                bool has_subdirs = false;
                for (const auto& sub : fs::directory_iterator(entry.path(), ec)) {
                    if (sub.is_directory(ec)) {
                        has_subdirs = true;
                        // Check if subdirectory looks like a versioned release
                        // (contains manufacturer folders like "Nintendo", "Commodore")
                        std::string sub_name = sub.path().filename().string();
                        std::string sub_lower = sub_name;
                        std::transform(sub_lower.begin(), sub_lower.end(),
                                       sub_lower.begin(), ::tolower);
                        if (sub_lower.substr(0, 5) == "tosec") {
                            // Versioned release folder — add it
                            try_candidate(sub.path().string());
                        }
                    }
                }
                // If the tosec dir itself contains non-tosec subdirs,
                // it's likely the root with manufacturer folders inside.
                if (has_subdirs) {
                    try_candidate(entry.path().string());
                }
            }
        }
        } catch (const fs::filesystem_error&) {}
    }
};

} // namespace scan_roots

#endif // CERMU_HAS_GUI
