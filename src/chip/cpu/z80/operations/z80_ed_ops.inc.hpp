/*
 * z80_ed_ops.inc.hpp — ED-Prefix Instruction Handlers for Z80
 *
 * Handles all ED-prefixed instructions: extended loads, 16-bit ADC/SBC,
 * block transfers, block search, block I/O, special operations.
 *
 * The ED opcode has already been fetched as a second M1 cycle.
 * These handlers manage the remaining M-cycles.
 *
 * Included inside z80_t class with Z80_TEMPLATE_CONTEXT defined.
 */

#include "inc_lint_prevention.hpp"

#ifndef Z80_SKIP_IMPLEMENTATION

// ========================================================================
// ED: IN r,(C) — 12T (M1:4 + M1:4 + io:4)
// ========================================================================
bus_state_t op_in_r_c(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_io_read(pins, regs_.bc);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2: return pins; // IO extra wait
    case 3: {
        uint8_t val = BUS_GET_DATA(pins);
        bus_finish_io(pins);
        uint8_t y = (ed_opcode_ >> 3) & 7;
        if (y != 6) set_reg8_direct(y, val); // IN (C) just sets flags, discards value
        regs_.f = (regs_.f & Flags::C) | sz53p_table[val];
        regs_.wz = regs_.bc + 1;
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// ED: OUT (C),r — 12T (M1:4 + M1:4 + io:4)
// ========================================================================
bus_state_t op_out_c_r(bus_state_t pins) {
    uint8_t y = (ed_opcode_ >> 3) & 7;
    uint8_t val = (y != 6) ? get_reg8_direct(y) : 0;
    switch (step_++) {
    case 0: return bus_setup_io_write(pins, regs_.bc, val);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2: return pins;
    case 3:
        bus_finish_io(pins);
        regs_.wz = regs_.bc + 1;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// ED: ADC HL,rr / SBC HL,rr — 15T (M1:4 + M1:4 + internal:7)
// ========================================================================
bus_state_t op_adc_sbc_hl(bus_state_t pins) {
    switch (step_++) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6:
        return pins;
    }
    uint8_t p = (ed_opcode_ >> 4) & 3;
    uint16_t val = get_reg16(p);
    if (ed_opcode_ & 0x08) {
        alu_adc16(val);
    } else {
        alu_sbc16(val);
    }
    regs_.wz = regs_.hl; // Actually WZ = HL_before + 1, but close enough
    transition_to_fetch();
    return pins;
}

// ========================================================================
// ED: LD (nn),rr — 20T (M1:4 + M1:4 + read_lo:3 + read_hi:3 + write_lo:3 + write_hi:3)
// ========================================================================
bus_state_t op_ld_nn_rr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.pc);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_.pc++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_.pc);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_.pc++;
        bus_finish_mem(pins);
        return pins;
    case 6: {
        uint8_t p = (ed_opcode_ >> 4) & 3;
        return bus_setup_mem_write(pins, addr_latch_, static_cast<uint8_t>(get_reg16(p) & 0xFF));
    }
    case 7: if (!wait_check(pins)) return pins; return pins;
    case 8:
        bus_finish_mem(pins);
        return pins;
    case 9: {
        uint8_t p = (ed_opcode_ >> 4) & 3;
        return bus_setup_mem_write(pins, addr_latch_ + 1, static_cast<uint8_t>(get_reg16(p) >> 8));
    }
    case 10: if (!wait_check(pins)) return pins; return pins;
    case 11:
        bus_finish_mem(pins);
        regs_.wz = addr_latch_ + 1;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// ED: LD rr,(nn) — 20T
// ========================================================================
bus_state_t op_ed_ld_rr_nn(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.pc);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_.pc++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_.pc);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_.pc++;
        bus_finish_mem(pins);
        return pins;
    case 6: return bus_setup_mem_read(pins, addr_latch_);
    case 7: if (!wait_check(pins)) return pins; return pins;
    case 8:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 9: return bus_setup_mem_read(pins, addr_latch_ + 1);
    case 10: if (!wait_check(pins)) return pins; return pins;
    case 11: {
        uint16_t val = data_latch_ | (static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8);
        bus_finish_mem(pins);
        uint8_t p = (ed_opcode_ >> 4) & 3;
        set_reg16(p, val);
        regs_.wz = addr_latch_ + 1;
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// ED: LD A,I / LD A,R — 9T (M1:4 + M1:4 + internal:1)
// ========================================================================
bus_state_t op_ld_a_ir(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins; // 1 internal T-state
    }
    if (ed_opcode_ == 0x57) {
        // LD A,I
        regs_.a = regs_.i;
    } else {
        // LD A,R
        regs_.a = (regs_.r & 0x80) | (regs_.r & 0x7F);
    }
    regs_.f = (regs_.f & Flags::C)
            | sz53_table[regs_.a]
            | (regs_.iff2 ? Flags::PV : 0);
    transition_to_fetch();
    return pins;
}

