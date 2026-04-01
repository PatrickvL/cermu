/**
 * Commodore Load Helpers — Implementation
 *
 * Shared format-result-to-memory application logic for all Commodore 8-bit
 * systems.  See commodore_load_helpers.h for design rationale.
 */

#include "core/cermu.hpp"
#include "systems/commodore/commodore_load_helpers.hpp"
#include "core/formats/tap_format.hpp"
#include "core/formats/crt_format.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
// Internal helpers
// ============================================================================

/**
 * Write a contiguous block of bytes using the context's callbacks.
 * Uses write_block if available, otherwise falls back to write_byte loop.
 */
static void ctx_write_block(const commodore_load_context_t* ctx,
                            uint16_t addr, const uint8_t* data, size_t len)
{
    if (ctx->write_block) {
        ctx->write_block(ctx->mem_ctx, addr, data, len);
    } else {
        for (size_t i = 0; i < len; i++) {
            ctx->write_byte(ctx->mem_ctx, (uint16_t)(addr + i), data[i]);
        }
    }
}

/**
 * Write a little-endian 16-bit value to two consecutive memory addresses.
 */
static void ctx_write_le16(const commodore_load_context_t* ctx,
                           uint16_t addr, uint16_t value)
{
    ctx->write_byte(ctx->mem_ctx, addr,     (uint8_t)(value & 0xFF));
    ctx->write_byte(ctx->mem_ctx, (uint16_t)(addr + 1), (uint8_t)(value >> 8));
}

/**
 * Check whether an address matches any of the system's declared BASIC start
 * addresses.
 */
static bool is_basic_start(const commodore_load_context_t* ctx, uint16_t addr)
{
    for (int i = 0; i < COMMODORE_MAX_BASIC_STARTS && ctx->basic_start_addrs[i] != 0; i++) {
        if (ctx->basic_start_addrs[i] == addr)
            return true;
    }
    return false;
}

/**
 * Update Commodore BASIC zero-page pointers ($2B–$32) so that LIST and RUN
 * work correctly after loading a BASIC program.
 *
 * These addresses are identical across C64, VIC-20, C16, C128, PET:
 *   TXTTAB  $2B/$2C  = start of BASIC text
 *   VARTAB  $2D/$2E  = end of BASIC text (start of variables)
 *   ARYTAB  $2F/$30  = start of arrays
 *   STREND  $31/$32  = end of arrays
 */
static void set_basic_pointers(const commodore_load_context_t* ctx,
                               uint16_t start_addr, uint16_t end_addr)
{
    ctx_write_le16(ctx, 0x2B, start_addr);
    ctx_write_le16(ctx, 0x2D, end_addr);
    ctx_write_le16(ctx, 0x2F, end_addr);
    ctx_write_le16(ctx, 0x31, end_addr);
}

/**
 * Set only the BASIC end pointers ($2D–$32), used when an archive loads
 * a BASIC program and we only need the end pointer updated.
 */
[[maybe_unused]] static void set_basic_end_pointers(const commodore_load_context_t* ctx,
                                   uint16_t end_addr)
{
    ctx_write_le16(ctx, 0x2D, end_addr);
    ctx_write_le16(ctx, 0x2F, end_addr);
    ctx_write_le16(ctx, 0x31, end_addr);
}

/**
 * Inject a NUL-terminated ASCII string into the Commodore KERNAL keyboard
 * buffer ($0277, count at $C6).  Shared by C64, VIC-20, C16, C128.
 */
static void default_inject_keys(const commodore_load_context_t* ctx,
                                const char* str)
{
    if (ctx->inject_keys) {
        ctx->inject_keys(ctx->keys_ctx, str);
        return;
    }
    /* Standard Commodore keyboard buffer at $0277, count at $00C6 */
    int len = (int)strlen(str);
    if (len > 10) len = 10;  /* buffer limit on most Commodore systems */
    for (int i = 0; i < len; i++) {
        ctx->write_byte(ctx->mem_ctx, (uint16_t)(0x0277 + i), (uint8_t)str[i]);
    }
    ctx->write_byte(ctx->mem_ctx, 0x00C6, (uint8_t)len);
}

/**
 * Try to extract a SYS address from a filename.
 * Looks for patterns like "game_SYS4096.prg" → 4096.
 * Returns -1 if not found.
 */
