/*
 * mc6809_decoder.cpp — MC6809 Disassembler / Assembler
 *
 * Algorithmic opcode decoding for the Motorola 6809.  The 6809 has a
 * highly regular opcode map on pages 1/2/3.  The decoder uses lookup
 * tables for the base mnemonic + addressing mode, then formats the
 * addressing mode operand.
 *
 * Addressing modes:
 *   INH  — Inherent (no operand)
 *   IMM8 — Immediate 8-bit
 *   IMM16— Immediate 16-bit
 *   DIR  — Direct page (DP:nn)
 *   EXT  — Extended (nnnn)
 *   IDX  — Indexed (complex post-byte)
 *   REL8 — Relative 8-bit (branch)
 *   REL16— Relative 16-bit (long branch)
 *   R2R  — Register-to-register (TFR/EXG post-byte)
 *   STK  — Stack register list (PSH/PUL post-byte)
 *   IMB  — Immediate + memory (HD6309 OIM/AIM/EIM/TIM — 2 post-bytes)
 */

#include "mc6809_decoder.hpp"

#include <cstdio>
#include <cstring>
#include <cctype>

// ============================================================================
// ADDRESSING MODE ENUMERATION
// ============================================================================

enum AddrMode : uint8_t {
    AM_INH,     // Inherent
    AM_IMM8,    // Immediate 8-bit (#nn)
    AM_IMM16,   // Immediate 16-bit (#nnnn)
    AM_DIR,     // Direct page
    AM_EXT,     // Extended
    AM_IDX,     // Indexed
    AM_REL8,    // 8-bit relative branch
    AM_REL16,   // 16-bit relative branch
    AM_R2R,     // Register-to-register (TFR/EXG)
    AM_STK,     // Stack register list (PSH/PUL)
    AM_IMB_DIR, // Immediate + direct (HD6309 OIM/AIM/EIM/TIM)
    AM_IMB_IDX, // Immediate + indexed (HD6309)
    AM_IMB_EXT, // Immediate + extended (HD6309)
    AM_ILL,     // Illegal / undefined
    AM_IMM32,   // Immediate 32-bit (#nnnnnnnn, HD6309 LDQ)
};

// ============================================================================
// OPCODE TABLE ENTRY
// ============================================================================

struct OpcodeEntry {
    const char* mnemonic;
    AddrMode    mode;
};

// ============================================================================
// PAGE 1 OPCODE TABLE (no prefix)
// ============================================================================

