/*
 * z80_decoder.cpp — Z80 CPU Instruction Decoder and Disassembler
 *
 * Decodes Z80 instructions using direct bit-field extraction from opcodes
 * (the standard x/y/z/p/q scheme from Sean Young's "Undocumented Z80").
 *
 * Handles all prefix combinations:
 *   - No prefix:     base opcodes (256)
 *   - CB xx:         bit/shift/rotate operations
 *   - ED xx:         extended instructions
 *   - DD/FD:         IX/IY register substitution
 *   - DD CB dd xx:   indexed bit operations (FD CB dd xx for IY)
 */

#include "z80_decoder.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ============================================================================
// STRING TABLES
// ============================================================================

static const char* r_names[8]    = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };
static const char* rp_names[4]   = { "BC", "DE", "HL", "SP" };
static const char* rp2_names[4]  = { "BC", "DE", "HL", "AF" };
static const char* cc_names[8]   = { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" };
static const char* alu_names[8]  = { "ADD A,", "ADC A,", "SUB ", "SBC A,", "AND ", "XOR ", "OR ", "CP " };
static const char* rot_names[8]  = { "RLC", "RRC", "RL", "RR", "SLA", "SRA", "SLL", "SRL" };
static const char* im_names[8]   = { "0", "0/1", "1", "2", "0", "0/1", "1", "2" };

// IX/IY variants of register names
static const char* r_ix_names[8] = { "B", "C", "D", "E", "IXH", "IXL", "(IX%+d)", "A" };
static const char* r_iy_names[8] = { "B", "C", "D", "E", "IYH", "IYL", "(IY%+d)", "A" };

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// Append formatted string to buffer, updating position
#if defined(__GNUC__) || defined(__clang__)
static int emit(char* buf, size_t bufsize, int pos, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));
#endif
static int emit(char* buf, size_t bufsize, int pos, const char* fmt, ...)
{
    if (pos < 0 || static_cast<size_t>(pos) >= bufsize) return pos;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + pos, bufsize - pos, fmt, args);
    va_end(args);
    return pos + n;
}

// Get register name with IX/IY substitution for H/L/(HL)
static const char* reg_name(uint8_t idx, uint8_t prefix) {
    if (prefix == 0xDD) return r_ix_names[idx];
    if (prefix == 0xFD) return r_iy_names[idx];
    return r_names[idx];
}

// Get register pair name with IX/IY substitution for HL
static const char* rp_name(uint8_t idx, uint8_t prefix) {
    if (idx == 2) { // HL
        if (prefix == 0xDD) return "IX";
        if (prefix == 0xFD) return "IY";
    }
    return rp_names[idx];
}

static const char* rp2_name(uint8_t idx, uint8_t prefix) {
    if (idx == 2) { // HL
        if (prefix == 0xDD) return "IX";
        if (prefix == 0xFD) return "IY";
    }
    return rp2_names[idx];
}

// Format (IX+d)/(IY+d) displacement
static int emit_indexed(char* buf, size_t bufsize, int pos, uint8_t prefix, int8_t d) {
    const char* reg = (prefix == 0xDD) ? "IX" : "IY";
    if (d >= 0) {
        return emit(buf, bufsize, pos, "(%s+$%02X)", reg, d);
    } else {
        return emit(buf, bufsize, pos, "(%s-$%02X)", reg, -d);
    }
}

// ============================================================================
// CB PREFIX DECODE
// ============================================================================

static int decode_cb(char* buf, size_t bufsize, uint8_t op) {
    uint8_t x = (op >> 6) & 3;
    uint8_t y = (op >> 3) & 7;
    uint8_t z = op & 7;
    int pos = 0;

    switch (x) {
    case 0: // Shift/rotate
        pos = emit(buf, bufsize, pos, "%s %s", rot_names[y], r_names[z]);
        break;
    case 1: // BIT
        pos = emit(buf, bufsize, pos, "BIT %d,%s", y, r_names[z]);
        break;
    case 2: // RES
        pos = emit(buf, bufsize, pos, "RES %d,%s", y, r_names[z]);
        break;
    case 3: // SET
        pos = emit(buf, bufsize, pos, "SET %d,%s", y, r_names[z]);
        break;
    }
    return pos;
}

