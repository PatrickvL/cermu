/**
 * Shared file-loading helpers — Implementation
 *
 * See format_load_helpers.hpp for API documentation.
 */

#include "core/cermu.hpp"
#include "core/formats/format_load_helpers.hpp"
#include "core/formats/tape_common.hpp"
#include "core/formats/disk_image_common.hpp"
#include "chip/cpu/cpu_chip_base.hpp"
#include <cstring>

// ============================================================================
// Extract exec address from format metadata
// ============================================================================

/// Try to extract an exec address from the metadata blob.
/// Returns the address, or 0 if no valid exec address was found.
static uint32_t extract_exec_addr(const format_load_result_t& result) {
    // Check for disk image metadata first (larger struct)
    if (result.metadata_size >= sizeof(disk_image::file_entry_t)) {
        const auto& entry = *reinterpret_cast<const disk_image::file_entry_t*>(result.metadata);
        if (entry.type == disk_image::DISK_FILE_CODE && entry.exec_addr != 0)
            return entry.exec_addr;
    }

    // Check for tape metadata
    if (result.metadata_size >= sizeof(tape::block_t)) {
        const auto& block = *reinterpret_cast<const tape::block_t*>(result.metadata);
        if (block.type == tape::TAPE_BLOCK_CODE && block.exec_addr != 0)
            return block.exec_addr;
    }

    return 0;
}

// ============================================================================
// format_apply_program
// ============================================================================

bool format_apply_program(const format_load_result_t& result,
                          const format_apply_config_t& cfg) {
    if (!cfg.ram || cfg.ram_size == 0) return false;

    const char* name = cfg.system_name ? cfg.system_name : "System";

    // ── FORMAT_LOAD_PROGRAM ─────────────────────────────────────────────
    if (result.type == FORMAT_LOAD_PROGRAM && result.program.data) {
        uint16_t addr = result.program.load_addr;
        size_t len = result.program.data_size;

        // Calculate RAM offset and clamp
        if (addr >= cfg.ram_base) {
            size_t ram_offset = addr - cfg.ram_base;
            if (ram_offset + len > cfg.ram_size)
                len = cfg.ram_size - ram_offset;
            std::memcpy(cfg.ram + ram_offset, result.program.data, len);
        } else {
            // Load address below RAM base — clamp to start of RAM
            if (len > cfg.ram_size) len = cfg.ram_size;
            std::memcpy(cfg.ram, result.program.data, len);
        }

        // Extract and apply exec address
        uint32_t exec_addr = extract_exec_addr(result);
        if (exec_addr != 0 && exec_addr != 0xFFFFFFFF && cfg.cpu) {
            cfg.cpu->set_pc(exec_addr);
            log_info("%s: Loaded %zu bytes at $%04X, exec $%04X\n",
                     name, len, addr, exec_addr);
        } else {
            log_info("%s: Loaded %zu bytes at $%04X\n", name, len, addr);
        }
        return true;
    }

    // ── FORMAT_LOAD_RAW ─────────────────────────────────────────────────
    if (result.type == FORMAT_LOAD_RAW && result.program.data) {
        uint16_t addr = cfg.raw_load_addr;
        size_t len = result.program.data_size;

        size_t ram_offset = (addr >= cfg.ram_base) ? (addr - cfg.ram_base) : 0;
        if (ram_offset + len > cfg.ram_size)
            len = cfg.ram_size - ram_offset;

        std::memcpy(cfg.ram + ram_offset, result.program.data, len);
        log_info("%s: Raw loaded %zu bytes at $%04X\n", name, len, addr);
        return true;
    }

    return false;
}
