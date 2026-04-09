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
