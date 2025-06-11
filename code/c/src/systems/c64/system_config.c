#include "system_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4996)  // Disable deprecation warnings for string functions
#include <windows.h>
#include <direct.h>
#define PATH_SEPARATOR '\\'
#define getcwd _getcwd
#else
#include <unistd.h>
#include <sys/stat.h>
#define PATH_SEPARATOR '/'
#endif

const rom_config_t* system_config_get_default_roms(void) {
    static const rom_config_t default_roms = {
        .basic_rom_filenames = {
            "basic.901226-01.bin",
            "basic.rom", 
            "901226-01.bin",
            NULL,
            NULL
        },
        .kernal_rom_filenames = {
            "kernal.901227-03.bin",
            "kernal.rom",
            "901227-03.bin", 
            NULL,
            NULL
        },
        .chargen_rom_filenames = {
            "characters.901225-01.bin",
            "char.rom",
            "chargen.rom",
            "901225-01.bin",
            NULL,
            NULL
        }
    };
    return &default_roms;
}

bool system_config_discover_rom_root(const char* system_name, char* out_path, size_t path_size) {
    if (!system_name || !out_path || path_size < 256) {
        return false;
    }
    
    char current_path[1024];
    char test_path[1024];
    
    // Get executable directory or current working directory
#ifdef _WIN32
    DWORD result = GetModuleFileNameA(NULL, current_path, sizeof(current_path));
    if (result == 0) {
        return false;
    }
    // Remove executable filename, keep directory
    char* last_slash = strrchr(current_path, PATH_SEPARATOR);
    if (last_slash) {
        *last_slash = '\0';
    }
#else
    if (!getcwd(current_path, sizeof(current_path))) {
        return false;
    }
#endif
      // Search upwards for data folder
    char search_path[1024];
    strncpy(search_path, current_path, sizeof(search_path) - 1);
    search_path[sizeof(search_path) - 1] = '\0';
    
    for (int depth = 0; depth < 10; depth++) {  // Limit search depth
        // Construct test path: search_path/data/system_name/roms
        snprintf(test_path, sizeof(test_path), "%s%cdata%c%s%croms", 
                 search_path, PATH_SEPARATOR, PATH_SEPARATOR, system_name, PATH_SEPARATOR);
        
        // Check if directory exists
#ifdef _WIN32
        DWORD attrs = GetFileAttributesA(test_path);
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
#else
        struct stat st;
        if (stat(test_path, &st) == 0 && S_ISDIR(st.st_mode)) {
#endif            // Found the ROM directory
            strncpy(out_path, test_path, path_size - 1);
            out_path[path_size - 1] = '\0';
            printf("ROM root discovered: %s\n", out_path);
            return true;
        }
        
        // Move up one directory level
        char* last_separator = strrchr(search_path, PATH_SEPARATOR);
        if (!last_separator || last_separator == search_path) {
            break;  // Reached root
        }
        *last_separator = '\0';
    }
    
    printf("Warning: Could not find ROM root folder for system '%s'\n", system_name);
    return false;
}

#ifdef _WIN32
#pragma warning(pop)
#endif
