#pragma once

// =============================================================================
// file_browser.hpp — VFS-aware file/directory browser for the Cermu launcher
// =============================================================================
//
// Standalone directory navigator for Zone B of the launcher panel:
//   - Breadcrumb path bar with archive segment highlighting
//   - Three-column file list (Name, Size, Type) with sorting
//   - Search filter and format-based extension filter
//   - VFS-transparent: archives and containers appear as directories
//   - Click/double-click navigation following design doc §5 conventions
//
// Scans real directories via std::filesystem and delegates archive-interior
// browsing to VfsFileSystem (which backs ImGuiFileDialog).
//
// =============================================================================

#ifdef CERMU_HAS_GUI

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "gui/launcher_theme.hpp"
#include "gui/vfs_file_system.hpp"
#include "core/formats/format_handler.hpp"
#include "core/config/path_discovery.hpp"

/// A single entry in the file browser listing.
struct FileBrowserEntry {
    std::string name;           ///< Display name (filename only)
    std::string full_path;      ///< Full path (filesystem or VFS)
    size_t      size = 0;       ///< File size in bytes (0 for dirs)
    bool        is_directory = false;
    bool        is_archive   = false;  ///< ZIP/7z/RAR — always navigable
    bool        is_container = false;  ///< D64/T64/LNX — navigable when enabled
};

/// Sort column for the file list.
enum class FileBrowserSort { Name, Size, Type };

/**
 * FileBrowser — directory navigator for the launcher's Zone B.
 *
 * Usage:
 *   1. Call set_formats() to control extension filtering (nullptr = show all).
 *   2. Call navigate_to() to set the current path.
 *   3. Call render() every frame inside the parent ImGui window/child.
 *   4. Poll selected_file() and file_activated() for selection/launch events.
 */
class FileBrowser {
public:
    FileBrowser();

    /// Set the format filter based on a system's supported_formats.
    /// nullptr = show all files.
    void set_formats(const format_descriptor_t* const* formats);

    /// Navigate to a directory (real or VFS path).
    void navigate_to(const std::string& path);

    /// Navigate to the system's default data folder.
    void navigate_to_system_data(const char* data_folder, const char* const* aliases);

    /// Navigate to parent directory.
    void navigate_up();

    /// Get the current directory path.
    const std::string& current_path() const { return current_path_; }

    /// Get the currently selected file path (empty if none).
    const std::string& selected_file() const { return selected_file_; }

    /// True when a file was double-clicked or Enter pressed (launch trigger).
    bool file_activated() const { return file_activated_; }

    /// Clear the activation flag after processing.
    void clear_activation() { file_activated_ = false; }

    /// Clear file selection and any probe trigger.
    void clear_selection() { selected_file_.clear(); file_selection_changed_ = false; }

    /// True when the selected file changed this frame (trigger for probe).
    bool file_selection_changed() const { return file_selection_changed_; }

    /// Render the file browser UI. Call inside an ImGui child/window.
    void render();

private:
    // State
    std::string current_path_;
    std::string selected_file_;
    bool        file_activated_ = false;
    bool        file_selection_changed_ = false;

    // Directory contents
    std::vector<FileBrowserEntry> entries_;
    std::vector<FileBrowserEntry> filtered_entries_;

    // Filtering
    char search_buf_[256] = {};
    const format_descriptor_t* const* active_formats_ = nullptr;
    std::vector<std::string> active_extensions_;  // cached from active_formats_

    // Sorting
    FileBrowserSort sort_column_ = FileBrowserSort::Name;
    bool sort_ascending_ = true;

    // Rendering
    void render_path_bar();
    void render_file_list();

    // Helpers
    void scan_directory();
    void apply_filter_and_sort();
    bool matches_format_filter(const FileBrowserEntry& entry) const;
    bool matches_search(const FileBrowserEntry& entry) const;
    void rebuild_extension_cache();

    static std::string format_size(size_t bytes);
    static std::string get_type_label(const FileBrowserEntry& entry);
};

// =============================================================================
// Implementation
// =============================================================================

