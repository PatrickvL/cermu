#pragma once

/**
 * Shared file-loading helpers — reduces per-system boilerplate.
 *
 * format_apply_program() handles the common pattern of copying program data
 * into system RAM, extracting exec addresses from format metadata, and
 * optionally setting the CPU program counter.
 *
 * Systems supply a small config struct describing their RAM layout and CPU,
 * then call format_apply_program() on the format_load_result_t.
 */

#include "core/formats/format_handler.hpp"
#include <cstdint>
#include <cstddef>
#include <string>

class CpuChipBase;

// ============================================================================
// Configuration for format_apply_program
// ============================================================================

struct format_apply_config_t {
    uint8_t*     ram;           ///< Pointer to emulated RAM buffer
    size_t       ram_size;      ///< Total size of the RAM buffer in bytes
    uint16_t     ram_base;      ///< Base address of the RAM in the address space
                                ///< (0 for most systems, $7800 for VZ, etc.)
    CpuChipBase* cpu;           ///< CPU chip for set_pc() — may be nullptr
    const char*  system_name;   ///< System name for log messages
    uint16_t     raw_load_addr; ///< Default load address for FORMAT_LOAD_RAW
};

// ============================================================================
// API
// ============================================================================

/**
 * Apply a format_load_result_t to system RAM.
 *
 * Handles FORMAT_LOAD_PROGRAM and FORMAT_LOAD_RAW result types:
 * - Copies program data into RAM at the specified load address
 * - Extracts exec addresses from tape::block_t / disk_image::file_entry_t metadata
 * - Sets CPU PC via CpuChipBase::set_pc() when an exec address is found
 * - Logs the operation with the system name
 *
 * Does NOT call result.release() — the caller is responsible for that.
 *
 * @return true if data was successfully applied
 */
bool format_apply_program(const format_load_result_t& result,
                          const format_apply_config_t& cfg);

/**
 * Full load-file pipeline: format_load_file → format_apply_program → release.
 *
 * Replaces the common 15-line boilerplate pattern found in most system
 * load_file() implementations.  Calls format_load_file(), applies the result
 * via format_apply_program(), releases resources, and logs on failure.
 *
 * @return true if a file was loaded and applied successfully
 */
bool format_load_and_apply(const char* filepath, const format_apply_config_t& cfg);

/**
 * Load a raw ROM image from disk, mirror it into a destination buffer, set
 * program_title, and log the result.
 *
 * Used by cartridge-based systems (ColecoVision, SG-1000, SVI, etc.) where
 * smaller ROMs are mirrored to fill a fixed-size address window.
 *
 * @param filepath        Path to the ROM file (supports VFS / archive paths)
 * @param dest            Destination buffer (e.g. board_.cart.data())
 * @param window_size     Address window to fill via mirroring (e.g. 0x8000)
 * @param max_rom_size    Maximum acceptable ROM size (usually == window_size)
 * @param system_name     Name string for log messages
 * @param program_title   [out] Receives the bare filename
 * @return true on success
 */
bool load_raw_rom_mirrored(const char* filepath, uint8_t* dest,
                           size_t window_size, size_t max_rom_size,
                           const char* system_name,
                           std::string& program_title);
