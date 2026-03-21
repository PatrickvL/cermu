// m680x0_arith_ops.inc.hpp — Arithmetic, logic, and shift instruction handlers
//
// Group 0 (bit manipulation / MOVEP / immediate: ORI, ANDI, SUBI, ADDI, EORI, CMPI, BTST, …)
// Group 5 (ADDQ / SUBQ / Scc / DBcc)
// Group 8 (OR / DIVU / DIVS / SBCD)
// Group 9 (SUB / SUBA / SUBX)
// Group B (CMP / CMPA / CMPM / EOR)
// Group C (AND / MULU / MULS / ABCD / EXG)
// Group D (ADD / ADDA / ADDX)
// Group E (shift / rotate: ASL, ASR, LSL, LSR, ROL, ROR, ROXL, ROXR)
//
// Included inside m680x0_t class body with M680X0_TEMPLATE_CONTEXT defined.

#include "chip/cpu/m680x0/operations/inc_lint_prevention.hpp"

// ── Group 0: Bit manipulation / MOVEP / Immediate ───────────────
inline bus_state_t decode_group0(bus_state_t pins, uint16_t opcode) {
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);

    // BTST/BCHG/BCLR/BSET with dynamic bit# (Dn) — 0000 rrr1 xxmm mrrr
    // Also MOVEP: 0000 rrr1 xx00 1rrr (ea_mode == 1)
    if (opcode & 0x0100) {
        uint8_t dn = (opcode >> 9) & 7;

        // MOVEP: ea_mode == 1 (address register)
        if (ea_mode == 1) {
            uint8_t opmode = (opcode >> 6) & 3;
            int16_t disp = static_cast<int16_t>(consume_extension_word());
            uint32_t base = get_a(ea_reg) + disp;
            if (mem_read_ && mem_write_) {
                if (opmode <= 1) {
                    // MOVEP memory→Dn: read bytes from alternating addresses
                    auto read_byte = [&](uint32_t addr) -> uint8_t {
                        uint32_t a = addr & address_mask();
                        uint16_t word = mem_read_(mem_ctx_, a & ~1u);
                        clocks_remaining_ += 4;
                        return (a & 1) ? static_cast<uint8_t>(word) : static_cast<uint8_t>(word >> 8);
                    };
                    if (opmode == 0) {
                        // MOVEP.w (d16,An),Dn — 16 clocks
                        uint8_t hi = read_byte(base);
                        uint8_t lo = read_byte(base + 2);
                        set_d_w(dn, static_cast<uint16_t>((hi << 8) | lo));
                    } else {
                        // MOVEP.l (d16,An),Dn — 24 clocks
                        uint8_t b0 = read_byte(base);
                        uint8_t b1 = read_byte(base + 2);
                        uint8_t b2 = read_byte(base + 4);
                        uint8_t b3 = read_byte(base + 6);
                        set_d(dn, (static_cast<uint32_t>(b0) << 24) | (static_cast<uint32_t>(b1) << 16) |
                                  (static_cast<uint32_t>(b2) << 8) | b3);
                    }
                } else {
                    // MOVEP Dn→memory: write bytes to alternating addresses
                    auto write_byte = [&](uint32_t addr, uint8_t val) {
                        uint32_t a = addr & address_mask();
                        uint32_t even = a & ~1u;
                        uint16_t existing = mem_read_(mem_ctx_, even);
                        uint16_t word = (a & 1) ? ((existing & 0xFF00) | val)
                                                : ((val << 8) | (existing & 0x00FF));
                        mem_write_(mem_ctx_, even, word);
                        clocks_remaining_ += 4;
                    };
                    if (opmode == 2) {
                        // MOVEP.w Dn,(d16,An) — 16 clocks
                        uint16_t val = get_d_w(dn);
                        write_byte(base, static_cast<uint8_t>(val >> 8));
                        write_byte(base + 2, static_cast<uint8_t>(val));
                    } else {
                        // MOVEP.l Dn,(d16,An) — 24 clocks
                        uint32_t val = get_d(dn);
                        write_byte(base, static_cast<uint8_t>(val >> 24));
                        write_byte(base + 2, static_cast<uint8_t>(val >> 16));
                        write_byte(base + 4, static_cast<uint8_t>(val >> 8));
                        write_byte(base + 6, static_cast<uint8_t>(val));
                    }
                }
            }
            return do_prefetch(pins);
        }

        uint8_t op_type = (opcode >> 6) & 3;  // 0=BTST, 1=BCHG, 2=BCLR, 3=BSET
        uint8_t bit_num;

        if (ea_mode == 0) {
            // Dn — bit number modulo 32
            bit_num = get_d(dn) & 31;
            uint32_t val = get_d(ea_reg);
            alu_btst(val, bit_num);
            switch (op_type) {
                case 0: break;  // BTST — just test
                case 1: set_d(ea_reg, val ^ (1u << bit_num)); break;  // BCHG
                case 2: set_d(ea_reg, val & ~(1u << bit_num)); break; // BCLR
                case 3: set_d(ea_reg, val | (1u << bit_num)); break;  // BSET
            }
            // Dn bit ops timing depends on operation and bit position:
            // BTST Dn: always 6; BCHG/BSET Dn: 6|8; BCLR Dn: 8|10
            uint8_t idle;
            if (op_type == 0)      idle = 2;                                // BTST: always 6 clk
            else if (op_type == 2) idle = (bit_num >= 16) ? 6 : 4;         // BCLR: 10|8 clk
            else                   idle = (bit_num >= 16) ? 4 : 2;         // BCHG/BSET: 8|6 clk
            return do_idle_then_prefetch(pins, idle);
        }
        // Memory modes: bit number modulo 8, operate on byte
        bit_num = get_d(dn) & 7;
        uint32_t val = read_ea(ea_mode, ea_reg, OpSize::Byte);
        alu_btst(val, bit_num);
        if (op_type != 0) {
            uint32_t result;
            switch (op_type) {
                case 1: result = val ^ (1u << bit_num); break;  // BCHG
                case 2: result = val & ~(1u << bit_num); break; // BCLR
                case 3: result = val | (1u << bit_num); break;  // BSET
                default: result = val; break;
            }
            write_back_ea(result, OpSize::Byte);
        }
        return do_prefetch(pins);
    }

    // Static bit operations or immediate — 0000 rrr0 xxmm mrrr
    uint8_t upper = (opcode >> 9) & 7;

    // BTST/BCHG/BCLR/BSET with static bit# (immediate) — upper == 4
    if (upper == 4) {
        uint8_t op_type = (opcode >> 6) & 3;
        // Bit number from extension word (IRC)
        uint8_t bit_num_raw = static_cast<uint8_t>(consume_extension_word() & 0xFF);

        if (ea_mode == 0) {
            uint8_t bit_num = bit_num_raw & 31;
            uint32_t val = get_d(ea_reg);
            alu_btst(val, bit_num);
            switch (op_type) {
                case 0: break;
                case 1: set_d(ea_reg, val ^ (1u << bit_num)); break;
                case 2: set_d(ea_reg, val & ~(1u << bit_num)); break;
                case 3: set_d(ea_reg, val | (1u << bit_num)); break;
            }
            // Static bit Dn timing: BTST 10, BCHG/BSET 12, BCLR 14
            uint8_t idle;
            if (op_type == 0)      idle = 2;   // BTST: 10 clk
            else if (op_type == 2) idle = 6;   // BCLR: 14 clk
            else                   idle = 4;   // BCHG/BSET: 12 clk
            return do_idle_then_prefetch(pins, idle);
        }
        // Memory modes: bit number modulo 8, operate on byte
        {
            uint8_t bit_num = bit_num_raw & 7;
            uint32_t val = read_ea(ea_mode, ea_reg, OpSize::Byte);
            alu_btst(val, bit_num);
            if (op_type != 0) {
                uint32_t result;
                switch (op_type) {
                    case 1: result = val ^ (1u << bit_num); break;
                    case 2: result = val & ~(1u << bit_num); break;
                    case 3: result = val | (1u << bit_num); break;
                    default: result = val; break;
                }
                write_back_ea(result, OpSize::Byte);
            }
            return do_prefetch(pins);
        }
    }

    // MOVEP: 0000 rrr1 0x00 1rrr — already handled above (bit 8 set)
    // Remaining: ORI, ANDI, SUBI, ADDI, EORI, CMPI
    // Encoding: 0000 xxx0 ssxx xxxx  where xxx = operation:
    //   000=ORI, 001=ANDI, 010=SUBI, 011=ADDI, 100=(static bit), 101=EORI, 110=CMPI

    // Special: ORI/ANDI/EORI to CCR (ea=0x3C) or SR (ea=0x7C)
    // These are matched before normal size/EA processing
    if ((upper == 0 || upper == 1 || upper == 5) &&
        ((opcode & 0xFF) == 0x3C || (opcode & 0xFF) == 0x7C)) {
        bool to_sr = (opcode & 0xFF) == 0x7C;
        if (to_sr && !(regs_.sr & SRBits::S))
            return exception(pins, Vector::PRIVILEGE_VIOLATION);
        uint16_t imm = consume_extension_word();
        if (to_sr) {
            switch (upper) {
                case 0: set_sr(regs_.sr | imm); break;
                case 1: set_sr(regs_.sr & imm); break;
                case 5: set_sr(regs_.sr ^ imm); break;
            }
        } else {
            uint8_t imm8 = static_cast<uint8_t>(imm);
            switch (upper) {
                case 0: set_ccr(get_ccr() | imm8); break;
                case 1: set_ccr(get_ccr() & imm8); break;
                case 5: set_ccr(get_ccr() ^ imm8); break;
            }
        }
        // ORI/ANDI/EORI to CCR/SR: 20 clocks total
        // consume_extension_word (4) + dummy re-read + internal (12) + prefetch (4) = 20
        clocks_remaining_ += 12;
        return do_prefetch(pins);
    }

    OpSize sz;
    uint8_t size_field = (opcode >> 6) & 3;
    if (size_field == 3) return do_prefetch(pins);  // Invalid size encoding

    sz = static_cast<OpSize>(size_field);

    // Read immediate value from prefetch
    uint32_t imm;
    if (sz == OpSize::Long) {
        imm = static_cast<uint32_t>(consume_extension_word()) << 16;
        imm |= consume_extension_word();
    } else {
        imm = consume_extension_word() & size_mask(sz);
    }

    // Read destination
    uint32_t dst;
    if (ea_mode == 0) {
        dst = read_dn(ea_reg, sz);
    } else {
        dst = read_ea(ea_mode, ea_reg, sz);
    }

    uint32_t result;
    switch (upper) {
        case 0:  // ORI
            result = alu_or(imm, dst, sz);
            break;
        case 1:  // ANDI
            result = alu_and(imm, dst, sz);
            break;
        case 2:  // SUBI
            result = alu_sub(imm, dst, sz);
            break;
        case 3:  // ADDI
            result = alu_add(imm, dst, sz);
            break;
        case 5:  // EORI
            result = alu_eor(imm, dst, sz);
            break;
        case 6:  // CMPI
            alu_cmp(imm, dst, sz);
            // CMPI.l #imm,Dn: 14 clocks (2 idle)
            if (ea_mode == 0 && sz == OpSize::Long) return do_idle_then_prefetch(pins, 2);
            return do_prefetch(pins);
        default:
            return do_prefetch(pins);
    }

    if (ea_mode == 0) {
        write_dn(ea_reg, result, sz);
        // ORI/ANDI/EORI/SUBI/ADDI .l #imm,Dn: 16 clocks (4 idle)
        if (sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
    } else {
        // ea_addr_ already set by read_ea — use write_back_ea to avoid
        // recalculating (which would double PostInc/PreDec side effects)
        write_back_ea(result, sz);
    }
    return do_prefetch(pins);
}

