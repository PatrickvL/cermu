#include "core/storage/rom_loader.h"
#include "core/cermu.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool rom_loader_load_file(const char* file_paths[], size_t expected_size, 
                         uint8_t** out_buffer, size_t* out_size) {
    if (!file_paths || !out_buffer || !out_size) {
        return false;
    }
    
    *out_buffer = NULL;
    *out_size = 0;
    
    // Try each file path until one succeeds
    for (int i = 0; file_paths[i] != NULL; i++) {
        FILE* file = fopen(file_paths[i], "rb");
        if (!file) {
            continue; // Try next path
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
                   file_paths[i], file_size, expected_size);
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
        printf("Successfully loaded ROM: %s (%zu bytes)\n", file_paths[i], *out_size);
        return true;
    }
    
    // All paths failed
    printf("Failed to load ROM from any of the specified paths\n");
    return false;
}

bool rom_loader_load_to_buffer(const char* file_paths[], size_t expected_size,
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

bool rom_loader_load_from_root(const char* rom_root_path, const char* filenames[], 
                              size_t expected_size, uint8_t* dest_buffer, size_t dest_size) {
    if (!rom_root_path || !filenames || !dest_buffer || dest_size == 0) {
        return false;
    }
    
    // Count number of filenames to construct paths array
    int filename_count = 0;
    while (filenames[filename_count] != NULL && filename_count < 10) {
        filename_count++;
    }
    
    if (filename_count == 0) {
        return false;
    }
    
    // Construct full paths by combining rom_root_path with each filename
    const char* full_paths[11];  // filename_count + 1 for NULL terminator
    char path_buffers[10][1024];  // Static buffers for constructed paths
    
    for (int i = 0; i < filename_count; i++) {
        snprintf(path_buffers[i], sizeof(path_buffers[i]), "%s%c%s", 
                 rom_root_path, 
                 CERMU_PATH_SEPARATOR,
                 filenames[i]);
        full_paths[i] = path_buffers[i];
    }
    full_paths[filename_count] = NULL;
    
    // Use existing rom_loader_load_to_buffer function
    return rom_loader_load_to_buffer(full_paths, expected_size, dest_buffer, dest_size);
}

bool rom_loader_verify_md5(const uint8_t* buffer, size_t size, const char* expected_md5) {
    // TODO: Implement MD5 verification
    // For now, just return true to allow ROM loading without verification
    (void)buffer;
    (void)size;
    (void)expected_md5;
    return true;
}