static int sys_addr_from_filename(const char* filepath)
{
    const char* basename = filepath;
    const char* sep = strrchr(filepath, '/');
    if (!sep) sep = strrchr(filepath, '\\');
    if (sep) basename = sep + 1;

    for (const char* p = basename; *p; p++) {
        if ((p[0] == 'S' || p[0] == 's') &&
            (p[1] == 'Y' || p[1] == 'y') &&
            (p[2] == 'S' || p[2] == 's') &&
            p[3] >= '0' && p[3] <= '9') {
            return atoi(p + 3);
        }
    }
    return -1;
}

// ============================================================================
// FORMAT_LOAD_PROGRAM handler
// ============================================================================

static bool handle_program(const commodore_load_context_t* ctx,
                           const format_load_result_t* result,
                           const char* filepath)
{
    const program_data_t* prg = &result->program;

    log_info("%s: Loading %s: $%04X-$%04X (%zu bytes)\n",
           ctx->system_name, format_load_type_name(result->type),
           prg->load_addr, prg->end_addr, prg->data_size);

    /* Validate address range */
    if (prg->data_size == 0 || (uint32_t)prg->load_addr + prg->data_size > 0x10000) {
        log_info("%s: Invalid PRG address range: $%04X-$%04X\n",
               ctx->system_name, prg->load_addr, prg->end_addr);
        return false;
    }

    /* Write program data into system memory */
    ctx_write_block(ctx, prg->load_addr, prg->data, prg->data_size);

    /* BASIC program? */
    if (is_basic_start(ctx, prg->load_addr)) {
        /* Parse for SYS address */
        uint16_t run_addr = 0;
        if (ctx->basic_params && ctx->mem_read) {
            commodore_basic_sys_t sys_result = {};
            if (commodore_basic_parse_sys(ctx->mem_read, ctx->mem_ctx,
                                          prg->load_addr,
                                          ctx->basic_params,
                                          10, &sys_result)) {
                run_addr = sys_result.sys_address;
                log_info("%s: Found SYS %u on BASIC line %u\n",
                       ctx->system_name, run_addr, sys_result.line_number);
            }
        }

        /* Update BASIC pointers */
        set_basic_pointers(ctx, prg->load_addr, prg->end_addr);

        /* Auto-run: inject RUN into the keyboard buffer.
         *
         * The system is responsible for deferring the call to this
         * function until KERNAL/BASIC boot has completed (BASIC READY state).
         * At that point:
         *   - KERNAL has initialized all hardware (CIA, VIC-II, SID, IRQ vectors)
         *   - BASIC has run its cold-start (including NEW, which zeroes $0801)
         *   - The BASIC input loop is waiting for keyboard input
         *
         * Writing the program data NOW (after NEW) means it won't be corrupted.
         * Injecting "RUN\r" into the keyboard buffer causes BASIC to execute
         * the program normally — for SYS-stub programs, BASIC tokenizes RUN,
         * finds the SYS statement, and jumps to the machine-language target.
         *
         * FUTURE OPTIMIZATION: Programs that load outside the BASIC area
         * (e.g. raw ML at $C000+) and don't depend on KERNAL/BASIC-
         * initialized state could be loaded before boot completes, reducing
         * perceived startup latency.  Combined with KERNAL memory-test/clear
         * loop patching, this could enable near-instant startup for such files.
         * This is not yet implemented; the current approach prioritizes
         * correctness over speed.
         */
        if (run_addr != 0) {
            log_info("%s: Auto-running from $%04X\n",
                   ctx->system_name, run_addr);
        }
        default_inject_keys(ctx, "RUN\r");
        log_info("%s: Set BASIC pointers and injected RUN command\n",
               ctx->system_name);
    } else {
        /* Machine language program at non-BASIC address */
        uint16_t run_addr = 0;

        /* Try SYS-from-filename if enabled */
        if (ctx->try_sys_from_filename) {
            int addr = sys_addr_from_filename(filepath);
            if (addr >= 0 && addr <= 65535) {
                run_addr = (uint16_t)addr;
            }
        }

        if (run_addr != 0) {
            if (ctx->set_pc) {
                log_info("%s: Auto-running ML from $%04X\n",
                       ctx->system_name, run_addr);
                ctx->set_pc(ctx->pc_ctx, run_addr);
            } else {
                /* Inject SYS command into keyboard buffer */
                char cmd[16];
                int len = snprintf(cmd, sizeof(cmd), "SYS%u\r", run_addr);
                if (len > 0 && len <= 10) {
                    default_inject_keys(ctx, cmd);
                    log_info("%s: Injected auto-start: SYS%u\n",
                           ctx->system_name, run_addr);
                }
            }
        } else {
            log_info("%s: No SYS found — program loaded, use RUN to start\n",
                   ctx->system_name);
        }
    }

    return true;
}