// DD CB dd xx / FD CB dd xx
static int decode_ddfd_cb(char* buf, size_t bufsize, uint8_t prefix, int8_t d, uint8_t op) {
    uint8_t x = (op >> 6) & 3;
    uint8_t y = (op >> 3) & 7;
    uint8_t z = op & 7;
    int pos = 0;

    // All DD/FD CB operations work on (IX+d)/(IY+d)
    char indexed[16];
    const char* reg = (prefix == 0xDD) ? "IX" : "IY";
    if (d >= 0) {
        snprintf(indexed, sizeof(indexed), "(%s+$%02X)", reg, d);
    } else {
        snprintf(indexed, sizeof(indexed), "(%s-$%02X)", reg, -d);
    }

    switch (x) {
    case 0: // Shift/rotate
        if (z == 6) {
            pos = emit(buf, bufsize, pos, "%s %s", rot_names[y], indexed);
        } else {
            // Undocumented: result also copied to register z
            pos = emit(buf, bufsize, pos, "%s %s,%s", rot_names[y], indexed, r_names[z]);
        }
        break;
    case 1: // BIT
        pos = emit(buf, bufsize, pos, "BIT %d,%s", y, indexed);
        break;
    case 2: // RES
        if (z == 6) {
            pos = emit(buf, bufsize, pos, "RES %d,%s", y, indexed);
        } else {
            pos = emit(buf, bufsize, pos, "RES %d,%s,%s", y, indexed, r_names[z]);
        }
        break;
    case 3: // SET
        if (z == 6) {
            pos = emit(buf, bufsize, pos, "SET %d,%s", y, indexed);
        } else {
            pos = emit(buf, bufsize, pos, "SET %d,%s,%s", y, indexed, r_names[z]);
        }
        break;
    }
    return pos;
}

// ============================================================================
// ED PREFIX DECODE
// ============================================================================

static int decode_ed(char* buf, size_t bufsize, uint8_t op, const uint8_t* operands, int* extra_bytes) {
    uint8_t x = (op >> 6) & 3;
    uint8_t y = (op >> 3) & 7;
    uint8_t z = op & 7;
    uint8_t q = y & 1;
    uint8_t p = y >> 1;
    int pos = 0;
    *extra_bytes = 0;

    if (x == 1) {
        switch (z) {
        case 0: // IN r,(C)
            if (y != 6) {
                pos = emit(buf, bufsize, pos, "IN %s,(C)", r_names[y]);
            } else {
                pos = emit(buf, bufsize, pos, "IN (C)");
            }
            break;
        case 1: // OUT (C),r
            if (y != 6) {
                pos = emit(buf, bufsize, pos, "OUT (C),%s", r_names[y]);
            } else {
                pos = emit(buf, bufsize, pos, "OUT (C),0");
            }
            break;
        case 2: // SBC/ADC HL,rr
            if (q == 0) {
                pos = emit(buf, bufsize, pos, "SBC HL,%s", rp_names[p]);
            } else {
                pos = emit(buf, bufsize, pos, "ADC HL,%s", rp_names[p]);
            }
            break;
        case 3: { // LD (nn),rr / LD rr,(nn)
            uint16_t nn = operands[0] | (operands[1] << 8);
            *extra_bytes = 2;
            if (q == 0) {
                pos = emit(buf, bufsize, pos, "LD ($%04X),%s", nn, rp_names[p]);
            } else {
                pos = emit(buf, bufsize, pos, "LD %s,($%04X)", rp_names[p], nn);
            }
            break;
        }
        case 4: // NEG
            pos = emit(buf, bufsize, pos, "NEG");
            break;
        case 5: // RETI / RETN
            if (y == 1) {
                pos = emit(buf, bufsize, pos, "RETI");
            } else {
                pos = emit(buf, bufsize, pos, "RETN");
            }
            break;
        case 6: // IM
            pos = emit(buf, bufsize, pos, "IM %s", im_names[y]);
            break;
        case 7:
            switch (y) {
            case 0: pos = emit(buf, bufsize, pos, "LD I,A"); break;
            case 1: pos = emit(buf, bufsize, pos, "LD A,I"); break;
            case 2: pos = emit(buf, bufsize, pos, "LD R,A"); break;
            case 3: pos = emit(buf, bufsize, pos, "LD A,R"); break;
            case 4: pos = emit(buf, bufsize, pos, "RRD"); break;
            case 5: pos = emit(buf, bufsize, pos, "RLD"); break;
            default: pos = emit(buf, bufsize, pos, "NOP"); break;
            }
            break;
        }
        return pos;
    }

    if (x == 2 && z <= 3 && y >= 4) {
        // Block instructions
        static const char* bli_names[4][4] = {
            { "LDI",  "CPI",  "INI",  "OUTI" },  // y=4
            { "LDD",  "CPD",  "IND",  "OUTD" },  // y=5
            { "LDIR", "CPIR", "INIR", "OTIR" },   // y=6
            { "LDDR", "CPDR", "INDR", "OTDR" },   // y=7
        };
        pos = emit(buf, bufsize, pos, "%s", bli_names[y - 4][z]);
        return pos;
    }

    // Invalid ED opcode
    pos = emit(buf, bufsize, pos, "NOP");
    return pos;
}

