#pragma once

/**
 * Commodore BASIC SYS Address Parser
 *
 * Walks a tokenised BASIC V2/V3.5 program in system memory to locate
 * the SYS start address.  Uses a memory-read callback so it works with
 * any system's memory layout.
 *
 * This is a program-analysis module, not a file-format module—it operates
 * on data already loaded into a system's memory, regardless of which file
 * format delivered it there.
 */

#include <cstdint>

// ============================================================================
// Types
// ============================================================================

/** Memory read callback — reads a byte from the system's address space */
typedef uint8_t (*commodore_mem_read_fn)(void* ctx, uint16_t addr);

/**
 * System-specific BASIC parameters.
 * Each Commodore system has a different BASIC start address but shares
 * the same BASIC V2 tokenization.
 */
struct commodore_basic_params_t {
    uint16_t basic_start;           /**< BASIC program start address ($0801 C64, $1001 VIC-20/C16) */
    uint8_t  sys_token;             /**< SYS token value (0x9E for BASIC V2 / BASIC 3.5) */
    uint8_t  rem_token;             /**< REM token value (0x8F for all Commodore BASIC versions) */
    uint8_t  peek_token;            /**< PEEK token value (0xC2 for BASIC V2) */
    uint16_t basic_start_ptr_lo;    /**< Zero-page address of BASIC start pointer low byte */
    uint16_t basic_start_ptr_hi;    /**< Zero-page address of BASIC start pointer high byte */
};

/** Pre-defined BASIC parameters for common systems */
extern const commodore_basic_params_t COMMODORE_BASIC_C64;
extern const commodore_basic_params_t COMMODORE_BASIC_VIC20;
extern const commodore_basic_params_t COMMODORE_BASIC_C16;

/**
 * Result of BASIC SYS address parsing.
 */
struct commodore_basic_sys_t {
    uint16_t sys_address;           /**< SYS target address (0 = not found) */
    uint16_t line_number;           /**< BASIC line number containing SYS */
    bool     found;                 /**< Whether a SYS statement was found */
};

// ============================================================================
// BASIC Parser API
// ============================================================================

/**
 * Parse BASIC program to find a SYS address.
 *
 * Walks the BASIC line-link chain starting at `start_addr`, looking for
 * a SYS token.  Handles:
 *   - Simple numeric addresses (e.g., SYS 2061)
 *   - PEEK expressions (e.g., SYS PEEK(43)+PEEK(44)*256+offset)
 *   - Skips REM lines
 *   - Scans up to max_lines lines
 */
bool commodore_basic_parse_sys(commodore_mem_read_fn mem_read, void* mem_ctx,
                               uint16_t start_addr,
                               const commodore_basic_params_t* params,
                               int max_lines,
                               commodore_basic_sys_t* out_sys);

