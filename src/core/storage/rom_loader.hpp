#pragma once

#include <cstdint>
#include <cstddef>

// Separator for multi-filename strings.  Pipe is forbidden in Windows
// filenames and practically forbidden on Linux (shell metacharacter),
// so it can never collide with a real filename character.
inline constexpr char ROM_FILENAME_SEPARATOR = '|';

/**
 * Load ROM file data into memory buffer.
 * 
 * @param file_paths Pipe-separated candidate file paths ("path1|path2|path3")
 * @param expected_size Expected size of ROM file in bytes (0 = don't check size)
 * @param out_buffer Output buffer pointer (will be allocated if successful)
 * @param out_size Actual size of loaded ROM file
 * @return true if ROM was successfully loaded, false otherwise
 */
bool rom_loader_load_file(const char* file_paths, size_t expected_size, 
                         uint8_t** out_buffer, size_t* out_size);

/**
 * Load ROM file data and copy it to existing memory buffer using ROM root path.
 * 
 * @param rom_root_path Root path where ROM files are located
 * @param filenames Pipe-separated candidate filenames ("name1|name2|name3")
 * @param expected_size Expected size of ROM file in bytes
 * @param dest_buffer Destination buffer (must be pre-allocated)
 * @param dest_size Size of destination buffer
 * @return true if ROM was successfully loaded and copied, false otherwise
 */
bool rom_loader_load_from_root(const char* rom_root_path, const char* filenames, 
                              size_t expected_size, uint8_t* dest_buffer, size_t dest_size);

/**
 * Load ROM file data and copy it to existing memory buffer.
 * 
 * @param file_paths Pipe-separated candidate file paths ("path1|path2|path3")
 * @param expected_size Expected size of ROM file in bytes
 * @param dest_buffer Destination buffer (must be pre-allocated)
 * @param dest_size Size of destination buffer
 * @return true if ROM was successfully loaded and copied, false otherwise
 */
bool rom_loader_load_to_buffer(const char* file_paths, size_t expected_size,
                              uint8_t* dest_buffer, size_t dest_size);

/**
 * Verify ROM file using MD5 checksum.
 * 
 * @param buffer ROM data buffer
 * @param size Size of ROM data
 * @param expected_md5 Expected MD5 hash as hex string (32 characters)
 * @return true if checksum matches, false otherwise
 */
bool rom_loader_verify_md5(const uint8_t* buffer, size_t size, const char* expected_md5);