// ============================================================================
// BASE OPCODE DECODE
// ============================================================================

static int decode_base(char* buf, size_t bufsize, uint8_t op, uint8_t prefix,
                       const uint8_t* operands, int* extra_bytes, uint16_t instr_pc, int prefix_len) {
    uint8_t x = (op >> 6) & 3;
    uint8_t y = (op >> 3) & 7;
    uint8_t z = op & 7;
    uint8_t p = y >> 1;
    uint8_t q = y & 1;
    int pos = 0;
    *extra_bytes = 0;

    switch (x) {
    case 0:
        switch (z) {
        case 0:
            switch (y) {
            case 0: pos = emit(buf, bufsize, pos, "NOP"); break;
            case 1: pos = emit(buf, bufsize, pos, "EX AF,AF'"); break;
            case 2: {
                int8_t d = (int8_t)operands[0];
                *extra_bytes = 1;
                uint16_t target = (uint16_t)(instr_pc + prefix_len + 2 + d);
                pos = emit(buf, bufsize, pos, "DJNZ $%04X", target);
                break;
            }
            case 3: {
                int8_t d = (int8_t)operands[0];
                *extra_bytes = 1;
                uint16_t target = (uint16_t)(instr_pc + prefix_len + 2 + d);
                pos = emit(buf, bufsize, pos, "JR $%04X", target);
                break;
            }
            default: { // y=4..7: JR cc,d
                int8_t d = (int8_t)operands[0];
                *extra_bytes = 1;
                uint16_t target = (uint16_t)(instr_pc + prefix_len + 2 + d);
                pos = emit(buf, bufsize, pos, "JR %s,$%04X", cc_names[y - 4], target);
                break;
            }
            }
            break;
        case 1:
            if (q == 0) {
                uint16_t nn = operands[0] | (operands[1] << 8);
                *extra_bytes = 2;
                pos = emit(buf, bufsize, pos, "LD %s,$%04X", rp_name(p, prefix), nn);
            } else {
                pos = emit(buf, bufsize, pos, "ADD %s,%s",
                           prefix == 0xDD ? "IX" : (prefix == 0xFD ? "IY" : "HL"),
                           rp_name(p, prefix));
            }
            break;
        case 2:
            if (q == 0) {
                switch (p) {
                case 0: pos = emit(buf, bufsize, pos, "LD (BC),A"); break;
                case 1: pos = emit(buf, bufsize, pos, "LD (DE),A"); break;
                case 2: {
                    uint16_t nn = operands[0] | (operands[1] << 8);
                    *extra_bytes = 2;
                    pos = emit(buf, bufsize, pos, "LD ($%04X),%s", nn,
                               prefix == 0xDD ? "IX" : (prefix == 0xFD ? "IY" : "HL"));
                    break;
                }
                case 3: {
                    uint16_t nn = operands[0] | (operands[1] << 8);
                    *extra_bytes = 2;
                    pos = emit(buf, bufsize, pos, "LD ($%04X),A", nn);
                    break;
                }
                }
            } else {
                switch (p) {
                case 0: pos = emit(buf, bufsize, pos, "LD A,(BC)"); break;
                case 1: pos = emit(buf, bufsize, pos, "LD A,(DE)"); break;
                case 2: {
                    uint16_t nn = operands[0] | (operands[1] << 8);
                    *extra_bytes = 2;
                    pos = emit(buf, bufsize, pos, "LD %s,($%04X)",
                               prefix == 0xDD ? "IX" : (prefix == 0xFD ? "IY" : "HL"), nn);
                    break;
                }
                case 3: {
                    uint16_t nn = operands[0] | (operands[1] << 8);
                    *extra_bytes = 2;
                    pos = emit(buf, bufsize, pos, "LD A,($%04X)", nn);
                    break;
                }
                }
            }
            break;
        case 3:
            if (q == 0) {
                pos = emit(buf, bufsize, pos, "INC %s", rp_name(p, prefix));
            } else {
                pos = emit(buf, bufsize, pos, "DEC %s", rp_name(p, prefix));
            }
            break;
        case 4: // INC r
            if (y == 6 && prefix) {
                int8_t d = (int8_t)operands[0];
                *extra_bytes = 1;
                pos = emit(buf, bufsize, 0, "INC ");
                pos = emit_indexed(buf, bufsize, pos, prefix, d);
            } else if (y == 6) {
                pos = emit(buf, bufsize, pos, "INC (HL)");
            } else {
                pos = emit(buf, bufsize, pos, "INC %s", reg_name(y, prefix));
            }
            break;
        case 5: // DEC r
            if (y == 6 && prefix) {
                int8_t d = (int8_t)operands[0];
                *extra_bytes = 1;
                pos = emit(buf, bufsize, 0, "DEC ");
                pos = emit_indexed(buf, bufsize, pos, prefix, d);
            } else if (y == 6) {
                pos = emit(buf, bufsize, pos, "DEC (HL)");
            } else {
                pos = emit(buf, bufsize, pos, "DEC %s", reg_name(y, prefix));
            }
            break;
        case 6: // LD r,n
            if (y == 6 && prefix) {
                int8_t d = (int8_t)operands[0];
                uint8_t n = operands[1];
                *extra_bytes = 2;
                pos = emit(buf, bufsize, 0, "LD ");
                pos = emit_indexed(buf, bufsize, pos, prefix, d);
                pos = emit(buf, bufsize, pos, ",$%02X", n);
            } else if (y == 6) {
                *extra_bytes = 1;
                pos = emit(buf, bufsize, pos, "LD (HL),$%02X", operands[0]);
            } else {
                *extra_bytes = 1;
                pos = emit(buf, bufsize, pos, "LD %s,$%02X", reg_name(y, prefix), operands[0]);
            }
            break;
        case 7: {
            static const char* misc_names[8] = {
                "RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF"
            };
            pos = emit(buf, bufsize, pos, "%s", misc_names[y]);
            break;
        }
        }
        break;

    case 1: // LD block, HALT
        if (z == 6 && y == 6) {
            pos = emit(buf, bufsize, pos, "HALT");
        } else if (z == 6) {
            // LD r,(HL)/(IX+d)/(IY+d)
            if (prefix) {
                int8_t d = (int8_t)operands[0];
                *extra_bytes = 1;
                pos = emit(buf, bufsize, 0, "LD %s,", r_names[y]);
                pos = emit_indexed(buf, bufsize, pos, prefix, d);
            } else {
                pos = emit(buf, bufsize, pos, "LD %s,(HL)", r_names[y]);
            }
        } else if (y == 6) {
            // LD (HL)/(IX+d)/(IY+d),r
            if (prefix) {
                int8_t d = (int8_t)operands[0];
                *extra_bytes = 1;
                pos = emit(buf, bufsize, 0, "LD ");
                pos = emit_indexed(buf, bufsize, pos, prefix, d);
                pos = emit(buf, bufsize, pos, ",%s", r_names[z]);
            } else {
                pos = emit(buf, bufsize, pos, "LD (HL),%s", r_names[z]);
            }
        } else {
            // LD r,r' — IX/IY only substitutes H/L in dst and src
            pos = emit(buf, bufsize, pos, "LD %s,%s",
                       reg_name(y, prefix), reg_name(z, prefix));
        }
        break;

    case 2: // ALU A,r
        if (z == 6) {
            // ALU A,(HL)/(IX+d)/(IY+d)
            if (prefix) {
                int8_t d = (int8_t)operands[0];
                *extra_bytes = 1;
                pos = emit(buf, bufsize, 0, "%s", alu_names[y]);
                pos = emit_indexed(buf, bufsize, pos, prefix, d);
            } else {
                pos = emit(buf, bufsize, pos, "%s(HL)", alu_names[y]);
            }
        } else {
            pos = emit(buf, bufsize, pos, "%s%s", alu_names[y], reg_name(z, prefix));
        }
        break;

    case 3:
        switch (z) {
        case 0: // RET cc
            pos = emit(buf, bufsize, pos, "RET %s", cc_names[y]);
            break;
        case 1:
            if (q == 0) {
                pos = emit(buf, bufsize, pos, "POP %s", rp2_name(p, prefix));
            } else {
                switch (p) {
                case 0: pos = emit(buf, bufsize, pos, "RET"); break;
                case 1: pos = emit(buf, bufsize, pos, "EXX"); break;
                case 2: pos = emit(buf, bufsize, pos, "JP (%s)",
                                   prefix == 0xDD ? "IX" : (prefix == 0xFD ? "IY" : "HL")); break;
                case 3: pos = emit(buf, bufsize, pos, "LD SP,%s",
                                   prefix == 0xDD ? "IX" : (prefix == 0xFD ? "IY" : "HL")); break;
                }
            }
            break;
        case 2: { // JP cc,nn
            uint16_t nn = operands[0] | (operands[1] << 8);
            *extra_bytes = 2;
            pos = emit(buf, bufsize, pos, "JP %s,$%04X", cc_names[y], nn);
            break;
        }
        case 3:
            switch (y) {
            case 0: {
                uint16_t nn = operands[0] | (operands[1] << 8);
                *extra_bytes = 2;
                pos = emit(buf, bufsize, pos, "JP $%04X", nn);
                break;
            }
            case 1: // CB prefix — should not reach here in base decode
                pos = emit(buf, bufsize, pos, "CB PREFIX");
                break;
            case 2: {
                *extra_bytes = 1;
                pos = emit(buf, bufsize, pos, "OUT ($%02X),A", operands[0]);
                break;
            }
            case 3: {
                *extra_bytes = 1;
                pos = emit(buf, bufsize, pos, "IN A,($%02X)", operands[0]);
                break;
            }
            case 4:
                pos = emit(buf, bufsize, pos, "EX (SP),%s",
                           prefix == 0xDD ? "IX" : (prefix == 0xFD ? "IY" : "HL"));
                break;
            case 5: pos = emit(buf, bufsize, pos, "EX DE,HL"); break;
            case 6: pos = emit(buf, bufsize, pos, "DI"); break;
            case 7: pos = emit(buf, bufsize, pos, "EI"); break;
            }
            break;
        case 4: { // CALL cc,nn
            uint16_t nn = operands[0] | (operands[1] << 8);
            *extra_bytes = 2;
            pos = emit(buf, bufsize, pos, "CALL %s,$%04X", cc_names[y], nn);
            break;
        }
        case 5:
            if (q == 0) {
                pos = emit(buf, bufsize, pos, "PUSH %s", rp2_name(p, prefix));
            } else {
                switch (p) {
                case 0: {
                    uint16_t nn = operands[0] | (operands[1] << 8);
                    *extra_bytes = 2;
                    pos = emit(buf, bufsize, pos, "CALL $%04X", nn);
                    break;
                }
                case 1: // DD prefix
                case 2: // ED prefix
                case 3: // FD prefix — should not reach here
                    pos = emit(buf, bufsize, pos, "PREFIX");
                    break;
                }
            }
            break;
        case 6: { // ALU A,n
            *extra_bytes = 1;
            pos = emit(buf, bufsize, pos, "%s$%02X", alu_names[y], operands[0]);
            break;
        }
        case 7: // RST
            pos = emit(buf, bufsize, pos, "RST $%02X", y * 8);
            break;
        }
        break;
    }

    return pos;
}

