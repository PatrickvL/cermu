/*
 * m680x0_decoder.cpp — Motorola 680x0 Disassembler Implementation
 *
 * Standalone disassembler for the 68000 instruction set.
 * Big-endian instruction words are read from the memory pointer.
 *
 * Instruction format: 16-bit base word + 0-4 extension words
 *   Bits 15-12: instruction group
 *   Bits 11-6:  varies by group (opmode, register, size)
 *   Bits 5-3:   EA mode
 *   Bits 2-0:   EA register
 */

#include "chip/cpu/m680x0/m680x0_decoder.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>

// ── Helper: read big-endian word from memory ────────────────────

static inline uint16_t read_word(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

static inline uint32_t read_long(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |  static_cast<uint32_t>(p[3]);
}

// ── Output helper ───────────────────────────────────────────────

// GCC/Clang format-string checking attribute (no-op on MSVC)
#if defined(__GNUC__) || defined(__clang__)
  #define M68K_PRINTF_FMT(fmtIdx, argIdx) __attribute__((format(printf, fmtIdx, argIdx)))
#else
  #define M68K_PRINTF_FMT(fmtIdx, argIdx)
#endif

struct Emitter {
    char* buf;
    int   size;
    int   pos;

    void emit(const char* fmt, ...) M68K_PRINTF_FMT(2, 3) {
        if (pos >= size) return;
        va_list args;
        va_start(args, fmt);
        int n = vsnprintf(buf + pos, static_cast<size_t>(size - pos), fmt, args);
        va_end(args);
        if (n > 0) pos += n;
    }
};

// ── Size suffix ─────────────────────────────────────────────────

static const char* size_suffix(uint8_t sz) {
    static const char* suffixes[] = { ".B", ".W", ".L" };
    return (sz < 3) ? suffixes[sz] : ".?";
}

// ── Register names ──────────────────────────────────────────────

static const char* d_reg(uint8_t r) {
    static const char* names[] = { "D0","D1","D2","D3","D4","D5","D6","D7" };
    return names[r & 7];
}

static const char* a_reg(uint8_t r) {
    static const char* names[] = { "A0","A1","A2","A3","A4","A5","A6","SP" };
    return names[r & 7];
}

// ── Condition code names ────────────────────────────────────────

static const char* cc_name(uint8_t cc) {
    static const char* names[] = {
        "T", "F", "HI", "LS", "CC", "CS", "NE", "EQ",
        "VC", "VS", "PL", "MI", "GE", "LT", "GT", "LE"
    };
    return names[cc & 0xF];
}

// ── EA formatting ───────────────────────────────────────────────
// Returns number of extension bytes consumed (0, 2, or 4)

static int format_ea(Emitter& e, uint8_t mode, uint8_t reg,
                     uint8_t sz, const uint8_t* ext, uint32_t pc_of_ext) {
    int consumed = 0;
    switch (mode) {
        case 0: e.emit("%s", d_reg(reg)); break;
        case 1: e.emit("%s", a_reg(reg)); break;
        case 2: e.emit("(%s)", a_reg(reg)); break;
        case 3: e.emit("(%s)+", a_reg(reg)); break;
        case 4: e.emit("-(%s)", a_reg(reg)); break;
        case 5: {
            int16_t disp = static_cast<int16_t>(read_word(ext));
            consumed = 2;
            if (disp < 0)
                e.emit("(-$%X,%s)", -disp, a_reg(reg));
            else
                e.emit("($%X,%s)", disp, a_reg(reg));
            break;
        }
        case 6: {
            uint16_t ew = read_word(ext);
            consumed = 2;
            uint8_t xn = (ew >> 12) & 7;
            bool is_a = (ew & 0x8000) != 0;
            bool is_long = (ew & 0x0800) != 0;
            int8_t disp8 = static_cast<int8_t>(ew & 0xFF);
            const char* xn_name = is_a ? a_reg(xn) : d_reg(xn);
            const char* xn_size = is_long ? ".L" : ".W";
            if (disp8 < 0)
                e.emit("(-$%X,%s,%s%s)", -disp8, a_reg(reg), xn_name, xn_size);
            else
                e.emit("($%X,%s,%s%s)", disp8, a_reg(reg), xn_name, xn_size);
            break;
        }
        case 7:
            switch (reg) {
                case 0: {  // Abs.W
                    uint16_t addr = read_word(ext);
                    consumed = 2;
                    e.emit("($%04X).W", addr);
                    break;
                }
                case 1: {  // Abs.L
                    uint32_t addr = read_long(ext);
                    consumed = 4;
                    e.emit("($%08X).L", addr);
                    break;
                }
                case 2: {  // (d16,PC)
                    int16_t disp = static_cast<int16_t>(read_word(ext));
                    consumed = 2;
                    uint32_t target = pc_of_ext + disp;
                    e.emit("($%08X,PC)", target);
                    break;
                }
                case 3: {  // (d8,PC,Xn)
                    uint16_t ew = read_word(ext);
                    consumed = 2;
                    uint8_t xn = (ew >> 12) & 7;
                    bool is_a = (ew & 0x8000) != 0;
                    bool is_long = (ew & 0x0800) != 0;
                    int8_t disp8 = static_cast<int8_t>(ew & 0xFF);
                    const char* xn_name = is_a ? a_reg(xn) : d_reg(xn);
                    const char* xn_size = is_long ? ".L" : ".W";
                    e.emit("($%X,PC,%s%s)", disp8 & 0xFF, xn_name, xn_size);
                    break;
                }
                case 4: {  // #imm
                    if (sz == 2) {  // Long
                        uint32_t imm = read_long(ext);
                        consumed = 4;
                        e.emit("#$%08X", imm);
                    } else {
                        uint16_t imm = read_word(ext);
                        consumed = 2;
                        if (sz == 0) imm &= 0xFF;  // Byte
                        e.emit("#$%X", imm);
                    }
                    break;
                }
                default:
                    e.emit("???");
                    break;
            }
            break;
        default:
            e.emit("???");
            break;
    }
    return consumed;
}

// ── EA extension word length (no output) ────────────────────────

static int ea_ext_length(uint8_t mode, uint8_t reg, uint8_t sz) {
    switch (mode) {
        case 0: case 1: case 2: case 3: case 4: return 0;
        case 5: return 2;
        case 6: return 2;
        case 7:
            switch (reg) {
                case 0: return 2;  // Abs.W
                case 1: return 4;  // Abs.L
                case 2: return 2;  // (d16,PC)
                case 3: return 2;  // (d8,PC,Xn)
                case 4: return (sz == 2) ? 4 : 2;  // #imm
                default: return 0;
            }
        default: return 0;
    }
}

// ── Decode size field ───────────────────────────────────────────

static inline uint8_t decode_size_field(uint16_t opcode) {
    return (opcode >> 6) & 3;  // 0=byte, 1=word, 2=long
}

// ── Main decode function ────────────────────────────────────────

static int decode(uint32_t pc, const uint8_t* mem, Emitter& e) {
    uint16_t opcode = read_word(mem);
    int len = 2;  // Minimum instruction length

    uint8_t group   = (opcode >> 12) & 0xF;
    uint8_t ea_mode = (opcode >> 3) & 7;
    uint8_t ea_reg  = opcode & 7;
    uint8_t reg     = (opcode >> 9) & 7;
    uint8_t opmode  = (opcode >> 6) & 7;

    switch (group) {
        // ── Group 0: Bit manipulation / MOVEP / Immediate ───────
        case 0x0: {
            if ((opcode & 0x0100) && (opcode & 0x0038) == 0x0008) {
                // MOVEP
                int16_t disp = static_cast<int16_t>(read_word(mem + 2));
                len += 2;
                uint8_t sz_bit = (opcode >> 6) & 3;
                if (sz_bit & 1) {
                    // Memory to register
                    e.emit("MOVEP%s ($%X,%s),%s",
                           (sz_bit & 2) ? ".L" : ".W",
                           disp & 0xFFFF, a_reg(ea_reg), d_reg(reg));
                } else {
                    // Register to memory
                    e.emit("MOVEP%s %s,($%X,%s)",
                           (sz_bit & 2) ? ".L" : ".W",
                           d_reg(reg), disp & 0xFFFF, a_reg(ea_reg));
                }
                break;
            }
            if (opcode & 0x0100) {
                // Dynamic bit ops: BTST/BCHG/BCLR/BSET Dn,<ea>
                static const char* bit_ops[] = { "BTST","BCHG","BCLR","BSET" };
                uint8_t bit_op = (opcode >> 6) & 3;
                e.emit("%s %s,", bit_ops[bit_op], d_reg(reg));
                len += format_ea(e, ea_mode, ea_reg, 0, mem + len, pc + len);
                break;
            }
            // Static bit ops or immediate instructions
            uint8_t sub = (opcode >> 9) & 7;
            if (sub == 4) {
                // BTST/BCHG/BCLR/BSET #imm,<ea>
                static const char* bit_ops[] = { "BTST","BCHG","BCLR","BSET" };
                uint8_t bit_op = (opcode >> 6) & 3;
                uint16_t bit_num = read_word(mem + 2);
                len += 2;
                e.emit("%s #%d,", bit_ops[bit_op], bit_num & 0xFF);
                len += format_ea(e, ea_mode, ea_reg, 0, mem + len, pc + len);
                break;
            }
            // ORI, ANDI, SUBI, ADDI, EORI, CMPI
            static const char* imm_ops[] = { "ORI","ANDI","SUBI","ADDI","???","EORI","CMPI","???" };
            uint8_t sz = decode_size_field(opcode);
            // Special: xxxI to CCR / SR
            if (ea_mode == 7 && ea_reg == 4) {
                // Should not reach here — these have specific ea_mode/reg combos
            }
            if (sz <= 2) {
                uint32_t imm;
                if (sz == 2) {
                    imm = read_long(mem + 2);
                    len += 4;
                } else {
                    imm = read_word(mem + 2);
                    len += 2;
                    if (sz == 0) imm &= 0xFF;
                }
                // Check for xxxI to CCR/SR
                if (ea_mode == 7 && ea_reg == 4) {
                    if (sz == 0)
                        e.emit("%s #$%X,CCR", imm_ops[sub], imm);
                    else
                        e.emit("%s #$%X,SR", imm_ops[sub], imm);
                } else {
                    e.emit("%s%s #$%X,", imm_ops[sub], size_suffix(sz), imm);
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                }
            } else {
                e.emit("DC.W $%04X", opcode);
            }
            break;
        }

        // ── Group 1: MOVE.B ─────────────────────────────────────
        case 0x1: {
            uint8_t dst_reg  = (opcode >> 9) & 7;
            uint8_t dst_mode = (opcode >> 6) & 7;
            e.emit("MOVE.B ");
            len += format_ea(e, ea_mode, ea_reg, 0, mem + len, pc + len);
            e.emit(",");
            len += format_ea(e, dst_mode, dst_reg, 0, mem + len, pc + len);
            break;
        }

        // ── Group 2: MOVE.L ─────────────────────────────────────
        case 0x2: {
            uint8_t dst_reg  = (opcode >> 9) & 7;
            uint8_t dst_mode = (opcode >> 6) & 7;
            if (dst_mode == 1) {
                e.emit("MOVEA.L ");
                len += format_ea(e, ea_mode, ea_reg, 2, mem + len, pc + len);
                e.emit(",%s", a_reg(dst_reg));
            } else {
                e.emit("MOVE.L ");
                len += format_ea(e, ea_mode, ea_reg, 2, mem + len, pc + len);
                e.emit(",");
                len += format_ea(e, dst_mode, dst_reg, 2, mem + len, pc + len);
            }
            break;
        }

        // ── Group 3: MOVE.W ─────────────────────────────────────
        case 0x3: {
            uint8_t dst_reg  = (opcode >> 9) & 7;
            uint8_t dst_mode = (opcode >> 6) & 7;
            if (dst_mode == 1) {
                e.emit("MOVEA.W ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",%s", a_reg(dst_reg));
            } else {
                e.emit("MOVE.W ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",");
                len += format_ea(e, dst_mode, dst_reg, 1, mem + len, pc + len);
            }
            break;
        }

        // ── Group 4: Miscellaneous ──────────────────────────────
        case 0x4: {
            // This is the most complex group — many subgroups
            if (opcode == 0x4E71) { e.emit("NOP"); break; }
            if (opcode == 0x4E70) { e.emit("RESET"); break; }
            if (opcode == 0x4E72) {
                uint16_t imm = read_word(mem + 2);
                len += 2;
                e.emit("STOP #$%04X", imm);
                break;
            }
            if (opcode == 0x4E73) { e.emit("RTE"); break; }
            if (opcode == 0x4E75) { e.emit("RTS"); break; }
            if (opcode == 0x4E76) { e.emit("TRAPV"); break; }
            if (opcode == 0x4E77) { e.emit("RTR"); break; }
            if ((opcode & 0xFFF0) == 0x4E40) {
                e.emit("TRAP #%d", opcode & 0xF);
                break;
            }
            if ((opcode & 0xFFF8) == 0x4E50) {
                // LINK An,#disp
                int16_t disp = static_cast<int16_t>(read_word(mem + 2));
                len += 2;
                e.emit("LINK %s,#$%X", a_reg(ea_reg), disp & 0xFFFF);
                break;
            }
            if ((opcode & 0xFFF8) == 0x4E58) {
                e.emit("UNLK %s", a_reg(ea_reg));
                break;
            }
            if ((opcode & 0xFFF8) == 0x4E60) {
                // MOVE An,USP
                e.emit("MOVE %s,USP", a_reg(ea_reg));
                break;
            }
            if ((opcode & 0xFFF8) == 0x4E68) {
                // MOVE USP,An
                e.emit("MOVE USP,%s", a_reg(ea_reg));
                break;
            }
            // LEA <ea>,An
            if ((opcode & 0xF1C0) == 0x41C0) {
                e.emit("LEA ");
                len += format_ea(e, ea_mode, ea_reg, 2, mem + len, pc + len);
                e.emit(",%s", a_reg(reg));
                break;
            }
            // PEA <ea>
            if ((opcode & 0xFFC0) == 0x4840) {
                e.emit("PEA ");
                len += format_ea(e, ea_mode, ea_reg, 2, mem + len, pc + len);
                break;
            }
            // JSR <ea>
            if ((opcode & 0xFFC0) == 0x4E80) {
                e.emit("JSR ");
                len += format_ea(e, ea_mode, ea_reg, 2, mem + len, pc + len);
                break;
            }
            // JMP <ea>
            if ((opcode & 0xFFC0) == 0x4EC0) {
                e.emit("JMP ");
                len += format_ea(e, ea_mode, ea_reg, 2, mem + len, pc + len);
                break;
            }
            // CHK <ea>,Dn
            if ((opcode & 0xF1C0) == 0x4180) {
                e.emit("CHK ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",%s", d_reg(reg));
                break;
            }
            // SWAP Dn
            if ((opcode & 0xFFF8) == 0x4840 && ea_mode == 0) {
                e.emit("SWAP %s", d_reg(ea_reg));
                break;
            }
            // EXT
            if ((opcode & 0xFFF8) == 0x4880) {
                e.emit("EXT.W %s", d_reg(ea_reg));
                break;
            }
            if ((opcode & 0xFFF8) == 0x48C0) {
                e.emit("EXT.L %s", d_reg(ea_reg));
                break;
            }
            // MOVEM
            if ((opcode & 0xFB80) == 0x4880) {
                uint16_t regmask = read_word(mem + 2);
                len += 2;
                uint8_t sz = (opcode & 0x0040) ? 2 : 1;  // .W or .L
                if (opcode & 0x0400) {
                    // Memory to registers
                    e.emit("MOVEM%s ", size_suffix(sz));
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                    e.emit(",#$%04X", regmask);
                } else {
                    // Registers to memory
                    e.emit("MOVEM%s #$%04X,", size_suffix(sz), regmask);
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                }
                break;
            }
            // NEG, NEGX, NOT, CLR, TST
            {
                uint8_t sub = (opcode >> 8) & 0xF;
                uint8_t sz = decode_size_field(opcode);
                const char* name = nullptr;
                switch (sub) {
                    case 0x0: name = "NEGX"; break;
                    case 0x2: name = "CLR"; break;
                    case 0x4: name = "NEG"; break;
                    case 0x6: name = "NOT"; break;
                    case 0xA: name = "TST"; break;
                    default: break;
                }
                if (name && sz <= 2) {
                    e.emit("%s%s ", name, size_suffix(sz));
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                    break;
                }
            }
            // MOVE to/from SR, MOVE to/from CCR
            if ((opcode & 0xFFC0) == 0x40C0) {
                e.emit("MOVE SR,");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                break;
            }
            if ((opcode & 0xFFC0) == 0x44C0) {
                e.emit("MOVE ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",CCR");
                break;
            }
            if ((opcode & 0xFFC0) == 0x46C0) {
                e.emit("MOVE ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",SR");
                break;
            }
            // TAS
            if ((opcode & 0xFFC0) == 0x4AC0) {
                e.emit("TAS ");
                len += format_ea(e, ea_mode, ea_reg, 0, mem + len, pc + len);
                break;
            }
            // Fallthrough
            e.emit("DC.W $%04X", opcode);
            break;
        }

        // ── Group 5: ADDQ / SUBQ / Scc / DBcc ──────────────────
        case 0x5: {
            uint8_t sz = decode_size_field(opcode);
            uint8_t quick_data = reg;
            if (quick_data == 0) quick_data = 8;

            if (sz == 3) {
                // Scc or DBcc
                uint8_t cc = (opcode >> 8) & 0xF;
                if (ea_mode == 1) {
                    // DBcc Dn,label
                    int16_t disp = static_cast<int16_t>(read_word(mem + 2));
                    len += 2;
                    uint32_t target = pc + 2 + disp;
                    e.emit("DB%s %s,$%08X", cc_name(cc), d_reg(ea_reg), target);
                } else {
                    // Scc <ea>
                    e.emit("S%s ", cc_name(cc));
                    len += format_ea(e, ea_mode, ea_reg, 0, mem + len, pc + len);
                }
                break;
            }

            bool is_sub = (opcode & 0x0100) != 0;
            e.emit("%s%s #%d,", is_sub ? "SUBQ" : "ADDQ",
                   size_suffix(sz), quick_data);
            len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
            break;
        }

        // ── Group 6: Bcc / BSR / BRA ────────────────────────────
        case 0x6: {
            uint8_t cc = (opcode >> 8) & 0xF;
            int8_t disp8 = static_cast<int8_t>(opcode & 0xFF);

            const char* mnem;
            if (cc == 0) mnem = "BRA";
            else if (cc == 1) mnem = "BSR";
            else {
                static char buf[8];
                snprintf(buf, sizeof(buf), "B%s", cc_name(cc));
                mnem = buf;
            }

            if (disp8 == 0) {
                // Word displacement
                int16_t disp = static_cast<int16_t>(read_word(mem + 2));
                len += 2;
                uint32_t target = pc + 2 + disp;
                e.emit("%s.W $%08X", mnem, target);
            } else if (disp8 == -1) {
                // Long displacement (68020+)
                uint32_t disp = read_long(mem + 2);
                len += 4;
                uint32_t target = pc + 2 + disp;
                e.emit("%s.L $%08X", mnem, target);
            } else {
                uint32_t target = pc + 2 + disp8;
                e.emit("%s.S $%08X", mnem, target);
            }
            break;
        }

        // ── Group 7: MOVEQ ─────────────────────────────────────
        case 0x7: {
            int8_t imm = static_cast<int8_t>(opcode & 0xFF);
            e.emit("MOVEQ #$%02X,%s", static_cast<uint8_t>(imm), d_reg(reg));
            break;
        }

        // ── Group 8: OR / DIV / SBCD ────────────────────────────
        case 0x8: {
            if ((opcode & 0xF1F0) == 0x8100) {
                // SBCD
                if (opcode & 0x0008) {
                    e.emit("SBCD -(%s),-(%s)", a_reg(ea_reg), a_reg(reg));
                } else {
                    e.emit("SBCD %s,%s", d_reg(ea_reg), d_reg(reg));
                }
                break;
            }
            if ((opcode & 0xF1C0) == 0x80C0) {
                // DIVU <ea>,Dn
                e.emit("DIVU ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",%s", d_reg(reg));
                break;
            }
            if ((opcode & 0xF1C0) == 0x81C0) {
                // DIVS <ea>,Dn
                e.emit("DIVS ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",%s", d_reg(reg));
                break;
            }
            // OR
            uint8_t sz = decode_size_field(opcode);
            if (sz > 2) { e.emit("DC.W $%04X", opcode); break; }
            if (opcode & 0x0100) {
                // OR Dn,<ea>
                e.emit("OR%s %s,", size_suffix(sz), d_reg(reg));
                len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
            } else {
                // OR <ea>,Dn
                e.emit("OR%s ", size_suffix(sz));
                len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                e.emit(",%s", d_reg(reg));
            }
            break;
        }

        // ── Group 9: SUB / SUBA / SUBX ─────────────────────────
        case 0x9: {
            if ((opcode & 0xF130) == 0x9100 && (opcode & 0x00C0) != 0x00C0) {
                uint8_t sz_bits = (opcode >> 6) & 3;
                if (sz_bits <= 2) {
                    // SUBX
                    if (opcode & 0x0008) {
                        e.emit("SUBX%s -(%s),-(%s)", size_suffix(sz_bits),
                               a_reg(ea_reg), a_reg(reg));
                    } else {
                        e.emit("SUBX%s %s,%s", size_suffix(sz_bits),
                               d_reg(ea_reg), d_reg(reg));
                    }
                    break;
                }
            }
            if ((opcode & 0xF0C0) == 0x90C0) {
                // SUBA
                uint8_t sz = (opcode & 0x0100) ? 2 : 1;
                e.emit("SUBA%s ", size_suffix(sz));
                len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                e.emit(",%s", a_reg(reg));
                break;
            }
            // SUB
            {
                uint8_t sz = decode_size_field(opcode);
                if (sz > 2) { e.emit("DC.W $%04X", opcode); break; }
                if (opcode & 0x0100) {
                    e.emit("SUB%s %s,", size_suffix(sz), d_reg(reg));
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                } else {
                    e.emit("SUB%s ", size_suffix(sz));
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                    e.emit(",%s", d_reg(reg));
                }
            }
            break;
        }

        // ── Group A: Line-A emulator ────────────────────────────
        case 0xA:
            e.emit("DC.W $%04X  ; LINE-A", opcode);
            break;

        // ── Group B: CMP / CMPA / CMPM / EOR ───────────────────
        case 0xB: {
            if ((opcode & 0xF0C0) == 0xB0C0) {
                // CMPA
                uint8_t sz = (opcode & 0x0100) ? 2 : 1;
                e.emit("CMPA%s ", size_suffix(sz));
                len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                e.emit(",%s", a_reg(reg));
                break;
            }
            if ((opcode & 0xF138) == 0xB108) {
                // CMPM (An)+,(An)+
                uint8_t sz = decode_size_field(opcode);
                e.emit("CMPM%s (%s)+,(%s)+", size_suffix(sz),
                       a_reg(ea_reg), a_reg(reg));
                break;
            }
            if (opcode & 0x0100) {
                // EOR Dn,<ea>
                uint8_t sz = decode_size_field(opcode);
                if (sz > 2) { e.emit("DC.W $%04X", opcode); break; }
                e.emit("EOR%s %s,", size_suffix(sz), d_reg(reg));
                len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
            } else {
                // CMP <ea>,Dn
                uint8_t sz = decode_size_field(opcode);
                if (sz > 2) { e.emit("DC.W $%04X", opcode); break; }
                e.emit("CMP%s ", size_suffix(sz));
                len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                e.emit(",%s", d_reg(reg));
            }
            break;
        }

        // ── Group C: AND / MUL / ABCD / EXG ────────────────────
        case 0xC: {
            if ((opcode & 0xF1F0) == 0xC100) {
                // ABCD
                if (opcode & 0x0008) {
                    e.emit("ABCD -(%s),-(%s)", a_reg(ea_reg), a_reg(reg));
                } else {
                    e.emit("ABCD %s,%s", d_reg(ea_reg), d_reg(reg));
                }
                break;
            }
            if ((opcode & 0xF1C0) == 0xC0C0) {
                // MULU <ea>,Dn
                e.emit("MULU ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",%s", d_reg(reg));
                break;
            }
            if ((opcode & 0xF1C0) == 0xC1C0) {
                // MULS <ea>,Dn
                e.emit("MULS ");
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                e.emit(",%s", d_reg(reg));
                break;
            }
            if ((opcode & 0xF1F8) == 0xC140) {
                // EXG Dx,Dy
                e.emit("EXG %s,%s", d_reg(reg), d_reg(ea_reg));
                break;
            }
            if ((opcode & 0xF1F8) == 0xC148) {
                // EXG Ax,Ay
                e.emit("EXG %s,%s", a_reg(reg), a_reg(ea_reg));
                break;
            }
            if ((opcode & 0xF1F8) == 0xC188) {
                // EXG Dx,Ay
                e.emit("EXG %s,%s", d_reg(reg), a_reg(ea_reg));
                break;
            }
            // AND
            {
                uint8_t sz = decode_size_field(opcode);
                if (sz > 2) { e.emit("DC.W $%04X", opcode); break; }
                if (opcode & 0x0100) {
                    e.emit("AND%s %s,", size_suffix(sz), d_reg(reg));
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                } else {
                    e.emit("AND%s ", size_suffix(sz));
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                    e.emit(",%s", d_reg(reg));
                }
            }
            break;
        }

        // ── Group D: ADD / ADDA / ADDX ─────────────────────────
        case 0xD: {
            if ((opcode & 0xF130) == 0xD100 && (opcode & 0x00C0) != 0x00C0) {
                uint8_t sz_bits = (opcode >> 6) & 3;
                if (sz_bits <= 2) {
                    // ADDX
                    if (opcode & 0x0008) {
                        e.emit("ADDX%s -(%s),-(%s)", size_suffix(sz_bits),
                               a_reg(ea_reg), a_reg(reg));
                    } else {
                        e.emit("ADDX%s %s,%s", size_suffix(sz_bits),
                               d_reg(ea_reg), d_reg(reg));
                    }
                    break;
                }
            }
            if ((opcode & 0xF0C0) == 0xD0C0) {
                // ADDA
                uint8_t sz = (opcode & 0x0100) ? 2 : 1;
                e.emit("ADDA%s ", size_suffix(sz));
                len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                e.emit(",%s", a_reg(reg));
                break;
            }
            // ADD
            {
                uint8_t sz = decode_size_field(opcode);
                if (sz > 2) { e.emit("DC.W $%04X", opcode); break; }
                if (opcode & 0x0100) {
                    e.emit("ADD%s %s,", size_suffix(sz), d_reg(reg));
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                } else {
                    e.emit("ADD%s ", size_suffix(sz));
                    len += format_ea(e, ea_mode, ea_reg, sz, mem + len, pc + len);
                    e.emit(",%s", d_reg(reg));
                }
            }
            break;
        }

        // ── Group E: Shift / Rotate ────────────────────────────
        case 0xE: {
            uint8_t sz = decode_size_field(opcode);
            if (sz == 3) {
                // Memory shift/rotate (word only)
                static const char* mem_ops[] = {
                    "ASR","ASL","LSR","LSL","ROXR","ROXL","ROR","ROL"
                };
                uint8_t type = (opcode >> 9) & 3;
                uint8_t dir  = (opcode >> 8) & 1;
                uint8_t idx = (type << 1) | dir;
                e.emit("%s ", mem_ops[idx]);
                len += format_ea(e, ea_mode, ea_reg, 1, mem + len, pc + len);
                break;
            }
            // Register shift/rotate
            static const char* reg_ops[] = {
                "ASR","ASL","LSR","LSL","ROXR","ROXL","ROR","ROL"
            };
            uint8_t type = (opcode >> 3) & 3;
            uint8_t dir  = (opcode >> 8) & 1;
            uint8_t idx = (type << 1) | dir;
            bool count_is_reg = (opcode & 0x0020) != 0;
            if (count_is_reg) {
                e.emit("%s%s %s,%s", reg_ops[idx], size_suffix(sz),
                       d_reg(reg), d_reg(ea_reg));
            } else {
                uint8_t count = reg;
                if (count == 0) count = 8;
                e.emit("%s%s #%d,%s", reg_ops[idx], size_suffix(sz),
                       count, d_reg(ea_reg));
            }
            break;
        }

        // ── Group F: Line-F emulator ────────────────────────────
        case 0xF:
            e.emit("DC.W $%04X  ; LINE-F", opcode);
            break;

        default:
            e.emit("DC.W $%04X", opcode);
            break;
    }

    return len;
}

// ── Length-only decode ───────────────────────────────────────────

static int length_only(const uint8_t* mem) {
    uint16_t opcode = read_word(mem);
    int len = 2;

    uint8_t group   = (opcode >> 12) & 0xF;
    uint8_t ea_mode = (opcode >> 3) & 7;
    uint8_t ea_reg  = opcode & 7;
    uint8_t sz;

    switch (group) {
        case 0x0:
            if ((opcode & 0x0100) && (opcode & 0x0038) == 0x0008) {
                return 4;  // MOVEP
            }
            if (opcode & 0x0100) {
                // Dynamic bit ops
                return 2 + ea_ext_length(ea_mode, ea_reg, 0);
            }
            // Immediate ops
            sz = decode_size_field(opcode);
            len += (sz == 2) ? 4 : 2;  // Immediate data
            if (!((opcode >> 9) & 4)) {
                len += ea_ext_length(ea_mode, ea_reg, sz);
            }
            return len;

        case 0x1:  // MOVE.B
            len += ea_ext_length(ea_mode, ea_reg, 0);
            len += ea_ext_length((opcode >> 6) & 7, (opcode >> 9) & 7, 0);
            return len;

        case 0x2:  // MOVE.L
            len += ea_ext_length(ea_mode, ea_reg, 2);
            if (((opcode >> 6) & 7) != 1) {
                len += ea_ext_length((opcode >> 6) & 7, (opcode >> 9) & 7, 2);
            }
            return len;

        case 0x3:  // MOVE.W
            len += ea_ext_length(ea_mode, ea_reg, 1);
            if (((opcode >> 6) & 7) != 1) {
                len += ea_ext_length((opcode >> 6) & 7, (opcode >> 9) & 7, 1);
            }
            return len;

        case 0x4: {
            // Complex — many sub-opcodes
            if (opcode == 0x4E71 || opcode == 0x4E70 || opcode == 0x4E73 ||
                opcode == 0x4E75 || opcode == 0x4E76 || opcode == 0x4E77) return 2;
            if (opcode == 0x4E72) return 4;  // STOP
            if ((opcode & 0xFFF0) == 0x4E40) return 2;  // TRAP
            if ((opcode & 0xFFF8) == 0x4E50) return 4;  // LINK
            if ((opcode & 0xFFF8) == 0x4E58) return 2;  // UNLK
            if ((opcode & 0xFFF8) == 0x4E60) return 2;  // MOVE An,USP
            if ((opcode & 0xFFF8) == 0x4E68) return 2;  // MOVE USP,An
            if ((opcode & 0xFB80) == 0x4880) {
                // MOVEM — has register mask word
                sz = (opcode & 0x0040) ? 2 : 1;
                return 4 + ea_ext_length(ea_mode, ea_reg, sz);
            }
            // Most group 4 ops: base + EA
            sz = decode_size_field(opcode);
            if (sz > 2) sz = 1;
            len += ea_ext_length(ea_mode, ea_reg, sz);
            return len;
        }

        case 0x5:
            sz = decode_size_field(opcode);
            if (sz == 3) {
                if (ea_mode == 1) return 4;  // DBcc
                return 2 + ea_ext_length(ea_mode, ea_reg, 0);  // Scc
            }
            return 2 + ea_ext_length(ea_mode, ea_reg, sz);

        case 0x6: {
            int8_t disp8 = static_cast<int8_t>(opcode & 0xFF);
            if (disp8 == 0) return 4;   // Word displacement
            if (disp8 == -1) return 6;  // Long displacement (68020+)
            return 2;
        }

        case 0x7:
            return 2;  // MOVEQ

        case 0x8:
            if ((opcode & 0xF1F0) == 0x8100) return 2;  // SBCD
            if ((opcode & 0xF1C0) == 0x80C0 || (opcode & 0xF1C0) == 0x81C0) {
                return 2 + ea_ext_length(ea_mode, ea_reg, 1);  // DIVU/DIVS
            }
            sz = decode_size_field(opcode);
            if (sz > 2) sz = 1;
            return 2 + ea_ext_length(ea_mode, ea_reg, sz);

        case 0x9:
        case 0xD:
            if ((opcode & 0xF0C0) == 0x90C0 || (opcode & 0xF0C0) == 0xD0C0) {
                sz = (opcode & 0x0100) ? 2 : 1;
                return 2 + ea_ext_length(ea_mode, ea_reg, sz);  // SUBA/ADDA
            }
            if ((opcode & 0xF130) == 0x9100 || (opcode & 0xF130) == 0xD100) {
                return 2;  // SUBX/ADDX
            }
            sz = decode_size_field(opcode);
            if (sz > 2) sz = 1;
            return 2 + ea_ext_length(ea_mode, ea_reg, sz);

        case 0xA:
        case 0xF:
            return 2;  // Line-A / Line-F

        case 0xB:
            if ((opcode & 0xF138) == 0xB108) return 2;  // CMPM
            if ((opcode & 0xF0C0) == 0xB0C0) {
                sz = (opcode & 0x0100) ? 2 : 1;
                return 2 + ea_ext_length(ea_mode, ea_reg, sz);
            }
            sz = decode_size_field(opcode);
            if (sz > 2) sz = 1;
            return 2 + ea_ext_length(ea_mode, ea_reg, sz);

        case 0xC:
            if ((opcode & 0xF1F0) == 0xC100) return 2;  // ABCD
            if ((opcode & 0xF1C0) == 0xC0C0 || (opcode & 0xF1C0) == 0xC1C0) {
                return 2 + ea_ext_length(ea_mode, ea_reg, 1);  // MULU/MULS
            }
            if ((opcode & 0xF130) == 0xC100) return 2;  // EXG
            sz = decode_size_field(opcode);
            if (sz > 2) sz = 1;
            return 2 + ea_ext_length(ea_mode, ea_reg, sz);

        case 0xE:
            sz = decode_size_field(opcode);
            if (sz == 3) return 2 + ea_ext_length(ea_mode, ea_reg, 1);
            return 2;

        default:
            return 2;
    }
}

// ── Public API ──────────────────────────────────────────────────

int m68k_disassemble(uint32_t pc, const uint8_t* memory, char* buffer, int buf_size) {
    if (!memory || !buffer || buf_size <= 0) return 2;
    buffer[0] = '\0';
    Emitter e{buffer, buf_size, 0};
    return decode(pc, memory, e);
}

int m68k_disassemble_monitor(uint32_t pc, const uint8_t* memory, char* buffer, int buf_size) {
    if (!memory || !buffer || buf_size <= 0) return 2;

    // First disassemble to get mnemonic and length
    char mnemonic[128];
    int len = m68k_disassemble(pc, memory, mnemonic, sizeof(mnemonic));

    // Format: "XXXXXXXX  XXXX XXXX XXXX XXXX XXXX  MNEMONIC"
    int pos = snprintf(buffer, static_cast<size_t>(buf_size), "%08X  ", pc);

    // Hex dump of instruction words
    for (int i = 0; i < len && i < 10; i += 2) {
        if (pos < buf_size - 1) {
            pos += snprintf(buffer + pos, static_cast<size_t>(buf_size - pos),
                           "%04X ", read_word(memory + i));
        }
    }
    // Pad to fixed width (5 words max = 25 chars)
    int hex_width = (len / 2) * 5;
    while (hex_width < 25 && pos < buf_size - 1) {
        buffer[pos++] = ' ';
        hex_width++;
    }

    // Append mnemonic
    if (pos < buf_size - 1) {
        snprintf(buffer + pos, static_cast<size_t>(buf_size - pos), " %s", mnemonic);
    }

    return len;
}

int m68k_instruction_length(uint32_t /*pc*/, const uint8_t* memory) {
    if (!memory) return 2;
    return length_only(memory);
}
