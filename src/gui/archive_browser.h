#pragma once

/**
 * ArchiveBrowser — ImGui popup for browsing contents of archives and containers
 *
 * Allows the user to navigate into .zip, .7z, .d64, .t64 and other
 * multi-file containers, view their directory listings, and select a
 * specific file to load.
 *
 * Two content sources:
 *   - Standard archives (.zip, .7z, .rar, …) — listed via VFS
 *   - Emulator containers (.d64, .t64) — listed via format-specific readers
 */

#ifdef CERMU_HAS_GUI

#include <string>
#include <vector>
#include <cstdint>

// ============================================================================
// Directory Entry (unified across source types)
// ============================================================================

struct ArchiveBrowserEntry {
    std::string display_name;   /**< UTF-8 name shown to user */
    std::string vfs_path;       /**< Full VFS path for loading (archive.zip!/file.prg) */
    bool        is_directory;   /**< Can be entered for deeper navigation */
    size_t      size;           /**< Uncompressed size in bytes (0 for dirs) */
    std::string type_label;     /**< Short type string: "PRG", "SEQ", "DIR", etc. */
    uint16_t    blocks;         /**< D64 block count (0 if N/A) */
    int         source_index;   /**< Index into source container (D64 entry idx, etc.) */
};

// ============================================================================
// ArchiveBrowser
// ============================================================================

class ArchiveBrowser {
public:
    ArchiveBrowser() = default;

    /**
     * Open the browser for a given archive/container path.
     * Scans the contents and prepares the popup.
     *
     * @param path          Real filesystem path to the archive or container
     * @param dialog_title  Title shown in the popup header (e.g. path basename)
     */
    void open(const std::string& path, const std::string& dialog_title = "");

    /** Close the browser and discard state. */
    void close();

    /** Is the browser popup currently open? */
    bool is_open() const { return open_; }

    /**
     * Render the browser popup.  Call every frame from update_frame().
     *
     * @return true when the user made a final selection (call get_selected_path())
     *         or canceled (check was_canceled()).
     */
    bool render();

    /** The full VFS path the user selected (valid after render() returns true and !was_canceled()). */
    const std::string& get_selected_path() const { return selected_path_; }

    /** The complete entry the user selected (valid after render() returns true and !was_canceled()). */
    const ArchiveBrowserEntry& get_selected_entry() const { return selected_entry_; }

    /** True if user closed via Cancel / X button rather than selecting a file. */
    bool was_canceled() const { return canceled_; }

    /** True if the selected file came from a D64 or T64 container (not a VFS archive). */
    bool is_container_source() const {
        return source_type_ == SourceType::D64 || source_type_ == SourceType::T64;
    }

    /** Filesystem path to the outermost archive/container. */
    const std::string& get_archive_path() const { return archive_path_; }

private:
    // --- State ---
    bool        open_       = false;
    bool        canceled_   = false;
    std::string archive_path_;          /**< Outermost real filesystem path */
    std::string current_subpath_;       /**< Current path within the archive ("" = root) */
    std::string title_;                 /**< Popup window title */
    std::string selected_path_;         /**< Result path */
    ArchiveBrowserEntry selected_entry_; /**< Full selected entry (valid after confirm) */

    // --- Content ---
    std::vector<ArchiveBrowserEntry> entries_;
    int selected_index_ = -1;

    // Cached raw data for container formats (D64, T64)
    uint8_t* container_data_     = nullptr;
    size_t   container_data_size_ = 0;
    enum class SourceType { None, Vfs, D64, T64 };
    SourceType source_type_      = SourceType::None;

    // --- Internal ---
    void scan_current_path();
    void scan_vfs();
    void scan_d64();
    void scan_t64();
    void free_container_data();
    void navigate_into(const ArchiveBrowserEntry& entry);
    void navigate_up();

    /** Build human-readable title from the archive path. */
    static std::string make_title(const std::string& path);

    /** D64 file type byte → short label. */
    static const char* d64_file_type_label(uint8_t file_type);
};

#endif // CERMU_HAS_GUI