// ============================================================================
// PUBLIC API
// ============================================================================

int z80_disassemble(uint16_t pc, const uint8_t* memory, char* buffer, size_t buf_size) {
    if (!buffer || buf_size < 8 || !memory) return 1;

    int offset = 0;
    uint8_t prefix = 0;

    // Consume DD/FD prefix(es) — last one wins
    while (memory[offset] == 0xDD || memory[offset] == 0xFD) {
        prefix = memory[offset];
        offset++;
    }

    uint8_t op = memory[offset];
    offset++;

    if (op == 0xCB) {
        if (prefix) {
            // DD CB dd xx / FD CB dd xx
            int8_t d = static_cast<int8_t>(memory[offset]);
            uint8_t cbop = memory[offset + 1];
            decode_ddfd_cb(buffer, buf_size, prefix, d, cbop);
            return offset + 2;
        } else {
            // CB xx
            uint8_t cbop = memory[offset];
            decode_cb(buffer, buf_size, cbop);
            return offset + 1;
        }
    }

    if (op == 0xED) {
        // ED cancels DD/FD prefix
        uint8_t edop = memory[offset];
        int extra = 0;
        decode_ed(buffer, buf_size, edop, memory + offset + 1, &extra);
        return offset + 1 + extra;
    }

    // Base opcode (with optional DD/FD prefix)
    int extra = 0;
    int prefix_len = prefix ? (offset - 1) : 0;
    decode_base(buffer, buf_size, op, prefix, memory + offset, &extra, pc, prefix_len);
    return offset + extra;
}

