#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Discover the data root folder by searching upwards from executable location.
 * Searches for 'data' folder containing system-specific files.
 * 
 * @param system_name System folder name (e.g., "c64", "vic20", "apple2")
 * @param out_path Buffer to store the discovered data root path
 * @param path_size Size of the output buffer
 * @return true if data root folder was found, false otherwise
 */
bool system_config_discover_data_root(const char* system_name, char* out_path, size_t path_size);

/**
 * Discover the ROM root folder by searching upwards from executable location.
 * Searches for 'data/system_name/roms' folder.
 * 
 * @param system_name System folder name (e.g., "c64", "vic20")
 * @param out_path Buffer to store the discovered ROM root path
 * @param path_size Size of the output buffer
 * @return true if ROM root folder was found, false otherwise
 */
bool system_config_discover_rom_root(const char* system_name, char* out_path, size_t path_size);


