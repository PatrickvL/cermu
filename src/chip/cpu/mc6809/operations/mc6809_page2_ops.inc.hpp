// ========================================================================
// mc6809_page2_ops.inc.hpp — Page 2 ($10 prefix) instruction handlers
// ========================================================================
//
// Page 2 opcodes are accessed via the $10 prefix byte.
// They include long conditional branches and additional 16-bit operations
// involving Y, S, and D registers.
//
// Included inside mc6809_t<Traits> class body.
// ========================================================================

/// Page 2 dispatcher — called when opcode $10 prefix detected
bus_state_t op_page2(bus_state_t pins) {
    switch (step_++) {
    case 0:
        return bus_setup_read(pins, regs_[PC]);
    case 1: {
        opcode_ = bus_read_data(pins);
        regs_[PC]++;
        step_ = 0;
        // Dispatch page 2 opcodes
        switch (opcode_) {
        // Long conditional branches
        case 0x21: transition_to(&mc6809_t::op_lbrn);   return pins;
        case 0x22: transition_to(&mc6809_t::op_lbhi);   return pins;
        case 0x23: transition_to(&mc6809_t::op_lbls);   return pins;
        case 0x24: transition_to(&mc6809_t::op_lbcc);   return pins;
        case 0x25: transition_to(&mc6809_t::op_lbcs);   return pins;
        case 0x26: transition_to(&mc6809_t::op_lbne);   return pins;
        case 0x27: transition_to(&mc6809_t::op_lbeq);   return pins;
        case 0x28: transition_to(&mc6809_t::op_lbvc);   return pins;
        case 0x29: transition_to(&mc6809_t::op_lbvs);   return pins;
        case 0x2A: transition_to(&mc6809_t::op_lbpl);   return pins;
        case 0x2B: transition_to(&mc6809_t::op_lbmi);   return pins;
        case 0x2C: transition_to(&mc6809_t::op_lbge);   return pins;
        case 0x2D: transition_to(&mc6809_t::op_lblt);   return pins;
        case 0x2E: transition_to(&mc6809_t::op_lbgt);   return pins;
        case 0x2F: transition_to(&mc6809_t::op_lble);   return pins;

        // SWI2
        case 0x3F: transition_to(&mc6809_t::op_swi2);   return pins;

        // CMPD
        case 0x83: transition_to(&mc6809_t::op_cmpd_imm);      return pins;
        case 0x93: transition_to(&mc6809_t::op_cmpd_direct);    return pins;
        case 0xA3: transition_to(&mc6809_t::op_cmpd_indexed);   return pins;
        case 0xB3: transition_to(&mc6809_t::op_cmpd_extended);  return pins;

        // CMPY
        case 0x8C: transition_to(&mc6809_t::op_cmpy_imm);      return pins;
        case 0x9C: transition_to(&mc6809_t::op_cmpy_direct);    return pins;
        case 0xAC: transition_to(&mc6809_t::op_cmpy_indexed);   return pins;
        case 0xBC: transition_to(&mc6809_t::op_cmpy_extended);  return pins;

        // LDY
        case 0x8E: transition_to(&mc6809_t::op_ldy_imm);      return pins;
        case 0x9E: transition_to(&mc6809_t::op_ldy_direct);    return pins;
        case 0xAE: transition_to(&mc6809_t::op_ldy_indexed);   return pins;
        case 0xBE: transition_to(&mc6809_t::op_ldy_extended);  return pins;

        // STY
        case 0x9F: transition_to(&mc6809_t::op_sty_direct);    return pins;
        case 0xAF: transition_to(&mc6809_t::op_sty_indexed);   return pins;
        case 0xBF: transition_to(&mc6809_t::op_sty_extended);  return pins;

        // LDS
        case 0xCE: transition_to(&mc6809_t::op_lds_imm);      return pins;
        case 0xDE: transition_to(&mc6809_t::op_lds_direct);    return pins;
        case 0xEE: transition_to(&mc6809_t::op_lds_indexed);   return pins;
        case 0xFE: transition_to(&mc6809_t::op_lds_extended);  return pins;

        // STS
        case 0xDF: transition_to(&mc6809_t::op_sts_direct);    return pins;
        case 0xEF: transition_to(&mc6809_t::op_sts_indexed);   return pins;
        case 0xFF: transition_to(&mc6809_t::op_sts_extended);  return pins;

        default:
            // Illegal page 2 opcode — treat as NOP
            transition_to_fetch();
            return pins;
        }
    }
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// LONG CONDITIONAL BRANCHES
// ========================================================================

// Macro for long conditional branch (4-byte operand, condition check)
#define MC6809_LBRANCH(name, cond_fn) \
    bus_state_t op_##name(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            data_hi_ = bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: { \
            int16_t offset = static_cast<int16_t>((static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins)); \
            regs_[PC]++; \
            if (cond_fn()) \
                regs_[PC] = static_cast<uint16_t>(regs_[PC] + offset); \
            transition_to_fetch(); \
            return pins; \
        } \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_LBRANCH(lbrn, cc_never)
MC6809_LBRANCH(lbhi, cc_hi)
MC6809_LBRANCH(lbls, cc_ls)
MC6809_LBRANCH(lbcc, cc_cc)
MC6809_LBRANCH(lbcs, cc_cs)
MC6809_LBRANCH(lbne, cc_ne)
MC6809_LBRANCH(lbeq, cc_eq)
MC6809_LBRANCH(lbvc, cc_vc)
MC6809_LBRANCH(lbvs, cc_vs)
MC6809_LBRANCH(lbpl, cc_pl)
MC6809_LBRANCH(lbmi, cc_mi)
MC6809_LBRANCH(lbge, cc_ge)
MC6809_LBRANCH(lblt, cc_lt)
MC6809_LBRANCH(lbgt, cc_gt)
MC6809_LBRANCH(lble, cc_le)

#undef MC6809_LBRANCH

// ========================================================================
// SWI2 — Software interrupt 2
// ========================================================================

bus_state_t op_swi2(bus_state_t pins) {
    switch (step_++) {
    case 0:
        regs_[CC] |= Flags::E;
        return bus_internal(pins);
    case 1:  return bus_setup_write(pins, --regs_[S], regs_[PC] & 0xFF);
    case 2:  return bus_setup_write(pins, --regs_[S], regs_[PC] >> 8);
    case 3:  return bus_setup_write(pins, --regs_[S], regs_[U] & 0xFF);
    case 4:  return bus_setup_write(pins, --regs_[S], regs_[U] >> 8);
    case 5:  return bus_setup_write(pins, --regs_[S], regs_[Y] & 0xFF);
    case 6:  return bus_setup_write(pins, --regs_[S], regs_[Y] >> 8);
    case 7:  return bus_setup_write(pins, --regs_[S], regs_[X] & 0xFF);
    case 8:  return bus_setup_write(pins, --regs_[S], regs_[X] >> 8);
    case 9:  return bus_setup_write(pins, --regs_[S], regs_[DP]);
    case 10: return bus_setup_write(pins, --regs_[S], regs_[B]);
    case 11: return bus_setup_write(pins, --regs_[S], regs_[A]);
    case 12: return bus_setup_write(pins, --regs_[S], regs_[CC]);
    case 13:
        // SWI2 does NOT mask I or F
        return bus_setup_read(pins, Vector::SWI2);
    case 14:
        regs_[PC] = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        return bus_setup_read(pins, Vector::SWI2 + 1);
    case 15:
        regs_[PC] |= bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// CMPD — Compare D (16-bit)
// ========================================================================

bus_state_t op_cmpd_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2: {
        uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        alu_cmp16(regs_[D], val);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

#define MC6809_CMP16_DIRECT(name, reg) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 2: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 3: { \
            uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            alu_cmp16(reg, val); \
            transition_to_fetch(); \
            return pins; \
        } \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_CMP16_INDEXED(name, reg) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
            } else { \
                ea_ = resolve_indexed_simple(postbyte_, pins); \
            } \
            return bus_setup_read(pins, ea_); \
        } \
        case 2: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 3: { \
            uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            alu_cmp16(reg, val); \
            transition_to_fetch(); \
            return pins; \
        } \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_CMP16_EXTENDED(name, reg) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 3: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 4: { \
            uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            alu_cmp16(reg, val); \
            transition_to_fetch(); \
            return pins; \
        } \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_CMP16_DIRECT(cmpd, regs_[D])
MC6809_CMP16_INDEXED(cmpd, regs_[D])
MC6809_CMP16_EXTENDED(cmpd, regs_[D])

MC6809_CMP16_DIRECT(cmpy, regs_[Y])
MC6809_CMP16_INDEXED(cmpy, regs_[Y])
MC6809_CMP16_EXTENDED(cmpy, regs_[Y])

#undef MC6809_CMP16_DIRECT
#undef MC6809_CMP16_INDEXED
#undef MC6809_CMP16_EXTENDED

// ========================================================================
// LDY, STY — Load/Store Y (16-bit)
// ========================================================================

bus_state_t op_ldy_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        regs_[Y] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        alu_ld16_flags(regs_[Y]);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// LDY direct/indexed/extended via macros
#define MC6809_OP_LD16_DIRECT(name, reg) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 2: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 3: \
            reg = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            alu_ld16_flags(reg); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_LD16_INDEXED(name, reg) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
            } else { \
                ea_ = resolve_indexed_simple(postbyte_, pins); \
            } \
            return bus_setup_read(pins, ea_); \
        } \
        case 2: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 3: \
            reg = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            alu_ld16_flags(reg); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_LD16_EXTENDED(name, reg) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            return bus_setup_read(pins, ea_); \
        case 3: \
            data_hi_ = bus_read_data(pins); \
            return bus_setup_read(pins, ea_ + 1); \
        case 4: \
            reg = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins); \
            alu_ld16_flags(reg); \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_LD16_DIRECT(ldy, regs_[Y])
