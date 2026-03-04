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

    /// Enable/disable treating emulator containers (.d64, .t64) as navigable
    /// folders.  Standard archives (.zip, .7z, …) are always virtual folders.
    /// Set this before opening a dialog so subsequent scans apply the mode.
    static void set_browse_containers(bool enabled) { s_browse_containers_ = enabled; }
    static bool browse_containers() { return s_browse_containers_; }

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

    /// True if an extension identifies a Commodore container (.d64, .t64).
    static bool has_container_extension(const std::string& path_or_name);

    /// True if the path/name should be browsed as a virtual folder (taking
    /// into account the current browse_containers flag).
    static bool is_browsable(const std::string& path_or_name);

private:
    static bool s_browse_containers_;

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

    /// Lowercase file extension (with leading dot) from a path or name.
    static std::string get_extension(const std::string& path);

    /// Scan a real filesystem directory (archives/containers appear as dirs).
    std::vector<IGFD::FileInfos> scan_real_directory(const std::string& path);

    /// Scan entries inside a VFS archive.
    std::vector<IGFD::FileInfos> scan_vfs_entries(const std::string& vfs_path,
                                                   const std::string& dialog_dir);

    /// Scan entries inside a D64/T64 container loaded into memory.
    std::vector<IGFD::FileInfos> scan_container_entries(const uint8_t* data, size_t size,
                                                         const std::string& ext,
                                                         const std::string& dialog_dir);
};

#define FILE_SYSTEM_OVERRIDE VfsFileSystem
