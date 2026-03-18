// m680x0_ops.inc.hpp — Instruction operation handlers (included mid-class)
//
// Provides stub implementations for all 68000 instruction group decoders.
// Each group corresponds to bits 15-12 of the instruction word.
// These will be expanded with full cycle-accurate implementations.
//
// Must be included inside m680x0_t class body with M680X0_TEMPLATE_CONTEXT defined.

#include "chip/cpu/m680x0/operations/inc_lint_prevention.hpp"

// ════════════════════════════════════════════════════════════════════
// Effective Address evaluation
// ════════════════════════════════════════════════════════════════════

// calc_ea: compute the effective address for a given mode/register/size.
// For register-direct modes, returns the register index.
// For memory modes, returns the memory address.
// NOTE: Extension words (displacement, index) consume prefetch — the
//       caller must arrange bus cycles for those reads.

inline uint32_t calc_ea(uint8_t mode, uint8_t reg, OpSize sz) {
    switch (static_cast<EAMode>(mode)) {
        case EAMode::DataRegDirect:
            return reg;  // Index into d[]
        case EAMode::AddrRegDirect:
            return reg;  // Index into a[]
        case EAMode::AddrRegIndirect:
            return get_a(reg);
        case EAMode::AddrRegPostInc: {
            uint32_t addr = get_a(reg);
            uint32_t inc = size_bytes(sz);
            // Stack pointer (A7) always stays word-aligned
            if (reg == 7 && sz == OpSize::Byte) inc = 2;
            set_a(reg, addr + inc);
            return addr;
        }
        case EAMode::AddrRegPreDec: {
            uint32_t dec = size_bytes(sz);
            if (reg == 7 && sz == OpSize::Byte) dec = 2;
            set_a(reg, get_a(reg) - dec);
            return get_a(reg);
        }
        case EAMode::AddrRegDisp: {
            // d16 from IRC (already prefetched)
            int16_t disp = static_cast<int16_t>(regs_.irc);
            regs_.pc += 2;  // Consume extension word
            return get_a(reg) + disp;
        }
        case EAMode::AddrRegIndex: {
            // Brief extension word from IRC
            uint16_t ext = regs_.irc;
            regs_.pc += 2;
            uint8_t  xn_reg  = (ext >> 12) & 7;
            bool     xn_is_a = (ext & 0x8000) != 0;
            bool     xn_long = (ext & 0x0800) != 0;
            int8_t   disp8   = static_cast<int8_t>(ext & 0xFF);
            int32_t  xn_val;
            if (xn_is_a) {
                xn_val = static_cast<int32_t>(get_a(xn_reg));
            } else {
                xn_val = xn_long ? static_cast<int32_t>(get_d(xn_reg))
                                 : static_cast<int32_t>(static_cast<int16_t>(get_d_w(xn_reg)));
            }
            return get_a(reg) + disp8 + xn_val;
        }
        case EAMode::Special:
            switch (reg) {
                case 0: {  // Abs.W
                    int16_t addr = static_cast<int16_t>(regs_.irc);
                    regs_.pc += 2;
                    return static_cast<uint32_t>(static_cast<int32_t>(addr));
                }
                case 1: {  // Abs.L
                    uint32_t addr = static_cast<uint32_t>(regs_.irc) << 16;
                    regs_.pc += 2;
                    // Need another prefetch for low word — simplified here
                    addr |= regs_.irc;
                    regs_.pc += 2;
                    return addr;
                }
                case 2: {  // (d16,PC)
                    uint32_t base = regs_.pc;
                    int16_t disp = static_cast<int16_t>(regs_.irc);
                    regs_.pc += 2;
                    return base + disp;
                }
                case 3: {  // (d8,PC,Xn)
                    uint32_t base = regs_.pc;
                    uint16_t ext = regs_.irc;
                    regs_.pc += 2;
                    uint8_t  xn_reg  = (ext >> 12) & 7;
                    bool     xn_is_a = (ext & 0x8000) != 0;
                    bool     xn_long = (ext & 0x0800) != 0;
                    int8_t   disp8   = static_cast<int8_t>(ext & 0xFF);
                    int32_t  xn_val;
                    if (xn_is_a) {
                        xn_val = static_cast<int32_t>(get_a(xn_reg));
                    } else {
                        xn_val = xn_long ? static_cast<int32_t>(get_d(xn_reg))
                                         : static_cast<int32_t>(static_cast<int16_t>(get_d_w(xn_reg)));
                    }
                    return base + disp8 + xn_val;
                }
                case 4: {  // #imm
                    // Immediate: data from IRC
                    return 0;  // read_ea handles immediate differently
                }
                default:
                    return 0;
            }
    }
    return 0;
}

// read_ea / write_ea — simplified synchronous versions
// In the full implementation these will use bus cycle handlers

