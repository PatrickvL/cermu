#include "core/storage/rom_loader.hpp"
#include "core/cermu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <dirent.h>
#include <strings.h>   // strcasecmp
#endif

// All filename/path arguments use pipe-separated format:
// "name1|name2|name3" — pipe (|) is forbidden in filenames on
// Windows and practically forbidden on Linux.
//
// Optional @offset suffix: "filename@1234" loads expected_size bytes
// starting at byte offset 1234 within the file.  This allows a single
// combined ROM image (e.g. 32 KB BASIC) to be mapped into multiple
// smaller chip slots.  Without @offset the entire file must match
// expected_size exactly.

// Advance to the next pipe-separated entry.  Copies up to buf_size-1
// characters of the current entry into buf (null-terminated).  Returns
// a pointer past the separator, or to the terminating '\0'.
static const char* next_entry(const char* p, char* buf, size_t buf_size) {
    const char* sep = strchr(p, ROM_FILENAME_SEPARATOR);
    size_t len = sep ? (size_t)(sep - p) : strlen(p);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, p, len);
    buf[len] = '\0';
    return sep ? sep + 1 : p + len;
}

// Parse an optional "@offset" suffix from path.
// If present, *at_sign is NUL-terminated (truncating the path) and
// the byte offset is written to *out_offset.  Returns true when a
// valid @offset was found.
static bool parse_offset_suffix(char* path, size_t* out_offset) {
    char* at = strrchr(path, '@');
    if (!at || at == path) return false;
    char* endptr;
    unsigned long val = strtoul(at + 1, &endptr, 10);
    if (*endptr != '\0') return false;   // trailing garbage
    *out_offset = (size_t)val;
    *at = '\0';                          // truncate path at '@'
    return true;
}

// Case-insensitive fopen fallback for case-sensitive filesystems (Linux).
// If the exact path fails, scan the directory for a case-insensitive match.
static FILE* fopen_case_insensitive(const char* path, const char* mode) {
    FILE* f = fopen(path, mode);
    if (f) return f;

#ifndef _WIN32
    // Split path into directory and filename
    const char* last_sep = strrchr(path, '/');
    if (!last_sep) return nullptr;       // relative name with no directory

    char dir[1024];
    size_t dir_len = (size_t)(last_sep - path);
    if (dir_len >= sizeof(dir)) return nullptr;
    memcpy(dir, path, dir_len);
    dir[dir_len] = '\0';

    const char* target_name = last_sep + 1;

    DIR* d = opendir(dir);
    if (!d) return nullptr;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (strcasecmp(entry->d_name, target_name) == 0) {
            // Reconstruct full path with the actual on-disk casing
            char resolved[2048];
            snprintf(resolved, sizeof(resolved), "%s/%s", dir, entry->d_name);
            closedir(d);
            return fopen(resolved, mode);
        }
    }
    closedir(d);
#endif

    return nullptr;
}