// ── Group 5: ADDQ / SUBQ / Scc / DBcc ──────────────────────────
inline bus_state_t decode_group5(bus_state_t pins, uint16_t opcode) {
    uint8_t quick_data = (opcode >> 9) & 7;
    if (quick_data == 0) quick_data = 8;  // 0 encodes 8

    uint8_t size_field = (opcode >> 6) & 3;
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);
    bool is_sub = (opcode & 0x0100) != 0;

    if (size_field == 3) {
        // Scc or DBcc
        if (ea_mode == 1) {
            // DBcc: 0101 cccc 1100 1rrr
            auto cc = static_cast<Condition>((opcode >> 8) & 0x0F);
            // displacement is relative to formal_opcode + 2 (= internal PC - 2)
            uint32_t branch_base = regs_.pc - 2;
            if (!test_condition(cc)) {
                // Condition false → decrement and branch
                int16_t dn = static_cast<int16_t>(get_d_w(ea_reg));
                dn--;
                set_d_w(ea_reg, static_cast<uint16_t>(dn));
                if (dn != -1) {
                    // Branch taken: 10 clocks (n np np)
                    int16_t disp = static_cast<int16_t>(regs_.irc);
                    regs_.pc = branch_base + disp;
                    clocks_remaining_ += 2;  // 2 idle clocks
                    return do_branch_prefetch(pins);
                } else {
                    // Counter expired: 14 clocks (nn np np)
                    // Consume displacement word (refill IRC) + idle + prefetch
                    consume_extension_word();
                    return do_idle_then_prefetch(pins, 6);
                }
            } else {
                // Condition true: 12 clocks (nn np np)
                // Consume displacement word (refill IRC) + idle + prefetch
                consume_extension_word();
                return do_idle_then_prefetch(pins, 4);
            }
        }
        // Scc: 0101 cccc 11xx xxxx
        auto cc = static_cast<Condition>((opcode >> 8) & 0x0F);
        bool cond = test_condition(cc);
        uint8_t val = cond ? 0xFF : 0x00;
        if (ea_mode == 0) {
            set_d_b(ea_reg, val);
            // Scc Dn: true → 6 clocks (2 idle), false → 4 clocks
            return cond ? do_idle_then_prefetch(pins, 2) : do_prefetch(pins);
        } else {
            // Memory: read-modify-write (dummy read + write)
            read_ea(ea_mode, ea_reg, OpSize::Byte);  // dummy read, sets ea_addr_
            write_back_ea(val, OpSize::Byte);
        }
        return do_prefetch(pins);
    }

    OpSize sz = static_cast<OpSize>(size_field);

    if (ea_mode == 1) {
        // ADDQ/SUBQ to An — full 32-bit, no flags affected, always 8 clocks
        uint32_t an = get_a(ea_reg);
        if (is_sub) an -= quick_data; else an += quick_data;
        set_a(ea_reg, an);
        return do_idle_then_prefetch(pins, 4);
    }

    uint32_t src = quick_data;
    uint32_t dst;
    if (ea_mode == 0) {
        dst = read_dn(ea_reg, sz);
    } else {
        dst = read_ea(ea_mode, ea_reg, sz);
    }

    uint32_t result;
    if (is_sub) {
        result = alu_sub(src, dst, sz);
    } else {
        result = alu_add(src, dst, sz);
    }

    if (ea_mode == 0) {
        write_dn(ea_reg, result, sz);
        // ADDQ/SUBQ Dn: .l → 8 clocks (4 idle)
        if (sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
    } else {
        // ea_addr_ already set by read_ea — use write_back_ea to avoid
        // recalculating (which would double PostInc/PreDec side effects)
        write_back_ea(result, sz);
    }
    return do_prefetch(pins);
}