inline FileBrowser::FileBrowser() {
    // Start at the user's home directory as fallback
    const char* home = std::getenv("HOME");
    if (home) {
        current_path_ = home;
    } else {
        current_path_ = "/";
    }
}

inline void FileBrowser::set_formats(const format_descriptor_t* const* formats) {
    active_formats_ = formats;
    rebuild_extension_cache();
    apply_filter_and_sort();
}

inline void FileBrowser::navigate_to(const std::string& path) {
    if (path.empty()) return;
    current_path_ = path;
    search_buf_[0] = '\0';
    selected_file_.clear();
    file_activated_ = false;
    file_selection_changed_ = false;
    scan_directory();
    apply_filter_and_sort();
}

inline void FileBrowser::navigate_to_system_data(const char* data_folder,
                                                  const char* const* aliases) {
    if (!data_folder) return;

    char path_buf[1024];
    bool found = false;

    // Try data_folder first, then aliases
    if (aliases) {
        found = system_config_discover_data_root(aliases, path_buf, sizeof(path_buf));
    }
    if (!found) {
        found = system_config_discover_data_root(data_folder, path_buf, sizeof(path_buf));
    }

    if (found) {
        navigate_to(std::string(path_buf));
    }
}

inline void FileBrowser::navigate_up() {
    if (current_path_.empty() || current_path_ == "/") return;

    // Handle VFS paths (with !/ boundaries)
    auto bang_pos = current_path_.rfind("!/");
    if (bang_pos != std::string::npos) {
        // Inside an archive — navigate up within it or exit
        auto inner = current_path_.substr(bang_pos + 2);
        auto slash = inner.rfind('/');
        if (slash != std::string::npos) {
            // Go up one level inside the archive
            navigate_to(current_path_.substr(0, bang_pos + 2 + slash));
        } else {
            // Exit the archive entirely
            navigate_to(current_path_.substr(0, bang_pos));
        }
        return;
    }

    // Regular filesystem path — go to parent
    std::filesystem::path p(current_path_);
    auto parent = p.parent_path();
    if (!parent.empty() && parent != p) {
        navigate_to(parent.string());
    }
}

inline void FileBrowser::scan_directory() {
    entries_.clear();

    // Check if we're inside a VFS path
    if (VfsFileSystem::is_virtual_path(current_path_)) {
        // Inside an archive or container — use VfsFileSystem to scan
        // Create a temporary VfsFileSystem instance for scanning
        VfsFileSystem vfs;
        VfsFileSystem::set_browse_containers(true);
        auto igfd_entries = vfs.ScanDirectory(current_path_);

        for (const auto& e : igfd_entries) {
            FileBrowserEntry entry;
            entry.name = e.fileNameExt;
            entry.full_path = current_path_ + "/" + e.fileNameExt;

            // Check if directory type
            if (e.fileType.isDir()) {
                entry.is_directory = true;
            } else {
                entry.size = e.fileSize;
                entry.is_archive = VfsFileSystem::has_archive_extension(entry.name);
                entry.is_container = VfsFileSystem::has_container_extension(entry.name);
            }
            entries_.push_back(std::move(entry));
        }
        return;
    }

    // Real filesystem scan
    namespace fs = std::filesystem;
    std::error_code ec;

    for (const auto& dir_entry : fs::directory_iterator(current_path_, ec)) {
        FileBrowserEntry entry;
        entry.name = dir_entry.path().filename().string();

        // Skip hidden files
        if (!entry.name.empty() && entry.name[0] == '.') continue;

        entry.full_path = dir_entry.path().string();

        if (dir_entry.is_directory(ec)) {
            entry.is_directory = true;
        } else if (dir_entry.is_regular_file(ec)) {
            entry.size = dir_entry.file_size(ec);
            entry.is_archive = VfsFileSystem::has_archive_extension(entry.name);
            entry.is_container = VfsFileSystem::has_container_extension(entry.name);
        } else {
            continue;  // Skip symlinks, etc.
        }

        entries_.push_back(std::move(entry));
    }
}

