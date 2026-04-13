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

#include "chip/cpu/z80/operations/inc_lint_prevention.hpp"

#ifndef Z80_SKIP_IMPLEMENTATION

// ========================================================================
// ED: IN r,(C) — 12T (M1:4 + M1:4 + io:4)
// ========================================================================
bus_state_t op_in_r_c(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_io_read(pins, regs_[BC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2: return pins; // IO extra wait
    case 3: {
        uint8_t val = BUS_GET_DATA(pins);
        bus_finish_io(pins);
        regs_[WZ] = regs_[BC] + 1; // WZ = BC + 1 (before register write modifies BC)
        uint8_t y = (ed_opcode_ >> 3) & 7;
        if (y != 6) set_reg8_direct(y, val); // IN (C) just sets flags, discards value
        regs_[F] = (regs_[F] & Fl::C) | sz53p_table[val];
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
    case 0: return bus_setup_io_write(pins, regs_[BC], val);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2: return pins;
    case 3:
        bus_finish_io(pins);
        regs_[WZ] = regs_[BC] + 1;
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
    case 0: case 1: case 2: case 3: case 4: case 5:
        return pins;
    case 6: {
        uint8_t p = (ed_opcode_ >> 4) & 3;
        uint16_t val = get_reg16(p);
        regs_[WZ] = regs_[HL] + 1; // WZ = HL_before + 1
        if (ed_opcode_ & 0x08) {
            alu_adc16(val);
        } else {
            alu_sbc16(val);
        }
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// ED: LD (nn),rr — 20T (M1:4 + M1:4 + read_lo:3 + read_hi:3 + write_lo:3 + write_hi:3)
// ========================================================================
bus_state_t op_ld_nn_rr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[PC]);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_[PC]++;
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
        regs_[WZ] = addr_latch_ + 1;
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
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[PC]);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_[PC]++;
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
        regs_[WZ] = addr_latch_ + 1;
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
    case 0: // 1 internal T-state
        if (ed_opcode_ == 0x57) {
            regs_[A] = regs_[I];
        } else {
            regs_[A] = (regs_[R] & 0x80) | (regs_[R] & 0x7F);
        }
        regs_[F] = (regs_[F] & Fl::C)
                | sz53_table[regs_[A]]
                | (iff2_ ? Fl::PV : 0);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// ED: LD I,A / LD R,A — 9T (M1:4 + M1:4 + internal:1)
// ========================================================================
bus_state_t op_ld_ir_a(bus_state_t pins) {
    switch (step_++) {
    case 0:
        if (ed_opcode_ == 0x47) {
            regs_[I] = regs_[A];
        } else {
            regs_[R] = regs_[A];
        }
        transition_to_fetch();
        return pins;
    }
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
    case 0: return bus_setup_mem_read(pins, regs_[SP]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_[SP]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[SP]);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_[SP]++;
        bus_finish_mem(pins);
        iff1_ = iff2_;
        regs_[PC] = addr_latch_;
        regs_[WZ] = addr_latch_;
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
    case 0: return bus_setup_mem_read(pins, regs_[HL]);
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
        return bus_setup_mem_write(pins, regs_[HL], data_latch_);
    }
    case 8: if (!wait_check(pins)) return pins; return pins;
    case 9:
        bus_finish_mem(pins);
        regs_[WZ] = regs_[HL] + 1;
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
    case 0: return bus_setup_mem_read(pins, regs_[HL]);
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
        return bus_setup_mem_write(pins, regs_[HL], data_latch_);
    }
    case 8: if (!wait_check(pins)) return pins; return pins;
    case 9:
        bus_finish_mem(pins);
        regs_[WZ] = regs_[HL] + 1;
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
    case 0: return bus_setup_mem_read(pins, regs_[HL]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_write(pins, regs_[DE], data_latch_);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        bus_finish_mem(pins);
        return pins;
    case 6: return pins; // 1st internal T-state
    case 7: { // 2nd internal T-state
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_[HL] += dir;
        regs_[DE] += dir;
        regs_[BC]--;
        uint8_t n = data_latch_ + regs_[A];
        regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::C))
                | (regs_[BC] ? Fl::PV : 0)
                | (n & Fl::X)
                | ((n << 4) & Fl::Y);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// BLOCK TRANSFER: LDIR/LDDR — 21/16T (16T base + 5T if BC != 0)
// ========================================================================
bus_state_t op_ldir_lddr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[HL]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_write(pins, regs_[DE], data_latch_);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        bus_finish_mem(pins);
        return pins;
    case 6: return pins; // 1st internal T-state
    case 7: { // 2nd internal T-state
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_[HL] += dir;
        regs_[DE] += dir;
        regs_[BC]--;
        uint8_t n = data_latch_ + regs_[A];
        regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::C))
                | (regs_[BC] ? Fl::PV : 0)
                | (n & Fl::X)
                | ((n << 4) & Fl::Y);
        if (regs_[BC] == 0) {
            transition_to_fetch(); // 16T total
            return pins;
        }
        // If BC != 0, continue with 5 more internal T-states
        return pins;
    }
    case 8: case 9: case 10: case 11: // 4 internal T-states (repeat)
        return pins;
    case 12: { // 5th internal T-state (repeat)
        regs_[PC] -= 2; // Back up to re-execute
        regs_[WZ] = regs_[PC] + 1;
        // In repeat path, Y/X come from PCi high byte (David Banks)
        uint8_t pch = static_cast<uint8_t>(regs_[PC] >> 8);
        regs_[F] = (regs_[F] & ~(Fl::Y | Fl::X)) | (pch & (Fl::Y | Fl::X));
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// BLOCK SEARCH: CPI/CPD — 16T (M1:4 + M1:4 + read:3 + internal:5)
// ========================================================================
bus_state_t op_cpi_cpd(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[HL]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: case 4: case 5: case 6: // 4 internal T-states
        return pins;
    case 7: { // 5th internal T-state
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        uint8_t val = data_latch_;
        uint8_t result = regs_[A] - val;
        uint8_t hc = (regs_[A] ^ val ^ result) & Fl::H;
        uint8_t n = result - (hc ? 1 : 0);
        regs_[HL] += dir;
        regs_[BC]--;
        regs_[F] = (regs_[F] & Fl::C)
                | Fl::N
                | (result ? 0 : Fl::Z)
                | (result & Fl::S)
                | hc
                | (regs_[BC] ? Fl::PV : 0)
                | (n & Fl::X)
                | ((n << 4) & Fl::Y);
        regs_[WZ] += dir;
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// BLOCK SEARCH: CPIR/CPDR — 21/16T
// ========================================================================
bus_state_t op_cpir_cpdr(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[HL]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: case 4: case 5: case 6: // 4 internal T-states
        return pins;
    case 7: { // 5th internal T-state
        int16_t dir = (ed_opcode_ & 0x08) ? -1 : 1;
        uint8_t val = data_latch_;
        uint8_t result = regs_[A] - val;
        uint8_t hc = (regs_[A] ^ val ^ result) & Fl::H;
        uint8_t n = result - (hc ? 1 : 0);
        regs_[HL] += dir;
        regs_[BC]--;
        regs_[F] = (regs_[F] & Fl::C)
                | Fl::N
                | (result ? 0 : Fl::Z)
                | (result & Fl::S)
                | hc
                | (regs_[BC] ? Fl::PV : 0)
                | (n & Fl::X)
                | ((n << 4) & Fl::Y);
        regs_[WZ] += dir;
        if (regs_[BC] == 0 || result == 0) {
            transition_to_fetch();
            return pins;
        }
        return pins;
    }
    case 8: case 9: case 10: case 11: // 4 internal T-states (repeat)
        return pins;
    case 12: { // 5th internal T-state (repeat)
        regs_[PC] -= 2;
        regs_[WZ] = regs_[PC] + 1;
        // In repeat path, Y/X come from PCi high byte (David Banks)
        uint8_t pch = static_cast<uint8_t>(regs_[PC] >> 8);
        regs_[F] = (regs_[F] & ~(Fl::Y | Fl::X)) | (pch & (Fl::Y | Fl::X));
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// BLOCK I/O: INI/IND — 16T (M1:4 + M1:5 + io:4 + write:3)
// ========================================================================
bus_state_t op_ini_ind(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins; // 1 extra internal T-state
    case 1: return bus_setup_io_read(pins, regs_[BC]);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3: return pins; // IO extra wait
    case 4:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_io(pins);
        return pins;
    case 5: return bus_setup_mem_write(pins, regs_[HL], data_latch_);
    case 6: if (!wait_check(pins)) return pins; return pins;
    case 7: {
        bus_finish_mem(pins);
        int dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_[HL] += dir;
        // WZ = BC ± 1 (using original B before decrement)
        regs_[WZ] = regs_[BC] + dir;
        regs_[B]--;
        // Undocumented flags for block I/O input
        uint16_t k = static_cast<uint16_t>(data_latch_) + ((regs_[C] + dir) & 0xFF);
        regs_[F] = sz53_table[regs_[B]]
                | ((data_latch_ & 0x80) ? Fl::N : 0)
                | ((k > 0xFF) ? (Fl::H | Fl::C) : 0)
                | parity_table[(k & 7) ^ regs_[B]];
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
    case 1: return bus_setup_io_read(pins, regs_[BC]);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3: return pins;
    case 4:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_io(pins);
        return pins;
    case 5: return bus_setup_mem_write(pins, regs_[HL], data_latch_);
    case 6: if (!wait_check(pins)) return pins; return pins;
    case 7: {
        bus_finish_mem(pins);
        int dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_[HL] += dir;
        // WZ = BC ± 1 (using original B before decrement)
        regs_[WZ] = regs_[BC] + dir;
        regs_[B]--;
        uint16_t k = static_cast<uint16_t>(data_latch_) + ((regs_[C] + dir) & 0xFF);
        bool hc = k > 0xFF;
        uint8_t nf = (data_latch_ & 0x80) ? Fl::N : 0;
        uint8_t hcf = hc ? (Fl::H | Fl::C) : 0;
        if (regs_[B] == 0) {
            // Non-repeat: standard flags (same as INI/IND)
            regs_[F] = sz53_table[regs_[B]] | nf | hcf
                    | parity_table[(k & 7) ^ regs_[B]];
            transition_to_fetch();
            return pins;
        }
        // Repeat: S from B, Y/X from PCi high byte (rewound PC), complex H/PV from David Banks
        // Reference: redcode/Z80 (Manuel Sainz) — INXR_OTXR_COMMON
        {
            uint8_t pch = static_cast<uint8_t>((regs_[PC] - 2) >> 8); // PCi = rewound PC
            uint8_t p = (k & 7) ^ regs_[B];
            uint8_t pv_hf;
            if (hc) {
                if (nf) {
                    pv_hf = (!(regs_[B] & 0x0F) ? Fl::H : 0)
                          | parity_table[p ^ ((regs_[B] - 1) & 7)];
                } else {
                    pv_hf = ((regs_[B] & 0x0F) == 0x0F ? Fl::H : 0)
                          | parity_table[p ^ ((regs_[B] + 1) & 7)];
                }
            } else {
                pv_hf = parity_table[p ^ (regs_[B] & 7)];
            }
            regs_[F] = (regs_[B] & Fl::S)
                    | (pch & (Fl::Y | Fl::X))
                    | nf | (hc ? Fl::C : 0)
                    | pv_hf;
        }
        return pins;
    }
    case 8: case 9: case 10: case 11: // 4 internal T-states (repeat)
        return pins;
    case 12: // 5th internal T-state (repeat)
        regs_[PC] -= 2;
        regs_[WZ] = regs_[PC] + 1;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// BLOCK I/O: OUTI/OUTD — 16T (M1:4 + M1:5 + read:3 + io:4)
// ========================================================================
bus_state_t op_outi_outd(bus_state_t pins) {
    switch (step_++) {
    case 0:
        regs_[B]--; // B decremented first for output instructions
        return pins;
    case 1: return bus_setup_mem_read(pins, regs_[HL]);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 4: return bus_setup_io_write(pins, regs_[BC], data_latch_);
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6: return pins;
    case 7: {
        bus_finish_io(pins);
        int dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_[HL] += dir;
        // WZ = BC ± 1 (B already decremented)
        regs_[WZ] = regs_[BC] + dir;
        // Undocumented flags for block I/O output: k = data + L (L after HL change)
        uint16_t k = static_cast<uint16_t>(data_latch_) + regs_[L];
        regs_[F] = sz53_table[regs_[B]]
                | ((data_latch_ & 0x80) ? Fl::N : 0)
                | ((k > 0xFF) ? (Fl::H | Fl::C) : 0)
                | parity_table[(k & 7) ^ regs_[B]];
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
    case 0:
        regs_[B]--; // B decremented first for output instructions
        return pins;
    case 1: return bus_setup_mem_read(pins, regs_[HL]);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 4: return bus_setup_io_write(pins, regs_[BC], data_latch_);
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6: return pins;
    case 7: {
        bus_finish_io(pins);
        int dir = (ed_opcode_ & 0x08) ? -1 : 1;
        regs_[HL] += dir;
        // WZ = BC ± 1 (B already decremented)
        regs_[WZ] = regs_[BC] + dir;
        uint16_t k = static_cast<uint16_t>(data_latch_) + regs_[L];
        bool hc = k > 0xFF;
        uint8_t nf = (data_latch_ & 0x80) ? Fl::N : 0;
        uint8_t hcf = hc ? (Fl::H | Fl::C) : 0;
        if (regs_[B] == 0) {
            // Non-repeat: standard flags (same as OUTI/OUTD)
            regs_[F] = sz53_table[regs_[B]] | nf | hcf
                    | parity_table[(k & 7) ^ regs_[B]];
            transition_to_fetch();
            return pins;
        }
        // Repeat: S from B, Y/X from PCi high byte (rewound PC), complex H/PV from David Banks
        // Reference: redcode/Z80 (Manuel Sainz) — INXR_OTXR_COMMON
        {
            uint8_t pch = static_cast<uint8_t>((regs_[PC] - 2) >> 8); // PCi = rewound PC
            uint8_t p = (k & 7) ^ regs_[B];
            uint8_t pv_hf;
            if (hc) {
                if (nf) {
                    pv_hf = (!(regs_[B] & 0x0F) ? Fl::H : 0)
                          | parity_table[p ^ ((regs_[B] - 1) & 7)];
                } else {
                    pv_hf = ((regs_[B] & 0x0F) == 0x0F ? Fl::H : 0)
                          | parity_table[p ^ ((regs_[B] + 1) & 7)];
                }
            } else {
                pv_hf = parity_table[p ^ (regs_[B] & 7)];
            }
            regs_[F] = (regs_[B] & Fl::S)
                    | (pch & (Fl::Y | Fl::X))
                    | nf | (hc ? Fl::C : 0)
                    | pv_hf;
        }
        return pins;
    }
    case 8: case 9: case 10: case 11: // 4 internal T-states (repeat)
        return pins;
    case 12: // 5th internal T-state (repeat)
        regs_[PC] -= 2;
        regs_[WZ] = regs_[PC] + 1;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

#endif // Z80_SKIP_IMPLEMENTATION

#include "chip/cpu/z80/operations/inc_lint_prevention_footer.hpp"
