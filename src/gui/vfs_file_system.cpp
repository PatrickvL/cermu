/**
 * VfsFileSystem — Implementation
 *
 * Custom IFileSystem for ImGuiFileDialog that routes directory scans
 * through cermu's VFS layer when the path contains an archive or
 * emulator container.  See vfs_file_system.h for the full design.
 */

#include "gui/vfs_file_system.hpp"
#include "core/vfs/vfs.hpp"
#include "core/formats/format_handler.hpp"
#include "core/formats/format_registry.hpp"
#include "core/cermu.hpp"

#ifdef CERMU_USE_STD_FILESYSTEM
    #include <filesystem>
    namespace cermu_fs = std::filesystem;
#else
    #include <dirent.h>
#endif
#include <sys/stat.h>
#ifdef _WIN32
    #include <direct.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <fstream>

// ============================================================================
// Static state
// ============================================================================

bool VfsFileSystem::s_browse_containers_ = true;
const format_descriptor_t* const* VfsFileSystem::s_active_formats_ = nullptr;

// ============================================================================
// Lifecycle
// ============================================================================

VfsFileSystem::VfsFileSystem()  = default;
VfsFileSystem::~VfsFileSystem() = default;

// ============================================================================
// Path utilities
// ============================================================================

std::string VfsFileSystem::get_extension(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos || dot == 0) return {};
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && dot < slash) return {};
    std::string ext = path.substr(dot);
    for (auto& c : ext)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return ext;
}

bool VfsFileSystem::has_archive_extension(const std::string& path_or_name) {
    std::string ext = get_extension(path_or_name);
    if (ext.empty()) return false;
    return vfs_is_archive_extension(ext.c_str());
}

bool VfsFileSystem::has_container_extension(const std::string& path_or_name) {
    std::string ext = get_extension(path_or_name);
    if (ext.empty()) return false;
    const auto* fmt = FormatRegistry::instance().find_by_extension(ext.c_str());
    return fmt && (fmt->capabilities & FORMAT_CAP_CONTAINER) && fmt->list_entries;
}

bool VfsFileSystem::is_browsable(const std::string& path_or_name) {
    if (has_archive_extension(path_or_name)) return true;
    if (s_browse_containers_ && has_container_extension(path_or_name)) return true;
    return false;
}

// ============================================================================
// split_at_archive — find the first real-file archive boundary
// ============================================================================

VfsFileSystem::PathSplit VfsFileSystem::split_at_archive(const std::string& path) {
    PathSplit result;

    std::string clean = path;
    while (clean.size() > 1 && clean.back() == '/')
        clean.pop_back();

    // Walk from left to right; at each '/' check whether the accumulated
    // prefix is a real regular file with a browsable extension.
    size_t pos = 0;
#ifdef _WIN32
    if (clean.size() >= 2 && clean[1] == ':') pos = 2;
#endif

    while (pos < clean.size()) {
        size_t next = clean.find('/', pos + 1);
        if (next == std::string::npos) next = clean.size();

        std::string prefix = clean.substr(0, next);

        struct stat sb;
        if (stat(prefix.c_str(), &sb) == 0 && S_ISREG(sb.st_mode)) {
            if (is_browsable(prefix)) {
                result.real_part    = prefix;
                result.has_boundary = true;
                if (next < clean.size())
                    result.virtual_part = clean.substr(next + 1);
                return result;
            }
        }

        pos = next;
    }

    // No boundary found — whole path is real.
    result.real_part = clean;
    return result;
}

// ============================================================================
// is_virtual_path / to_vfs_path
// ============================================================================

bool VfsFileSystem::is_virtual_path(const std::string& dialog_path) {
    auto split = split_at_archive(dialog_path);
    if (split.has_boundary) return true;
    // Also virtual if the path IS a browsable file (root of archive).
    struct stat sb;
    if (stat(dialog_path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode))
        if (is_browsable(dialog_path)) return true;
    return false;
}