bool rom_loader_load_file(const char* file_paths, size_t expected_size, 
                         uint8_t** out_buffer, size_t* out_size) {
    if (!file_paths || !out_buffer || !out_size) {
        return false;
    }
    
    *out_buffer = NULL;
    *out_size = 0;
    
    // Try each file path until one succeeds
    char path[1024];
    const char* p = file_paths;
    while (*p) {
        p = next_entry(p, path, sizeof(path));
        if (!path[0]) continue;

        // Check for @offset suffix before opening the file
        size_t file_offset = 0;
        bool has_offset = parse_offset_suffix(path, &file_offset);

        FILE* file = fopen_case_insensitive(path, "rb");
        if (!file) {
            continue;
        }
        
        // Get file size
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        
        if (file_size <= 0) {
            fclose(file);
            continue;
        }

        if (has_offset) {
            // Offset mode: read expected_size bytes from file_offset
            size_t read_size = expected_size > 0 ? expected_size : (size_t)file_size - file_offset;
            if (file_offset + read_size > (size_t)file_size) {
                fclose(file);
                continue;
            }
            fseek(file, (long)file_offset, SEEK_SET);
            uint8_t* buffer = (uint8_t*)malloc(read_size);
            if (!buffer) { fclose(file); continue; }
            size_t bytes_read = fread(buffer, 1, read_size, file);
            fclose(file);
            if (bytes_read != read_size) { free(buffer); continue; }
            *out_buffer = buffer;
            *out_size = read_size;
            printf("Successfully loaded ROM: %s @%zu (%zu bytes)\n", path, file_offset, read_size);
            return true;
        }

        // Whole-file mode: size must match exactly
        fseek(file, 0, SEEK_SET);
        
        // Check expected size if specified
        if (expected_size > 0 && (size_t)file_size != expected_size) {
            fclose(file);
            continue;
        }
        
        // Allocate buffer and read file
        uint8_t* buffer = (uint8_t*)malloc((size_t)file_size);
        if (!buffer) {
            fclose(file);
            continue;
        }
        
        size_t bytes_read = fread(buffer, 1, (size_t)file_size, file);
        fclose(file);
        
        if (bytes_read != (size_t)file_size) {
            free(buffer);
            continue;
        }
        
        // Success!
        *out_buffer = buffer;
        *out_size = (size_t)file_size;
        printf("Successfully loaded ROM: %s (%zu bytes)\n", path, *out_size);
        return true;
    }
    
    // All paths failed
    printf("Failed to load ROM from any of the specified paths\n");
    return false;
}

bool rom_loader_load_to_buffer(const char* file_paths, size_t expected_size,
                              uint8_t* dest_buffer, size_t dest_size) {
    if (!dest_buffer || dest_size == 0) {
        return false;
    }
    
    uint8_t* temp_buffer;
    size_t temp_size;
    
    if (!rom_loader_load_file(file_paths, expected_size, &temp_buffer, &temp_size)) {
        return false;
    }
    
    // Check if loaded data fits in destination buffer
    if (temp_size > dest_size) {
        printf("ROM data too large: %zu bytes (buffer size: %zu)\n", temp_size, dest_size);
        free(temp_buffer);
        return false;
    }
    
    // Copy data and clear any remaining buffer space
    memcpy(dest_buffer, temp_buffer, temp_size);
    if (temp_size < dest_size) {
        memset(dest_buffer + temp_size, 0, dest_size - temp_size);
    }
    
    free(temp_buffer);
    return true;
}

bool rom_loader_load_from_root(const char* rom_root_path, const char* filenames, 
                              size_t expected_size, uint8_t* dest_buffer, size_t dest_size) {
    if (!rom_root_path || !filenames || !dest_buffer || dest_size == 0) {
        return false;
    }
    
    // Build pipe-separated string of full paths
    char path_buf[10 * 1024];  // 10 KB should be plenty
    char* out = path_buf;
    char* end = path_buf + sizeof(path_buf) - 1;  // Reserve space for null terminator

    char name[256];
    const char* p = filenames;
    while (*p && out < end) {
        p = next_entry(p, name, sizeof(name));
        if (!name[0]) continue;
        if (out > path_buf) *out++ = ROM_FILENAME_SEPARATOR;
        int n = snprintf(out, (size_t)(end - out), "%s%c%s",
                         rom_root_path, CERMU_PATH_SEPARATOR, name);
        if (n < 0 || out + n >= end) break;
        out += n;
    }
    *out = '\0';

    return rom_loader_load_to_buffer(path_buf, expected_size, dest_buffer, dest_size);
}

bool rom_loader_verify_md5(const uint8_t* buffer, size_t size, const char* expected_md5) {
    // TODO: Implement MD5 verification
    // For now, just return true to allow ROM loading without verification
    (void)buffer;
    (void)size;
    (void)expected_md5;
    return true;
}