// ── Group 8: OR / DIV / SBCD ────────────────────────────────────
inline bus_state_t decode_group8(bus_state_t pins, uint16_t opcode) {
    uint8_t dn = (opcode >> 9) & 7;
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);
    uint8_t opmode = (opcode >> 6) & 7;

    // SBCD: opmode 4, ea_mode 0 (Dn) or 1 (-(An))
    if (opmode == 4 && (ea_mode == 0 || ea_mode == 1)) {
        if (ea_mode == 0) {
            // SBCD Dn,Dn
            uint8_t result = alu_sbcd(get_d_b(ea_reg), get_d_b(dn));
            set_d_b(dn, result);
            return do_idle_then_prefetch(pins, 2);  // 6 clocks (2 idle + 4 prefetch)
        }
        // SBCD -(An),-(An) — 18 clocks
        // Use calc_ea + direct read to avoid predec idle penalties
        ea_addr_ = calc_ea(4, ea_reg, OpSize::Byte);
        uint8_t src = 0;
        if (mem_read_) {
            uint32_t a = ea_addr_ & address_mask();
            uint16_t w = mem_read_(mem_ctx_, a & ~1u);
            src = (a & 1) ? static_cast<uint8_t>(w) : static_cast<uint8_t>(w >> 8);
            clocks_remaining_ += 4;
        }
        ea_addr_ = calc_ea(4, dn, OpSize::Byte);
        uint8_t dst = 0;
        if (mem_read_) {
            uint32_t a = ea_addr_ & address_mask();
            uint16_t w = mem_read_(mem_ctx_, a & ~1u);
            dst = (a & 1) ? static_cast<uint8_t>(w) : static_cast<uint8_t>(w >> 8);
            clocks_remaining_ += 4;
        }
        uint8_t result = alu_sbcd(src, dst);
        write_back_ea(result, OpSize::Byte);
        clocks_remaining_ += 2;  // BCD computation idle
        return do_prefetch(pins);
    }

    // DIVU: opmode 3
    if (opmode == 3) {
        uint16_t src;
        if (ea_mode == 0) {
            src = get_d_w(ea_reg);
        } else {
            src = static_cast<uint16_t>(read_ea(ea_mode, ea_reg, OpSize::Word));
        }
        if (src == 0) {
            return exception(pins, Vector::ZERO_DIVIDE);
        }
        uint32_t dividend = get_d(dn);
        // Overflow: upper word >= divisor
        if ((dividend >> 16) >= src) {
            // Set V, clear C; N, Z, X unchanged. Destination unchanged.
            uint8_t ccr = (get_ccr() & (Flags::X | Flags::N | Flags::Z)) | Flags::V;
            set_ccr(ccr);
            return do_idle_then_prefetch(pins, 6);  // overflow: 10 total (6 idle + 4 prefetch)
        }
        uint16_t quotient, remainder;
        alu_divu(dividend, src, quotient, remainder);
        set_d(dn, (static_cast<uint32_t>(remainder) << 16) | quotient);
        return do_idle_then_prefetch(pins, divu_idle_clocks(dividend, src));
    }

    // DIVS: opmode 7
    if (opmode == 7) {
        int16_t src;
        if (ea_mode == 0) {
            src = static_cast<int16_t>(get_d_w(ea_reg));
        } else {
            src = static_cast<int16_t>(read_ea(ea_mode, ea_reg, OpSize::Word));
        }
        if (src == 0) {
            return exception(pins, Vector::ZERO_DIVIDE);
        }
        int32_t dividend = static_cast<int32_t>(get_d(dn));
        int32_t result = dividend / src;
        bool dst_neg = dividend < 0;
        bool src_neg = src < 0;
        if (result > 32767 || result < -32768) {
            // Overflow — V set, C cleared; N, Z, X unchanged. Destination unchanged.
            uint8_t ccr = (get_ccr() & (Flags::X | Flags::N | Flags::Z)) | Flags::V;
            set_ccr(ccr);
            uint8_t idle = dst_neg ? 14 : 12;  // 18 or 16 total
            return do_idle_then_prefetch(pins, idle);
        }
        int16_t quotient = static_cast<int16_t>(result);
        int16_t remainder = static_cast<int16_t>(dividend % src);
        // Set flags
        uint8_t ccr = get_ccr() & Flags::X;
        if (static_cast<uint16_t>(quotient) == 0) ccr |= Flags::Z;
        if (quotient < 0)                         ccr |= Flags::N;
        set_ccr(ccr);
        set_d(dn, (static_cast<uint32_t>(static_cast<uint16_t>(remainder)) << 16) |
                    static_cast<uint16_t>(quotient));
        // DIVS timing: DIVU loop on abs values + sign-dependent overhead
        uint32_t abs_dst = static_cast<uint32_t>(dividend < 0 ? -dividend : dividend);
        uint16_t abs_src = static_cast<uint16_t>(src < 0 ? -src : src);
        uint8_t base = 20 + (dst_neg ? (src_neg ? 4 : 6) : (src_neg ? 2 : 0));
        return do_idle_then_prefetch(pins, base + divu_loop_cost(abs_dst, abs_src));
    }

    // OR: opmodes 0,1,2 (<ea> OR Dn → Dn) and 4,5,6 (Dn OR <ea> → <ea>)
    OpSize sz;
    switch (opmode & 3) {
        case 0: sz = OpSize::Byte; break;
        case 1: sz = OpSize::Word; break;
        case 2: sz = OpSize::Long; break;
        default: return do_prefetch(pins);
    }

    if (opmode < 3) {
        // <ea> OR Dn → Dn
        uint32_t src_val;
        if (ea_mode == 0) {
            src_val = read_dn(ea_reg, sz);
        } else if (ea_mode == 7 && ea_reg == 4) {
            // Immediate: inline path
            src_val = read_ea(ea_mode, ea_reg, sz);
        } else {
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_OR;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_READ_TO_DN_LONG : BUS_READ_TO_DN;
            return begin_ea_read(pins, ea_mode);
        }
        uint32_t dst_val = read_dn(dn, sz);
        uint32_t result = alu_or(src_val, dst_val, sz);
        write_dn(dn, result, sz);
        // OR .l Dn,Dn: 8 clocks (4 idle)
        if (ea_mode <= 1 && sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
    } else {
        // Dn OR <ea> → <ea>
        if (ea_mode >= 2) {
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_OR;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
            return begin_ea_read(pins, ea_mode);
        }
        uint32_t src_val = read_dn(dn, sz);
        uint32_t dst_val = read_ea(ea_mode, ea_reg, sz);
        uint32_t result = alu_or(src_val, dst_val, sz);
        write_ea(ea_mode, ea_reg, result, sz);
    }
    return do_prefetch(pins);
}

