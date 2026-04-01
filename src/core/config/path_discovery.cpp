#include "core/config/path_discovery.hpp"
#include "core/cermu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>

CERMU_MSVC_WARNING_PUSH
CERMU_MSVC_WARNING_DISABLE(4996)  /* Disable deprecation warnings for string functions */

#ifdef CERMU_PLATFORM_WINDOWS
#include <windows.h>
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#ifdef CERMU_PLATFORM_MACOS
#include <mach-o/dyld.h>  /* _NSGetExecutablePath */
#endif
#endif

#define PATH_SEPARATOR CERMU_PATH_SEPARATOR

// ---------------------------------------------------------------------------
// get_executable_dir — resolve the directory containing the running executable
//
// Priority: executable path (reliable across CWD changes) → CWD fallback.
// On Linux we read /proc/self/exe; on macOS _NSGetExecutablePath(); on Windows
// GetModuleFileNameA().  CWD is only used when the platform call fails.
// ---------------------------------------------------------------------------
static bool get_executable_dir(char* out, size_t out_size) {
    if (!out || out_size < 2) return false;

#ifdef CERMU_PLATFORM_WINDOWS
    DWORD n = GetModuleFileNameA(NULL, out, (DWORD)out_size);
    if (n == 0 || n >= out_size) return false;
#elif defined(CERMU_PLATFORM_MACOS)
    uint32_t bufsize = (uint32_t)out_size;
    if (_NSGetExecutablePath(out, &bufsize) != 0) {
        // Buffer too small or call failed — fall back to CWD
        if (!getcwd(out, out_size)) return false;
        return true;  // CWD has no filename to strip
    }
    // Resolve symlinks so we get the real directory
    char resolved[1024];
    if (realpath(out, resolved)) {
        strncpy(out, resolved, out_size - 1);
        out[out_size - 1] = '\0';
    }
#else  // Linux / other POSIX
    ssize_t len = readlink("/proc/self/exe", out, out_size - 1);
    if (len > 0) {
        out[len] = '\0';
    } else {
        // /proc not available — fall back to CWD
        if (!getcwd(out, out_size)) return false;
        return true;  // CWD has no filename to strip
    }
#endif

    // Strip the executable filename, keep just the directory
    char* last_sep = strrchr(out, PATH_SEPARATOR);
    if (last_sep && last_sep != out) {
        *last_sep = '\0';
    }
    return true;
}

// ---------------------------------------------------------------------------
// dir_exists — check if a path is an existing directory
// ---------------------------------------------------------------------------
static bool dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// ---------------------------------------------------------------------------
// search_upward_for_data — walk parent dirs looking for data/<system_name>
// ---------------------------------------------------------------------------
static bool search_upward_for_data(const char* start_dir, const char* system_name,
                                   char* out_path, size_t path_size) {
    char search_path[1024];
    char test_path[1024];
    strncpy(search_path, start_dir, sizeof(search_path) - 1);
    search_path[sizeof(search_path) - 1] = '\0';

    for (int depth = 0; depth < 10; depth++) {
        int len = snprintf(test_path, sizeof(test_path), "%s%cdata%c%s",
                           search_path, PATH_SEPARATOR, PATH_SEPARATOR, system_name);
        if (len < 0 || (size_t)len >= sizeof(test_path)) { /* truncated */ }
        else if (dir_exists(test_path)) {
            strncpy(out_path, test_path, path_size - 1);
            out_path[path_size - 1] = '\0';
            log_info("Data root discovered: %s\n", out_path);
            return true;
        }

        char* last_sep = strrchr(search_path, PATH_SEPARATOR);
        if (!last_sep || last_sep == search_path) break;
        *last_sep = '\0';
    }
    return false;
}

// ---------------------------------------------------------------------------
// search_upward_for_roms — walk parent dirs looking for data/<system>/roms
// ---------------------------------------------------------------------------
static bool search_upward_for_roms(const char* start_dir, const char* system_name,
                                   char* out_path, size_t path_size) {
    char search_path[1024];
    char test_path[1024];
    strncpy(search_path, start_dir, sizeof(search_path) - 1);
    search_path[sizeof(search_path) - 1] = '\0';

    for (int depth = 0; depth < 10; depth++) {
        int len = snprintf(test_path, sizeof(test_path), "%s%cdata%c%s%croms",
                           search_path, PATH_SEPARATOR, PATH_SEPARATOR,
                           system_name, PATH_SEPARATOR);
        if (len < 0 || (size_t)len >= sizeof(test_path)) { /* truncated */ }
        else if (dir_exists(test_path)) {
            strncpy(out_path, test_path, path_size - 1);
            out_path[path_size - 1] = '\0';
            log_info("ROM root discovered: %s\n", out_path);
            return true;
        }

        char* last_sep = strrchr(search_path, PATH_SEPARATOR);
        if (!last_sep || last_sep == search_path) break;
        *last_sep = '\0';
    }
    return false;
}

bool system_config_discover_data_root(const char* system_name, char* out_path, size_t path_size) {
    if (!system_name || !out_path || path_size < 256) {
        return false;
    }

    // 1) Search upward from executable directory (primary)
    char exe_dir[1024];
    if (get_executable_dir(exe_dir, sizeof(exe_dir))) {
        if (search_upward_for_data(exe_dir, system_name, out_path, path_size))
            return true;
    }

    // 2) Fallback: search upward from CWD (covers in-tree dev builds)
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) && strcmp(cwd, exe_dir) != 0) {
        if (search_upward_for_data(cwd, system_name, out_path, path_size))
            return true;
    }

    log_info("Warning: Could not find data root folder for system '%s'\n", system_name);
    return false;
}

bool system_config_discover_data_root(const char* const* names, char* out_path, size_t path_size) {
    if (!names || !out_path || path_size < 256) return false;
    for (const char* const* p = names; *p; ++p) {
        if (system_config_discover_data_root(*p, out_path, path_size))
            return true;
    }
    return false;
}

bool system_config_discover_rom_root(const char* system_name, char* out_path, size_t path_size) {
    if (!system_name || !out_path || path_size < 256) {
        return false;
    }

    // 1) Search upward from executable directory (primary)
    char exe_dir[1024];
    if (get_executable_dir(exe_dir, sizeof(exe_dir))) {
        if (search_upward_for_roms(exe_dir, system_name, out_path, path_size))
            return true;
    }

    // 2) Fallback: search upward from CWD (covers in-tree dev builds)
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) && strcmp(cwd, exe_dir) != 0) {
        if (search_upward_for_roms(cwd, system_name, out_path, path_size))
            return true;
    }

    log_info("Warning: Could not find ROM root folder for system '%s'\n", system_name);
    return false;
}

bool system_config_discover_rom_root(const char* const* names, char* out_path, size_t path_size) {
    if (!names || !out_path || path_size < 256) return false;
    for (const char* const* p = names; *p; ++p) {
        if (system_config_discover_rom_root(*p, out_path, path_size))
            return true;
    }
    return false;
}

CERMU_MSVC_WARNING_POP