static const OpcodeEntry page1_table[256] = {
    // 0x00-0x0F: Direct unary
    {"NEG",  AM_DIR},   {"???",  AM_ILL},   {"???",  AM_ILL},   {"COM",  AM_DIR},
    {"LSR",  AM_DIR},   {"???",  AM_ILL},   {"ROR",  AM_DIR},   {"ASR",  AM_DIR},
    {"ASL",  AM_DIR},   {"ROL",  AM_DIR},   {"DEC",  AM_DIR},   {"???",  AM_ILL},
    {"INC",  AM_DIR},   {"TST",  AM_DIR},   {"JMP",  AM_DIR},   {"CLR",  AM_DIR},

    // 0x10-0x1F: Page prefix / misc
    {"???",  AM_ILL},   {"???",  AM_ILL},   {"NOP",  AM_INH},   {"SYNC", AM_INH},
    {"???",  AM_ILL},   {"???",  AM_ILL},   {"LBRA", AM_REL16}, {"LBSR", AM_REL16},
    {"???",  AM_ILL},   {"DAA",  AM_INH},   {"ORCC", AM_IMM8},  {"???",  AM_ILL},
    {"ANDCC",AM_IMM8},  {"SEX",  AM_INH},   {"EXG",  AM_R2R},   {"TFR",  AM_R2R},

    // 0x20-0x2F: Short branches
    {"BRA",  AM_REL8},  {"BRN",  AM_REL8},  {"BHI",  AM_REL8},  {"BLS",  AM_REL8},
    {"BCC",  AM_REL8},  {"BCS",  AM_REL8},  {"BNE",  AM_REL8},  {"BEQ",  AM_REL8},
    {"BVC",  AM_REL8},  {"BVS",  AM_REL8},  {"BPL",  AM_REL8},  {"BMI",  AM_REL8},
    {"BGE",  AM_REL8},  {"BLT",  AM_REL8},  {"BGT",  AM_REL8},  {"BLE",  AM_REL8},

    // 0x30-0x3F: LEA / stack / misc
    {"LEAX", AM_IDX},   {"LEAY", AM_IDX},   {"LEAS", AM_IDX},   {"LEAU", AM_IDX},
    {"PSHS", AM_STK},   {"PULS", AM_STK},   {"PSHU", AM_STK},   {"PULU", AM_STK},
    {"???",  AM_ILL},   {"RTS",  AM_INH},   {"ABX",  AM_INH},   {"RTI",  AM_INH},
    {"CWAI", AM_IMM8},  {"MUL",  AM_INH},   {"???",  AM_ILL},   {"SWI",  AM_INH},

    // 0x40-0x4F: Inherent A
    {"NEGA", AM_INH},   {"???",  AM_ILL},   {"???",  AM_ILL},   {"COMA", AM_INH},
    {"LSRA", AM_INH},   {"???",  AM_ILL},   {"RORA", AM_INH},   {"ASRA", AM_INH},
    {"ASLA", AM_INH},   {"ROLA", AM_INH},   {"DECA", AM_INH},   {"???",  AM_ILL},
    {"INCA", AM_INH},   {"TSTA", AM_INH},   {"???",  AM_ILL},   {"CLRA", AM_INH},

    // 0x50-0x5F: Inherent B
    {"NEGB", AM_INH},   {"???",  AM_ILL},   {"???",  AM_ILL},   {"COMB", AM_INH},
    {"LSRB", AM_INH},   {"???",  AM_ILL},   {"RORB", AM_INH},   {"ASRB", AM_INH},
    {"ASLB", AM_INH},   {"ROLB", AM_INH},   {"DECB", AM_INH},   {"???",  AM_ILL},
    {"INCB", AM_INH},   {"TSTB", AM_INH},   {"???",  AM_ILL},   {"CLRB", AM_INH},

    // 0x60-0x6F: Indexed unary
    {"NEG",  AM_IDX},   {"???",  AM_ILL},   {"???",  AM_ILL},   {"COM",  AM_IDX},
    {"LSR",  AM_IDX},   {"???",  AM_ILL},   {"ROR",  AM_IDX},   {"ASR",  AM_IDX},
    {"ASL",  AM_IDX},   {"ROL",  AM_IDX},   {"DEC",  AM_IDX},   {"???",  AM_ILL},
    {"INC",  AM_IDX},   {"TST",  AM_IDX},   {"JMP",  AM_IDX},   {"CLR",  AM_IDX},

    // 0x70-0x7F: Extended unary
    {"NEG",  AM_EXT},   {"???",  AM_ILL},   {"???",  AM_ILL},   {"COM",  AM_EXT},
    {"LSR",  AM_EXT},   {"???",  AM_ILL},   {"ROR",  AM_EXT},   {"ASR",  AM_EXT},
    {"ASL",  AM_EXT},   {"ROL",  AM_EXT},   {"DEC",  AM_EXT},   {"???",  AM_ILL},
    {"INC",  AM_EXT},   {"TST",  AM_EXT},   {"JMP",  AM_EXT},   {"CLR",  AM_EXT},

    // 0x80-0x8F: Immediate A/D
    {"SUBA", AM_IMM8},  {"CMPA", AM_IMM8},  {"SBCA", AM_IMM8},  {"SUBD", AM_IMM16},
    {"ANDA", AM_IMM8},  {"BITA", AM_IMM8},  {"LDA",  AM_IMM8},  {"???",  AM_ILL},
    {"EORA", AM_IMM8},  {"ADCA", AM_IMM8},  {"ORA",  AM_IMM8},  {"ADDA", AM_IMM8},
    {"CMPX", AM_IMM16}, {"BSR",  AM_REL8},  {"LDX",  AM_IMM16}, {"???",  AM_ILL},

    // 0x90-0x9F: Direct A/D
    {"SUBA", AM_DIR},   {"CMPA", AM_DIR},   {"SBCA", AM_DIR},   {"SUBD", AM_DIR},
    {"ANDA", AM_DIR},   {"BITA", AM_DIR},   {"LDA",  AM_DIR},   {"STA",  AM_DIR},
    {"EORA", AM_DIR},   {"ADCA", AM_DIR},   {"ORA",  AM_DIR},   {"ADDA", AM_DIR},
    {"CMPX", AM_DIR},   {"JSR",  AM_DIR},   {"LDX",  AM_DIR},   {"STX",  AM_DIR},

    // 0xA0-0xAF: Indexed A/D
    {"SUBA", AM_IDX},   {"CMPA", AM_IDX},   {"SBCA", AM_IDX},   {"SUBD", AM_IDX},
    {"ANDA", AM_IDX},   {"BITA", AM_IDX},   {"LDA",  AM_IDX},   {"STA",  AM_IDX},
    {"EORA", AM_IDX},   {"ADCA", AM_IDX},   {"ORA",  AM_IDX},   {"ADDA", AM_IDX},
    {"CMPX", AM_IDX},   {"JSR",  AM_IDX},   {"LDX",  AM_IDX},   {"STX",  AM_IDX},

    // 0xB0-0xBF: Extended A/D
    {"SUBA", AM_EXT},   {"CMPA", AM_EXT},   {"SBCA", AM_EXT},   {"SUBD", AM_EXT},
    {"ANDA", AM_EXT},   {"BITA", AM_EXT},   {"LDA",  AM_EXT},   {"STA",  AM_EXT},
    {"EORA", AM_EXT},   {"ADCA", AM_EXT},   {"ORA",  AM_EXT},   {"ADDA", AM_EXT},
    {"CMPX", AM_EXT},   {"JSR",  AM_EXT},   {"LDX",  AM_EXT},   {"STX",  AM_EXT},

    // 0xC0-0xCF: Immediate B
    {"SUBB", AM_IMM8},  {"CMPB", AM_IMM8},  {"SBCB", AM_IMM8},  {"ADDD", AM_IMM16},
    {"ANDB", AM_IMM8},  {"BITB", AM_IMM8},  {"LDB",  AM_IMM8},  {"???",  AM_ILL},
    {"EORB", AM_IMM8},  {"ADCB", AM_IMM8},  {"ORB",  AM_IMM8},  {"ADDB", AM_IMM8},
    {"LDD",  AM_IMM16}, {"???",  AM_ILL},   {"LDU",  AM_IMM16}, {"???",  AM_ILL},

    // 0xD0-0xDF: Direct B
    {"SUBB", AM_DIR},   {"CMPB", AM_DIR},   {"SBCB", AM_DIR},   {"ADDD", AM_DIR},
    {"ANDB", AM_DIR},   {"BITB", AM_DIR},   {"LDB",  AM_DIR},   {"STB",  AM_DIR},
    {"EORB", AM_DIR},   {"ADCB", AM_DIR},   {"ORB",  AM_DIR},   {"ADDB", AM_DIR},
    {"LDD",  AM_DIR},   {"STD",  AM_DIR},   {"LDU",  AM_DIR},   {"STU",  AM_DIR},

    // 0xE0-0xEF: Indexed B
    {"SUBB", AM_IDX},   {"CMPB", AM_IDX},   {"SBCB", AM_IDX},   {"ADDD", AM_IDX},
    {"ANDB", AM_IDX},   {"BITB", AM_IDX},   {"LDB",  AM_IDX},   {"STB",  AM_IDX},
    {"EORB", AM_IDX},   {"ADCB", AM_IDX},   {"ORB",  AM_IDX},   {"ADDB", AM_IDX},
    {"LDD",  AM_IDX},   {"STD",  AM_IDX},   {"LDU",  AM_IDX},   {"STU",  AM_IDX},

    // 0xF0-0xFF: Extended B
    {"SUBB", AM_EXT},   {"CMPB", AM_EXT},   {"SBCB", AM_EXT},   {"ADDD", AM_EXT},
    {"ANDB", AM_EXT},   {"BITB", AM_EXT},   {"LDB",  AM_EXT},   {"STB",  AM_EXT},
    {"EORB", AM_EXT},   {"ADCB", AM_EXT},   {"ORB",  AM_EXT},   {"ADDB", AM_EXT},
    {"LDD",  AM_EXT},   {"STD",  AM_EXT},   {"LDU",  AM_EXT},   {"STU",  AM_EXT},
};