MC6809_OP_LD16_INDEXED(ldy, regs_[Y])
MC6809_OP_LD16_EXTENDED(ldy, regs_[Y])

// LDS — Loading S also arms NMI
bus_state_t op_lds_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        regs_[S] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        alu_ld16_flags(regs_[S]);
        nmi_armed_ = true;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_lds_direct(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, ea_);
    case 2:
        data_hi_ = bus_read_data(pins);
        return bus_setup_read(pins, ea_ + 1);
    case 3:
        regs_[S] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        alu_ld16_flags(regs_[S]);
        nmi_armed_ = true;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_lds_indexed(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1: {
        postbyte_ = bus_read_data(pins);
        regs_[PC]++;
        if (!(postbyte_ & 0x80)) {
            int8_t off5 = postbyte_ & 0x1F;
            if (off5 & 0x10) off5 |= 0xE0;
            ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5);
        } else {
            ea_ = resolve_indexed_simple(postbyte_, pins);
        }
        return bus_setup_read(pins, ea_);
    }
    case 2:
        data_hi_ = bus_read_data(pins);
        return bus_setup_read(pins, ea_ + 1);
    case 3:
        regs_[S] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        alu_ld16_flags(regs_[S]);
        nmi_armed_ = true;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

bus_state_t op_lds_extended(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2:
        ea_ |= bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, ea_);
    case 3:
        data_hi_ = bus_read_data(pins);
        return bus_setup_read(pins, ea_ + 1);
    case 4:
        regs_[S] = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        alu_ld16_flags(regs_[S]);
        nmi_armed_ = true;
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