inline void FileBrowser::apply_filter_and_sort() {
    filtered_entries_.clear();

    for (const auto& entry : entries_) {
        // Directories always pass format filter
        if (!entry.is_directory && !matches_format_filter(entry))
            continue;
        if (!matches_search(entry))
            continue;
        filtered_entries_.push_back(entry);
    }

    // Sort: directories first, then by selected column
    std::sort(filtered_entries_.begin(), filtered_entries_.end(),
        [this](const FileBrowserEntry& a, const FileBrowserEntry& b) {
            // Directories always first
            if (a.is_directory != b.is_directory)
                return a.is_directory;

            // Archives/containers treated as directories in sort
            bool a_nav = a.is_archive || a.is_container;
            bool b_nav = b.is_archive || b.is_container;
            if (a_nav != b_nav)
                return a_nav;

            bool asc = sort_ascending_;
            switch (sort_column_) {
                case FileBrowserSort::Size: {
                    bool lt = a.size < b.size;
                    return asc ? lt : !lt;
                }
                case FileBrowserSort::Type: {
                    auto ta = get_type_label(a);
                    auto tb = get_type_label(b);
                    int cmp = ta.compare(tb);
                    return asc ? (cmp < 0) : (cmp > 0);
                }
                case FileBrowserSort::Name:
                default: {
                    int cmp = strcasecmp(a.name.c_str(), b.name.c_str());
                    return asc ? (cmp < 0) : (cmp > 0);
                }
            }
        });
}

inline bool FileBrowser::matches_format_filter(const FileBrowserEntry& entry) const {
    // Archives are always visible
    if (entry.is_archive || entry.is_container) return true;

    // No filter set — show everything
    if (active_extensions_.empty()) return true;

    std::string ext = VfsFileSystem::get_extension(entry.name);
    if (ext.empty()) return false;

    for (const auto& ae : active_extensions_) {
        if (ext == ae) return true;
    }
    return false;
}

inline bool FileBrowser::matches_search(const FileBrowserEntry& entry) const {
    if (search_buf_[0] == '\0') return true;

    std::string name_lower = entry.name;
    std::string query_lower(search_buf_);
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
    return name_lower.find(query_lower) != std::string::npos;
}

inline void FileBrowser::rebuild_extension_cache() {
    active_extensions_.clear();
    if (!active_formats_) return;

    for (const format_descriptor_t* const* p = active_formats_; *p; ++p) {
        const format_descriptor_t* fmt = *p;
        if (fmt->extensions) {
            for (const char** ext = fmt->extensions; *ext; ++ext) {
                std::string e(*ext);
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                active_extensions_.push_back(e);
            }
        }
    }

    // Deduplicate
    std::sort(active_extensions_.begin(), active_extensions_.end());
    active_extensions_.erase(
        std::unique(active_extensions_.begin(), active_extensions_.end()),
        active_extensions_.end());
}

inline std::string FileBrowser::format_size(size_t bytes) {
    if (bytes == 0) return "";
    if (bytes < 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%zu B", bytes);
        return buf;
    }
    if (bytes < 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024.0));
    return buf;
}

inline std::string FileBrowser::get_type_label(const FileBrowserEntry& entry) {
    if (entry.is_directory) return "DIR";
    if (entry.is_archive) return "ARC";
    if (entry.is_container) return "IMG";

    std::string ext = VfsFileSystem::get_extension(entry.name);
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    return ext;
}

// =============================================================================
// Rendering
// =============================================================================

inline void FileBrowser::render() {
    file_selection_changed_ = false;
    file_activated_ = false;

    render_path_bar();
    render_file_list();

    // Keyboard: Backspace = navigate up (when no text input active)
    if (!ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        navigate_up();
    }
}

