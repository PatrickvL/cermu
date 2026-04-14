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
#include "utils/file_watcher.hpp"
#include "utils/rom_filename_parser.hpp"

/// A single entry in the file browser listing.
struct FileBrowserEntry {
    std::string name;           ///< Display name (filename only)
    std::string full_path;      ///< Full path (filesystem or VFS)
    size_t      size = 0;       ///< File size in bytes (0 for dirs)
    bool        is_directory = false;
    bool        is_archive   = false;  ///< ZIP/7z/RAR — always navigable
    bool        is_container = false;  ///< D64/T64/LNX — navigable when enabled
    bool        is_container_entry = false; ///< Entry inside a container (D64/T64/LNX directory)
    bool        is_foreign_format  = false; ///< Doesn't match the selected system's formats

    // Parsed from filename (TOSEC / No-Intro / GoodTools conventions)
    std::string parsed_title;   ///< Cleaned title (empty = use name)
    std::string region;         ///< Region code (e.g. "USA", "Europe")
    std::string year;           ///< Release year (e.g. "1985")
    std::vector<std::string> tags;   ///< Bracket tags: [!], [b], etc.
    std::vector<std::string> flags;  ///< Paren flags: (Unl), (Rev A), etc.
};

/// Sort column for the file list.
enum class FileBrowserSort { Name, Region, Year, Size, Type };

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

    /// Set external search paths (e.g. scan roots) to check when the
    /// system data folder isn't found via path discovery.  The browser
    /// searches these roots recursively (up to 3 levels) for a directory
    /// whose name matches one of the system aliases.
    void set_external_search_paths(const std::vector<std::string>& paths);

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

    // Deferred navigation — set during render, executed after the loop
    std::string pending_navigate_;

    // Helpers
    void scan_directory();
    void apply_filter_and_sort();
    void parse_entry_metadata(FileBrowserEntry& entry);
    bool matches_format_filter(const FileBrowserEntry& entry) const;
    bool matches_search(const FileBrowserEntry& entry) const;
    bool matches_region_filter(const FileBrowserEntry& entry) const;
    void rebuild_extension_cache();

    static std::string format_size(size_t bytes);
    static std::string get_type_label(const FileBrowserEntry& entry);

    // Region filter
    std::string region_filter_;  // empty = show all

    // True when browsing inside a container (D64/T64/LNX directory)
    bool inside_container_ = false;

    // External search paths (scan roots / TOSEC collections)
    std::vector<std::string> external_search_paths_;

    // Filesystem watch — auto-rescan when directory contents change on disk
    file_watcher::FileWatcher dir_watcher_;
    uint32_t watcher_cooldown_ = 0;  // frames to skip after rescan (debounce)
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

    // Watch real directories for changes (skip VFS paths)
    if (path.find("!/") == std::string::npos)
        dir_watcher_.watch(path);
    else
        dir_watcher_.unwatch();
    watcher_cooldown_ = 0;
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
        return;
    }

    // Fallback: search external paths (scan roots / TOSEC collections) for
    // a directory whose name case-insensitively matches one of the aliases.
    if (aliases) {
        namespace fs = std::filesystem;
        std::error_code ec;

        // Collect alias names for case-insensitive matching
        std::vector<std::string> alias_lower;
        for (const char* const* p = aliases; *p; ++p) {
            std::string a(*p);
            std::transform(a.begin(), a.end(), a.begin(), ::tolower);
            alias_lower.push_back(std::move(a));
        }
        // Also try the data_folder name
        {
            std::string df(data_folder);
            std::transform(df.begin(), df.end(), df.begin(), ::tolower);
            alias_lower.push_back(std::move(df));
        }

        // BFS through external roots up to 3 levels deep
        for (const auto& root : external_search_paths_) {
            if (!fs::is_directory(root, ec)) continue;

            // Queue: (path, depth)
            std::vector<std::pair<std::string, int>> queue;
            queue.push_back({root, 0});

            for (size_t qi = 0; qi < queue.size(); ++qi) {
                const auto& [dir, depth] = queue[qi];
                if (depth > 3) continue;

                try {
                for (const auto& entry : fs::directory_iterator(dir, ec)) {
                    if (!entry.is_directory(ec)) continue;
                    std::string name = entry.path().filename().string();
                    std::string name_lower = name;
                    std::transform(name_lower.begin(), name_lower.end(),
                                   name_lower.begin(), ::tolower);

                    // Check if this directory name matches any alias
                    for (const auto& alias : alias_lower) {
                        if (name_lower == alias) {
                            navigate_to(entry.path().string());
                            return;
                        }
                    }

                    // Enqueue subdirectories for deeper search
                    if (depth < 3) {
                        queue.push_back({entry.path().string(), depth + 1});
                    }
                }
                } catch (const fs::filesystem_error&) {
                    // Skip inaccessible directories
                }
            }
        }
    }
}