int z80_disassemble_monitor(uint16_t pc, const uint8_t* memory, char* buffer, size_t buf_size) {
    if (!buffer || buf_size < 16 || !memory) return 1;

    // First disassemble to get length
    char disasm[48];
    int len = z80_disassemble(pc, memory, disasm, sizeof(disasm));

    // Format: "XXXX  HH HH HH HH  MNEMONIC"
    int pos = snprintf(buffer, buf_size, "%04X  ", pc);

    // Hex bytes
    for (int i = 0; i < len && i < 4; i++) {
        pos += snprintf(buffer + pos, buf_size - pos, "%02X ", memory[i]);
    }
    // Pad to fixed width (4 bytes * 3 chars = 12)
    for (int i = len; i < 4; i++) {
        pos += snprintf(buffer + pos, buf_size - pos, "   ");
    }
    pos += snprintf(buffer + pos, buf_size - pos, " %s", disasm);

    return len;
}

int z80_instruction_length(const uint8_t* memory) {
    if (!memory) return 1;

    int offset = 0;
    uint8_t prefix = 0;

    while (memory[offset] == 0xDD || memory[offset] == 0xFD) {
        prefix = memory[offset];
        offset++;
    }

    uint8_t op = memory[offset];
    offset++;

    if (op == 0xCB) {
        if (prefix) return offset + 2; // DD CB dd xx
        return offset + 1;             // CB xx
    }

    if (op == 0xED) {
        uint8_t edop = memory[offset];
        uint8_t x = (edop >> 6) & 3;
        uint8_t z = edop & 7;
        // ED instructions with 16-bit immediate: LD (nn),rr / LD rr,(nn)
        if (x == 1 && z == 3) return offset + 3; // +2 for nn
        return offset + 1; // Most ED ops: opcode only
    }

    // Base opcode length calculation:
    uint8_t x = (op >> 6) & 3;
    uint8_t y = (op >> 3) & 7;
    uint8_t z = op & 7;
    uint8_t p = y >> 1;
    uint8_t q = y & 1;

    switch (x) {
    case 0:
        switch (z) {
        case 0:
            if (y >= 2) return offset + 1; // DJNZ d, JR d, JR cc,d
            return offset;
        case 1:
            if (q == 0) return offset + 2; // LD rr,nn
            return offset;
        case 2:
            if (p == 2 || p == 3) return offset + 2; // LD (nn),HL/A, LD HL/A,(nn)
            return offset;
        case 3: return offset;
        case 4: // INC r
            if (y == 6 && prefix) return offset + 1; // INC (IX+d)
            return offset;
        case 5: // DEC r
            if (y == 6 && prefix) return offset + 1;
            return offset;
        case 6: // LD r,n
            if (y == 6 && prefix) return offset + 2; // LD (IX+d),n
            return offset + 1;
        case 7: return offset;
        }
        break;
    case 1:
        // (HL) with prefix adds displacement byte
        if ((z == 6 || y == 6) && prefix && !(z == 6 && y == 6)) return offset + 1;
        return offset;
    case 2:
        if (z == 6 && prefix) return offset + 1;
        return offset;
    case 3:
        switch (z) {
        case 0: return offset;
        case 1: return offset;
        case 2: return offset + 2; // JP cc,nn
        case 3:
            if (y == 0) return offset + 2; // JP nn
            if (y == 2 || y == 3) return offset + 1; // OUT/IN
            return offset;
        case 4: return offset + 2; // CALL cc,nn
        case 5:
            if (q == 0) return offset;
            if (p == 0) return offset + 2; // CALL nn
            return offset;
        case 6: return offset + 1; // ALU A,n
        case 7: return offset;
        }
        break;
    }
    return offset;
}
