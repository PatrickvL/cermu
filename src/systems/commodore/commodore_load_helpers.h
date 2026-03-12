#pragma once

/**
 * Commodore Load Helpers — Shared format-result-to-memory application logic
 *
 * All Commodore 8-bit systems (C64, VIC-20, C16/Plus4, C128, PET, ...)
 * share the same fundamental load flow:
 *
 *   1. Call format_load_file() to get a format_load_result_t
 *   2. Switch on result.type (PROGRAM / ARCHIVE / METADATA / RAW)
 *   3. Write bytes into system memory
 *   4. Set BASIC pointers ($2B–$32) for BASIC programs
 *   5. Parse SYS address and optionally auto-run
 *
 * The only things that differ between systems are:
 *   - The memory write mechanism (direct array, banked write function, etc.)
 *   - Which addresses constitute a BASIC start (e.g. $0801, $1001, $0401)
 *   - The default raw binary load address
 *   - How auto-run is triggered (set PC, inject keyboard command, etc.)
 *   - The BASIC tokenization parameters for SYS parsing
 *
 * This module captures the shared logic in a single function that uses
 * a small context struct of system-specific callbacks + parameters.
 */

#include "core/formats/format_handler.h"
#include "systems/commodore/basic_parser.h"

#include <cstdint>

#include <cstddef>
// ============================================================================
// Maximum constants
// ============================================================================

/** Maximum number of BASIC start addresses a system can declare */
#define COMMODORE_MAX_BASIC_STARTS  8

// ============================================================================
// Callback types
// ============================================================================

/**
 * Write a single byte to system memory at the given address.
 * @param ctx   Opaque context (e.g. pointer to memory struct or RAM array)
 * @param addr  16-bit address
 * @param val   Byte to write
 */
typedef void (*commodore_write_byte_fn)(void* ctx, uint16_t addr, uint8_t val);

/**
 * Write a contiguous block of bytes to system memory.
 * If NULL, the helper falls back to calling write_byte in a loop.
 * @param ctx   Opaque context
 * @param addr  Starting 16-bit address
 * @param data  Source data
 * @param len   Number of bytes
 */
typedef void (*commodore_write_block_fn)(void* ctx, uint16_t addr,
                                         const uint8_t* data, size_t len);

/**
 * Set the program-counter / execution address for auto-run.
 * If NULL, auto-run via PC is not available (system logs a message instead).
 * @param ctx   Opaque context (e.g. pointer to CPU struct)
 * @param addr  Execution address
 */
typedef void (*commodore_set_pc_fn)(void* ctx, uint16_t addr);

/**
 * Inject a string into the system's KERNAL keyboard buffer.
 * If NULL, the helper uses the standard Commodore keyboard buffer at
 * $0277 with count at $C6 (shared by C64, VIC-20, C16, C128, PET).
 * @param ctx   Opaque context
 * @param str   NUL-terminated ASCII string to inject
 */
typedef void (*commodore_inject_keys_fn)(void* ctx, const char* str);

// ============================================================================
// Load Context — per-system configuration for shared load logic
// ============================================================================

/**
 * System-specific context for the shared Commodore load helper.
 *
 * Populate one of these with the system's callbacks and parameters,
 * then pass it to commodore_apply_load_result().
 */
struct commodore_load_context_t {
    /* --- Identity --------------------------------------------------------- */
    const char* system_name;        /**< Short name for printf: "C64", "VIC20", "C16" */

    /* --- Memory access ---------------------------------------------------- */
    commodore_write_byte_fn   write_byte;   /**< Required: write a single byte */
    commodore_write_block_fn  write_block;  /**< Optional: bulk write (NULL → byte loop) */
    commodore_mem_read_fn     mem_read;     /**< Required: read a byte (for SYS parsing) */
    void*                     mem_ctx;      /**< Context for write_byte/write_block/mem_read */

    /* --- BASIC configuration ---------------------------------------------- */
    const commodore_basic_params_t* basic_params;  /**< SYS parsing token set */

    /**
     * NULL-terminated list of addresses that are valid BASIC program starts.
     * E.g. C64: {0x0801, 0}, VIC-20: {0x0401, 0x1001, 0x1201, 0}, C16: {0x1001, 0}
     */
    uint16_t basic_start_addrs[COMMODORE_MAX_BASIC_STARTS];

    /* --- Default addresses ------------------------------------------------ */
    uint16_t default_raw_addr;      /**< Load address for FORMAT_LOAD_RAW ($C000, $A000, etc.) */

    /* --- Auto-run --------------------------------------------------------- */
    commodore_set_pc_fn   set_pc;   /**< Optional: set CPU program counter for direct ML run */
    void*                 pc_ctx;   /**< Context for set_pc callback */

    commodore_inject_keys_fn inject_keys;  /**< Optional: custom keyboard injection */
    void*                    keys_ctx;     /**< Context for inject_keys callback */

    /**
     * If true, extract SYS address from filename when the program is not
     * loaded at a BASIC start address (e.g. "game_SYS4096.prg" → SYS4096).
     * VIC-20 uses this; C64 and C16 do not.
     */
    bool try_sys_from_filename;
};

// ============================================================================
// Main API
// ============================================================================

/**
 * Apply a format load result to system memory using the shared Commodore
 * load flow.
 *
 * This handles:
 *   - FORMAT_LOAD_PROGRAM:  Write program, set BASIC pointers, parse SYS, auto-run
 *   - FORMAT_LOAD_RAW:      Write at default_raw_addr
 *   - FORMAT_LOAD_ARCHIVE:  Iterate files[], write each, set BASIC pointers, inject RUN
 *   - FORMAT_LOAD_METADATA: Log TAP/CRT info (not loadable)
 *
 * @param ctx       System-specific configuration (populated by caller)
 * @param result    Load result from format_load_file()
 * @param filepath  Original filepath (used for SYS-from-filename extraction)
 * @return true if data was loaded into memory, false otherwise
 */
bool commodore_apply_load_result(const commodore_load_context_t* ctx,
                                 const format_load_result_t* result,
                                 const char* filepath);