std::string VfsFileSystem::to_vfs_path(const std::string& dialog_path) {
    auto split = split_at_archive(dialog_path);
    if (!split.has_boundary) return dialog_path;
    if (split.virtual_part.empty()) return split.real_part;

    std::string result = split.real_part;

    // Break virtual_part into segments.
    std::vector<std::string> segments;
    {
        size_t pos = 0;
        while (pos < split.virtual_part.size()) {
            size_t next = split.virtual_part.find('/', pos);
            if (next == std::string::npos) next = split.virtual_part.size();
            if (next > pos)
                segments.push_back(split.virtual_part.substr(pos, next - pos));
            pos = next + 1;
        }
    }

    // Accumulate segments in groups.  At each browsable file boundary emit
    // the group with "!/" separator, then start a new group.
    std::vector<std::string> group;
    for (const auto& seg : segments) {
        group.push_back(seg);
        if (is_browsable(seg)) {
            result += "!/";
            for (size_t i = 0; i < group.size(); ++i) {
                if (i > 0) result += "/";
                result += group[i];
            }
            group.clear();
        }
    }

    // Flush remaining (non-archive) tail.
    if (!group.empty()) {
        result += "!/";
        for (size_t i = 0; i < group.size(); ++i) {
            if (i > 0) result += "/";
            result += group[i];
        }
    }

    return result;
}

// ============================================================================
// IFileSystem — directory / file queries
// ============================================================================

bool VfsFileSystem::IsDirectoryCanBeOpened(const std::string& vName) {
    if (vName.empty()) return false;

    // Real directory?
#ifdef CERMU_USE_STD_FILESYSTEM
    { std::error_code ec; if (cermu_fs::is_directory(vName, ec)) return true; }
#else
    DIR* pDir = opendir(vName.c_str());
    if (pDir) { closedir(pDir); return true; }
#endif

    // Real file that we browse as a virtual folder?
    struct stat sb;
    if (stat(vName.c_str(), &sb) == 0 && S_ISREG(sb.st_mode))
        if (is_browsable(vName)) return true;

    // Inside a virtual folder?
    if (split_at_archive(vName).has_boundary) return true;

    return false;
}

bool VfsFileSystem::IsDirectoryExist(const std::string& vName) {
    if (vName.empty()) return false;

#ifdef CERMU_USE_STD_FILESYSTEM
    { std::error_code ec; if (cermu_fs::is_directory(vName, ec)) return true; }
#else
    DIR* pDir = opendir(vName.c_str());
    if (pDir) { closedir(pDir); return true; }
#endif

    struct stat sb;
    if (stat(vName.c_str(), &sb) == 0 && S_ISREG(sb.st_mode))
        if (is_browsable(vName)) return true;

    if (split_at_archive(vName).has_boundary) return true;

    return false;
}

bool VfsFileSystem::IsFileExist(const std::string& vName) {
    if (vName.empty()) return false;
    std::ifstream f(vName, std::ios::in);
    if (f.is_open()) { f.close(); return true; }
    // Virtual entries: return false so the dialog accepts the selection
    // without an overwrite prompt.
    return false;
}

bool VfsFileSystem::CreateDirectoryIfNotExist(const std::string& vName) {
    if (vName.empty()) return false;
    if (IsDirectoryExist(vName)) return true;
    if (split_at_archive(vName).has_boundary) return false; // can't create inside archive
#ifdef _WIN32
    return _mkdir(vName.c_str()) == 0;
#else
    return mkdir(vName.c_str(), 0755) == 0;
#endif
}

bool VfsFileSystem::IsDirectory(const std::string& vFilePathName) {
#ifdef CERMU_USE_STD_FILESYSTEM
    { std::error_code ec; if (cermu_fs::is_directory(vFilePathName, ec)) return true; }
#else
    DIR* pDir = opendir(vFilePathName.c_str());
    if (pDir) { closedir(pDir); return true; }
#endif

    struct stat sb;
    if (stat(vFilePathName.c_str(), &sb) == 0 && S_ISREG(sb.st_mode))
        if (is_browsable(vFilePathName)) return true;

    // Virtual path through an archive — any path that passes through an
    // archive boundary is treated as a directory.  This covers both plain
    // directories within the archive (e.g. ".../roms.7z/G") and browsable
    // containers nested inside (e.g. ".../archive.zip/disk.d64").
    // Consistent with IsDirectoryCanBeOpened / IsDirectoryExist.
    if (split_at_archive(vFilePathName).has_boundary)
        return true;

    return false;
}