// ============================================================================
// PAGE 2 TABLE ($10 prefix)
// ============================================================================
// Only a subset of opcodes are valid on page 2.  Most are long branches
// and 16-bit Y/S variants of page 1 instructions.

static const OpcodeEntry page2_table[256] = {
    // 0x00-0x1F: Illegal (except long branches at 0x21-0x2F)
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},

    // 0x20-0x2F: Long branches
    {"LBRA", AM_REL16}, {"LBRN", AM_REL16}, {"LBHI", AM_REL16}, {"LBLS", AM_REL16},
    {"LBCC", AM_REL16}, {"LBCS", AM_REL16}, {"LBNE", AM_REL16}, {"LBEQ", AM_REL16},
    {"LBVC", AM_REL16}, {"LBVS", AM_REL16}, {"LBPL", AM_REL16}, {"LBMI", AM_REL16},
    {"LBGE", AM_REL16}, {"LBLT", AM_REL16}, {"LBGT", AM_REL16}, {"LBLE", AM_REL16},

    // 0x30-0x3F: SWI2 at 0x3F
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL},
    {"???",  AM_ILL}, {"???",  AM_ILL}, {"???",  AM_ILL}, {"SWI2", AM_INH},

    // 0x40-0x7F: Illegal
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},

    // 0x80-0x8F: CMPD/CMPY immediate, LDY immediate
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"CMPD", AM_IMM16},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"CMPY", AM_IMM16},{"???",  AM_ILL},  {"LDY",  AM_IMM16},{"???",  AM_ILL},

    // 0x90-0x9F: CMPD/CMPY direct, LDY/STY direct
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"CMPD", AM_DIR},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"CMPY", AM_DIR},  {"???",  AM_ILL},  {"LDY",  AM_DIR},  {"STY",  AM_DIR},

    // 0xA0-0xAF: CMPD/CMPY indexed, LDY/STY indexed
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"CMPD", AM_IDX},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"CMPY", AM_IDX},  {"???",  AM_ILL},  {"LDY",  AM_IDX},  {"STY",  AM_IDX},

    // 0xB0-0xBF: CMPD/CMPY extended, LDY/STY extended
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"CMPD", AM_EXT},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"CMPY", AM_EXT},  {"???",  AM_ILL},  {"LDY",  AM_EXT},  {"STY",  AM_EXT},

    // 0xC0-0xCF: LDS immediate
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"LDS",  AM_IMM16},{"???",  AM_ILL},

    // 0xD0-0xDF: LDS/STS direct
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"LDS",  AM_DIR},  {"STS",  AM_DIR},

    // 0xE0-0xEF: LDS/STS indexed
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"LDS",  AM_IDX},  {"STS",  AM_IDX},

    // 0xF0-0xFF: LDS/STS extended
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"LDS",  AM_EXT},  {"STS",  AM_EXT},
};

// ============================================================================
// PAGE 3 TABLE ($11 prefix)
// ============================================================================
// CMPU and CMPS instructions only.

static const OpcodeEntry page3_table[256] = {
    // 0x00-0x3F: SWI3 at 0x3F
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"SWI3",AM_INH},

    // 0x40-0x7F: Illegal
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},

    // 0x80-0x8F: CMPU/CMPS immediate
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"CMPU", AM_IMM16},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"CMPS", AM_IMM16},{"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},

    // 0x90-0x9F: CMPU/CMPS direct
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"CMPU", AM_DIR},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"CMPS", AM_DIR},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},

    // 0xA0-0xAF: CMPU/CMPS indexed
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"CMPU", AM_IDX},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"CMPS", AM_IDX},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},

    // 0xB0-0xBF: CMPU/CMPS extended
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"CMPU", AM_EXT},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},
    {"CMPS", AM_EXT},  {"???",  AM_ILL},  {"???",  AM_ILL},  {"???",  AM_ILL},

    // 0xC0-0xFF: Illegal
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
    {"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},
};

// ============================================================================
// REGISTER NAME TABLES
// ============================================================================

