#include "core/storage/rom_loader.hpp"
#include "core/cermu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// All filename/path arguments use pipe-separated format:
// "name1|name2|name3" — pipe (|) is forbidden in filenames on
// Windows and practically forbidden on Linux.

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
        FILE* file = fopen(path, "rb");
        if (!file) {
            continue;
        }
        
        // Get file size
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        if (file_size <= 0) {
            fclose(file);
            continue;
        }
        
        // Check expected size if specified
        if (expected_size > 0 && (size_t)file_size != expected_size) {
            printf("ROM file %s has incorrect size: %ld bytes (expected %zu)\n", 
                   p, file_size, expected_size);
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