// ========================================================================
// ED: LD I,A / LD R,A — 9T (M1:4 + M1:4 + internal:1)
// ========================================================================
bus_state_t op_ld_ir_a(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins;
    }
    if (ed_opcode_ == 0x47) {
        regs_.i = regs_.a;
    } else {
        regs_.r = regs_.a;
    }
    transition_to_fetch();
    return pins;
}

// ========================================================================
// ED: NEG — 8T (M1:4 + M1:4, execution inline during decode)
// Already handled inline in ed_decode. This is a placeholder.
// ========================================================================

// ========================================================================
// ED: RETI / RETN — 14T (M1:4 + M1:4 + read_lo:3 + read_hi:3)
// ========================================================================
bus_state_t op_reti_retn(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.sp);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_.sp++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_.sp);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_.sp++;
        bus_finish_mem(pins);
        regs_.iff1 = regs_.iff2;
        regs_.pc = addr_latch_;
        regs_.wz = addr_latch_;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// ED: RLD — 18T (M1:4 + M1:4 + read:3 + internal:4 + write:3)
// ========================================================================
bus_state_t op_rld(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.hl);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: case 4: case 5: case 6: // 4 internal T-states
        return pins;
    case 7: {
        uint8_t mem_val = data_latch_;
        alu_rld(mem_val);
        data_latch_ = mem_val;
        return bus_setup_mem_write(pins, regs_.hl, data_latch_);
    }
    case 8: if (!wait_check(pins)) return pins; return pins;
    case 9:
        bus_finish_mem(pins);
        regs_.wz = regs_.hl + 1;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// ED: RRD — 18T (same structure as RLD)
// ========================================================================
bus_state_t op_rrd(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.hl);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: case 4: case 5: case 6:
        return pins;
    case 7: {
        uint8_t mem_val = data_latch_;
        alu_rrd(mem_val);
        data_latch_ = mem_val;
        return bus_setup_mem_write(pins, regs_.hl, data_latch_);
    }
    case 8: if (!wait_check(pins)) return pins; return pins;
    case 9:
        bus_finish_mem(pins);
        regs_.wz = regs_.hl + 1;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// BLOCK TRANSFER: LDI/LDD — 16T (M1:4 + M1:4 + read:3 + write:3 + internal:2)
// ========================================================================
bus_state_t op_ldi_ldd(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.hl);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_write(pins, regs_.de, data_latch_);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        bus_finish_mem(pins);
        return pins;
    case 6: case 7: // 2 internal T-states
        return pins;
    }
    {
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_.hl += dir;
        regs_.de += dir;
        regs_.bc--;
        uint8_t n = data_latch_ + regs_.a;
        regs_.f = (regs_.f & (Flags::S | Flags::Z | Flags::C))
                | (regs_.bc ? Flags::PV : 0)
                | (n & Flags::X)
                | ((n << 4) & Flags::Y);
    }
    transition_to_fetch();
    return pins;
}

// ========================================================================
// BLOCK TRANSFER: LDIR/LDDR — 21/16T (16T base + 5T if BC != 0)
// ========================================================================
bus_state_t op_ldir_lddr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.hl);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_write(pins, regs_.de, data_latch_);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        bus_finish_mem(pins);
        return pins;
    case 6: case 7: {
        if (step_ == 8) { // After the 2 internal T-states
            int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
            regs_.hl += dir;
            regs_.de += dir;
            regs_.bc--;
            uint8_t n = data_latch_ + regs_.a;
            regs_.f = (regs_.f & (Flags::S | Flags::Z | Flags::C))
                    | (regs_.bc ? Flags::PV : 0)
                    | (n & Flags::X)
                    | ((n << 4) & Flags::Y);
            if (regs_.bc == 0) {
                transition_to_fetch(); // 16T total
            }
            // If BC != 0, continue with 5 more internal T-states
        }
        return pins;
    }
    case 8: case 9: case 10: case 11: case 12: // 5 internal T-states (repeat)
        return pins;
    }
    regs_.pc -= 2; // Back up to re-execute
    regs_.wz = regs_.pc + 1;
    transition_to_fetch();
    return pins;
}

// ========================================================================
// BLOCK SEARCH: CPI/CPD — 16T (M1:4 + M1:4 + read:3 + internal:5)
// ========================================================================
bus_state_t op_cpi_cpd(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.hl);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: case 4: case 5: case 6: case 7: // 5 internal T-states
        return pins;
    }
    {
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        uint8_t val = data_latch_;
        uint8_t result = regs_.a - val;
        uint8_t hc = (regs_.a ^ val ^ result) & Flags::H;
        uint8_t n = result - (hc ? 1 : 0);
        regs_.hl += dir;
        regs_.bc--;
        regs_.f = (regs_.f & Flags::C)
                | Flags::N
                | (result ? 0 : Flags::Z)
                | (result & Flags::S)
                | hc
                | (regs_.bc ? Flags::PV : 0)
                | (n & Flags::X)
                | ((n << 4) & Flags::Y);
        regs_.wz += dir;
    }
    transition_to_fetch();
    return pins;
}