static const char* const idx_reg_names[4] = { "X", "Y", "U", "S" };

static const char* const tfr_reg_names[16] = {
    "D", "X", "Y", "U", "S", "PC", "W", "V",
    "A", "B", "CC", "DP", "?", "?", "E", "F"
};

// ============================================================================
// INDEXED POST-BYTE DECODER
// ============================================================================
// Returns the number of additional bytes consumed after the post-byte itself.

static int decode_indexed(const uint8_t* mem, int pos, char* buf, size_t buf_size,
                          int* out_pos, bool length_only) {
    uint8_t pb = mem[pos];
    int extra = 0;
    const char* reg = idx_reg_names[(pb >> 5) & 3];
    bool indirect = false;

    if (!(pb & 0x80)) {
        // 5-bit signed offset: bits 4:0, sign-extended from bit 4
        int8_t off = static_cast<int8_t>((pb & 0x1F) | ((pb & 0x10) ? 0xE0 : 0));
        if (!length_only) {
            if (off == 0)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, ",%s", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "%d,%s", off, reg);
        }
        return 0;
    }

    // Bit 7 = 1: use mode field (bits 3:0) + indirect bit (bit 4)
    indirect = (pb & 0x10) != 0;
    uint8_t mode = pb & 0x0F;

    switch (mode) {
    case 0x00: // ,R+
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[,%s+]", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, ",%s+", reg);
        }
        break;
    case 0x01: // ,R++
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[,%s++]", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, ",%s++", reg);
        }
        break;
    case 0x02: // ,-R
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[,-%s]", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, ",-%s", reg);
        }
        break;
    case 0x03: // ,--R
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[,--%s]", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, ",--%s", reg);
        }
        break;
    case 0x04: // ,R (zero offset)
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[,%s]", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, ",%s", reg);
        }
        break;
    case 0x05: // B,R
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[B,%s]", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "B,%s", reg);
        }
        break;
    case 0x06: // A,R
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[A,%s]", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "A,%s", reg);
        }
        break;
    case 0x08: { // n8,R
        int8_t off = static_cast<int8_t>(mem[pos + 1]);
        extra = 1;
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[%d,%s]", off, reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "%d,%s", off, reg);
        }
        break;
    }
    case 0x09: { // n16,R
        int16_t off = static_cast<int16_t>((mem[pos + 1] << 8) | mem[pos + 2]);
        extra = 2;
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[%d,%s]", off, reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "%d,%s", off, reg);
        }
        break;
    }
    case 0x0B: // D,R
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[D,%s]", reg);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "D,%s", reg);
        }
        break;
    case 0x0C: { // n8,PCR
        int8_t off = static_cast<int8_t>(mem[pos + 1]);
        extra = 1;
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[%d,PCR]", off);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "%d,PCR", off);
        }
        break;
    }
    case 0x0D: { // n16,PCR
        int16_t off = static_cast<int16_t>((mem[pos + 1] << 8) | mem[pos + 2]);
        extra = 2;
        if (!length_only) {
            if (indirect)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[%d,PCR]", off);
            else
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "%d,PCR", off);
        }
        break;
    }
    case 0x0F:
        if (indirect) {
            // Extended indirect [nnnn]
            uint16_t addr = (mem[pos + 1] << 8) | mem[pos + 2];
            extra = 2;
            if (!length_only)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "[$%04X]", addr);
        } else {
            // Illegal when not indirect
            if (!length_only)
                *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "???");
        }
        break;
    default:
        if (!length_only)
            *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "???");
        break;
    }

    return extra;
}

// ============================================================================
// STACK REGISTER LIST DECODER
// ============================================================================

static void decode_stack_regs(uint8_t pb, bool is_u_stack, char* buf, size_t buf_size,
                              int* out_pos) {
    // The "other" stack pointer: PSHS/PULS use U, PSHU/PULU use S
    const char* other_sp = is_u_stack ? "S" : "U";
    bool first = true;

    auto emit = [&](const char* name) {
        if (!first) *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, ",");
        *out_pos += snprintf(buf + *out_pos, buf_size - *out_pos, "%s", name);
        first = false;
    };

    if (pb & 0x01) emit("CC");
    if (pb & 0x02) emit("A");
    if (pb & 0x04) emit("B");
    if (pb & 0x08) emit("DP");
    if (pb & 0x10) emit("X");
    if (pb & 0x20) emit("Y");
    if (pb & 0x40) emit(other_sp);
    if (pb & 0x80) emit("PC");
}

// ============================================================================
// CORE DECODE — shared between disassemble and instruction_length
// ============================================================================

struct DecodeResult {
    const char* mnemonic;
    int         length;       // total instruction length in bytes
};