inline void FileBrowser::render_path_bar() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, launcher_theme::kBarBg);
    ImGui::BeginChild("##PathBar", ImVec2(0, 28), false);

    ImGui::SetCursorPos(ImVec2(8, 4));

    // Home/default button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kAccentTeal);
    if (ImGui::SmallButton("\xe2\x8c\x82")) { // ⌂
        // Navigate to home / ROMs root — fallback to home dir
        const char* home = std::getenv("HOME");
        if (home) navigate_to(home);
    }
    ImGui::PopStyleColor(2);

    // Breadcrumb segments
    ImGui::SameLine();

    // Parse path into segments, highlighting archive boundaries
    std::string path = current_path_;

    // Split on '/' and '!/'
    std::vector<std::pair<std::string, bool>> segments;  // <segment, is_archive_boundary>
    std::string accumulator;

    size_t i = 0;
    // Handle leading / on Unix
    if (!path.empty() && path[0] == '/') {
        segments.push_back({"/", false});
        i = 1;
    }

    while (i < path.size()) {
        if (path[i] == '!' && i + 1 < path.size() && path[i+1] == '/') {
            // Archive boundary
            if (!accumulator.empty()) {
                segments.push_back({accumulator, true});
                accumulator.clear();
            }
            i += 2;
        } else if (path[i] == '/') {
            if (!accumulator.empty()) {
                segments.push_back({accumulator, false});
                accumulator.clear();
            }
            i++;
        } else {
            accumulator += path[i];
            i++;
        }
    }
    if (!accumulator.empty()) {
        segments.push_back({accumulator, false});
    }

    // Render clickable breadcrumbs
    std::string rebuilt_path;
    for (size_t s = 0; s < segments.size(); ++s) {
        const auto& [seg, is_archive] = segments[s];

        if (s == 0 && seg == "/") {
            rebuilt_path = "/";
        } else {
            if (is_archive) {
                rebuilt_path += seg + "!/";
            } else {
                if (!rebuilt_path.empty() && rebuilt_path.back() != '/')
                    rebuilt_path += '/';
                rebuilt_path += seg;
            }
        }

        ImGui::SameLine(0, 0);

        // Separator
        if (s > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
            ImGui::Text(" > ");
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 0);
        }

        // Segment button
        ImVec4 text_color = is_archive ? launcher_theme::kBreadcrumbArchive
                                       : launcher_theme::kTextSecondary;
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, launcher_theme::kFileRowHover);

        std::string btn_id = "##seg" + std::to_string(s);
        if (ImGui::SmallButton((seg + btn_id).c_str())) {
            // Navigate to this path
            if (s < segments.size() - 1) {
                navigate_to(rebuilt_path);
            }
        }
        ImGui::PopStyleColor(3);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