// ========================================================================
// BLOCK SEARCH: CPIR/CPDR — 21/16T
// ========================================================================
bus_state_t op_cpir_cpdr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.hl);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: case 4: case 5: case 6: case 7:
        if (step_ == 8) {
            int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
            uint8_t val = data_latch_;
            uint8_t result = regs_.a - val;
            uint8_t hc = (regs_.a ^ val ^ result) & Flags::H;
            uint8_t n = result - (hc ? 1 : 0);
            regs_.hl += dir;
            regs_.bc--;
            regs_.f = (regs_.f & Flags::C)
                    | Flags::N
                    | (result ? 0 : Flags::Z)
                    | (result & Flags::S)
                    | hc
                    | (regs_.bc ? Flags::PV : 0)
                    | (n & Flags::X)
                    | ((n << 4) & Flags::Y);
            regs_.wz += dir;
            if (regs_.bc == 0 || result == 0) {
                transition_to_fetch();
            }
        }
        return pins;
    case 8: case 9: case 10: case 11: case 12:
        return pins;
    }
    regs_.pc -= 2;
    regs_.wz = regs_.pc + 1;
    transition_to_fetch();
    return pins;
}

// ========================================================================
// BLOCK I/O: INI/IND — 16T (M1:4 + M1:5 + io:4 + write:3)
// ========================================================================
bus_state_t op_ini_ind(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins; // 1 extra internal T-state
    case 1: return bus_setup_io_read(pins, regs_.bc);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3: return pins; // IO extra wait
    case 4:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_io(pins);
        return pins;
    case 5: return bus_setup_mem_write(pins, regs_.hl, data_latch_);
    case 6: if (!wait_check(pins)) return pins; return pins;
    case 7: {
        bus_finish_mem(pins);
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_.hl += dir;
        regs_.b--;
        regs_.f = sz53_table[regs_.b]
                | Flags::N // TODO: full undocumented flag behavior
                | (regs_.b == 0 ? Flags::Z : 0);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// BLOCK I/O: INIR/INDR — 21/16T
// ========================================================================
bus_state_t op_inir_indr(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins;
    case 1: return bus_setup_io_read(pins, regs_.bc);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3: return pins;
    case 4:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_io(pins);
        return pins;
    case 5: return bus_setup_mem_write(pins, regs_.hl, data_latch_);
    case 6: if (!wait_check(pins)) return pins; return pins;
    case 7: {
        bus_finish_mem(pins);
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_.hl += dir;
        regs_.b--;
        regs_.f = sz53_table[regs_.b]
                | Flags::N
                | (regs_.b == 0 ? Flags::Z : 0);
        if (regs_.b == 0) {
            transition_to_fetch();
        }
        return pins;
    }
    case 8: case 9: case 10: case 11: case 12:
        return pins;
    }
    regs_.pc -= 2;
    transition_to_fetch();
    return pins;
}

// ========================================================================
// BLOCK I/O: OUTI/OUTD — 16T (M1:4 + M1:5 + read:3 + io:4)
// ========================================================================
bus_state_t op_outi_outd(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins; // 1 extra internal
    case 1: return bus_setup_mem_read(pins, regs_.hl);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 4: return bus_setup_io_write(pins, regs_.bc, data_latch_);
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6: return pins;
    case 7: {
        bus_finish_io(pins);
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_.hl += dir;
        regs_.b--;
        regs_.f = sz53_table[regs_.b]
                | Flags::N
                | (regs_.b == 0 ? Flags::Z : 0);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// BLOCK I/O: OTIR/OTDR — 21/16T
// ========================================================================
bus_state_t op_otir_otdr(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins;
    case 1: return bus_setup_mem_read(pins, regs_.hl);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 4: return bus_setup_io_write(pins, regs_.bc, data_latch_);
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6: return pins;
    case 7: {
        bus_finish_io(pins);
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_.hl += dir;
        regs_.b--;
        regs_.f = sz53_table[regs_.b]
                | Flags::N
                | (regs_.b == 0 ? Flags::Z : 0);
        if (regs_.b == 0) {
            transition_to_fetch();
        }
        return pins;
    }
    case 8: case 9: case 10: case 11: case 12:
        return pins;
    }
    regs_.pc -= 2;
    transition_to_fetch();
    return pins;
}

#endif // Z80_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