static DecodeResult decode_instruction(uint16_t pc, const uint8_t* memory,
                                       char* buffer, size_t buf_size,
                                       bool length_only) {
    int pos = 0;                     // byte position into memory
    int prefix_bytes = 0;            // 0, 1 ($10), or 1 ($11)
    const OpcodeEntry* entry;

    // Check for page prefix
    if (memory[0] == 0x10) {
        prefix_bytes = 1;
        entry = &page2_table[memory[1]];
        pos = 2;
    } else if (memory[0] == 0x11) {
        prefix_bytes = 1;
        entry = &page3_table[memory[1]];
        pos = 2;
    } else {
        entry = &page1_table[memory[0]];
        pos = 1;
    }

    int out_pos = 0;

    // Emit mnemonic
    if (!length_only) {
        out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "%-6s", entry->mnemonic);
    }

    // Format operand based on addressing mode
    switch (entry->mode) {
    case AM_INH:
        // No operand
        break;

    case AM_IMM8:
        if (!length_only)
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "#$%02X", memory[pos]);
        pos += 1;
        break;

    case AM_IMM16:
        if (!length_only) {
            uint16_t val = (memory[pos] << 8) | memory[pos + 1];
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "#$%04X", val);
        }
        pos += 2;
        break;

    case AM_IMM32:
        if (!length_only) {
            uint32_t val = (static_cast<uint32_t>(memory[pos]) << 24) |
                           (static_cast<uint32_t>(memory[pos + 1]) << 16) |
                           (static_cast<uint32_t>(memory[pos + 2]) << 8) |
                           memory[pos + 3];
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "#$%08X", val);
        }
        pos += 4;
        break;

    case AM_DIR:
        if (!length_only)
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "<$%02X", memory[pos]);
        pos += 1;
        break;

    case AM_EXT:
        if (!length_only) {
            uint16_t addr = (memory[pos] << 8) | memory[pos + 1];
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "$%04X", addr);
        }
        pos += 2;
        break;

    case AM_IDX: {
        int extra = decode_indexed(memory, pos, buffer, buf_size, &out_pos, length_only);
        pos += 1 + extra;  // post-byte + extension bytes
        break;
    }

    case AM_REL8: {
        int8_t off = static_cast<int8_t>(memory[pos]);
        uint16_t target = static_cast<uint16_t>(pc + prefix_bytes + 2 + off);
        if (!length_only)
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "$%04X", target);
        pos += 1;
        break;
    }

    case AM_REL16: {
        int16_t off = static_cast<int16_t>((memory[pos] << 8) | memory[pos + 1]);
        uint16_t target = static_cast<uint16_t>(pc + prefix_bytes + 3 + off);
        if (!length_only)
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "$%04X", target);
        pos += 2;
        break;
    }

    case AM_R2R: {
        uint8_t pb = memory[pos];
        uint8_t src = (pb >> 4) & 0x0F;
        uint8_t dst = pb & 0x0F;
        if (!length_only)
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "%s,%s",
                               tfr_reg_names[src], tfr_reg_names[dst]);
        pos += 1;
        break;
    }

    case AM_STK: {
        uint8_t pb = memory[pos];
        // Determine if it's U stack (PSHU=0x36, PULU=0x37) or S stack (PSHS=0x34, PULS=0x35)
        uint8_t base_opcode = memory[prefix_bytes];
        bool is_u_stack = (base_opcode == 0x36 || base_opcode == 0x37);
        if (!length_only)
            decode_stack_regs(pb, is_u_stack, buffer, buf_size, &out_pos);
        pos += 1;
        break;
    }

    case AM_IMB_DIR:
        if (!length_only)
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "#$%02X,<$%02X",
                               memory[pos], memory[pos + 1]);
        pos += 2;
        break;

    case AM_IMB_IDX: {
        if (!length_only)
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "#$%02X,", memory[pos]);
        pos += 1;
        int extra = decode_indexed(memory, pos, buffer, buf_size, &out_pos, length_only);
        pos += 1 + extra;
        break;
    }

    case AM_IMB_EXT:
        if (!length_only) {
            uint16_t addr = (memory[pos + 1] << 8) | memory[pos + 2];
            out_pos += snprintf(buffer + out_pos, buf_size - out_pos, "#$%02X,$%04X",
                               memory[pos], addr);
        }
        pos += 3;
        break;

    case AM_ILL:
        // pos already correct (1 or 2 with prefix)
        break;
    }

    if (!length_only)
        buffer[out_pos] = '\0';

    return { entry->mnemonic, pos };
}

// ============================================================================
// PUBLIC API
// ============================================================================

int mc6809_disassemble(uint16_t pc, const uint8_t* memory,
                       char* buffer, size_t buf_size) {
    DecodeResult r = decode_instruction(pc, memory, buffer, buf_size, false);
    return r.length;
}

int mc6809_instruction_length(const uint8_t* memory) {
    char dummy[1];
    DecodeResult r = decode_instruction(0, memory, dummy, 0, true);
    return r.length;
}

int mc6809_disassemble_monitor(uint16_t pc, const uint8_t* memory,
                               char* buffer, size_t buf_size) {
    // Disassemble first to get instruction text + length
    char mnem_buf[64];
    int len = mc6809_disassemble(pc, memory, mnem_buf, sizeof(mnem_buf));

    // Format: "XXXX HH HH HH HH HH   MNEMONIC"
    int pos = 0;
    pos += snprintf(buffer + pos, buf_size - pos, "%04X ", pc);

    for (int i = 0; i < len; i++)
        pos += snprintf(buffer + pos, buf_size - pos, "%02X ", memory[i]);

    // Pad hex bytes to 5 bytes max (15 chars) for alignment
    int hex_chars = len * 3;
    while (hex_chars < 15 && pos < static_cast<int>(buf_size) - 1) {
        buffer[pos++] = ' ';
        hex_chars++;
    }

    // Append mnemonic
    snprintf(buffer + pos, buf_size - pos, "%s", mnem_buf);

    return len;
}

// ============================================================================
// ASSEMBLER
// ============================================================================
//
// Simple single-pass assembler: parses a mnemonic line, resolves the
// addressing mode from the operand syntax, and emits the corresponding
// machine code bytes.

// --- Helper: case-insensitive prefix match, returns pointer past match or null ---
static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

