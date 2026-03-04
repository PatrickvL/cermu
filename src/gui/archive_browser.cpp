#ifdef CERMU_HAS_GUI

#include "archive_browser.h"
#include "imgui.h"
#include "../core/vfs/vfs.h"
#include "../core/formats/d64_format.h"
#include "../core/formats/t64_format.h"
#include "../core/formats/format_handler.h"
#include "../core/encoding/petscii.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Helpers
// ============================================================================

std::string ArchiveBrowser::make_title(const std::string& path) {
    std::string name = vfs_filename(path.c_str());
    if (name.empty()) name = path;
    return name;
}

const char* ArchiveBrowser::d64_file_type_label(uint8_t file_type) {
    switch (file_type & D64_FTYPE_MASK) {
        case D64_FTYPE_DEL: return "DEL";
        case D64_FTYPE_SEQ: return "SEQ";
        case D64_FTYPE_PRG: return "PRG";
        case D64_FTYPE_USR: return "USR";
        case D64_FTYPE_REL: return "REL";
        default:            return "???";
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void ArchiveBrowser::open(const std::string& path, const std::string& dialog_title) {
    close();  // clean slate

    archive_path_     = path;
    current_subpath_.clear();
    open_             = true;
    canceled_         = false;
    selected_path_.clear();
    selected_index_   = -1;

    title_ = dialog_title.empty() ? make_title(path) : dialog_title;

    // Determine source type from extension
    std::string ext = vfs_extension(path.c_str());
    // Lowercase the extension for comparison
    for (auto& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    if (ext == ".d64") {
        source_type_ = SourceType::D64;
    } else if (ext == ".t64") {
        source_type_ = SourceType::T64;
    } else if (vfs_is_archive_extension(ext.c_str())) {
        source_type_ = SourceType::Vfs;
    } else {
        // Try VFS as fallback
        source_type_ = SourceType::Vfs;
    }

    // For D64/T64, pre-load the container data
    if (source_type_ == SourceType::D64 || source_type_ == SourceType::T64) {
        container_data_ = vfs_read_file(path.c_str(), &container_data_size_);
        if (!container_data_) {
            printf("ArchiveBrowser: failed to read container: %s\n", path.c_str());
            close();
            return;
        }
    }

    scan_current_path();

    if (entries_.empty()) {
        printf("ArchiveBrowser: no entries found in: %s\n", path.c_str());
    }
}

void ArchiveBrowser::close() {
    open_ = false;
    entries_.clear();
    selected_index_ = -1;
    selected_path_.clear();
    current_subpath_.clear();
    free_container_data();
    source_type_ = SourceType::None;
}

void ArchiveBrowser::free_container_data() {
    if (container_data_) {
        free(container_data_);
        container_data_ = nullptr;
        container_data_size_ = 0;
    }
}

// ============================================================================
// Scanning — populate entries_ from the current location
// ============================================================================

void ArchiveBrowser::scan_current_path() {
    entries_.clear();
    selected_index_ = -1;

    switch (source_type_) {
        case SourceType::Vfs: scan_vfs(); break;
        case SourceType::D64: scan_d64(); break;
        case SourceType::T64: scan_t64(); break;
        default: break;
    }
}

void ArchiveBrowser::scan_vfs() {
    // Build VFS path: archive_path + subpath
    std::string vfs_path = archive_path_;
    if (!current_subpath_.empty()) {
        vfs_path = vfs_join_path(vfs_path, current_subpath_);
    }

    auto vfs_entries = vfs_list_entries(vfs_path.c_str());

    for (const auto& ve : vfs_entries) {
        // Skip "." and ".."
        if (ve.name == "." || ve.name == "..") continue;

        ArchiveBrowserEntry entry;
        entry.display_name  = ve.name;
        entry.vfs_path      = ve.full_path;
        entry.is_directory  = (ve.type == VfsEntryType::Directory || ve.type == VfsEntryType::Archive);
        entry.size          = ve.size;
        entry.blocks        = 0;
        entry.source_index  = -1;

        // Derive type label
        if (entry.is_directory) {
            entry.type_label = ve.type == VfsEntryType::Archive ? "Archive" : "Dir";
        } else {
            std::string ext = vfs_extension(ve.name.c_str());
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
            // Uppercase the extension for display
            for (auto& c : ext) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            entry.type_label = ext;
        }

        entries_.push_back(std::move(entry));
    }

    // Sort: directories first, then alphabetically
    std::sort(entries_.begin(), entries_.end(), [](const ArchiveBrowserEntry& a, const ArchiveBrowserEntry& b) {
        if (a.is_directory != b.is_directory) return a.is_directory > b.is_directory;
        return a.display_name < b.display_name;
    });
}

void ArchiveBrowser::scan_d64() {
    if (!container_data_) return;

    commodore_d64_t d64{};
    if (!d64.open_mem(container_data_, container_data_size_)) {
        printf("ArchiveBrowser: D64 open_mem failed\n");
        return;
    }

    commodore_d64_directory_t dir{};
    if (!d64.read_directory(&dir)) {
        printf("ArchiveBrowser: D64 read_directory failed\n");
        d64.close();
        return;
    }

    for (int i = 0; i < dir.count; ++i) {
        const auto& de = dir.entries[i];

        // Skip deleted/invisible entries
        if ((de.file_type & D64_FTYPE_MASK) == D64_FTYPE_DEL) continue;

        ArchiveBrowserEntry entry;

        // Convert PETSCII filename to displayable ASCII
        char name_buf[17];
        memcpy(name_buf, de.filename, 17);
        // petscii_trim_padding already called by read_directory()
        // Convert remaining PETSCII to ASCII
        for (int j = 0; j < 16 && name_buf[j]; ++j) {
            name_buf[j] = petscii_to_ascii(static_cast<uint8_t>(name_buf[j]));
        }

        entry.display_name  = name_buf;
        entry.is_directory  = false;
        entry.size          = static_cast<size_t>(de.size_blocks) * 254;
        entry.type_label    = d64_file_type_label(de.file_type);
        entry.blocks        = de.size_blocks;
        entry.source_index  = i;

        // Build a VFS-style path for identification (used by load logic)
        // Format: "/path/to/disk.d64!/FILENAME.PRG"
        entry.vfs_path = archive_path_ + "!/" + entry.display_name;

        entries_.push_back(std::move(entry));
    }

    d64.close();
}

void ArchiveBrowser::scan_t64() {
    if (!container_data_) return;

    commodore_t64_t t64{};
    if (!t64.open_mem(container_data_, container_data_size_)) {
        printf("ArchiveBrowser: T64 open_mem failed\n");
        return;
    }

    commodore_t64_directory_t dir{};
    if (!t64.read_directory(&dir)) {
        printf("ArchiveBrowser: T64 read_directory failed\n");
        t64.close();
        return;
    }

    for (int i = 0; i < dir.count; ++i) {
        const auto& te = dir.entries[i];

        ArchiveBrowserEntry entry;

        char name_buf[17];
        memcpy(name_buf, te.filename, 17);
        for (int j = 0; j < 16 && name_buf[j]; ++j) {
            name_buf[j] = petscii_to_ascii(static_cast<uint8_t>(name_buf[j]));
        }

        entry.display_name  = name_buf;
        entry.is_directory  = false;
        entry.size          = te.data_size;
        entry.type_label    = "PRG";
        entry.blocks        = 0;
        entry.source_index  = i;
        entry.vfs_path      = archive_path_ + "!/" + entry.display_name;

        entries_.push_back(std::move(entry));
    }

    t64.close();
}

// ============================================================================
// Navigation
// ============================================================================

void ArchiveBrowser::navigate_into(const ArchiveBrowserEntry& entry) {
    if (!entry.is_directory) return;

    if (current_subpath_.empty()) {
        current_subpath_ = entry.display_name;
    } else {
        current_subpath_ += "/" + entry.display_name;
    }

    scan_current_path();
}

void ArchiveBrowser::navigate_up() {
    if (current_subpath_.empty()) return;

    auto last_sep = current_subpath_.rfind('/');
    if (last_sep == std::string::npos) {
        current_subpath_.clear();
    } else {
        current_subpath_ = current_subpath_.substr(0, last_sep);
    }

    scan_current_path();
}

// ============================================================================
// Render — ImGui popup
// ============================================================================

bool ArchiveBrowser::render() {
    if (!open_) return false;

    bool result = false;  // true when user made a final choice (OK or Cancel)

    // Build window title with the archive name
    std::string window_title = "Browse: " + title_ + "###ArchiveBrowser";

    bool window_open = true;
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 300), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::SetNextWindowSize(ImVec2(640, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(window_title.c_str(), &window_open,
                     ImGuiWindowFlags_NoCollapse)) {

        // Path breadcrumb
        {
            ImGui::TextDisabled("%s", archive_path_.c_str());
            if (!current_subpath_.empty()) {
                ImGui::SameLine();
                ImGui::Text("/ %s", current_subpath_.c_str());
            }
        }

        // ".." button when inside a subpath
        if (!current_subpath_.empty()) {
            if (ImGui::Selectable("..", false, ImGuiSelectableFlags_None)) {
                navigate_up();
            }
        }

        ImGui::Separator();

        // File table
        const float footer_height = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        if (ImGui::BeginChild("##FileList", ImVec2(0, -footer_height), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {

            ImGuiTableFlags table_flags = ImGuiTableFlags_RowBg
                                        | ImGuiTableFlags_BordersInnerV
                                        | ImGuiTableFlags_Resizable
                                        | ImGuiTableFlags_ScrollY
                                        | ImGuiTableFlags_SizingStretchProp;

            int num_columns = (source_type_ == SourceType::D64) ? 4 : 3;

            if (ImGui::BeginTable("##ArchiveEntries", num_columns, table_flags)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None, 4.0f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                if (source_type_ == SourceType::D64) {
                    ImGui::TableSetupColumn("Blocks", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                }
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
                    const auto& entry = entries_[i];
                    ImGui::TableNextRow();

                    // Name column
                    ImGui::TableNextColumn();
                    bool is_selected = (selected_index_ == i);

                    // Display name with directory indicator
                    std::string label = entry.is_directory
                        ? (std::string("[") + entry.display_name + "]")
                        : entry.display_name;

                    if (ImGui::Selectable(label.c_str(), is_selected,
                                          ImGuiSelectableFlags_SpanAllColumns |
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                        selected_index_ = i;

                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            if (entry.is_directory) {
                                navigate_into(entry);
                            } else {
                                // Double-click on file = select and confirm
                                selected_path_ = entry.vfs_path;
                                selected_entry_ = entry;
                                result = true;
                            }
                        }
                    }

                    // Type column
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.type_label.c_str());

                    // Blocks column (D64 only)
                    if (source_type_ == SourceType::D64) {
                        ImGui::TableNextColumn();
                        if (entry.blocks > 0) {
                            ImGui::Text("%u", entry.blocks);
                        }
                    }

                    // Size column
                    ImGui::TableNextColumn();
                    if (!entry.is_directory && entry.size > 0) {
                        if (entry.size >= 1024 * 1024) {
                            ImGui::Text("%.1f MB", entry.size / (1024.0 * 1024.0));
                        } else if (entry.size >= 1024) {
                            ImGui::Text("%.1f KB", entry.size / 1024.0);
                        } else {
                            ImGui::Text("%zu B", entry.size);
                        }
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        // Footer with OK / Cancel buttons
        ImGui::Separator();
        {
            bool has_selection = selected_index_ >= 0
                              && selected_index_ < static_cast<int>(entries_.size())
                              && !entries_[selected_index_].is_directory;

            if (!has_selection) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("OK", ImVec2(100, 0))) {
                if (has_selection) {
                    selected_path_ = entries_[selected_index_].vfs_path;
                    selected_entry_ = entries_[selected_index_];
                    result = true;
                }
            }
            if (!has_selection) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                canceled_ = true;
                result = true;
            }

            // Show entry count
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu entries)", entries_.size());
        }
    }
    ImGui::End();

    // Window close button (X)
    if (!window_open) {
        canceled_ = true;
        result = true;
    }

    if (result) {
        open_ = false;
        free_container_data();
    }

    return result;
}

#endif // CERMU_HAS_GUI