// ============================================================================
// FORMAT_LOAD_METADATA handler
// ============================================================================

static bool handle_metadata(const commodore_load_context_t* ctx,
                            const format_load_result_t* result)
{
    if (result->format && strcmp(result->format->name, "TAP") == 0) {
        const commodore_tap_header_t* hdr =
            (const commodore_tap_header_t*)result->metadata;
        log_info("%s: TAP file detected (platform=%u, version=%u)\n",
               ctx->system_name, hdr->platform, hdr->version);
        log_info("%s: TAP tape emulation not yet implemented\n",
               ctx->system_name);
    } else if (result->format && strcmp(result->format->name, "CRT") == 0) {
        const commodore_crt_header_t* hdr =
            (const commodore_crt_header_t*)result->metadata;
        log_info("%s: CRT cartridge: \"%s\" (type=%u, exrom=%u, game=%u)\n",
               ctx->system_name, hdr->name,
               hdr->hardware_type, hdr->exrom, hdr->game);
        log_info("%s: CRT cartridge loading not yet fully implemented\n",
               ctx->system_name);
    } else {
        log_info("%s: Unknown metadata format: %s\n",
               ctx->system_name,
               result->format ? result->format->name : "(null)");
    }
    return false;  /* metadata-only formats are not directly loadable */
}

// ============================================================================
// FORMAT_LOAD_RAW handler
// ============================================================================

static bool handle_raw(const commodore_load_context_t* ctx,
                       const format_load_result_t* result)
{
    const uint16_t addr = ctx->default_raw_addr;
    const program_data_t* prg = &result->program;

    log_info("%s: Loading BIN at default $%04X (%zu bytes)\n",
           ctx->system_name, addr, prg->data_size);

    if ((uint32_t)addr + prg->data_size > 0x10000) {
        log_info("%s: BIN too large for address space\n", ctx->system_name);
        return false;
    }

    ctx_write_block(ctx, addr, prg->data, prg->data_size);
    return true;
}

// ============================================================================
// FORMAT_LOAD_ARCHIVE handler
// ============================================================================

static bool handle_archive(const commodore_load_context_t* ctx,
                           const format_load_result_t* result)
{
    log_info("%s: Loading archive with %d files\n",
           ctx->system_name, result->file_count);

    bool any_basic = false;
    uint16_t basic_load_addr = 0;
    uint16_t basic_end_addr = 0;

    for (int f = 0; f < result->file_count; f++) {
        const program_data_t* prg = &result->files[f];

        if (prg->data_size == 0 ||
            (uint32_t)prg->load_addr + prg->data_size > 0x10000) {
            log_info("%s: Archive file %d: Invalid address range $%04X-$%04X, skipping\n",
                   ctx->system_name, f, prg->load_addr, prg->end_addr);
            continue;
        }

        log_info("%s: Archive file %d: $%04X-$%04X (%zu bytes)\n",
               ctx->system_name, f, prg->load_addr, prg->end_addr, prg->data_size);

        ctx_write_block(ctx, prg->load_addr, prg->data, prg->data_size);

        /* Track if any file is a BASIC program */
        if (is_basic_start(ctx, prg->load_addr)) {
            any_basic = true;
            basic_load_addr = prg->load_addr;
            basic_end_addr = prg->end_addr;
        }
    }

    if (any_basic) {
        /* Set all BASIC pointers (start + end) and inject RUN */
        set_basic_pointers(ctx, basic_load_addr, basic_end_addr);
        default_inject_keys(ctx, "RUN\r");
        log_info("%s: Archive: Set BASIC pointers ($%04X-$%04X) and injected RUN\n",
               ctx->system_name, basic_load_addr, basic_end_addr);
    }

    return result->file_count > 0;
}

// ============================================================================
// Public API
// ============================================================================

bool commodore_apply_load_result(const commodore_load_context_t* ctx,
                                 const format_load_result_t* result,
                                 const char* filepath)
{
    if (!ctx || !result) return false;

    switch (result->type) {
        case FORMAT_LOAD_PROGRAM:
            return handle_program(ctx, result, filepath);

        case FORMAT_LOAD_METADATA:
            return handle_metadata(ctx, result);

        case FORMAT_LOAD_RAW:
            return handle_raw(ctx, result);

        case FORMAT_LOAD_ARCHIVE:
            return handle_archive(ctx, result);

        default:
            log_info("%s: Unsupported load result type: %d\n",
                   ctx->system_name, result->type);
            return false;
    }
}