#undef MC6809_OP_LD16_DIRECT
#undef MC6809_OP_LD16_INDEXED
#undef MC6809_OP_LD16_EXTENDED

// STY
#define MC6809_OP_ST16_DIRECT(name, reg) \
    bus_state_t op_##name##_direct(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = (static_cast<uint16_t>(regs_[DP]) << 8) | bus_read_data(pins); \
            regs_[PC]++; \
            alu_ld16_flags(reg); \
            return bus_setup_write(pins, ea_, reg >> 8); \
        case 2: \
            return bus_setup_write(pins, ea_ + 1, reg & 0xFF); \
        case 3: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_ST16_INDEXED(name, reg) \
    bus_state_t op_##name##_indexed(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: { \
            postbyte_ = bus_read_data(pins); \
            regs_[PC]++; \
            if (!(postbyte_ & 0x80)) { \
                int8_t off5 = postbyte_ & 0x1F; \
                if (off5 & 0x10) off5 |= 0xE0; \
                ea_ = static_cast<uint16_t>(get_index_reg((postbyte_ >> 5) & 0x03) + off5); \
            } else { \
                ea_ = resolve_indexed_simple(postbyte_, pins); \
            } \
            alu_ld16_flags(reg); \
            return bus_setup_write(pins, ea_, reg >> 8); \
        } \
        case 2: \
            return bus_setup_write(pins, ea_ + 1, reg & 0xFF); \
        case 3: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

#define MC6809_OP_ST16_EXTENDED(name, reg) \
    bus_state_t op_##name##_extended(bus_state_t pins) { \
        switch (step_++) { \
        case 0: return bus_setup_read(pins, regs_[PC]); \
        case 1: \
            ea_ = static_cast<uint16_t>(bus_read_data(pins)) << 8; \
            regs_[PC]++; \
            return bus_setup_read(pins, regs_[PC]); \
        case 2: \
            ea_ |= bus_read_data(pins); \
            regs_[PC]++; \
            alu_ld16_flags(reg); \
            return bus_setup_write(pins, ea_, reg >> 8); \
        case 3: \
            return bus_setup_write(pins, ea_ + 1, reg & 0xFF); \
        case 4: \
            transition_to_fetch(); \
            return pins; \
        default: transition_to_fetch(); return pins; \
        } \
    }

MC6809_OP_ST16_DIRECT(sty, regs_[Y])
MC6809_OP_ST16_INDEXED(sty, regs_[Y])
MC6809_OP_ST16_EXTENDED(sty, regs_[Y])

MC6809_OP_ST16_DIRECT(sts, regs_[S])
MC6809_OP_ST16_INDEXED(sts, regs_[S])
MC6809_OP_ST16_EXTENDED(sts, regs_[S])

#undef MC6809_OP_ST16_DIRECT
#undef MC6809_OP_ST16_INDEXED
#undef MC6809_OP_ST16_EXTENDED
