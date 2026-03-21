// m680x0_ctrl_ops.inc.hpp — Control flow instruction handlers
//
// Group 4 (miscellaneous: NOP, RTS, RTE, TRAP, JSR, JMP, LEA, …)
// Group 6 (Bcc / BSR / BRA)
//
// Included inside m680x0_t class body with M680X0_TEMPLATE_CONTEXT defined.

#include "chip/cpu/m680x0/operations/inc_lint_prevention.hpp"

// ── Group 4: Miscellaneous ──────────────────────────────────────
inline bus_state_t decode_group4(bus_state_t pins, uint16_t opcode) {
    // NEG, NOT, CLR, TST, NEGX, MOVEM, LEA, PEA, CHK, EXT, SWAP,
    // NOP, STOP, RTE, RTR, RTS, JSR, JMP, TRAP, LINK, UNLK, etc.

    uint8_t ea_mode = instr_ea_mode(opcode);
    uint8_t ea_reg  = instr_ea_reg(opcode);

    // ── NEGX.b/w/l (0100 0000 ssxx xxxx) ────────────────────────
    if ((opcode & 0xFF00) == 0x4000 && (opcode & 0x00C0) != 0x00C0) {
        OpSize sz = static_cast<OpSize>((opcode >> 6) & 3);
        if (ea_mode == 0) {
            // Dn mode
            uint32_t val = read_dn(ea_reg, sz);
            uint32_t result = alu_subx(val, 0, sz);
            write_dn(ea_reg, result, sz);
            return (sz == OpSize::Long) ? do_idle_then_prefetch(pins, 2) : do_prefetch(pins);
        }
        // Memory: read-modify-write
        ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
        reg_idx_ = 0;  // unused for unary ops
        op_sz_ = sz;
        pending_op_ = OP_NEGX;
        bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
        return begin_ea_read(pins, ea_mode);
    }

    // ── CLR.b/w/l (0100 0010 ssxx xxxx) ─────────────────────────
    if ((opcode & 0xFF00) == 0x4200 && (opcode & 0x00C0) != 0x00C0) {
        OpSize sz = static_cast<OpSize>((opcode >> 6) & 3);
        if (ea_mode == 0) {
            write_dn(ea_reg, 0, sz);
            alu_clr(sz);
            // CLR Dn: 4 (b/w), 6 (l)
            return (sz == OpSize::Long) ? do_idle_then_prefetch(pins, 2) : do_prefetch(pins);
        }
        // Memory: 68000 reads then writes 0 (RMW)
        ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
        reg_idx_ = 0;
        op_sz_ = sz;
        pending_op_ = OP_CLR;
        bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
        return begin_ea_read(pins, ea_mode);
    }

    // ── NEG.b/w/l (0100 0100 ssxx xxxx) ─────────────────────────
    if ((opcode & 0xFF00) == 0x4400 && (opcode & 0x00C0) != 0x00C0) {
        OpSize sz = static_cast<OpSize>((opcode >> 6) & 3);
        if (ea_mode == 0) {
            uint32_t val = read_dn(ea_reg, sz);
            uint32_t result = alu_neg(val, sz);
            write_dn(ea_reg, result, sz);
            return (sz == OpSize::Long) ? do_idle_then_prefetch(pins, 2) : do_prefetch(pins);
        }
        ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
        reg_idx_ = 0;
        op_sz_ = sz;
        pending_op_ = OP_NEG;
        bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
        return begin_ea_read(pins, ea_mode);
    }

    // ── NOT.b/w/l (0100 0110 ssxx xxxx) ─────────────────────────
    if ((opcode & 0xFF00) == 0x4600 && (opcode & 0x00C0) != 0x00C0) {
        OpSize sz = static_cast<OpSize>((opcode >> 6) & 3);
        if (ea_mode == 0) {
            uint32_t val = read_dn(ea_reg, sz);
            uint32_t result = alu_not(val, sz);
            write_dn(ea_reg, result, sz);
            return (sz == OpSize::Long) ? do_idle_then_prefetch(pins, 2) : do_prefetch(pins);
        }
        ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
        reg_idx_ = 0;
        op_sz_ = sz;
        pending_op_ = OP_NOT;
        bus_op_mode_ = (sz == OpSize::Long) ? BUS_RMW_LONG : BUS_RMW;
        return begin_ea_read(pins, ea_mode);
    }

    // ── MOVE from SR (0100 0000 11xx xxxx) ───────────────────────
    if ((opcode & 0xFFC0) == 0x40C0) {
        if (ea_mode == 0) {
            // Dn ← SR — 6 clocks (2 idle + 4 prefetch)
            set_d_w(ea_reg, regs_.sr);
            return do_idle_then_prefetch(pins, 2);
        }
        // Memory modes: TODO
        return do_prefetch(pins);
    }

    // ── MOVE to CCR (0100 0100 11xx xxxx) ────────────────────────
    if ((opcode & 0xFFC0) == 0x44C0) {
        if (ea_mode == 0) {
            set_ccr(static_cast<uint8_t>(get_d_w(ea_reg)));
            return do_idle_then_prefetch(pins, 8);  // 12 clocks total
        }
        uint32_t val = read_ea(ea_mode, ea_reg, OpSize::Word);
        set_ccr(static_cast<uint8_t>(val));
        return do_prefetch(pins);
    }

    // ── MOVE to SR (0100 0110 11xx xxxx) ─────────────────────────
    if ((opcode & 0xFFC0) == 0x46C0) {
        if (!(regs_.sr & SRBits::S)) {
            return exception(pins, Vector::PRIVILEGE_VIOLATION);
        }
        if (ea_mode == 0) {
            set_sr(get_d_w(ea_reg));
            return do_idle_then_prefetch(pins, 8);  // 12 clocks total
        }
        uint32_t val = read_ea(ea_mode, ea_reg, OpSize::Word);
        set_sr(static_cast<uint16_t>(val));
        return do_prefetch(pins);
    }

    // ── TST.b/w/l (0100 1010 ssxx xxxx) ─────────────────────────
    if ((opcode & 0xFF00) == 0x4A00 && (opcode & 0x00C0) != 0x00C0) {
        OpSize sz = static_cast<OpSize>((opcode >> 6) & 3);
        if (ea_mode == 0) {
            alu_tst(read_dn(ea_reg, sz), sz);
            return do_prefetch(pins);
        }
        // Memory: read-only (just test flags)
        ea_addr_ = calc_ea(ea_mode, ea_reg, sz);
        reg_idx_ = 0;
        op_sz_ = sz;
        pending_op_ = OP_TST;
        bus_op_mode_ = BUS_READ_ONLY;
        return begin_ea_read(pins, ea_mode);
    }

    // ── TAS (0100 1010 11xx xxxx) ────────────────────────────────
    if ((opcode & 0xFFC0) == 0x4AC0) {
        if (ea_mode == 0) {
            uint8_t val = get_d_b(ea_reg);
            alu_tst(val, OpSize::Byte);
            set_d_b(ea_reg, val | 0x80);
            return do_prefetch(pins);
        }
        // Memory modes: TODO (indivisible read-modify-write cycle)
        return do_prefetch(pins);
    }

    // ── NBCD (0100 1000 00xx xxxx) ───────────────────────────────
    if ((opcode & 0xFFC0) == 0x4800) {
        if (ea_mode == 0) {
            // NBCD Dn: negate BCD (0 - dst - X)
            uint8_t result = alu_sbcd(get_d_b(ea_reg), 0);
            set_d_b(ea_reg, result);
            return do_idle_then_prefetch(pins, 2);  // 6 clocks (2 idle + 4 prefetch)
        }
        // TODO: NBCD <ea> — needs bus cycles
        return do_prefetch(pins);
    }

    // ── SWAP (0100 1000 0100 0rrr) ──────────────────────────────
    if ((opcode & 0xFFF8) == 0x4840) {
        uint32_t val = get_d(ea_reg);
        val = ((val >> 16) & 0xFFFF) | ((val & 0xFFFF) << 16);
        set_d(ea_reg, val);
        // N, Z set on full 32-bit result; V, C cleared
        uint8_t ccr = get_ccr() & Flags::X;
        if (val == 0)           ccr |= Flags::Z;
        if (val & 0x80000000)   ccr |= Flags::N;
        set_ccr(ccr);
        return do_prefetch(pins);
    }

    // ── PEA (0100 1000 01xx xxxx) ────────────────────────────────
    // NOTE: SWAP is 0x4840-0x4847, PEA uses EA modes 2-7 (bits 5-3 ≥ 2)
    if ((opcode & 0xFFC0) == 0x4840 && ea_mode >= 2) {
        uint32_t ea = calc_ea(ea_mode, ea_reg, OpSize::Long);
        set_a(7, get_a(7) - 4);
        sync_sp();
        if (mem_write_) {
            uint32_t sp = get_a(7) & address_mask();
            mem_write_(mem_ctx_, sp,     static_cast<uint16_t>((ea >> 16) & 0xFFFF));
            mem_write_(mem_ctx_, sp + 2, static_cast<uint16_t>(ea & 0xFFFF));
            clocks_remaining_ += 8;  // 2 × 4-clock writes
        }
        return do_prefetch(pins);
    }

    // ── EXT.W (0100 1000 1000 0rrr) = $4880-$4887 ──────────────
    if ((opcode & 0xFFF8) == 0x4880) {
        uint32_t val = get_d(ea_reg);
        uint32_t result = alu_ext_bw(val);
        // EXT.W: modify low word only, keep high word
        set_d(ea_reg, (val & 0xFFFF0000) | result);
        return do_prefetch(pins);
    }

    // ── EXT.L (0100 1000 1100 0rrr) = $48C0-$48C7 ──────────────
    if ((opcode & 0xFFF8) == 0x48C0) {
        uint32_t val = get_d(ea_reg);
        uint32_t result = alu_ext_wl(val);
        set_d(ea_reg, result);
        return do_prefetch(pins);
    }

    // ── MOVEM (0100 1x00 1xxx xxxx / 0100 1x10 0xxx xxxx) ──────
    if ((opcode & 0xFB80) == 0x4880 && ea_mode >= 2) {
        uint16_t mask = consume_extension_word();
        bool to_reg = (opcode & 0x0400) != 0;  // 1 = memory→register
        OpSize sz = (opcode & 0x0040) ? OpSize::Long : OpSize::Word;
        uint32_t step = (sz == OpSize::Long) ? 4 : 2;

        if (to_reg) {
            // Memory → registers (postincrement allowed, predecrement not)
            uint32_t addr;
            if (ea_mode == 3) {
                // (An)+ — start from An, update An after
                addr = get_a(ea_reg);
            } else {
                addr = calc_ea(ea_mode, ea_reg, sz);
            }

            if (mem_read_) {
                // Transfer registers in order: D0-D7, then A0-A7
                for (int i = 0; i < 16; i++) {
                    if (!(mask & (1 << i))) continue;
                    uint32_t a = addr & address_mask();
                    if (sz == OpSize::Word) {
                        int16_t val = static_cast<int16_t>(mem_read_(mem_ctx_, a));
                        if (i < 8) set_d(i, static_cast<uint32_t>(static_cast<int32_t>(val)));
                        else       set_a(i - 8, static_cast<uint32_t>(static_cast<int32_t>(val)));
                    } else {
                        uint16_t hi = mem_read_(mem_ctx_, a);
                        uint16_t lo = mem_read_(mem_ctx_, (a + 2) & address_mask());
                        uint32_t val = (static_cast<uint32_t>(hi) << 16) | lo;
                        if (i < 8) set_d(i, val);
                        else       set_a(i - 8, val);
                    }
                    clocks_remaining_ += (sz == OpSize::Long) ? 8 : 4;
                    addr += step;
                }
                // (An)+ updates the address register AFTER all transfers
                // (even if An is in the register list, the updated value is from memory)
                if (ea_mode == 3) {
                    set_a(ea_reg, addr);
                    if (ea_reg == 7) sync_sp();
                }
                // 68000 MOVEM mem→reg does one extra read (discarded) at the end
                mem_read_(mem_ctx_, addr & address_mask());
                clocks_remaining_ += 4;
            }
        } else {
            // Registers → memory (predecrement allowed, postincrement not)
            if (ea_mode == 4) {
                // -(An): register mask is REVERSED: A7→A0 at bit 0→7, D7→D0 at bit 8→15
                // Addresses decrement from An
                uint32_t addr = get_a(ea_reg);
                if (mem_write_) {
                    for (int i = 0; i < 16; i++) {
                        if (!(mask & (1 << i))) continue;
                        addr -= step;
                        uint32_t a = addr & address_mask();
                        // Reversed order: bit 0 = A7, bit 7 = A0, bit 8 = D7, bit 15 = D0
                        uint32_t val;
                        if (i < 8)
                            val = get_a(7 - i);
                        else
                            val = get_d(15 - i);
                        if (sz == OpSize::Word) {
                            mem_write_(mem_ctx_, a, static_cast<uint16_t>(val & 0xFFFF));
                        } else {
                            mem_write_(mem_ctx_, a,
                                static_cast<uint16_t>((val >> 16) & 0xFFFF));
                            mem_write_(mem_ctx_, (a + 2) & address_mask(),
                                static_cast<uint16_t>(val & 0xFFFF));
                        }
                        clocks_remaining_ += (sz == OpSize::Long) ? 8 : 4;
                    }
                    set_a(ea_reg, addr);
                    if (ea_reg == 7) sync_sp();
                }
            } else {
                // Other modes: normal mask order (D0→D7 at bits 0→7, A0→A7 at bits 8→15)
                uint32_t addr = calc_ea(ea_mode, ea_reg, sz);
                if (mem_write_) {
                    for (int i = 0; i < 16; i++) {
                        if (!(mask & (1 << i))) continue;
                        uint32_t a = addr & address_mask();
                        uint32_t val;
                        if (i < 8) val = get_d(i);
                        else       val = get_a(i - 8);
                        if (sz == OpSize::Word) {
                            mem_write_(mem_ctx_, a, static_cast<uint16_t>(val & 0xFFFF));
                        } else {
                            mem_write_(mem_ctx_, a,
                                static_cast<uint16_t>((val >> 16) & 0xFFFF));
                            mem_write_(mem_ctx_, (a + 2) & address_mask(),
                                static_cast<uint16_t>(val & 0xFFFF));
                        }
                        clocks_remaining_ += (sz == OpSize::Long) ? 8 : 4;
                        addr += step;
                    }
                }
            }
        }
        return do_prefetch(pins);
    }

    // ── LEA (0100 rrr1 11xx xxxx) ────────────────────────────────
    if ((opcode & 0xF1C0) == 0x41C0) {
        uint8_t an = (opcode >> 9) & 7;
        uint32_t addr = calc_ea(ea_mode, ea_reg, OpSize::Long);
        set_a(an, addr);
        return do_prefetch(pins);
    }

    // ── CHK (0100 rrr1 10xx xxxx) ────────────────────────────────
    if ((opcode & 0xF1C0) == 0x4180) {
        // TODO: CHK instruction
        return do_prefetch(pins);
    }

    // ── NOP: 0100_1110_0111_0001 = $4E71 ─────────────────────────
    if (opcode == 0x4E71) {
        return do_prefetch(pins);
    }

    // ── RESET: $4E70 ─────────────────────────────────────────────
    if (opcode == 0x4E70) {
        if (!(regs_.sr & SRBits::S))
            return exception(pins, Vector::PRIVILEGE_VIOLATION);
        // Assert RESET line — simplified: just do prefetch
        return do_prefetch(pins);
    }

    // ── STOP: $4E72 ──────────────────────────────────────────────
    if (opcode == 0x4E72) {
        if (!(regs_.sr & SRBits::S))
            return exception(pins, Vector::PRIVILEGE_VIOLATION);
        // Load new SR from extension word
        set_sr(consume_extension_word());
        stopped_ = true;
        return do_prefetch(pins);
    }

    // ── RTE: $4E73 ───────────────────────────────────────────────
    if (opcode == 0x4E73) {
        if (!(regs_.sr & SRBits::S)) {
            return exception(pins, Vector::PRIVILEGE_VIOLATION);
        }
        // Pop SR and PC from supervisor stack
        if (mem_read_) {
            uint32_t sp = get_a(7) & address_mask();
            uint16_t new_sr = mem_read_(mem_ctx_, sp);
            uint16_t pc_hi  = mem_read_(mem_ctx_, sp + 2);
            uint16_t pc_lo  = mem_read_(mem_ctx_, sp + 4);
            set_a(7, get_a(7) + 6);
            sync_sp();
            set_sr(new_sr);
            regs_.pc = (static_cast<uint32_t>(pc_hi) << 16) | pc_lo;
            clocks_remaining_ += 12;  // 3 × 4-clock reads from stack
            return do_branch_prefetch(pins);
        }
        return do_prefetch(pins);
    }

    // ── RTS: $4E75 ───────────────────────────────────────────
    if (opcode == 0x4E75) {
        // Pop PC from stack
        if (mem_read_) {
            uint32_t sp = get_a(7) & address_mask();
            uint16_t pc_hi = mem_read_(mem_ctx_, sp);
            uint16_t pc_lo = mem_read_(mem_ctx_, sp + 2);
            set_a(7, get_a(7) + 4);
            sync_sp();
            regs_.pc = (static_cast<uint32_t>(pc_hi) << 16) | pc_lo;
            clocks_remaining_ += 8;  // 2 × 4-clock reads from stack
            return do_branch_prefetch(pins);
        }
        return do_prefetch(pins);
    }

    // ── TRAPV: $4E76 ─────────────────────────────────────────────
    if (opcode == 0x4E76) {
        if (get_ccr() & Flags::V) {
            return exception(pins, Vector::TRAPV_INSTR);
        }
        return do_prefetch(pins);
    }

    // ── RTR: $4E77 ───────────────────────────────────────────────
    if (opcode == 0x4E77) {
        // Pop CCR then PC from stack
        if (mem_read_) {
            uint32_t sp = get_a(7) & address_mask();
            uint16_t new_ccr = mem_read_(mem_ctx_, sp);
            uint16_t pc_hi   = mem_read_(mem_ctx_, sp + 2);
            uint16_t pc_lo   = mem_read_(mem_ctx_, sp + 4);
            set_a(7, get_a(7) + 6);
            sync_sp();
            set_ccr(static_cast<uint8_t>(new_ccr));  // Only CCR (low byte of SR)
            regs_.pc = (static_cast<uint32_t>(pc_hi) << 16) | pc_lo;
            clocks_remaining_ += 12;  // 3 × 4-clock reads from stack
            return do_branch_prefetch(pins);
        }
        return do_prefetch(pins);
    }

    // ── MOVE USP: 0100 1110 0110 xrrr ────────────────────────────
    if ((opcode & 0xFFF0) == 0x4E60) {
        if (!(regs_.sr & SRBits::S))
            return exception(pins, Vector::PRIVILEGE_VIOLATION);
        if (opcode & 0x0008) {
            // MOVE USP, An
            set_a(ea_reg, regs_.usp);
            sync_sp();  // If An == A7, keep SSP in sync
        } else {
            // MOVE An, USP
            regs_.usp = get_a(ea_reg);
        }
        return do_prefetch(pins);
    }

    // ── LINK: 0100 1110 0101 0rrr ────────────────────────────────
    if ((opcode & 0xFFF8) == 0x4E50) {
        // Push An, An = SP, SP += displacement (signed)
        int16_t disp = static_cast<int16_t>(consume_extension_word());
        set_a(7, get_a(7) - 4);
        sync_sp();
        if (mem_write_) {
            uint32_t sp = get_a(7) & address_mask();
            uint32_t an_val = get_a(ea_reg);
            mem_write_(mem_ctx_, sp,     static_cast<uint16_t>((an_val >> 16) & 0xFFFF));
            mem_write_(mem_ctx_, sp + 2, static_cast<uint16_t>(an_val & 0xFFFF));
            clocks_remaining_ += 8;  // 2 × 4-clock writes
        }
        set_a(ea_reg, get_a(7));
        set_a(7, get_a(7) + disp);
        sync_sp();
        return do_prefetch(pins);
    }

    // ── UNLK: 0100 1110 0101 1rrr ────────────────────────────────
    if ((opcode & 0xFFF8) == 0x4E58) {
        // SP = An, pop An from stack
        set_a(7, get_a(ea_reg));
        sync_sp();
        if (mem_read_) {
            uint32_t sp = get_a(7) & address_mask();
            uint16_t hi = mem_read_(mem_ctx_, sp);
            uint16_t lo = mem_read_(mem_ctx_, sp + 2);
            set_a(ea_reg, (static_cast<uint32_t>(hi) << 16) | lo);
            clocks_remaining_ += 8;  // 2 × 4-clock reads
        }
        set_a(7, get_a(7) + 4);
        sync_sp();
        return do_prefetch(pins);
    }

    // ── TRAP: 0100 1110 0100 vvvv ────────────────────────────────
    if ((opcode & 0xFFF0) == 0x4E40) {
        uint8_t trap_vec = opcode & 0x0F;
        return exception(pins, Vector::TRAP_BASE + trap_vec);
    }

    // ── JSR: 0100 1110 10xx xxxx ─────────────────────────────────
    if ((opcode & 0xFFC0) == 0x4E80) {
        uint32_t target = calc_ea(ea_mode, ea_reg, OpSize::Long);
        // Push return address (formal PC of next instruction = internal PC - 2)
        uint32_t return_pc = regs_.pc - 2;
        set_a(7, get_a(7) - 4);
        sync_sp();
        if (mem_write_) {
            uint32_t sp = get_a(7) & address_mask();
            mem_write_(mem_ctx_, sp,     static_cast<uint16_t>((return_pc >> 16) & 0xFFFF));
            mem_write_(mem_ctx_, sp + 2, static_cast<uint16_t>(return_pc & 0xFFFF));
            clocks_remaining_ += 8;  // 2 × 4-clock writes
        }
        regs_.pc = target;
        return do_branch_prefetch(pins);
    }

    // ── JMP: 0100 1110 11xx xxxx ─────────────────────────────────
    if ((opcode & 0xFFC0) == 0x4EC0) {
        uint32_t target = calc_ea(ea_mode, ea_reg, OpSize::Long);
        regs_.pc = target;
        return do_branch_prefetch(pins);
    }

    // Fallthrough: illegal / unimplemented
    return exception(pins, Vector::ILLEGAL_INSTR);
}