inline void FileBrowser::set_external_search_paths(const std::vector<std::string>& paths) {
    external_search_paths_ = paths;
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
    inside_container_ = false;

    // Handle paths with "!/" VFS delimiter (archive/container boundaries).
    // The FileBrowser uses "!/" to denote entering a browsable file;
    // VfsFileSystem::ScanDirectory expects the real path without the
    // trailing "!/" for container-at-root (Case 1).
    std::string scan_path = current_path_;
    {
        // Strip trailing "!/" or "/" to get the canonical scan target.
        while (scan_path.size() > 1 && scan_path.back() == '/')
            scan_path.pop_back();
        if (scan_path.size() >= 2 &&
            scan_path[scan_path.size() - 1] == '!' &&
            (scan_path.size() < 3 || scan_path[scan_path.size() - 2] != '/')) {
            // Path ends with "!" (from "container.d64!/" after stripping '/'):
            // remove the trailing '!' so ScanDirectory sees the real file path.
            scan_path.pop_back();
        }
    }

    // Check if we're inside a VFS path
    if (VfsFileSystem::is_virtual_path(scan_path)) {
        // Inside an archive or container — use VfsFileSystem to scan
        VfsFileSystem vfs;
        VfsFileSystem::set_browse_containers(true);
        auto igfd_entries = vfs.ScanDirectory(scan_path);

        // Detect container-entry mode: if scan_path is a container file
        // (D64/T64/LNX), the entries inside are container entries.
        bool is_container_root = VfsFileSystem::has_container_extension(scan_path);
        inside_container_ = is_container_root;

        for (const auto& e : igfd_entries) {
            FileBrowserEntry entry;
            entry.name = e.fileNameExt;
            // Avoid double-slash: current_path_ may already end with '/'
            if (!current_path_.empty() && current_path_.back() == '/')
                entry.full_path = current_path_ + e.fileNameExt;
            else
                entry.full_path = current_path_ + "/" + e.fileNameExt;

            // Check if directory type
            if (e.fileType.isDir()) {
                entry.is_directory = true;
            } else {
                entry.size = e.fileSize;
                entry.is_archive = VfsFileSystem::has_archive_extension(entry.name);
                entry.is_container = VfsFileSystem::has_container_extension(entry.name);
                entry.is_container_entry = is_container_root;
            }
            entries_.push_back(std::move(entry));
        }
        // Parse metadata for all entries
        for (auto& e : entries_) parse_entry_metadata(e);
        return;
    }

    // Real filesystem scan
    namespace fs = std::filesystem;
    std::error_code ec;

    try {
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
    } catch (const fs::filesystem_error&) {
        // Iterator increment can throw on I/O errors; eat it gracefully
    }

    // Parse metadata for all entries
    for (auto& e : entries_) parse_entry_metadata(e);
}