// ── Group 9: SUB / SUBA / SUBX ─────────────────────────────────
inline bus_state_t decode_group9(bus_state_t pins, uint16_t opcode) {
    uint8_t dn = (opcode >> 9) & 7;
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);
    uint8_t opmode = (opcode >> 6) & 7;

    // SUBX: opmode 4 (byte), 5 (word), 6 (long) with ea_mode 0 or 1
    if ((opmode == 4 || opmode == 5 || opmode == 6) &&
        (ea_mode == 0 || ea_mode == 1)) {
        OpSize sz = static_cast<OpSize>(opmode - 4);
        if (ea_mode == 0) {
            // Dn - Dn
            uint32_t result = alu_subx(read_dn(ea_reg, sz), read_dn(dn, sz), sz);
            write_dn(dn, result, sz);
            // SUBX Dn .l: 8 clocks (4 idle)
            if (sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
            return do_prefetch(pins);
        }
        // SUBX -(An),-(An): .b/.w = 18 clocks, .l = 30 clocks
        // Use calc_ea + direct read to avoid predec idle penalties
        ea_addr_ = calc_ea(4, ea_reg, sz);
        uint32_t src = 0;
        if (mem_read_) {
            uint32_t addr = ea_addr_ & address_mask();
            if (sz == OpSize::Long) {
                src = (static_cast<uint32_t>(mem_read_(mem_ctx_, addr)) << 16) |
                       mem_read_(mem_ctx_, (addr + 2) & address_mask());
                clocks_remaining_ += 8;
            } else if (sz == OpSize::Byte) {
                uint16_t w = mem_read_(mem_ctx_, addr & ~1u);
                src = (addr & 1) ? (w & 0xFF) : (w >> 8);
                clocks_remaining_ += 4;
            } else {
                src = mem_read_(mem_ctx_, addr);
                clocks_remaining_ += 4;
            }
        }
        ea_addr_ = calc_ea(4, dn, sz);
        uint32_t dst = 0;
        if (mem_read_) {
            uint32_t addr = ea_addr_ & address_mask();
            if (sz == OpSize::Long) {
                dst = (static_cast<uint32_t>(mem_read_(mem_ctx_, addr)) << 16) |
                       mem_read_(mem_ctx_, (addr + 2) & address_mask());
                clocks_remaining_ += 8;
            } else if (sz == OpSize::Byte) {
                uint16_t w = mem_read_(mem_ctx_, addr & ~1u);
                dst = (addr & 1) ? (w & 0xFF) : (w >> 8);
                clocks_remaining_ += 4;
            } else {
                dst = mem_read_(mem_ctx_, addr);
                clocks_remaining_ += 4;
            }
        }
        uint32_t result = alu_subx(src, dst, sz);
        write_back_ea(result, sz);
        // SUBX -(An) .l: 30 clocks, .b/.w: 18 clocks (2 idle)
        clocks_remaining_ += 2;  // BCD/extend computation idle
        return do_prefetch(pins);
    }

    // SUBA: opmode 3 (word), 7 (long)
    if (opmode == 3 || opmode == 7) {
        OpSize src_sz = (opmode == 3) ? OpSize::Word : OpSize::Long;
        uint32_t src_val;
        if (ea_mode == 0) {
            src_val = read_dn(ea_reg, src_sz);
        } else if (ea_mode == 1) {
            src_val = get_a(ea_reg);
        } else {
            src_val = read_ea(ea_mode, ea_reg, src_sz);
        }
        // Word source is sign-extended to 32 bits
        if (src_sz == OpSize::Word) {
            src_val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(src_val)));
        }
        set_a(dn, get_a(dn) - src_val);
        // SUBA does not affect flags — always 8 clocks for register source
        if (ea_mode <= 1) return do_idle_then_prefetch(pins, 4);
        return do_prefetch(pins);
    }

    // SUB: opmodes 0,1,2 (<ea> - Dn → Dn) and 4,5,6 = Dn,<ea> → <ea> (if not SUBX)
    OpSize sz = static_cast<OpSize>(opmode & 3);
    if (opmode < 3) {
        // <ea> → Dn
        uint32_t src_val;
        if (ea_mode == 0) {
            src_val = read_dn(ea_reg, sz);
        } else if (ea_mode == 1) {
            src_val = get_a(ea_reg);
            if (sz == OpSize::Word)
                src_val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(src_val)));
        } else if (ea_mode == 7 && ea_reg == 4) {
            // Immediate: inline path
            src_val = read_ea(ea_mode, ea_reg, sz);
        } else {
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_SUB;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_READ_TO_DN_LONG : BUS_READ_TO_DN;
            return begin_ea_read(pins, ea_mode);
        }
        uint32_t result = alu_sub(src_val, read_dn(dn, sz), sz);
        write_dn(dn, result, sz);
        // SUB .l Dn,Dn: 8 clocks (4 idle)
        if (ea_mode <= 1 && sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
        return do_prefetch(pins);
    }
    // opmode 4,5,6: SUB Dn → <ea>
    if (opmode >= 4 && opmode <= 6) {
        sz = static_cast<OpSize>(opmode - 4);
        if (ea_mode >= 2) {
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_SUB;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
            return begin_ea_read(pins, ea_mode);
        }
    }
    return do_prefetch(pins);
}

