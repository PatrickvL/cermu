#pragma once

#include <cstdint>

#include <cstddef>

// Forward declaration
class MemoryChip;

/**
 * Load a PRG file into RAM.
 * PRG format: 2-byte little-endian load address followed by data.
 * 
 * @param filename Path to the PRG file
 * @param ram Pointer to RAM chip where data will be loaded
 * @param out_load_address Output parameter for the load address read from file
 * @param out_sys_address Output parameter for SYS address if found in BASIC program (0 if not found)
 * @return true on success, false on failure
 */
bool c64_test_load_prg_file(const char* filename, MemoryChip* ram, 
                            uint16_t* out_load_address, uint16_t* out_sys_address);

/**
 * Load a BIN file into RAM at a specific address.
 * BIN format: raw binary data with no header.
 * 
 * @param filename Path to the BIN file
 * @param ram Pointer to RAM chip where data will be loaded
 * @param load_address Address where the binary should be loaded
 * @return true on success, false on failure
 */
bool c64_test_load_bin_file(const char* filename, MemoryChip* ram, uint16_t load_address);

/**
 * Parse a BASIC program in RAM to extract the SYS address.
 * Looks for the first SYS command in the BASIC program.
 *
 * @param ram Pointer to RAM chip containing the BASIC program
 * @param start_address Starting address of the BASIC program (typically $0801)
 * @param load_address Load address of the program (used for PEEK expression evaluation)
 * @return SYS address if found, 0 if not found
 */
uint16_t c64_test_parse_sys_address(MemoryChip* ram, uint16_t start_address, uint16_t load_address);