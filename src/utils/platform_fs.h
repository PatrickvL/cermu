#pragma once

/*
 * platform_fs.h - Cross-platform filesystem utilities (C++)
 *
 * Wraps directory iteration, mkdir, path normalization, and other
 * OS-specific filesystem operations behind a single portable API.
 * All platform conditionals live here (via cermu.h); callers just
 * include this header and use the cermu_* / CERMU_* symbols.
 */

#include "../core/cermu.h"

#include <string>
#include <sys/stat.h>

/* ========================================================================== */
/* DIRECTORY ITERATION INCLUDES */
/* ========================================================================== */

#ifdef CERMU_USE_STD_FILESYSTEM
    #include <filesystem>
    namespace cermu_fs = std::filesystem;
#else
    #include <dirent.h>
#endif

/* ========================================================================== */
/* SEH INCLUDES */
/* ========================================================================== */

#ifdef CERMU_HAS_SEH
    #include <excpt.h>
#endif

/* ========================================================================== */
/* PLATFORM-SPECIFIC INCLUDES (path discovery, mkdir, etc.) */
/* ========================================================================== */

#ifdef CERMU_PLATFORM_WINDOWS
    #include <direct.h>
    #ifndef cermu_getcwd
        #define cermu_getcwd _getcwd
    #endif
#else
    #include <unistd.h>
    #ifndef cermu_getcwd
        #define cermu_getcwd getcwd
    #endif
#endif

/* ========================================================================== */
/* PATH UTILITIES */
/* ========================================================================== */

/* Normalise all path separators to the platform's native separator in-place. */
static inline void cermu_normalize_path(std::string& path) {
#ifdef CERMU_PLATFORM_WINDOWS
    for (auto& c : path) { if (c == '/') c = '\\'; }
#else
    for (auto& c : path) { if (c == '\\') c = '/'; }
#endif
}

/* Create a directory (and parents) in a cross-platform way.
   Returns 0 on success or if the directory already exists. */
static inline int cermu_mkdir_p(const std::string& dir) {
#ifdef CERMU_PLATFORM_WINDOWS
    return system(("if not exist \"" + dir + "\" mkdir \"" + dir + "\"").c_str());
#else
    return system(("mkdir -p " + dir).c_str());
#endif
}

/* Check whether a path is an existing directory. */
static inline bool cermu_dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