// ── Group B: CMP / CMPA / CMPM / EOR ───────────────────────────
inline bus_state_t decode_groupB(bus_state_t pins, uint16_t opcode) {
    uint8_t dn = (opcode >> 9) & 7;
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);
    uint8_t opmode = (opcode >> 6) & 7;

    // CMPA: opmode 3 (word), 7 (long)
    if (opmode == 3 || opmode == 7) {
        OpSize src_sz = (opmode == 3) ? OpSize::Word : OpSize::Long;
        uint32_t src_val;
        if (ea_mode == 0) {
            src_val = read_dn(ea_reg, src_sz);
        } else if (ea_mode == 1) {
            src_val = get_a(ea_reg);
        } else {
            src_val = read_ea(ea_mode, ea_reg, src_sz);
        }
        if (src_sz == OpSize::Word) {
            src_val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(src_val)));
        }
        alu_cmp(src_val, get_a(dn), OpSize::Long);
        // CMPA: always 6 clocks for register source
        if (ea_mode <= 1) return do_idle_then_prefetch(pins, 2);
        return do_prefetch(pins);
    }

    // EOR: opmode 4,5,6 — Dn EOR <ea> → <ea>
    // CMPM: opmode 4,5,6 with ea_mode 1 (post-increment) → (An)+,(An)+
    if (opmode >= 4 && opmode <= 6) {
        OpSize sz = static_cast<OpSize>(opmode - 4);
        if (ea_mode == 1) {
            // CMPM: (Ay)+,(Ax)+
            // Read source from (Ay)+, then destination from (Ax)+
            uint32_t src = read_ea(3, ea_reg, sz);   // (ea_reg)+ as source
            uint32_t dst = read_ea(3, dn, sz);       // (dn)+ as destination
            alu_cmp(src, dst, sz);
            return do_prefetch(pins);
        }
        // EOR: Dn → <ea>
        uint32_t src_val = read_dn(dn, sz);
        if (ea_mode == 0) {
            uint32_t dst_val = read_dn(ea_reg, sz);
            uint32_t result = alu_eor(src_val, dst_val, sz);
            write_dn(ea_reg, result, sz);
            // EOR .l Dn,Dn: 8 clocks (4 idle)
            if (sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
        } else {
            // EOR Dn, <ea> — read-modify-write
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_EOR;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
            return begin_ea_read(pins, ea_mode);
        }
        return do_prefetch(pins);
    }

    // CMP: opmode 0,1,2 — <ea> - Dn (flags only, no write)
    if (opmode < 3) {
        OpSize sz = static_cast<OpSize>(opmode);
        uint32_t src_val;
        if (ea_mode == 0) {
            src_val = read_dn(ea_reg, sz);
        } else if (ea_mode == 1) {
            src_val = get_a(ea_reg);
            if (sz == OpSize::Word)
                src_val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(src_val)));
        } else if (ea_mode == 7 && ea_reg == 4) {
            // Immediate: inline path
            src_val = read_ea(ea_mode, ea_reg, sz);
        } else {
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_CMP;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_READ_TO_DN_LONG : BUS_READ_ONLY;
            return begin_ea_read(pins, ea_mode);
        }
        alu_cmp(src_val, read_dn(dn, sz), sz);
        // CMP .l Dn,Dn: 6 clocks (2 idle)
        if (ea_mode <= 1 && sz == OpSize::Long) return do_idle_then_prefetch(pins, 2);
    }
    return do_prefetch(pins);
}