inline void FileBrowser::render_file_list() {
    // Search input with placeholder hint
    ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kSearchInputBg);
    ImGui::PushItemWidth(200);
    if (ImGui::InputTextWithHint("##FileSearch", "Title, year, region, format\xe2\x80\xa6",
                                  search_buf_, sizeof(search_buf_))) {
        apply_filter_and_sort();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Format filter combo
    {
        const char* label = active_formats_ ? "Filtered" : "All files";
        ImGui::PushStyleColor(ImGuiCol_FrameBg, launcher_theme::kSearchInputBg);
        ImGui::PushItemWidth(90);
        // Display-only for now — toggling handled via set_formats()
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
        ImGui::Text("[%s]", label);
        ImGui::PopStyleColor();
        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
    ImGui::Text("%d items", static_cast<int>(filtered_entries_.size()));
    ImGui::PopStyleColor();

    // Column headers
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
    float name_width = ImGui::GetContentRegionAvail().x - 70 - 60 - 16;
    ImGui::SetCursorPosX(8);
    {
        auto sort_header = [&](const char* label, FileBrowserSort col, float width) {
            bool is_active = (sort_column_ == col);
            if (is_active) ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextSecondary);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            std::string id = std::string(label) + "##sort" + std::to_string(static_cast<int>(col));
            if (ImGui::SmallButton(id.c_str())) {
                if (sort_column_ == col) {
                    sort_ascending_ = !sort_ascending_;
                } else {
                    sort_column_ = col;
                    sort_ascending_ = true;
                }
                apply_filter_and_sort();
            }
            ImGui::PopStyleColor();

            // Sort indicator: active column shows direction, others show inactive indicator
            ImGui::SameLine(0, 2);
            if (is_active) {
                ImGui::Text(sort_ascending_ ? "\xe2\x96\xb2" : "\xe2\x96\xbc");  // ▲ ▼
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
                ImGui::Text("\xe2\x87\x85");  // ⇅
                ImGui::PopStyleColor();
            }
        };

        sort_header("Name", FileBrowserSort::Name, name_width);
        ImGui::SameLine(name_width + 8);
        sort_header("Size", FileBrowserSort::Size, 70);
        ImGui::SameLine(name_width + 78);
        sort_header("Type", FileBrowserSort::Type, 60);
    }
    ImGui::PopStyleColor();

    ImGui::Separator();

    // File list (scrollable)
    ImGui::BeginChild("##FileList", ImVec2(0, 0), false);

    for (int i = 0; i < static_cast<int>(filtered_entries_.size()); ++i) {
        const auto& entry = filtered_entries_[i];
        bool is_selected = (!selected_file_.empty() && entry.full_path == selected_file_);

        ImGui::PushID(i);

        // Row colors
        if (is_selected) {
            ImGui::PushStyleColor(ImGuiCol_Header, launcher_theme::kFileRowSelected);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, launcher_theme::kFileRowSelected);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, launcher_theme::kFileRowHover);
        }

        if (ImGui::Selectable("##entry", is_selected,
                              ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns,
                              ImVec2(0, 20))) {

            if (entry.is_directory || entry.is_archive || entry.is_container) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    // Double-click navigable entry: navigate into
                    std::string target = entry.full_path;
                    if (entry.is_archive || entry.is_container) {
                        target += "!/";
                    }
                    navigate_to(target);
                } else {
                    // Single-click dir: just select (no navigation)
                    selected_file_ = entry.full_path;
                    file_selection_changed_ = true;
                }
            } else {
                // File
                if (ImGui::IsMouseDoubleClicked(0)) {
                    // Double-click file: activate (launch)
                    selected_file_ = entry.full_path;
                    file_activated_ = true;
                } else {
                    // Single-click file: select
                    if (selected_file_ != entry.full_path) {
                        selected_file_ = entry.full_path;
                        file_selection_changed_ = true;
                    }
                }
            }
        }

        ImGui::PopStyleColor(2);

        // Overlay: icon + name
        ImVec2 row_min = ImGui::GetItemRectMin();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Icon
        const char* icon = "\xf0\x9f\x93\x84";  // 📄
        ImVec4 icon_color = launcher_theme::kTextMuted;
        if (entry.is_directory) {
            icon = "\xf0\x9f\x93\x81";  // 📁
            icon_color = launcher_theme::kAccentTeal;
        } else if (entry.is_archive) {
            icon = "\xf0\x9f\x93\xa6";  // 📦
            icon_color = launcher_theme::kBreadcrumbArchive;
        } else if (entry.is_container) {
            icon = "\xf0\x9f\x92\xbe";  // 💾
            icon_color = launcher_theme::kAccentBlue;
        }

        dl->AddText(ImVec2(row_min.x + 4, row_min.y + 2),
                     ImGui::GetColorU32(icon_color), icon);

        // File name
        dl->AddText(ImVec2(row_min.x + 24, row_min.y + 2),
                     ImGui::GetColorU32(launcher_theme::kTextPrimary),
                     entry.name.c_str());

        // Size column
        if (!entry.is_directory) {
            std::string size_str = format_size(entry.size);
            dl->AddText(ImVec2(row_min.x + name_width + 8, row_min.y + 2),
                         ImGui::GetColorU32(launcher_theme::kTextMuted),
                         size_str.c_str());
        }

        // Type column
        std::string type_str = get_type_label(entry);
        dl->AddText(ImVec2(row_min.x + name_width + 78, row_min.y + 2),
                     ImGui::GetColorU32(launcher_theme::kTextDimmed),
                     type_str.c_str());

        ImGui::PopID();
    }

    // Empty state
    if (filtered_entries_.empty()) {
        float cx = ImGui::GetWindowWidth() * 0.5f;
        float cy = ImGui::GetWindowHeight() * 0.5f;
        ImGui::SetCursorPos(ImVec2(cx - 90, cy - 10));
        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
        ImGui::Text("No files found in this directory");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
}

#endif // CERMU_HAS_GUI
