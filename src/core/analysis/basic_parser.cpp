/**
 * Commodore BASIC SYS Address Parser — Implementation
 */

#include "basic_parser.h"
#include "../cermu.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Use cermu_strncasecmp (maps to _strnicmp on MSVC, strncasecmp on POSIX) */
#define strncasecmp cermu_strncasecmp

// ============================================================================
// BASIC V2 Parameters — Pre-defined for Common Systems
// ============================================================================

const commodore_basic_params_t COMMODORE_BASIC_C64 = {
    0x0801,  /* basic_start */
    0x9E,    /* sys_token */
    0x8F,    /* rem_token */
    0xC2,    /* peek_token */
    0x2B,    /* basic_start_ptr_lo */
    0x2C,    /* basic_start_ptr_hi */
};

const commodore_basic_params_t COMMODORE_BASIC_VIC20 = {
    0x1001,  /* basic_start */
    0x9E,    /* sys_token */
    0x8F,    /* rem_token */
    0xC2,    /* peek_token */
    0x2B,    /* basic_start_ptr_lo */
    0x2C,    /* basic_start_ptr_hi */
};

const commodore_basic_params_t COMMODORE_BASIC_C16 = {
    0x1001,  /* basic_start */
    0x9E,    /* sys_token — same in BASIC 3.5 */
    0x8F,    /* rem_token */
    0xC2,    /* peek_token */
    0x2B,    /* basic_start_ptr_lo */
    0x2C,    /* basic_start_ptr_hi */
};

// ============================================================================
// Internal — SYS Expression Evaluator
// ============================================================================

static uint16_t evaluate_sys_expression(const uint8_t* expr, size_t len,
                                        uint16_t basic_start,
                                        uint8_t peek_token) {
    size_t pos = 0;

    while (pos < len && expr[pos] == ' ') pos++;
    if (pos >= len) return 0;

    /* Case 1: Simple numeric address */
    if (isdigit(expr[pos])) {
        uint32_t addr = 0;
        while (pos < len && isdigit(expr[pos])) {
            addr = addr * 10 + (expr[pos] - '0');
            pos++;
        }
        return (addr <= 0xFFFF) ? (uint16_t)addr : 0;
    }

    /* Case 2: Tokenized PEEK expression ($C2 = PEEK token) */
    if (expr[pos] == peek_token) {
        uint16_t offset = 0;
        for (int i = (int)len - 1; i >= 0; i--) {
            if (isdigit(expr[i])) {
                int j = i;
                uint32_t num = 0;
                uint32_t mult = 1;
                while (j >= 0 && isdigit(expr[j])) {
                    num += (expr[j] - '0') * mult;
                    mult *= 10;
                    j--;
                }
                if (num != 43 && num != 44 && num != 256 &&
                    num != 45 && num != 46) {
                    offset = (num <= 0xFFFF) ? (uint16_t)num : 0;
                    break;
                }
                i = j + 1;
            }
        }
        return basic_start + offset;
    }

    /* Case 3: Text "PEEK" (un-tokenized) */
    if (pos + 4 <= len && strncasecmp((const char*)&expr[pos], "PEEK", 4) == 0) {
        uint16_t offset = 0;
        for (int i = (int)len - 1; i >= 0; i--) {
            if (isdigit(expr[i])) {
                int j = i;
                uint32_t num = 0;
                uint32_t mult = 1;
                while (j >= 0 && isdigit(expr[j])) {
                    num += (expr[j] - '0') * mult;
                    mult *= 10;
                    j--;
                }
                if (num != 43 && num != 44 && num != 256 &&
                    num != 45 && num != 46) {
                    offset = (num <= 0xFFFF) ? (uint16_t)num : 0;
                    break;
                }
                i = j + 1;
            }
        }
        return basic_start + offset;
    }

    return 0;
}

// ============================================================================
// BASIC Parser
// ============================================================================

bool commodore_basic_parse_sys(commodore_mem_read_fn mem_read, void* mem_ctx,
                               uint16_t start_addr,
                               const commodore_basic_params_t* params,
                               int max_lines,
                               commodore_basic_sys_t* out_sys) {
    if (!mem_read || !params || !out_sys) return false;
    memset(out_sys, 0, sizeof(*out_sys));

    uint16_t current = start_addr;

    for (int line = 0; line < max_lines; line++) {
        if (current + 4 >= 0xFFFF) break;
        uint16_t next_line = mem_read(mem_ctx, current) |
                             (mem_read(mem_ctx, current + 1) << 8);
        if (next_line == 0x0000) break;

        uint16_t line_number = mem_read(mem_ctx, current + 2) |
                               (mem_read(mem_ctx, current + 3) << 8);
        uint16_t line_data = current + 4;

        bool is_rem = false;
        for (uint16_t p = line_data; p < next_line; p++) {
            uint8_t b = mem_read(mem_ctx, p);
            if (b == params->rem_token) { is_rem = true; break; }
            if (b == 0x00) break;
            if (b == params->sys_token) break;
            if (b != ' ' && b != ':') break;
        }
        if (is_rem) { current = next_line; continue; }

        for (uint16_t pos = line_data; pos < next_line; pos++) {
            uint8_t b = mem_read(mem_ctx, pos);
            if (b == 0x00) break;

            if (b == params->sys_token) {
                pos++;
                while (pos < next_line && mem_read(mem_ctx, pos) == ' ') pos++;

                uint8_t expr_buf[256];
                size_t expr_len = 0;
                while (pos < next_line && expr_len < sizeof(expr_buf) - 1) {
                    uint8_t eb = mem_read(mem_ctx, pos);
                    if (eb == 0x00 || eb == ':') break;
                    expr_buf[expr_len++] = eb;
                    pos++;
                }

                uint16_t sys_addr = evaluate_sys_expression(
                    expr_buf, expr_len, start_addr, params->peek_token);

                if (sys_addr != 0) {
                    out_sys->sys_address = sys_addr;
                    out_sys->line_number = line_number;
                    out_sys->found = true;
                    printf("BASICParser: Line %u: SYS %u ($%04X)\n",
                           line_number, sys_addr, sys_addr);
                    return true;
                }
            }
        }
        current = next_line;
    }
    return false;
}