// ============================================================================
// IFileSystem — path parsing
// ============================================================================

IGFD::Utils::PathStruct VfsFileSystem::ParsePathFileName(const std::string& vPathFileName) {
    // Pure string-based parsing — works for both real and virtual paths.
    IGFD::Utils::PathStruct res;
    if (vPathFileName.empty()) return res;

    std::string pfn = vPathFileName;
    IGFD::Utils::ReplaceString(pfn, "\\", "/");

    size_t lastSlash = pfn.find_last_of('/');
    if (lastSlash != std::string::npos) {
        res.name = pfn.substr(lastSlash + 1);
        res.path = pfn.substr(0, lastSlash);
        res.isOk = true;
    }

    size_t lastPoint = pfn.find_last_of('.');
    if (lastPoint != std::string::npos) {
        if (!res.isOk) {
            res.name = pfn;
            res.isOk = true;
        }
        res.ext = pfn.substr(lastPoint + 1);
        IGFD::Utils::ReplaceString(res.name, "." + res.ext, "");
    }

    if (!res.isOk) {
        res.name = std::move(pfn);
        res.isOk = true;
    }

    return res;
}

// ============================================================================
// IFileSystem — devices
// ============================================================================

std::vector<IGFD::PathDisplayedName> VfsFileSystem::GetDevicesList() {
    return {};  // no device list on Linux
}

// ============================================================================
// IFileSystem — date & size
// ============================================================================

void VfsFileSystem::GetFileDateAndSize(const std::string& vFilePathName,
                                        const IGFD::FileType& vFileType,
                                        std::string& voDate, size_t& voSize) {
    // Cached value from the last ScanDirectory (virtual entries).
    auto it = cached_sizes_.find(vFilePathName);
    if (it != cached_sizes_.end()) {
        voSize = it->second;
        voDate.clear();
        return;
    }

    // Real filesystem stat.
    struct stat si{};
    if (stat(vFilePathName.c_str(), &si) == 0) {
        struct tm* _tm = localtime(&si.st_mtime);
        if (_tm) {
            char buf[64];
            size_t len = strftime(buf, sizeof(buf), "%Y/%m/%d %H:%M", _tm);
            if (len) voDate.assign(buf, len);
        }
        if (!vFileType.isDir())
            voSize = static_cast<size_t>(si.st_size);
    }
}

// ============================================================================
// IFileSystem — ScanDirectory  (the main entry point)
// ============================================================================

/// Find the container format descriptor for a given path/name, or nullptr.
static const format_descriptor_t* find_container_format(const std::string& path_or_name) {
    std::string ext = VfsFileSystem::get_extension(path_or_name);
    if (ext.empty()) return nullptr;
    const auto* fmt = FormatRegistry::instance().find_by_extension(ext.c_str());
    if (fmt && (fmt->capabilities & FORMAT_CAP_CONTAINER) && fmt->list_entries)
        return fmt;
    return nullptr;
}