// ── Group C: AND / MUL / ABCD / EXG ────────────────────────────
inline bus_state_t decode_groupC(bus_state_t pins, uint16_t opcode) {
    uint8_t dn = (opcode >> 9) & 7;
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);
    uint8_t opmode = (opcode >> 6) & 7;

    // ABCD: opmode 4, ea_mode 0 (Dn) or 1 (-(An))
    if (opmode == 4 && (ea_mode == 0 || ea_mode == 1)) {
        if (ea_mode == 0) {
            // ABCD Dn,Dn
            uint8_t result = alu_abcd(get_d_b(ea_reg), get_d_b(dn));
            set_d_b(dn, result);
            return do_idle_then_prefetch(pins, 2);  // 6 clocks (2 idle + 4 prefetch)
        }
        // ABCD -(An),-(An) — 18 clocks
        ea_addr_ = calc_ea(4, ea_reg, OpSize::Byte);
        uint8_t src = 0;
        if (mem_read_) {
            uint32_t a = ea_addr_ & address_mask();
            uint16_t w = mem_read_(mem_ctx_, a & ~1u);
            src = (a & 1) ? static_cast<uint8_t>(w) : static_cast<uint8_t>(w >> 8);
            clocks_remaining_ += 4;
        }
        ea_addr_ = calc_ea(4, dn, OpSize::Byte);
        uint8_t dst = 0;
        if (mem_read_) {
            uint32_t a = ea_addr_ & address_mask();
            uint16_t w = mem_read_(mem_ctx_, a & ~1u);
            dst = (a & 1) ? static_cast<uint8_t>(w) : static_cast<uint8_t>(w >> 8);
            clocks_remaining_ += 4;
        }
        uint8_t result = alu_abcd(src, dst);
        write_back_ea(result, OpSize::Byte);
        clocks_remaining_ += 2;  // BCD computation idle
        return do_prefetch(pins);
    }

    // MULU: opmode 3
    if (opmode == 3) {
        uint16_t src;
        if (ea_mode == 0) {
            src = get_d_w(ea_reg);
        } else {
            src = static_cast<uint16_t>(read_ea(ea_mode, ea_reg, OpSize::Word));
        }
        uint32_t result = alu_mulu(src, get_d_w(dn));
        set_d(dn, result);
        // MULU timing: 38 + 2*popcount(source_word) total clocks
        uint8_t idle = 34 + 2 * __builtin_popcount(src);
        return do_idle_then_prefetch(pins, idle);
    }

    // MULS: opmode 7
    if (opmode == 7) {
        uint16_t src_raw;
        if (ea_mode == 0) {
            src_raw = get_d_w(ea_reg);
        } else {
            src_raw = static_cast<uint16_t>(read_ea(ea_mode, ea_reg, OpSize::Word));
        }
        uint32_t result = alu_muls(static_cast<int16_t>(src_raw),
                                   static_cast<int16_t>(get_d_w(dn)));
        set_d(dn, result);
        // MULS timing: 38 + 2*popcount((src ^ (src<<1)) & 0xFFFF)
        uint16_t transitions = (src_raw ^ (src_raw << 1)) & 0xFFFF;
        uint8_t idle = 34 + 2 * __builtin_popcount(transitions);
        return do_idle_then_prefetch(pins, idle);
    }

    // EXG: 1100 rrr1 0100 0rrr (Dx,Dy), 1100 rrr1 0100 1rrr (Ax,Ay),
    //       1100 rrr1 1000 1rrr (Dx,Ay)
    // Note: checked AFTER MULU/MULS to avoid false matches (mask overlaps opmode 7)
    if ((opcode & 0xF130) == 0xC100) {
        uint8_t exg_mode = (opcode >> 3) & 0x1F;
        switch (exg_mode) {
            case 0x08: {  // Dx,Dy
                uint32_t tmp = get_d(dn);
                set_d(dn, get_d(ea_reg));
                set_d(ea_reg, tmp);
                break;
            }
            case 0x09: {  // Ax,Ay
                uint32_t tmp = get_a(dn);
                set_a(dn, get_a(ea_reg));
                set_a(ea_reg, tmp);
                break;
            }
            case 0x11: {  // Dx,Ay
                uint32_t tmp = get_d(dn);
                set_d(dn, get_a(ea_reg));
                set_a(ea_reg, tmp);
                break;
            }
            default: break;
        }
        // Sync A7 ↔ SSP/USP after exchange
        sync_sp();
        return do_idle_then_prefetch(pins, 2);  // EXG: 6 clocks (2 idle)
    }

    // AND: opmodes 0,1,2 (<ea> AND Dn → Dn) and 4,5,6 (Dn AND <ea> → <ea>)
    OpSize sz;
    switch (opmode & 3) {
        case 0: sz = OpSize::Byte; break;
        case 1: sz = OpSize::Word; break;
        case 2: sz = OpSize::Long; break;
        default: return do_prefetch(pins);
    }

    if (opmode < 3) {
        // <ea> AND Dn → Dn
        uint32_t src_val;
        if (ea_mode == 0) {
            src_val = read_dn(ea_reg, sz);
        } else if (ea_mode == 7 && ea_reg == 4) {
            // Immediate: inline path
            src_val = read_ea(ea_mode, ea_reg, sz);
        } else {
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_AND;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_READ_TO_DN_LONG : BUS_READ_TO_DN;
            return begin_ea_read(pins, ea_mode);
        }
        uint32_t result = alu_and(src_val, read_dn(dn, sz), sz);
        write_dn(dn, result, sz);
        // AND .l Dn,Dn: 8 clocks (4 idle)
        if (sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
    } else {
        // Dn AND <ea> → <ea>
        if (ea_mode >= 2) {
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_AND;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
            return begin_ea_read(pins, ea_mode);
        }
    }
    return do_prefetch(pins);
}