inline uint32_t read_ea(uint8_t mode, uint8_t reg, OpSize sz) {
    if (mode == static_cast<uint8_t>(EAMode::DataRegDirect)) {
        return read_dn(reg, sz);
    }
    if (mode == static_cast<uint8_t>(EAMode::AddrRegDirect)) {
        return get_a(reg);
    }
    if (mode == static_cast<uint8_t>(EAMode::Special) && reg == 4) {
        // Immediate
        if (sz == OpSize::Long) {
            uint32_t val = static_cast<uint32_t>(regs_.irc) << 16;
            regs_.pc += 2;
            val |= regs_.irc;
            regs_.pc += 2;
            return val;
        } else {
            uint32_t val = regs_.irc;
            regs_.pc += 2;
            return val & size_mask(sz);
        }
    }
    // Memory modes — calculate address, then the bus cycle reads the value
    ea_addr_ = calc_ea(mode, reg, sz);
    // Actual memory read happens through bus cycles — return 0 as placeholder
    return data_latch_ & size_mask(sz);
}

inline void write_ea(uint8_t mode, uint8_t reg, uint32_t value, OpSize sz) {
    if (mode == static_cast<uint8_t>(EAMode::DataRegDirect)) {
        write_dn(reg, value, sz);
        return;
    }
    if (mode == static_cast<uint8_t>(EAMode::AddrRegDirect)) {
        set_a(reg, value);
        return;
    }
    // Memory write — the actual bus cycle is handled by the caller
    ea_addr_ = calc_ea(mode, reg, sz);
    data_latch_ = value;
}

// ════════════════════════════════════════════════════════════════════
// Group decoders — stub implementations
// ════════════════════════════════════════════════════════════════════
//
// Each decoder parses the instruction word and performs the operation.
// For now, most just advance to the next prefetch (NOP behavior).
// They will be expanded with full implementations.

// ── Group 0: Bit manipulation / MOVEP / Immediate ───────────────
inline bus_state_t decode_group0(bus_state_t pins, uint16_t opcode) {
    // ORI, ANDI, EORI, SUBI, ADDI, CMPI, BTST, BCHG, BCLR, BSET, MOVEP
    // TODO: implement
    transition_to_prefetch();
    return pins;
}

// ── Groups 1/2/3: MOVE ──────────────────────────────────────────
inline bus_state_t decode_move(bus_state_t pins, uint16_t opcode, OpSize sz) {
    // MOVE.B / MOVE.W / MOVE.L  — bits 11-6 = dst, bits 5-0 = src
    uint8_t src_mode = instr_ea_mode(opcode);
    uint8_t src_reg  = instr_ea_reg(opcode);
    uint8_t dst_reg  = (opcode >> 9) & 7;
    uint8_t dst_mode = (opcode >> 6) & 7;

    uint32_t value = read_ea(src_mode, src_reg, sz);
    write_ea(dst_mode, dst_reg, value, sz);

    // MOVE sets N, Z, clears V and C
    alu_tst(value, sz);

    transition_to_prefetch();
    return pins;
}

// ── Group 4: Miscellaneous ──────────────────────────────────────
inline bus_state_t decode_group4(bus_state_t pins, uint16_t opcode) {
    // NEG, NOT, CLR, TST, NEGX, MOVEM, LEA, PEA, CHK, EXT, SWAP,
    // NOP, STOP, RTE, RTR, RTS, JSR, JMP, TRAP, LINK, UNLK, etc.

    // Quick decode of common instructions by bits 11-6
    uint8_t bits_11_6 = (opcode >> 6) & 0x3F;

    // NOP: 0100_1110_0111_0001 = $4E71
    if (opcode == 0x4E71) {
        transition_to_prefetch();
        return pins;
    }

    // RTS: 0100_1110_0111_0101 = $4E75
    if (opcode == 0x4E75) {
        // Pop PC from stack
        // TODO: bus cycles for stack read
        transition_to_prefetch();
        return pins;
    }

    // RTE: 0100_1110_0111_0011 = $4E73
    if (opcode == 0x4E73) {
        if (!(regs_.sr & SRBits::S)) {
            return exception(pins, Vector::PRIVILEGE_VIOLATION);
        }
        // TODO: restore SR and PC from stack
        transition_to_prefetch();
        return pins;
    }

    // TRAP #vector: 0100_1110_0100_vvvv
    if ((opcode & 0xFFF0) == 0x4E40) {
        uint8_t trap_vec = opcode & 0x0F;
        return exception(pins, Vector::TRAP_BASE + trap_vec);
    }

    // TODO: remaining group 4 instructions
    transition_to_prefetch();
    return pins;
}