// ── Group 6: Bcc / BSR / BRA ────────────────────────────────────
inline bus_state_t decode_group6(bus_state_t pins, uint16_t opcode) {
    auto cc = static_cast<Condition>((opcode >> 8) & 0x0F);
    int8_t disp8 = static_cast<int8_t>(opcode & 0xFF);

    // Displacement is always relative to the address of the displacement
    // itself (formal_opcode + 2). Save branch base BEFORE consuming ext word.
    uint32_t branch_base = regs_.pc - 2;  // = formal_opcode + 2

    int32_t displacement;
    if (disp8 == 0) {
        // Word displacement from extension word
        // Note: branches don't use consume_extension_word() because the
        // IRC refill would read from the sequential address, not the branch target.
        displacement = static_cast<int16_t>(regs_.irc);
        regs_.pc += 2;
    } else if constexpr (has_long_branch()) {
        if (disp8 == -1) {
            // Long displacement (68020+) from two extension words
            displacement = static_cast<int32_t>(
                (static_cast<uint32_t>(regs_.irc) << 16)); // high word
            regs_.pc += 2;
            if (mem_read_) {
                displacement |= mem_read_(mem_ctx_, regs_.pc & address_mask());
            } else {
                displacement |= regs_.irc;
            }
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
        regs_.pc = branch_base + displacement;
        return do_branch_prefetch(pins);
    }
    if (cc == Condition::F) {
        // BSR — branch to subroutine
        // Return address = formal address of next instruction = internal PC - 2
        uint32_t return_pc = regs_.pc - 2;
        set_a(7, get_a(7) - 4);
        sync_sp();
        if (mem_write_) {
            uint32_t sp = get_a(7) & address_mask();
            mem_write_(mem_ctx_, sp,     static_cast<uint16_t>((return_pc >> 16) & 0xFFFF));
            mem_write_(mem_ctx_, sp + 2, static_cast<uint16_t>(return_pc & 0xFFFF));
            clocks_remaining_ += 10;  // 2 × 4-clock writes + 2 idle
        }
        regs_.pc = branch_base + displacement;
        return do_branch_prefetch(pins);
    }

    // Bcc — conditional branch
    if (test_condition(cc)) {
        regs_.pc = branch_base + displacement;
        return do_branch_prefetch(pins);
    }
    return do_prefetch(pins);
}

#include "chip/cpu/m680x0/operations/inc_lint_prevention_footer.hpp"
