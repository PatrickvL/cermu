#pragma once

/**
 * VfsFileSystem — Custom IFileSystem for ImGuiFileDialog
 *
 * Presents archives (.zip, .7z, .rar, …) and optionally emulator containers
 * (.d64, .t64) as navigable virtual folders inside the file dialog.
 * Delegates to the real filesystem for regular directories.
 *
 * When a dialog path passes through an archive/container boundary the class
 * translates it to a cermu VFS path (with "!/" delimiters) and uses the VFS
 * layer for listing and reading.  Nesting is fully supported: a D64 inside a
 * ZIP inside a 7z is all browsable.
 *
 * This header is included from two places:
 *   1. ImGuiFileDialog.cpp — via CUSTOM_FILESYSTEM_INCLUDE (IGFD types in scope)
 *   2. Project code (e.g. system_gui.cpp) — for the static utility methods
 * Including ImGuiFileDialog.h is safe in both cases (#pragma once).
 */

#include "ImGuiFileDialog.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class VfsFileSystem : public IGFD::IFileSystem {
public:
    VfsFileSystem();
    ~VfsFileSystem() override;

    // ---- IFileSystem interface -------------------------------------------
    bool IsDirectoryCanBeOpened(const std::string& vName) override;
    bool IsDirectoryExist(const std::string& vName) override;
    bool IsFileExist(const std::string& vName) override;
    bool CreateDirectoryIfNotExist(const std::string& vName) override;
    IGFD::Utils::PathStruct ParsePathFileName(const std::string& vPathFileName) override;
    std::vector<IGFD::FileInfos> ScanDirectory(const std::string& vPath) override;
    bool IsDirectory(const std::string& vFilePathName) override;
    std::vector<IGFD::PathDisplayedName> GetDevicesList() override;
    void GetFileDateAndSize(const std::string& vFilePathName,
                            const IGFD::FileType& vFileType,
                            std::string& voDate, size_t& voSize) override;

    // ---- Configuration ---------------------------------------------------

    /// Enable/disable treating container formats (D64, T64, LNX, …) as
    /// navigable folders.  Standard archives (.zip, .7z, …) are always
    /// virtual folders.  Set before opening a dialog.
    static void set_browse_containers(bool enabled) { s_browse_containers_ = enabled; }
    static bool browse_containers() { return s_browse_containers_; }

    /// Set the active system's supported formats for extension filtering.
    /// When set, scan results only include files matching these formats
    /// (plus archives, which are always visible).  Pass nullptr to disable
    /// filtering and show all files.
    static void set_active_formats(const struct format_descriptor_t* const* formats) {
        s_active_formats_ = formats;
    }

    // ---- Path translation utilities (public for use by loading code) -----

    /// Translate a file-dialog path to a VFS-loadable path.
    /// Every archive/container boundary in the path is converted to "!/":
    ///   /path/archive.zip/subdir/game.prg  →  /path/archive.zip!/subdir/game.prg
    ///   /path/disk.d64/GAME.prg            →  /path/disk.d64!/GAME.prg
    ///   /path/outer.zip/inner.7z/game.nes  →  /path/outer.zip!/inner.7z!/game.nes
    static std::string to_vfs_path(const std::string& dialog_path);

    /// True if the dialog path passes through at least one archive/container.
    static bool is_virtual_path(const std::string& dialog_path);

    /// True if an extension identifies a standard archive (.zip, .7z, …).
    static bool has_archive_extension(const std::string& path_or_name);

    /// True if an extension identifies a browsable container format
    /// (FORMAT_CAP_CONTAINER).  Queries the format registry — no hardcoded
    /// extension list.
    static bool has_container_extension(const std::string& path_or_name);

    /// True if the path/name should be browsed as a virtual folder (taking
    /// into account the current browse_containers flag).
    static bool is_browsable(const std::string& path_or_name);

private:
    static bool s_browse_containers_;
    static const struct format_descriptor_t* const* s_active_formats_;

    /// Cached sizes from the latest ScanDirectory call so that
    /// GetFileDateAndSize doesn't need to re-open the archive.
    mutable std::unordered_map<std::string, size_t> cached_sizes_;

    /// Result of split_at_archive().
    struct PathSplit {
        std::string real_part;      ///< Filesystem path up to and including the archive
        std::string virtual_part;   ///< Path inside the archive (empty = root)
        bool has_boundary = false;  ///< True if an archive boundary was found
    };

    /// Find the first real‐file archive/container boundary in a path.
    static PathSplit split_at_archive(const std::string& path);

public:
    /// Lowercase file extension (with leading dot) from a path or name.
    static std::string get_extension(const std::string& path);

private:
    /// Scan a real filesystem directory (archives/containers appear as dirs).
    std::vector<IGFD::FileInfos> scan_real_directory(const std::string& path);

    /// Scan entries inside a VFS archive.
    std::vector<IGFD::FileInfos> scan_vfs_entries(const std::string& vfs_path,
                                                   const std::string& dialog_dir);

    /// Scan entries inside a container format (D64, T64, LNX, …) loaded into
    /// memory.  Uses the format descriptor's list_entries() callback.
    std::vector<IGFD::FileInfos> scan_container_entries(
        const struct format_descriptor_t* fmt,
        const uint8_t* data, size_t size,
        const std::string& dialog_dir);

    /// True if the given extension (with dot) matches the active system's
    /// formats, or if no active formats are set (show everything).
    static bool is_active_format_ext(const std::string& ext);
};

#define FILE_SYSTEM_OVERRIDE VfsFileSystem
