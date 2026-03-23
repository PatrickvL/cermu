// ========================================================================
// mc6809_page3_ops.inc.hpp — Page 3 ($11 prefix) instruction handlers
// ========================================================================
//
// Page 3 opcodes are accessed via the $11 prefix byte.
// They include SWI3, CMPU, and CMPS.
//
// Included inside mc6809_t<Traits> class body.
// ========================================================================

/// Page 3 dispatcher — called when opcode $11 prefix detected
bus_state_t op_page3(bus_state_t pins) {
    switch (step_++) {
    case 0:
        return bus_setup_read(pins, regs_[PC]);
    case 1: {
        opcode_ = bus_read_data(pins);
        regs_[PC]++;
        step_ = 0;
        switch (opcode_) {
        // SWI3
        case 0x3F: transition_to(&mc6809_t::op_swi3);   return pins;

        // CMPU
        case 0x83: transition_to(&mc6809_t::op_cmpu_imm);      return pins;
        case 0x93: transition_to(&mc6809_t::op_cmpu_direct);    return pins;
        case 0xA3: transition_to(&mc6809_t::op_cmpu_indexed);   return pins;
        case 0xB3: transition_to(&mc6809_t::op_cmpu_extended);  return pins;

        // CMPS
        case 0x8C: transition_to(&mc6809_t::op_cmps_imm);      return pins;
        case 0x9C: transition_to(&mc6809_t::op_cmps_direct);    return pins;
        case 0xAC: transition_to(&mc6809_t::op_cmps_indexed);   return pins;
        case 0xBC: transition_to(&mc6809_t::op_cmps_extended);  return pins;

        default:
            // Illegal page 3 opcode — treat as NOP
            transition_to_fetch();
            return pins;
        }
    }
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// SWI3 — Software interrupt 3
// ========================================================================

bus_state_t op_swi3(bus_state_t pins) {
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
        // SWI3 does NOT mask I or F
        return bus_setup_read(pins, Vector::SWI3);
    case 14:
        regs_[PC] = static_cast<uint16_t>(bus_read_data(pins)) << 8;
        return bus_setup_read(pins, Vector::SWI3 + 1);
    case 15:
        regs_[PC] |= bus_read_data(pins);
        transition_to_fetch();
        return pins;
    default: transition_to_fetch(); return pins;
    }
}

// ========================================================================
// CMPU — Compare U (16-bit)
// ========================================================================

bus_state_t op_cmpu_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2: {
        uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        alu_cmp16(regs_[U], val);
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

MC6809_CMP16_DIRECT(cmpu, regs_[U])
MC6809_CMP16_INDEXED(cmpu, regs_[U])
MC6809_CMP16_EXTENDED(cmpu, regs_[U])

// ========================================================================
// CMPS — Compare S (16-bit)
// ========================================================================

bus_state_t op_cmps_imm(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_read(pins, regs_[PC]);
    case 1:
        data_hi_ = bus_read_data(pins);
        regs_[PC]++;
        return bus_setup_read(pins, regs_[PC]);
    case 2: {
        uint16_t val = (static_cast<uint16_t>(data_hi_) << 8) | bus_read_data(pins);
        regs_[PC]++;
        alu_cmp16(regs_[S], val);
        transition_to_fetch();
        return pins;
    }
    default: transition_to_fetch(); return pins;
    }
}

MC6809_CMP16_DIRECT(cmps, regs_[S])
MC6809_CMP16_INDEXED(cmps, regs_[S])
MC6809_CMP16_EXTENDED(cmps, regs_[S])

#undef MC6809_CMP16_DIRECT
#undef MC6809_CMP16_INDEXED
#undef MC6809_CMP16_EXTENDED