static bool ci_match(const char* a, const char* b) {
    while (*b) {
        if (toupper(static_cast<unsigned char>(*a)) != toupper(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return true;
}

// Parse a hex or decimal number.  Returns number of chars consumed, or 0 on failure.
static int parse_number(const char* p, uint32_t* out) {
    const char* start = p;
    if (*p == '$') {
        // Hex
        ++p;
        if (!isxdigit(static_cast<unsigned char>(*p))) return 0;
        uint32_t val = 0;
        while (isxdigit(static_cast<unsigned char>(*p))) {
            val = (val << 4) | (isdigit(static_cast<unsigned char>(*p))
                ? *p - '0' : (toupper(static_cast<unsigned char>(*p)) - 'A' + 10));
            ++p;
        }
        *out = val;
        return static_cast<int>(p - start);
    }
    if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        // 0x hex
        p += 2;
        if (!isxdigit(static_cast<unsigned char>(*p))) return 0;
        uint32_t val = 0;
        while (isxdigit(static_cast<unsigned char>(*p))) {
            val = (val << 4) | (isdigit(static_cast<unsigned char>(*p))
                ? *p - '0' : (toupper(static_cast<unsigned char>(*p)) - 'A' + 10));
            ++p;
        }
        *out = val;
        return static_cast<int>(p - start);
    }
    // Decimal (allow leading minus for relative offsets)
    bool neg = false;
    if (*p == '-') { neg = true; ++p; }
    if (!isdigit(static_cast<unsigned char>(*p))) return 0;
    uint32_t val = 0;
    while (isdigit(static_cast<unsigned char>(*p))) {
        val = val * 10 + (*p - '0');
        ++p;
    }
    if (neg) val = static_cast<uint32_t>(-static_cast<int32_t>(val));
    *out = val;
    return static_cast<int>(p - start);
}

// Resolve a TFR/EXG register name.  Returns register code (0-15) or -1.
static int parse_tfr_reg(const char* p, int* len) {
    struct { const char* name; int code; int nlen; } regs[] = {
        {"CC", 0x0A, 2}, {"DP", 0x0B, 2}, {"PC", 0x05, 2},
        {"D",  0x00, 1}, {"X",  0x01, 1}, {"Y",  0x02, 1},
        {"U",  0x03, 1}, {"S",  0x04, 1}, {"W",  0x06, 1},
        {"V",  0x07, 1}, {"A",  0x08, 1}, {"B",  0x09, 1},
        {"E",  0x0E, 1}, {"F",  0x0F, 1},
    };
    for (auto& r : regs) {
        if (ci_match(p, r.name)) {
            // Make sure next char is not alphanumeric (avoid "DP" matching "D")
            char next = p[r.nlen];
            if (next && isalnum(static_cast<unsigned char>(next))) continue;
            *len = r.nlen;
            return r.code;
        }
    }
    return -1;
}

// Search opcode tables for a mnemonic + addressing mode match.
// Returns the full opcode bytes (prefix + opcode) and fills *opcode_len.
struct OpcodeMatch {
    uint8_t bytes[2];  // bytes[0] = prefix (0x10/0x11) or opcode, bytes[1] = opcode if prefixed
    int     prefix_len; // 0 or 1
    bool    found;
};

static OpcodeMatch find_opcode(const char* mnemonic, AddrMode mode) {
    OpcodeMatch result = {{0, 0}, 0, false};

    // Search page 1
    for (int i = 0; i < 256; i++) {
        if (page1_table[i].mode == mode && ci_match(page1_table[i].mnemonic, mnemonic)
            && strlen(page1_table[i].mnemonic) == strlen(mnemonic)) {
            result.bytes[0] = static_cast<uint8_t>(i);
            result.prefix_len = 0;
            result.found = true;
            return result;
        }
    }
    // Search page 2
    for (int i = 0; i < 256; i++) {
        if (page2_table[i].mode == mode && ci_match(page2_table[i].mnemonic, mnemonic)
            && strlen(page2_table[i].mnemonic) == strlen(mnemonic)) {
            result.bytes[0] = 0x10;
            result.bytes[1] = static_cast<uint8_t>(i);
            result.prefix_len = 1;
            result.found = true;
            return result;
        }
    }
    // Search page 3
    for (int i = 0; i < 256; i++) {
        if (page3_table[i].mode == mode && ci_match(page3_table[i].mnemonic, mnemonic)
            && strlen(page3_table[i].mnemonic) == strlen(mnemonic)) {
            result.bytes[0] = 0x11;
            result.bytes[1] = static_cast<uint8_t>(i);
            result.prefix_len = 1;
            result.found = true;
            return result;
        }
    }
    return result;
}

// Determine which addressing modes a mnemonic supports by checking all tables
struct MnemonicModes {
    bool has[16] = {};  // indexed by AddrMode
};

static MnemonicModes get_mnemonic_modes(const char* mnemonic) {
    MnemonicModes modes{};
    auto check = [&](const OpcodeEntry* table) {
        for (int i = 0; i < 256; i++) {
            if (table[i].mode != AM_ILL && ci_match(table[i].mnemonic, mnemonic)
                && strlen(table[i].mnemonic) == strlen(mnemonic)) {
                modes.has[table[i].mode] = true;
            }
        }
    };
    check(page1_table);
    check(page2_table);
    check(page3_table);
    return modes;
}

int mc6809_assemble(const char* text, uint16_t pc,
                    uint8_t* out, size_t out_size) {
    if (out_size < 5) return -1;

    const char* p = skip_ws(text);

    // Extract mnemonic (up to first space or end)
    char mnem[8];
    int mlen = 0;
    while (*p && !isspace(static_cast<unsigned char>(*p)) && mlen < 7)
        mnem[mlen++] = *p++;
    mnem[mlen] = '\0';
    if (mlen == 0) return -1;

    p = skip_ws(p);

    // Determine addressing mode from operand syntax
    MnemonicModes modes = get_mnemonic_modes(mnem);

    // No operand → inherent
    if (!*p || *p == ';') {
        OpcodeMatch m = find_opcode(mnem, AM_INH);
        if (!m.found) return -1;
        int n = 0;
        for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
        return n;
    }

    // Immediate (#)
    if (*p == '#') {
        ++p;
        uint32_t val;
        if (!parse_number(p, &val)) return -1;

        // Try IMM16 first (wider), then IMM8
        AddrMode imm_mode = AM_IMM8;
        if (modes.has[AM_IMM16]) imm_mode = AM_IMM16;
        if (modes.has[AM_IMM32]) imm_mode = AM_IMM32;
        if (!modes.has[imm_mode] && modes.has[AM_IMM8]) imm_mode = AM_IMM8;

        OpcodeMatch m = find_opcode(mnem, imm_mode);
        if (!m.found) return -1;
        int n = 0;
        for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
        if (imm_mode == AM_IMM8)  { out[n++] = val & 0xFF; }
        else if (imm_mode == AM_IMM16) { out[n++] = (val >> 8) & 0xFF; out[n++] = val & 0xFF; }
        else { out[n++] = (val >> 24) & 0xFF; out[n++] = (val >> 16) & 0xFF;
               out[n++] = (val >> 8) & 0xFF; out[n++] = val & 0xFF; }
        return n;
    }

    // TFR/EXG register pair (e.g., "A,B")
    if (modes.has[AM_R2R]) {
        int slen;
        int src = parse_tfr_reg(p, &slen);
        if (src >= 0) {
            const char* q = skip_ws(p + slen);
            if (*q == ',') {
                q = skip_ws(q + 1);
                int dlen;
                int dst = parse_tfr_reg(q, &dlen);
                if (dst >= 0) {
                    OpcodeMatch m = find_opcode(mnem, AM_R2R);
                    if (!m.found) return -1;
                    int n = 0;
                    for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
                    out[n++] = static_cast<uint8_t>((src << 4) | dst);
                    return n;
                }
            }
        }
    }

    // Stack register list (PSHS/PULS/PSHU/PULU)
    if (modes.has[AM_STK]) {
        uint8_t mask = 0;
        const char* q = p;
        bool is_u_stack = (ci_match(mnem, "PSHU") || ci_match(mnem, "PULU"));
        while (*q) {
            q = skip_ws(q);
            int rlen;
            int code = parse_tfr_reg(q, &rlen);
            if (code < 0) break;
            // Map TFR register codes to stack bit mask
            switch (code) {
            case 0x0A: mask |= 0x01; break; // CC
            case 0x08: mask |= 0x02; break; // A
            case 0x09: mask |= 0x04; break; // B
            case 0x0B: mask |= 0x08; break; // DP
            case 0x01: mask |= 0x10; break; // X
            case 0x02: mask |= 0x20; break; // Y
            case 0x03: if (!is_u_stack) mask |= 0x40; break; // U
            case 0x04: if (is_u_stack) mask |= 0x40; break;  // S
            case 0x05: mask |= 0x80; break; // PC
            case 0x00: mask |= 0x06; break; // D = A+B
            }
            q += rlen;
            q = skip_ws(q);
            if (*q == ',') q++;
        }
        if (mask) {
            OpcodeMatch m = find_opcode(mnem, AM_STK);
            if (!m.found) return -1;
            int n = 0;
            for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
            out[n++] = mask;
            return n;
        }
    }

    // Direct page (<$nn)
    if (*p == '<') {
        ++p;
        uint32_t val;
        if (!parse_number(p, &val)) return -1;
        OpcodeMatch m = find_opcode(mnem, AM_DIR);
        if (!m.found) return -1;
        int n = 0;
        for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
        out[n++] = val & 0xFF;
        return n;
    }

    // Branch targets — check if this mnemonic supports REL8 or REL16
    if (modes.has[AM_REL8] || modes.has[AM_REL16]) {
        uint32_t target;
        if (parse_number(p, &target)) {
            if (modes.has[AM_REL8]) {
                OpcodeMatch m = find_opcode(mnem, AM_REL8);
                if (m.found) {
                    int instr_len = m.prefix_len + 2;
                    int16_t off = static_cast<int16_t>(target - (pc + instr_len));
                    if (off >= -128 && off <= 127) {
                        int n = 0;
                        for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
                        out[n++] = static_cast<uint8_t>(off);
                        return n;
                    }
                }
            }
            if (modes.has[AM_REL16]) {
                OpcodeMatch m = find_opcode(mnem, AM_REL16);
                if (m.found) {
                    int instr_len = m.prefix_len + 3;
                    int16_t off = static_cast<int16_t>(target - (pc + instr_len));
                    int n = 0;
                    for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
                    out[n++] = (off >> 8) & 0xFF;
                    out[n++] = off & 0xFF;
                    return n;
                }
            }
        }
    }

    // Extended address ($nnnn or bare number > $FF)
    {
        uint32_t val;
        int consumed = parse_number(p, &val);
        if (consumed > 0) {
            const char* after = p + consumed;
            after = skip_ws(after);
            if (!*after || *after == ';') {
                // Determine if direct or extended
                if (val <= 0xFF && modes.has[AM_DIR] && !modes.has[AM_EXT]) {
                    OpcodeMatch m = find_opcode(mnem, AM_DIR);
                    if (m.found) {
                        int n = 0;
                        for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
                        out[n++] = val & 0xFF;
                        return n;
                    }
                }
                if (modes.has[AM_EXT]) {
                    OpcodeMatch m = find_opcode(mnem, AM_EXT);
                    if (m.found) {
                        int n = 0;
                        for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];
                        out[n++] = (val >> 8) & 0xFF;
                        out[n++] = val & 0xFF;
                        return n;
                    }
                }
            }
        }
    }

    // Indexed addressing — complex, handle common patterns
    // Patterns: ,R  ,R+  ,R++  ,-R  ,--R  n,R  A,R  B,R  D,R  n,PCR  [...]
    if (modes.has[AM_IDX]) {
        OpcodeMatch m = find_opcode(mnem, AM_IDX);
        if (!m.found) return -1;

        int n = 0;
        for (int i = 0; i <= m.prefix_len; i++) out[n++] = m.bytes[i];

        // Parse indexed operand
        const char* q = p;

        // Check for indirect [...]
        bool indirect = false;
        if (*q == '[') {
            indirect = true;
            q = skip_ws(q + 1);
        }

        // Accumulator offset (A, B, D, followed by comma)
        auto try_acc_offset = [&](const char* q_in) -> int {
            char c = toupper(static_cast<unsigned char>(*q_in));
            if ((c == 'A' || c == 'B' || c == 'D') && q_in[1]) {
                const char* r = skip_ws(q_in + 1);
                if (*r == ',') {
                    r = skip_ws(r + 1);
                    // Parse register name
                    for (int ri = 0; ri < 4; ri++) {
                        if (toupper(static_cast<unsigned char>(*r)) == idx_reg_names[ri][0]) {
                            uint8_t pb = 0x80 | (ri << 5);
                            if (c == 'A') pb |= 0x06;
                            else if (c == 'B') pb |= 0x05;
                            else pb |= 0x0B;  // D
                            if (indirect) pb |= 0x10;
                            out[n++] = pb;
                            return n;
                        }
                    }
                }
            }
            return -1;
        };

        int result = try_acc_offset(q);
        if (result > 0) return result;

        // Comma-first forms: ,R  ,R+  ,R++  ,-R  ,--R
        if (*q == ',') {
            q = skip_ws(q + 1);

            // Pre-decrement
            if (*q == '-') {
                q++;
                bool double_dec = (*q == '-');
                if (double_dec) q++;
                for (int ri = 0; ri < 4; ri++) {
                    if (toupper(static_cast<unsigned char>(*q)) == idx_reg_names[ri][0]) {
                        uint8_t pb = 0x80 | (ri << 5) | (double_dec ? 0x03 : 0x02);
                        if (indirect) pb |= 0x10;
                        out[n++] = pb;
                        return n;
                    }
                }
                return -1;
            }

            // ,R or ,R+ or ,R++
            for (int ri = 0; ri < 4; ri++) {
                if (toupper(static_cast<unsigned char>(*q)) == idx_reg_names[ri][0]) {
                    const char* after = skip_ws(q + 1);
                    if (indirect) {
                        // Expect ]
                        if (*after == ']' || *after == '\0') {
                            out[n++] = 0x80 | (ri << 5) | 0x14; // ,R indirect
                            return n;
                        }
                    }
                    if (*after == '+') {
                        bool double_inc = (after[1] == '+');
                        uint8_t pb = 0x80 | (ri << 5) | (double_inc ? 0x01 : 0x00);
                        if (indirect) pb |= 0x10;
                        out[n++] = pb;
                        return n;
                    }
                    // Zero offset ,R
                    uint8_t pb = 0x80 | (ri << 5) | 0x04;
                    if (indirect) pb |= 0x10;
                    out[n++] = pb;
                    return n;
                }
            }
            return -1;
        }

        // Numeric offset: n,R or n,PCR
        uint32_t offset;
        int consumed = parse_number(q, &offset);
        if (consumed > 0) {
            q += consumed;
            q = skip_ws(q);
            if (*q == ',') {
                q = skip_ws(q + 1);
                // Check for PCR
                if (ci_match(q, "PCR")) {
                    int16_t off16 = static_cast<int16_t>(offset);
                    if (!indirect && off16 >= -128 && off16 <= 127) {
                        out[n++] = 0x8C | (indirect ? 0x10 : 0);
                        out[n++] = static_cast<uint8_t>(off16);
                        return n;
                    }
                    out[n++] = 0x8D | (indirect ? 0x10 : 0);
                    out[n++] = (off16 >> 8) & 0xFF;
                    out[n++] = off16 & 0xFF;
                    return n;
                }
                // Index register
                for (int ri = 0; ri < 4; ri++) {
                    if (toupper(static_cast<unsigned char>(*q)) == idx_reg_names[ri][0]) {
                        int16_t off16 = static_cast<int16_t>(offset);
                        // 5-bit offset: -16..+15, no indirect
                        if (!indirect && off16 >= -16 && off16 <= 15) {
                            out[n++] = static_cast<uint8_t>((ri << 5) | (off16 & 0x1F));
                            return n;
                        }
                        // 8-bit offset
                        if (off16 >= -128 && off16 <= 127) {
                            out[n++] = 0x88 | (ri << 5) | (indirect ? 0x10 : 0);
                            out[n++] = static_cast<uint8_t>(off16);
                            return n;
                        }
                        // 16-bit offset
                        out[n++] = 0x89 | (ri << 5) | (indirect ? 0x10 : 0);
                        out[n++] = (off16 >> 8) & 0xFF;
                        out[n++] = off16 & 0xFF;
                        return n;
                    }
                }
            }
        }

        return -1;
    }

    return -1;
}