std::vector<IGFD::FileInfos> VfsFileSystem::ScanDirectory(const std::string& vPath) {
    cached_sizes_.clear();

    std::string path = vPath;
    while (path.size() > 1 && path.back() == '/')
        path.pop_back();

    // --- Case 1: path IS a real browsable file (archive/container at leaf) ---
    struct stat sb;
    if (stat(path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode) && is_browsable(path)) {
        const format_descriptor_t* fmt = find_container_format(path);
        if (fmt) {
            // Container format — read and parse its directory.
            size_t data_size = 0;
            uint8_t* data = vfs_read_file(path.c_str(), &data_size);
            if (data) {
                auto res = scan_container_entries(fmt, data, data_size, path);
                free(data);
                return res;
            }
            return {};
        }
        // VFS archive — list root via VFS.
        return scan_vfs_entries(path, path);
    }

    // --- Case 2: path passes through a real archive --------------
    auto split = split_at_archive(path);
    if (split.has_boundary) {
        std::string vfs_path = to_vfs_path(path);

        // Check whether the leaf of the VFS path is a container that needs
        // format-specific scanning.
        size_t last_delim = vfs_path.rfind("!/");
        if (last_delim != std::string::npos) {
            std::string leaf = vfs_path.substr(last_delim + 2);
            const format_descriptor_t* fmt = find_container_format(leaf);
            if (fmt && s_browse_containers_) {
                size_t data_size = 0;
                uint8_t* data = vfs_read_file(vfs_path.c_str(), &data_size);
                if (data) {
                    auto res = scan_container_entries(fmt, data, data_size, path);
                    free(data);
                    return res;
                }
                return {};
            }
        }

        // Standard VFS listing.
        return scan_vfs_entries(vfs_path, path);
    }

    // --- Case 3: plain filesystem directory --------------------------
    return scan_real_directory(path);
}

// ============================================================================
// Scanning — real filesystem
// ============================================================================

std::vector<IGFD::FileInfos> VfsFileSystem::scan_real_directory(const std::string& path) {
    std::vector<IGFD::FileInfos> res;

#ifdef CERMU_USE_STD_FILESYSTEM
    std::error_code ec;
    for (const auto& entry : cermu_fs::directory_iterator(path, ec)) {
        IGFD::FileType ft;

        if (entry.is_symlink(ec)) {
            ft.SetSymLink(true);
            auto target = cermu_fs::status(entry.path(), ec);
            if (!ec) {
                if (cermu_fs::is_regular_file(target))
                    ft.SetContent(IGFD::FileType::ContentType::File);
                else if (cermu_fs::is_directory(target))
                    ft.SetContent(IGFD::FileType::ContentType::Directory);
            }
        } else if (entry.is_directory(ec)) {
            ft.SetContent(IGFD::FileType::ContentType::Directory);
        } else if (entry.is_regular_file(ec)) {
            ft.SetContent(IGFD::FileType::ContentType::File);
        }

        if (ft.isValid()) {
            std::string name = entry.path().filename().string();
            // Present browsable archives/containers as directories.
            if (ft.isFile() && is_browsable(name))
                ft.SetContent(IGFD::FileType::ContentType::Directory);

            IGFD::FileInfos info;
            info.filePath    = path;
            info.fileNameExt = name;
            info.fileType    = ft;
            res.push_back(info);
        }
    }

    // Sort by filename to match scandir's alphasort behavior.
    std::sort(res.begin(), res.end(),
              [](const IGFD::FileInfos& a, const IGFD::FileInfos& b) {
                  return a.fileNameExt < b.fileNameExt;
              });
#else
    struct dirent** files = nullptr;
    int n = scandir(path.c_str(), &files, nullptr,
                    [](const struct dirent** a, const struct dirent** b) -> int {
                        return strcoll((*a)->d_name, (*b)->d_name);
                    });

    if (n > 0 && files) {
        for (int i = 0; i < n; ++i) {
            struct dirent* ent = files[i];
            IGFD::FileType ft;

            switch (ent->d_type) {
                case DT_DIR:
                    ft.SetContent(IGFD::FileType::ContentType::Directory);
                    break;
                case DT_REG:
                    ft.SetContent(IGFD::FileType::ContentType::File);
                    break;
#if defined(_IGFD_UNIX_) || (DT_LNK != DT_UNKNOWN)
                case DT_LNK:
#endif
                case DT_UNKNOWN: {
                    struct stat lsb{};
                    std::string fpn = path + "/" + ent->d_name;
                    if (!stat(fpn.c_str(), &lsb)) {
                        if (lsb.st_mode & S_IFLNK)
                            ft.SetSymLink(true);
                        if (S_ISREG(lsb.st_mode))
                            ft.SetContent(IGFD::FileType::ContentType::File);
                        else if (S_ISDIR(lsb.st_mode))
                            ft.SetContent(IGFD::FileType::ContentType::Directory);
                    }
                    break;
                }
                default: break;
            }

            if (ft.isValid()) {
                // Present browsable archives/containers as directories.
                if (ft.isFile() && is_browsable(ent->d_name))
                    ft.SetContent(IGFD::FileType::ContentType::Directory);

                IGFD::FileInfos info;
                info.filePath    = path;
                info.fileNameExt = ent->d_name;
                info.fileType    = ft;
                res.push_back(info);
            }
        }
        for (int i = 0; i < n; ++i) free(files[i]);
        free(files);
    }
#endif

    return res;
}