// ── Group D: ADD / ADDA / ADDX ─────────────────────────────────
inline bus_state_t decode_groupD(bus_state_t pins, uint16_t opcode) {
    uint8_t dn = (opcode >> 9) & 7;
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);
    uint8_t opmode = (opcode >> 6) & 7;

    // ADDX: opmode 4,5,6 with ea_mode 0 or 1
    if ((opmode == 4 || opmode == 5 || opmode == 6) &&
        (ea_mode == 0 || ea_mode == 1)) {
        OpSize sz = static_cast<OpSize>(opmode - 4);
        if (ea_mode == 0) {
            uint32_t result = alu_addx(read_dn(ea_reg, sz), read_dn(dn, sz), sz);
            write_dn(dn, result, sz);
            // ADDX Dn .l: 8 clocks (4 idle)
            if (sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
            return do_prefetch(pins);
        }
        // ADDX -(An),-(An): .b/.w = 18 clocks, .l = 30 clocks
        ea_addr_ = calc_ea(4, ea_reg, sz);
        uint32_t src = 0;
        if (mem_read_) {
            uint32_t addr = ea_addr_ & address_mask();
            if (sz == OpSize::Long) {
                src = (static_cast<uint32_t>(mem_read_(mem_ctx_, addr)) << 16) |
                       mem_read_(mem_ctx_, (addr + 2) & address_mask());
                clocks_remaining_ += 8;
            } else if (sz == OpSize::Byte) {
                uint16_t w = mem_read_(mem_ctx_, addr & ~1u);
                src = (addr & 1) ? (w & 0xFF) : (w >> 8);
                clocks_remaining_ += 4;
            } else {
                src = mem_read_(mem_ctx_, addr);
                clocks_remaining_ += 4;
            }
        }
        ea_addr_ = calc_ea(4, dn, sz);
        uint32_t dst = 0;
        if (mem_read_) {
            uint32_t addr = ea_addr_ & address_mask();
            if (sz == OpSize::Long) {
                dst = (static_cast<uint32_t>(mem_read_(mem_ctx_, addr)) << 16) |
                       mem_read_(mem_ctx_, (addr + 2) & address_mask());
                clocks_remaining_ += 8;
            } else if (sz == OpSize::Byte) {
                uint16_t w = mem_read_(mem_ctx_, addr & ~1u);
                dst = (addr & 1) ? (w & 0xFF) : (w >> 8);
                clocks_remaining_ += 4;
            } else {
                dst = mem_read_(mem_ctx_, addr);
                clocks_remaining_ += 4;
            }
        }
        uint32_t result = alu_addx(src, dst, sz);
        write_back_ea(result, sz);
        clocks_remaining_ += 2;  // extend computation idle
        return do_prefetch(pins);
    }

    // ADDA: opmode 3 (word), 7 (long)
    if (opmode == 3 || opmode == 7) {
        OpSize src_sz = (opmode == 3) ? OpSize::Word : OpSize::Long;
        uint32_t src_val;
        if (ea_mode == 0) {
            src_val = read_dn(ea_reg, src_sz);
        } else if (ea_mode == 1) {
            src_val = get_a(ea_reg);
        } else {
            src_val = read_ea(ea_mode, ea_reg, src_sz);
        }
        if (src_sz == OpSize::Word) {
            src_val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(src_val)));
        }
        set_a(dn, get_a(dn) + src_val);
        // ADDA does not affect flags — always 8 clocks for register source
        if (ea_mode <= 1) return do_idle_then_prefetch(pins, 4);
        return do_prefetch(pins);
    }

    // ADD: opmodes 0,1,2 (<ea> + Dn → Dn)
    OpSize sz = static_cast<OpSize>(opmode & 3);
    if (opmode < 3) {
        uint32_t src_val;
        if (ea_mode == 0) {
            src_val = read_dn(ea_reg, sz);
        } else if (ea_mode == 1) {
            src_val = get_a(ea_reg);
            if (sz == OpSize::Word)
                src_val = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(src_val)));
        } else if (ea_mode == 7 && ea_reg == 4) {
            // Immediate: inline path
            src_val = read_ea(ea_mode, ea_reg, sz);
        } else {
            // Memory EA: use bus cycle handlers
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_ADD;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_READ_TO_DN_LONG : BUS_READ_TO_DN;
            return begin_ea_read(pins, ea_mode);
        }
        uint32_t result = alu_add(src_val, read_dn(dn, sz), sz);
        write_dn(dn, result, sz);
        // ADD .l Dn,Dn: 8 clocks (4 idle)
        if (ea_mode <= 1 && sz == OpSize::Long) return do_idle_then_prefetch(pins, 4);
        return do_prefetch(pins);
    }
    // opmode 4,5,6: ADD Dn → <ea> (memory destination)
    if (opmode >= 4 && opmode <= 6) {
        sz = static_cast<OpSize>(opmode - 4);
        if (ea_mode >= 2) {
            ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
            reg_idx_ = dn;
            op_sz_ = sz;
            pending_op_ = OP_ADD;
            bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
            return begin_ea_read(pins, ea_mode);
        }
    }
    return do_prefetch(pins);
}