// ── Group 5: ADDQ / SUBQ / Scc / DBcc ──────────────────────────
inline bus_state_t decode_group5(bus_state_t pins, uint16_t opcode) {
    uint8_t quick_data = (opcode >> 9) & 7;
    if (quick_data == 0) quick_data = 8;  // 0 encodes 8

    bool is_sub = (opcode & 0x0100) != 0;
    uint8_t size_field = (opcode >> 6) & 3;

    if (size_field == 3) {
        // DBcc or Scc
        // TODO: implement
        transition_to_prefetch();
        return pins;
    }

    OpSize sz = static_cast<OpSize>(size_field);
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);

    uint32_t src = quick_data;
    uint32_t dst = read_ea(ea_mode, ea_reg, sz);
    uint32_t result;

    if (is_sub) {
        result = alu_sub(src, dst, sz);
    } else {
        result = alu_add(src, dst, sz);
    }
    write_ea(ea_mode, ea_reg, result, sz);

    transition_to_prefetch();
    return pins;
}

// ── Group 6: Bcc / BSR / BRA ────────────────────────────────────
inline bus_state_t decode_group6(bus_state_t pins, uint16_t opcode) {
    auto cc = static_cast<Condition>((opcode >> 8) & 0x0F);
    int8_t disp8 = static_cast<int8_t>(opcode & 0xFF);

    int32_t displacement;
    if (disp8 == 0) {
        // Word displacement from extension word
        displacement = static_cast<int16_t>(regs_.irc);
        regs_.pc += 2;
    } else if constexpr (has_long_branch()) {
        if (disp8 == -1) {
            // Long displacement (68020+) from two extension words
            displacement = static_cast<int32_t>(
                (static_cast<uint32_t>(regs_.irc) << 16)); // high word
            regs_.pc += 2;
            displacement |= regs_.irc;  // low word
            regs_.pc += 2;
        } else {
            displacement = disp8;
        }
    } else {
        displacement = disp8;
    }

    // BRA (cc = T, but opcode convention: cc=0 is BRA, cc=1 is BSR)
    if (cc == Condition::T) {
        // BRA — always branch
        regs_.pc = regs_.pc + displacement - 2;  // -2 because PC already advanced past opcode
        transition_to_prefetch();
        return pins;
    }
    if (cc == Condition::F) {
        // BSR — branch to subroutine
        // Push return address
        regs_.a[7] -= 4;
        // TODO: write PC to stack via bus cycles
        regs_.pc = regs_.pc + displacement - 2;
        transition_to_prefetch();
        return pins;
    }

    // Bcc — conditional branch
    if (test_condition(cc)) {
        regs_.pc = regs_.pc + displacement - 2;
    }
    transition_to_prefetch();
    return pins;
}

// ── Group 7: MOVEQ ─────────────────────────────────────────────
inline bus_state_t decode_moveq(bus_state_t pins, uint16_t opcode) {
    uint8_t reg = (opcode >> 9) & 7;
    int8_t  imm = static_cast<int8_t>(opcode & 0xFF);
    uint32_t value = static_cast<uint32_t>(static_cast<int32_t>(imm));
    set_d(reg, value);

    // Set N, Z; clear V, C
    uint8_t ccr = get_ccr() & Flags::X;
    if (value == 0)                  ccr |= Flags::Z;
    if (value & 0x80000000)          ccr |= Flags::N;
    set_ccr(ccr);

    transition_to_prefetch();
    return pins;
}

// ── Group 8: OR / DIV / SBCD ────────────────────────────────────
inline bus_state_t decode_group8(bus_state_t pins, uint16_t opcode) {
    // TODO: implement OR, DIVU, DIVS, SBCD
    transition_to_prefetch();
    return pins;
}

// ── Group 9: SUB / SUBA / SUBX ─────────────────────────────────
inline bus_state_t decode_group9(bus_state_t pins, uint16_t opcode) {
    // TODO: implement SUB, SUBA, SUBX
    transition_to_prefetch();
    return pins;
}

// ── Group B: CMP / CMPA / CMPM / EOR ───────────────────────────
inline bus_state_t decode_groupB(bus_state_t pins, uint16_t opcode) {
    // TODO: implement CMP, CMPA, CMPM, EOR
    transition_to_prefetch();
    return pins;
}

// ── Group C: AND / MUL / ABCD / EXG ────────────────────────────
inline bus_state_t decode_groupC(bus_state_t pins, uint16_t opcode) {
    // TODO: implement AND, MULU, MULS, ABCD, EXG
    transition_to_prefetch();
    return pins;
}

// ── Group D: ADD / ADDA / ADDX ─────────────────────────────────
inline bus_state_t decode_groupD(bus_state_t pins, uint16_t opcode) {
    // TODO: implement ADD, ADDA, ADDX
    transition_to_prefetch();
    return pins;
}

// ── Group E: Shift / Rotate ────────────────────────────────────
inline bus_state_t decode_groupE(bus_state_t pins, uint16_t opcode) {
    // TODO: implement ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR
    transition_to_prefetch();
    return pins;
}

#include "chip/cpu/m680x0/operations/inc_lint_prevention_footer.hpp"