// ============================================================================
// Scanning — VFS (archives supported by libarchive)
// ============================================================================

std::vector<IGFD::FileInfos> VfsFileSystem::scan_vfs_entries(
    const std::string& vfs_path, const std::string& dialog_dir) {

    std::vector<IGFD::FileInfos> res;

    // ".." for navigating up / out of the archive.
    {
        IGFD::FileInfos dd;
        dd.filePath    = dialog_dir;
        dd.fileNameExt = "..";
        dd.fileType.SetContent(IGFD::FileType::ContentType::Directory);
        res.push_back(dd);
    }

    auto entries = vfs_list_entries(vfs_path.c_str());

    for (const auto& ve : entries) {
        if (ve.name == "." || ve.name == "..") continue;

        IGFD::FileInfos info;
        info.filePath    = dialog_dir;
        info.fileNameExt = ve.name;

        if (ve.type == VfsEntryType::Directory || ve.type == VfsEntryType::Archive) {
            info.fileType.SetContent(IGFD::FileType::ContentType::Directory);
        } else if (is_browsable(ve.name)) {
            // Containers inside an archive appear as folders.
            info.fileType.SetContent(IGFD::FileType::ContentType::Directory);
        } else {
            info.fileType.SetContent(IGFD::FileType::ContentType::File);
        }

        cached_sizes_[dialog_dir + "/" + ve.name] = ve.size;
        res.push_back(info);
    }

    return res;
}

// ============================================================================
// Scanning — container formats (D64, T64, LNX, …)
// ============================================================================

std::vector<IGFD::FileInfos> VfsFileSystem::scan_container_entries(
    const format_descriptor_t* fmt,
    const uint8_t* data, size_t size,
    const std::string& dialog_dir) {

    std::vector<IGFD::FileInfos> res;

    // ".." for navigating up / out of the container.
    {
        IGFD::FileInfos dd;
        dd.filePath    = dialog_dir;
        dd.fileNameExt = "..";
        dd.fileType.SetContent(IGFD::FileType::ContentType::Directory);
        res.push_back(dd);
    }

    if (!fmt || !fmt->list_entries) return res;

    // Ask the format to list its directory entries.
    static constexpr int MAX_ENTRIES = 512;
    format_container_entry_t entries[MAX_ENTRIES];
    int count = fmt->list_entries(data, size, entries, MAX_ENTRIES);
    if (count < 0) return res;

    for (int i = 0; i < count; ++i) {
        const auto& entry = entries[i];

        IGFD::FileInfos info;
        info.filePath    = dialog_dir;
        info.fileNameExt = entry.display_name;
        info.fileType.SetContent(IGFD::FileType::ContentType::File);

        cached_sizes_[dialog_dir + "/" + entry.display_name] = entry.size;
        res.push_back(info);
    }

    return res;
}

// ============================================================================
// Active format filtering
// ============================================================================

bool VfsFileSystem::is_active_format_ext(const std::string& ext) {
    if (!s_active_formats_ || !s_active_formats_[0]) return true;  // no filter
    if (ext.empty()) return false;

    for (const format_descriptor_t* const* p = s_active_formats_; *p; ++p) {
        const format_descriptor_t* fmt = *p;
        for (const char* const* e = fmt->extensions; e && *e; ++e) {
            if (cermu_strcasecmp(ext.c_str(), *e) == 0)
                return true;
        }
    }
    // Archives are always visible (they may contain supported files).
    if (vfs_is_archive_extension(ext.c_str()))
        return true;
    return false;
}