inline void FileBrowser::apply_filter_and_sort() {
    filtered_entries_.clear();

    for (auto entry : entries_) {
        // Directories always pass format filter
        if (!entry.is_directory && !matches_format_filter(entry))
            continue;
        if (!matches_search(entry))
            continue;
        if (!matches_region_filter(entry))
            continue;

        // Mark files that don't match the selected system's formats.
        // Inside containers (D64/T64/LNX): all entries pass the filter,
        // but files with extensions that belong to a different system get
        // the foreign flag.  Container entries (no ext) are never foreign.
        entry.is_foreign_format = false;
        if (active_formats_ && !entry.is_directory &&
            !entry.is_archive && !entry.is_container &&
            !entry.is_container_entry) {
            std::string ext = VfsFileSystem::get_extension(entry.name);
            if (!ext.empty()) {
                bool found = false;
                for (const auto& ae : active_extensions_) {
                    if (ext == ae) { found = true; break; }
                }
                entry.is_foreign_format = !found;
            }
        }

        filtered_entries_.push_back(std::move(entry));
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
                    if (a.size != b.size)
                        return asc ? (a.size < b.size) : (a.size > b.size);
                    // Fall through to name for stable tie-breaking
                    break;
                }
                case FileBrowserSort::Type: {
                    auto ta = get_type_label(a);
                    auto tb = get_type_label(b);
                    int cmp = ta.compare(tb);
                    if (cmp != 0) return asc ? (cmp < 0) : (cmp > 0);
                    break;  // tie-break by name
                }
                case FileBrowserSort::Region: {
                    int cmp = strcasecmp(a.region.c_str(), b.region.c_str());
                    if (cmp != 0) return asc ? (cmp < 0) : (cmp > 0);
                    break;  // tie-break by name
                }
                case FileBrowserSort::Year: {
                    int cmp = a.year.compare(b.year);
                    if (cmp != 0) return asc ? (cmp < 0) : (cmp > 0);
                    break;  // tie-break by name
                }
                case FileBrowserSort::Name:
                default:
                    break;  // handled below
            }

            // Name-based tie-breaker (always reached for equal primary keys)
            const auto& na = a.parsed_title.empty() ? a.name : a.parsed_title;
            const auto& nb = b.parsed_title.empty() ? b.name : b.parsed_title;
            int cmp = strcasecmp(na.c_str(), nb.c_str());
            return asc ? (cmp < 0) : (cmp > 0);
        });
}

inline bool FileBrowser::matches_format_filter(const FileBrowserEntry& entry) const {
    // Archives and containers are always visible (navigable)
    if (entry.is_archive || entry.is_container) return true;

    // Container entries (files inside D64/T64/LNX) always pass —
    // they have no file extension, but are implicitly loadable.
    if (entry.is_container_entry) return true;

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

    std::string query_lower(search_buf_);
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

    // Match against name
    std::string name_lower = entry.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    if (name_lower.find(query_lower) != std::string::npos) return true;

    // Match against parsed title
    if (!entry.parsed_title.empty()) {
        std::string title_lower = entry.parsed_title;
        std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), ::tolower);
        if (title_lower.find(query_lower) != std::string::npos) return true;
    }

    // Match against region and year
    std::string region_lower = entry.region;
    std::transform(region_lower.begin(), region_lower.end(), region_lower.begin(), ::tolower);
    if (!region_lower.empty() && region_lower.find(query_lower) != std::string::npos) return true;
    if (!entry.year.empty() && entry.year.find(query_lower) != std::string::npos) return true;

    return false;
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

inline void FileBrowser::parse_entry_metadata(FileBrowserEntry& entry) {
    if (entry.is_directory) return;
    auto info = rom_filename::parse(entry.name);
    entry.parsed_title = std::move(info.title);
    entry.region       = std::move(info.region);
    entry.year         = std::move(info.year);
    entry.tags         = std::move(info.tags);
    entry.flags        = std::move(info.flags);
}

