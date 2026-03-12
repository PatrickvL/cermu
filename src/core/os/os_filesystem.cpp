/**
 * Core OS Abstraction Layer — Filesystem Implementation
 *
 * Contains all platform-specific file I/O, existence checks, directory
 * listing, and path utilities.  POSIX on Unix, Win32 on Windows.
 * Nothing outside this file should include <dirent.h>, <sys/stat.h>,
 * <windows.h>, or use fopen/fread/fclose directly.
 */

#include "core/os/os.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

// --- Platform headers ---
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#else
#include <windows.h>
#endif

// ============================================================================
// File I/O
// ============================================================================

uint8_t* os_read_file(const char* path, size_t* out_size) {
    if (!path || !out_size) return nullptr;
    *out_size = 0;

    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) { fclose(f); return nullptr; }

    auto* buf = static_cast<uint8_t*>(malloc(static_cast<size_t>(sz)));
    if (!buf) { fclose(f); return nullptr; }

    if (fread(buf, 1, static_cast<size_t>(sz), f) != static_cast<size_t>(sz)) {
        free(buf);
        fclose(f);
        return nullptr;
    }

    fclose(f);
    *out_size = static_cast<size_t>(sz);
    return buf;
}

bool os_write_file(const char* path, const uint8_t* data, size_t size) {
    if (!path || (!data && size > 0)) return false;

    FILE* f = fopen(path, "wb");
    if (!f) return false;

    if (size > 0) {
        if (fwrite(data, 1, size, f) != size) {
            fclose(f);
            return false;
        }
    }

    fclose(f);
    return true;
}

// ============================================================================
// Filesystem Queries
// ============================================================================

bool os_file_exists(const char* path) {
    if (!path) return false;
#ifndef _WIN32
    struct stat st;
    return stat(path, &st) == 0;
#else
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#endif
}

bool os_is_regular_file(const char* path) {
    if (!path) return false;
#ifndef _WIN32
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#else
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES &&
           !(attrs & FILE_ATTRIBUTE_DIRECTORY);
#endif
}

bool os_is_directory(const char* path) {
    if (!path) return false;
#ifndef _WIN32
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#else
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES &&
           (attrs & FILE_ATTRIBUTE_DIRECTORY);
#endif
}

// ============================================================================
// Directory Listing
// ============================================================================

std::vector<OsDirEntry> os_list_directory(const char* path) {
    std::vector<OsDirEntry> result;
    if (!path) return result;

#ifndef _WIN32
    DIR* dir = opendir(path);
    if (!dir) return result;

    std::string base = path;
    if (!base.empty() && base.back() != '/') base += '/';

    struct dirent* de;
    while ((de = readdir(dir)) != nullptr) {
        if (de->d_name[0] == '.' &&
            (de->d_name[1] == '\0' ||
             (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;

        OsDirEntry entry;
        entry.name = de->d_name;
        entry.is_dir = false;
        entry.size = 0;

        std::string full = base + de->d_name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0) {
            entry.is_dir = S_ISDIR(st.st_mode);
            entry.size = entry.is_dir ? 0 : static_cast<size_t>(st.st_size);
        }

        result.push_back(std::move(entry));
    }
    closedir(dir);

#else
    std::string pattern = std::string(path) + "\\*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return result;

    do {
        if (fd.cFileName[0] == '.' &&
            (fd.cFileName[1] == '\0' ||
             (fd.cFileName[1] == '.' && fd.cFileName[2] == '\0')))
            continue;

        OsDirEntry entry;
        entry.name = fd.cFileName;
        entry.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry.size = entry.is_dir ? 0 : static_cast<size_t>(
            (static_cast<uint64_t>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow);

        result.push_back(std::move(entry));
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#endif

    return result;
}

// ============================================================================
// Path Utilities
// ============================================================================

const char* os_find_extension(const char* path) {
    if (!path) return nullptr;

    const char* dot = nullptr;
    const char* sep = path;

    // Walk to the last path component, then find the last dot
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') sep = p + 1;
    }
    for (const char* p = sep; *p; ++p) {
        if (*p == '.') dot = p;
    }
    return dot;
}

bool os_extension_match(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower(static_cast<unsigned char>(*a)) !=
            tolower(static_cast<unsigned char>(*b)))
            return false;
        a++;
        b++;
    }
    return *a == *b;
}

void os_normalize_path(std::string& path) {
#ifdef _WIN32
    for (auto& c : path) { if (c == '/') c = '\\'; }
#else
    for (auto& c : path) { if (c == '\\') c = '/'; }
#endif
}

int os_mkdir_p(const std::string& dir) {
#ifdef _WIN32
    return system(("if not exist \"" + dir + "\" mkdir \"" + dir + "\"").c_str());
#else
    return system(("mkdir -p " + dir).c_str());
#endif
}