// ── Group E: Shift / Rotate ────────────────────────────────────
inline bus_state_t decode_groupE(bus_state_t pins, uint16_t opcode) {
    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);

    // Memory shifts: size field = 3 (bits 7-6 = 11)
    // 1110 0xx0 11xx xxxx (right) or 1110 0xx1 11xx xxxx (left)
    if (((opcode >> 6) & 3) == 3) {
        // Memory shift/rotate by 1
        uint8_t op_type = (opcode >> 9) & 3;  // 0=ASd, 1=LSd, 2=ROXd, 3=ROd
        bool dir_left = (opcode & 0x0100) != 0;
        uint32_t val = read_ea(ea_mode, ea_reg, OpSize::Word);
        uint32_t result;
        if (dir_left) {
            switch (op_type) {
                case 0: result = alu_asl(val, 1, OpSize::Word); break;
                case 1: result = alu_lsl(val, 1, OpSize::Word); break;
                case 2: result = alu_roxl(val, 1, OpSize::Word); break;
                case 3: result = alu_rol(val, 1, OpSize::Word); break;
                default: result = val;
            }
        } else {
            switch (op_type) {
                case 0: result = alu_asr(val, 1, OpSize::Word); break;
                case 1: result = alu_lsr(val, 1, OpSize::Word); break;
                case 2: result = alu_roxr(val, 1, OpSize::Word); break;
                case 3: result = alu_ror(val, 1, OpSize::Word); break;
                default: result = val;
            }
        }
        write_back_ea(result, OpSize::Word);
        return do_prefetch(pins);
    }

    // Register shifts: 1110 ccc d ss i tt rrr
    uint8_t count_field = (opcode >> 9) & 7;
    bool dir_left = (opcode & 0x0100) != 0;
    OpSize sz = static_cast<OpSize>((opcode >> 6) & 3);
    bool use_reg = (opcode & 0x0020) != 0;
    uint8_t op_type = (opcode >> 3) & 3;  // 0=AS, 1=LS, 2=ROX, 3=RO

    uint8_t count;
    if (use_reg) {
        count = get_d(count_field) & 63;
    } else {
        count = (count_field == 0) ? 8 : count_field;
    }

    uint32_t val = read_dn(ea_reg, sz);
    uint32_t result;

    if (dir_left) {
        switch (op_type) {
            case 0: result = alu_asl(val, count, sz); break;
            case 1: result = alu_lsl(val, count, sz); break;
            case 2: result = alu_roxl(val, count, sz); break;
            case 3: result = alu_rol(val, count, sz); break;
            default: result = val;
        }
    } else {
        switch (op_type) {
            case 0: result = alu_asr(val, count, sz); break;
            case 1: result = alu_lsr(val, count, sz); break;
            case 2: result = alu_roxr(val, count, sz); break;
            case 3: result = alu_ror(val, count, sz); break;
            default: result = val;
        }
    }

    write_dn(ea_reg, result, sz);
    // Register shifts: 6+2n clocks (b/w), 8+2n clocks (.l) → idle = 2+2n or 4+2n
    uint8_t idle = (sz == OpSize::Long) ? (4 + 2 * count) : (2 + 2 * count);
    return do_idle_then_prefetch(pins, idle);
}

#include "chip/cpu/m680x0/operations/inc_lint_prevention_footer.hpp"