inline bool FileBrowser::matches_region_filter(const FileBrowserEntry& entry) const {
    if (region_filter_.empty()) return true;
    if (entry.is_directory) return true;
    // Match short code
    auto sr = rom_filename::short_region(entry.region);
    return sr == rom_filename::short_region(region_filter_);
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

    // Auto-rescan on filesystem changes (inotify)
    if (watcher_cooldown_ > 0) {
        --watcher_cooldown_;
    } else if (dir_watcher_.poll_changed()) {
        scan_directory();
        apply_filter_and_sort();
        watcher_cooldown_ = 30;  // debounce: skip 30 frames (~0.5s at 60fps)
    }

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
            // Defer navigation to after rendering (same reason as file list)
            if (s < segments.size() - 1) {
                pending_navigate_ = rebuilt_path;
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

    // ── Filter pills ────────────────────────────────────────────────────────

    // Helper lambda for rendering a filter pill (returns true if clicked)
    auto render_pill = [](const char* label, const char* value, bool is_active) -> bool {
        ImVec4 bg = ImVec4(0.035f, 0.047f, 0.102f, 1.0f);
        ImVec4 border = is_active ? launcher_theme::kFilterActiveBorder
                                 : ImVec4(0.063f, 0.094f, 0.133f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bg.x * 1.3f, bg.y * 1.3f, bg.z * 1.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 4);

        ImGui::PushStyleColor(ImGuiCol_Text,
            is_active ? launcher_theme::kAccentBlue : launcher_theme::kTextMuted);
        std::string btn_id = std::string(value) + "##pill_" + label;
        bool clicked = ImGui::SmallButton(btn_id.c_str());
        ImGui::PopStyleColor();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        return clicked;
    };

    // REGION pill — clickable cycle through detected regions
    {
        const char* rgn_val = region_filter_.empty() ? "All" : region_filter_.c_str();
        bool rgn_active = !region_filter_.empty();
        if (render_pill("REGION", rgn_val, rgn_active)) {
            // Cycle through detected regions
            std::vector<std::string> regions;
            for (const auto& e : entries_) {
                if (!e.region.empty()) {
                    auto sr = std::string(rom_filename::short_region(e.region));
                    if (std::find(regions.begin(), regions.end(), sr) == regions.end())
                        regions.push_back(sr);
                }
            }
            std::sort(regions.begin(), regions.end());

            if (regions.empty()) {
                region_filter_.clear();
            } else if (region_filter_.empty()) {
                region_filter_ = regions.front();
            } else {
                auto it = std::find(regions.begin(), regions.end(), region_filter_);
                if (it == regions.end() || ++it == regions.end())
                    region_filter_.clear();  // wrap to "All"
                else
                    region_filter_ = *it;
            }
            apply_filter_and_sort();
        }
    }
    ImGui::SameLine(0, 6);

    // FORMAT pill
    {
        const char* fmt_val = active_formats_ ? "Filtered" : "All";
        bool fmt_active = (active_formats_ != nullptr);
        render_pill("FORMAT", fmt_val, fmt_active);
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextDimmed);
    ImGui::Text("%d items", static_cast<int>(filtered_entries_.size()));
    ImGui::PopStyleColor();

    // ── Column headers ──────────────────────────────────────────────────────
    // Layout: Name (flexible) | Region (50px) | Year (40px) | Size (70px) | Type (60px)
    ImGui::PushStyleColor(ImGuiCol_Text, launcher_theme::kTextMuted);
    constexpr float kRegionW = 50, kYearW = 40, kSizeW = 70, kTypeW = 60;
    float name_width = ImGui::GetContentRegionAvail().x - kRegionW - kYearW - kSizeW - kTypeW - 16;
    float col_region = name_width + 8;
    float col_year   = col_region + kRegionW;
    float col_size   = col_year + kYearW;
    float col_type   = col_size + kSizeW;

    ImGui::SetCursorPosX(8);
    {
        auto sort_header = [&](const char* label, FileBrowserSort col, float /*width*/) {
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

            // Sort indicator
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
        ImGui::SameLine(col_region);
        sort_header("Rgn", FileBrowserSort::Region, kRegionW);
        ImGui::SameLine(col_year);
        sort_header("Year", FileBrowserSort::Year, kYearW);
        ImGui::SameLine(col_size);
        sort_header("Size", FileBrowserSort::Size, kSizeW);
        ImGui::SameLine(col_type);
        sort_header("Type", FileBrowserSort::Type, kTypeW);
    }
    ImGui::PopStyleColor();

    ImGui::Separator();

    // ── File list (scrollable) ──────────────────────────────────────────────
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
                    pending_navigate_ = entry.full_path;
                    if (entry.is_archive || entry.is_container) {
                        pending_navigate_ += "!/";
                    }
                } else {
                    selected_file_ = entry.full_path;
                    file_selection_changed_ = true;
                }
            } else {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    selected_file_ = entry.full_path;
                    file_activated_ = true;
                } else {
                    if (selected_file_ != entry.full_path) {
                        selected_file_ = entry.full_path;
                        file_selection_changed_ = true;
                    }
                }
            }
        }

        ImGui::PopStyleColor(2);

        // Overlay: icon + columns
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

        // Name column (show parsed title for files, raw name for dirs)
        const char* display_name = (!entry.parsed_title.empty() && !entry.is_directory)
                                   ? entry.parsed_title.c_str()
                                   : entry.name.c_str();
        ImVec4 name_color = entry.is_foreign_format
                            ? launcher_theme::kTextForeignFormat
                            : launcher_theme::kTextPrimary;
        dl->AddText(ImVec2(row_min.x + 24, row_min.y + 2),
                     ImGui::GetColorU32(name_color),
                     display_name);

        // Tag/flag badges — rendered as small colored pills after the title
        if (!entry.tags.empty() || !entry.flags.empty()) {
            ImVec2 text_size = ImGui::CalcTextSize(display_name);
            float badge_x = row_min.x + 24 + text_size.x + 6;
            float badge_y = row_min.y + 2;
            float badge_h = text_size.y;

            auto draw_badge = [&](const char* label, ImVec4 color) {
                ImVec2 lsz = ImGui::CalcTextSize(label);
                float pw = lsz.x + 6;  // pill width with padding
                // Pill background
                dl->AddRectFilled(
                    ImVec2(badge_x, badge_y),
                    ImVec2(badge_x + pw, badge_y + badge_h),
                    ImGui::GetColorU32(launcher_theme::kTagBg), 3.0f);
                // Text
                dl->AddText(ImVec2(badge_x + 3, badge_y),
                             ImGui::GetColorU32(color), label);
                badge_x += pw + 3;  // advance for next badge
            };

            // Bracket tags: [!], [b], [h], etc.
            for (const auto& t : entry.tags) {
                ImVec4 c = launcher_theme::kTagDefault;
                if (t == "!")                                 c = launcher_theme::kTagVerified;
                else if (t == "b" || t == "b1" || t == "b2") c = launcher_theme::kTagBadDump;
                else if (t == "h" || t.substr(0, 1) == "h")  c = launcher_theme::kTagHack;
                else if (t == "o" || t == "o1")              c = launcher_theme::kTagOverdump;
                else if (t == "a" || t == "a1" || t == "a2") c = launcher_theme::kTagAlternate;
                else if (t == "p" || t == "p1")              c = launcher_theme::kTagPirate;
                std::string label = "[" + t + "]";
                draw_badge(label.c_str(), c);
            }

            // Parenthesized flags: (Unl), (Proto), (Beta), etc.
            for (const auto& f : entry.flags) {
                ImVec4 c = launcher_theme::kTagDefault;
                if (f == "Unl" || f == "Unlicensed")         c = launcher_theme::kTagUnlicensed;
                else if (f == "Proto" || f == "Prototype")   c = launcher_theme::kTagProto;
                else if (f == "Beta")                        c = launcher_theme::kTagProto;
                else if (f == "cr" || f == "Crack")          c = launcher_theme::kTagPirate;
                draw_badge(f.c_str(), c);
            }
        }

        // Region column
        if (!entry.region.empty()) {
            auto sr = rom_filename::short_region(entry.region);
            dl->AddText(ImVec2(row_min.x + col_region, row_min.y + 2),
                         ImGui::GetColorU32(launcher_theme::kTextMuted),
                         sr.data(), sr.data() + sr.size());
        }

        // Year column
        if (!entry.year.empty()) {
            dl->AddText(ImVec2(row_min.x + col_year, row_min.y + 2),
                         ImGui::GetColorU32(launcher_theme::kTextMuted),
                         entry.year.c_str());
        }

        // Size column
        if (!entry.is_directory) {
            std::string size_str = format_size(entry.size);
            dl->AddText(ImVec2(row_min.x + col_size, row_min.y + 2),
                         ImGui::GetColorU32(launcher_theme::kTextMuted),
                         size_str.c_str());
        }

        // Type column
        std::string type_str = get_type_label(entry);
        dl->AddText(ImVec2(row_min.x + col_type, row_min.y + 2),
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

    // Execute deferred navigation (must happen outside the entry loop
    // because navigate_to() invalidates filtered_entries_).
    if (!pending_navigate_.empty()) {
        std::string target = std::move(pending_navigate_);
        pending_navigate_.clear();
        navigate_to(target);
    }
}

#endif // CERMU_HAS_GUI
