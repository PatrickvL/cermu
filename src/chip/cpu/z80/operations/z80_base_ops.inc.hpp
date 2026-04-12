/*
 * z80_base_ops.inc.hpp — Base (unprefixed) Instruction Handlers for Z80
 *
 * Contains handlers for all instructions that follow the M1 fetch cycle.
 * Each handler manages the remaining M-cycles of its instruction.
 * Step counter (step_) starts at 0 when the handler is entered.
 *
 * Naming: op_<mnemonic> for instruction handlers.
 *
 * Included inside z80_t class with Z80_TEMPLATE_CONTEXT defined.
 */

#include "chip/cpu/z80/operations/inc_lint_prevention.hpp"

#ifndef Z80_SKIP_IMPLEMENTATION

// ========================================================================
// 8-BIT LOAD: LD r,n — 7T (M1:4 + read:3)
// ========================================================================
bus_state_t op_ld_r_n(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        set_reg8((opcode_ >> 3) & 7, BUS_GET_DATA(pins));
        regs_[PC]++;
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// 8-BIT LOAD: LD r,(HL) — 7T (M1:4 + read:3)
// With DD/FD: LD r,(IX+d) / LD r,(IY+d) — 19T (M1:4 + read_d:3 + idle:5 + read:3)
// ========================================================================
bus_state_t op_ld_r_hl(bus_state_t pins) {
    if (!has_ix_iy_prefix()) {
        // Standard LD r,(HL) — 3 extra T-states
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[HL]);
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            set_reg8_direct((opcode_ >> 3) & 7, BUS_GET_DATA(pins));
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    } else {
        // LD r,(IX+d) / LD r,(IY+d) — 15 extra T-states
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[PC]); // Read displacement
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
            regs_[PC]++;
            bus_finish_mem(pins);
            return pins;
        case 3: case 4: case 5: case 6: case 7: // 5 idle T-states
            return pins;
        case 8: {
            uint16_t addr = get_hl_addr();
            regs_[WZ] = addr;
            return bus_setup_mem_read(pins, addr); // Read from (IX+d)
        }
        case 9: if (!wait_check(pins)) return pins; return pins;
        case 10:
            set_reg8_direct((opcode_ >> 3) & 7, BUS_GET_DATA(pins));
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    }
    return pins;
}

// ========================================================================
// 8-BIT LOAD: LD (HL),r — 7T (M1:4 + write:3)
// With DD/FD: LD (IX+d),r / LD (IY+d),r — 19T
// ========================================================================
bus_state_t op_ld_hl_r(bus_state_t pins) {
    if (!has_ix_iy_prefix()) {
        switch (step_++) {
        case 0: return bus_setup_mem_write(pins, regs_[HL], get_reg8_direct(opcode_ & 7));
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    } else {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[PC]); // Read displacement
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
            regs_[PC]++;
            bus_finish_mem(pins);
            return pins;
        case 3: case 4: case 5: case 6: case 7:
            return pins;
        case 8: {
            uint16_t addr = get_hl_addr();
            regs_[WZ] = addr;
            return bus_setup_mem_write(pins, addr, get_reg8_direct(opcode_ & 7));
        }
        case 9: if (!wait_check(pins)) return pins; return pins;
        case 10:
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    }
    return pins;
}

// ========================================================================
// 8-BIT LOAD: LD (HL),n — 10T (M1:4 + read_n:3 + write:3)
// ========================================================================
bus_state_t op_ld_hl_n(bus_state_t pins) {
    if (!has_ix_iy_prefix()) {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[PC]); // Read immediate n
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            data_latch_ = BUS_GET_DATA(pins);
            regs_[PC]++;
            bus_finish_mem(pins);
            return pins;
        case 3: return bus_setup_mem_write(pins, regs_[HL], data_latch_);
        case 4: if (!wait_check(pins)) return pins; return pins;
        case 5:
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    } else {
        // LD (IX+d),n — 19T (M1:4 + read_d:3 + read_n:3 + idle:2 + write:3)
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[PC]); // Read displacement
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
            regs_[PC]++;
            bus_finish_mem(pins);
            return pins;
        case 3: return bus_setup_mem_read(pins, regs_[PC]); // Read n
        case 4: if (!wait_check(pins)) return pins; return pins;
        case 5:
            data_latch_ = BUS_GET_DATA(pins);
            regs_[PC]++;
            bus_finish_mem(pins);
            return pins;
        case 6: case 7: // 2 idle T-states
            return pins;
        case 8: {
            uint16_t addr = get_hl_addr();
            regs_[WZ] = addr;
            return bus_setup_mem_write(pins, addr, data_latch_);
        }
        case 9: if (!wait_check(pins)) return pins; return pins;
        case 10:
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    }
    return pins;
}

// ========================================================================
// 8-BIT LOAD: LD A,(BC) / LD A,(DE) — 7T (M1:4 + read:3)
// ========================================================================
bus_state_t op_ld_a_indirect(bus_state_t pins) {
    switch (step_++) {
    case 0: {
        uint16_t addr = (opcode_ & 0x10) ? regs_[DE] : regs_[BC];
        return bus_setup_mem_read(pins, addr);
    }
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        regs_[A] = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        // WZ = addr + 1
        uint16_t addr = (opcode_ & 0x10) ? regs_[DE] : regs_[BC];
        regs_[WZ] = addr + 1;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// 8-BIT LOAD: LD (BC),A / LD (DE),A — 7T (M1:4 + write:3)
// ========================================================================
bus_state_t op_ld_indirect_a(bus_state_t pins) {
    switch (step_++) {
    case 0: {
        uint16_t addr = (opcode_ & 0x10) ? regs_[DE] : regs_[BC];
        return bus_setup_mem_write(pins, addr, regs_[A]);
    }
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2: {
        bus_finish_mem(pins);
        // WZ: low byte = (addr + 1) & 0xFF, high byte = A
        uint16_t addr = (opcode_ & 0x10) ? regs_[DE] : regs_[BC];
        regs_[WZ] = (static_cast<uint16_t>(regs_[A]) << 8) | ((addr + 1) & 0xFF);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// 8-BIT LOAD: LD A,(nn) — 13T (M1:4 + read_lo:3 + read_hi:3 + read_data:3)
// ========================================================================
bus_state_t op_ld_a_nn(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]); // Read addr low
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[PC]); // Read addr high
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 6: return bus_setup_mem_read(pins, addr_latch_); // Read data
    case 7: if (!wait_check(pins)) return pins; return pins;
    case 8:
        regs_[A] = BUS_GET_DATA(pins);
        regs_[WZ] = addr_latch_ + 1;
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// 8-BIT LOAD: LD (nn),A — 13T
// ========================================================================
bus_state_t op_ld_nn_a(bus_state_t pins) {
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
    case 6: return bus_setup_mem_write(pins, addr_latch_, regs_[A]);
    case 7: if (!wait_check(pins)) return pins; return pins;
    case 8:
        bus_finish_mem(pins);
        // WZ: low byte = (nn + 1) & 0xFF, high byte = A
        regs_[WZ] = (static_cast<uint16_t>(regs_[A]) << 8) | ((addr_latch_ + 1) & 0xFF);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// 16-BIT LOAD: LD rr,nn — 10T (M1:4 + read_lo:3 + read_hi:3)
// ========================================================================
bus_state_t op_ld_rr_nn(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[PC]);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5: {
        uint16_t val = data_latch_ | (static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8);
        regs_[PC]++;
        bus_finish_mem(pins);
        set_reg16((opcode_ >> 4) & 3, val);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// 16-BIT LOAD: LD (nn),HL — 16T (M1:4 + read_lo:3 + read_hi:3 + write_lo:3 + write_hi:3)
// ========================================================================
bus_state_t op_ld_nn_hl(bus_state_t pins) {
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
    case 6: return bus_setup_mem_write(pins, addr_latch_, static_cast<uint8_t>(get_hl() & 0xFF));
    case 7: if (!wait_check(pins)) return pins; return pins;
    case 8:
        bus_finish_mem(pins);
        return pins;
    case 9: return bus_setup_mem_write(pins, addr_latch_ + 1, static_cast<uint8_t>(get_hl() >> 8));
    case 10: if (!wait_check(pins)) return pins; return pins;
    case 11:
        regs_[WZ] = addr_latch_ + 1;
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// 16-BIT LOAD: LD HL,(nn) — 16T
// ========================================================================
bus_state_t op_ld_hl_nn(bus_state_t pins) {
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
        regs_[WZ] = addr_latch_ + 1;
        bus_finish_mem(pins);
        set_hl(val);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// 16-BIT LOAD: LD SP,HL — 6T (M1:4 + internal:2)
// ========================================================================
bus_state_t op_ld_sp_hl(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins; // 1st internal T-state
    case 1: // 2nd internal T-state
        regs_[SP] = get_hl();
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// 8-BIT ALU: ADD/ADC/SUB/SBC/AND/XOR/OR/CP n — 7T (M1:4 + read:3)
// ========================================================================
bus_state_t op_alu_n(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        alu_op((opcode_ >> 3) & 7, BUS_GET_DATA(pins));
        regs_[PC]++;
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// 8-BIT ALU: ADD/ADC/SUB/SBC/AND/XOR/OR/CP (HL) — 7T (M1:4 + read:3)
// With DD/FD: uses (IX+d)/(IY+d) — 19T
// ========================================================================
bus_state_t op_alu_hl(bus_state_t pins) {
    if (!has_ix_iy_prefix()) {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[HL]);
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            alu_op((opcode_ >> 3) & 7, BUS_GET_DATA(pins));
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    } else {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[PC]);
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
            regs_[PC]++;
            bus_finish_mem(pins);
            return pins;
        case 3: case 4: case 5: case 6: case 7:
            return pins;
        case 8: {
            uint16_t addr = get_hl_addr();
            regs_[WZ] = addr;
            return bus_setup_mem_read(pins, addr);
        }
        case 9: if (!wait_check(pins)) return pins; return pins;
        case 10:
            alu_op((opcode_ >> 3) & 7, BUS_GET_DATA(pins));
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    }
    return pins;
}

// ========================================================================
// 8-BIT INC/DEC (HL) — 11T (M1:4 + read:3 + write:3 + internal:1)
// ========================================================================
bus_state_t op_inc_hl(bus_state_t pins) {
    if (!has_ix_iy_prefix()) {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[HL]);
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            data_latch_ = alu_inc(BUS_GET_DATA(pins));
            bus_finish_mem(pins);
            return pins;
        case 3: return pins; // 1 internal T-state
        case 4: return bus_setup_mem_write(pins, regs_[HL], data_latch_);
        case 5: if (!wait_check(pins)) return pins; return pins;
        case 6:
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    } else {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[PC]);
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
            regs_[PC]++;
            bus_finish_mem(pins);
            return pins;
        case 3: case 4: case 5: case 6: case 7:
            return pins;
        case 8: {
            uint16_t addr = get_hl_addr();
            regs_[WZ] = addr;
            addr_latch_ = addr;
            return bus_setup_mem_read(pins, addr);
        }
        case 9: if (!wait_check(pins)) return pins; return pins;
        case 10:
            data_latch_ = alu_inc(BUS_GET_DATA(pins));
            bus_finish_mem(pins);
            return pins;
        case 11: return pins;
        case 12: return bus_setup_mem_write(pins, addr_latch_, data_latch_);
        case 13: if (!wait_check(pins)) return pins; return pins;
        case 14:
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    }
    return pins;
}

bus_state_t op_dec_hl(bus_state_t pins) {
    if (!has_ix_iy_prefix()) {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[HL]);
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            data_latch_ = alu_dec(BUS_GET_DATA(pins));
            bus_finish_mem(pins);
            return pins;
        case 3: return pins;
        case 4: return bus_setup_mem_write(pins, regs_[HL], data_latch_);
        case 5: if (!wait_check(pins)) return pins; return pins;
        case 6:
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    } else {
        switch (step_++) {
        case 0: return bus_setup_mem_read(pins, regs_[PC]);
        case 1: if (!wait_check(pins)) return pins; return pins;
        case 2:
            displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
            regs_[PC]++;
            bus_finish_mem(pins);
            return pins;
        case 3: case 4: case 5: case 6: case 7:
            return pins;
        case 8: {
            uint16_t addr = get_hl_addr();
            regs_[WZ] = addr;
            addr_latch_ = addr;
            return bus_setup_mem_read(pins, addr);
        }
        case 9: if (!wait_check(pins)) return pins; return pins;
        case 10:
            data_latch_ = alu_dec(BUS_GET_DATA(pins));
            bus_finish_mem(pins);
            return pins;
        case 11: return pins;
        case 12: return bus_setup_mem_write(pins, addr_latch_, data_latch_);
        case 13: if (!wait_check(pins)) return pins; return pins;
        case 14:
            bus_finish_mem(pins);
            transition_to_fetch();
            return pins;
        }
    }
    return pins;
}

// ========================================================================
// 16-BIT INC/DEC rr — 6T (M1:4 + internal:2)
// ========================================================================
bus_state_t op_inc_rr(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins;
    case 1: {
        uint8_t p = (opcode_ >> 4) & 3;
        set_reg16(p, get_reg16(p) + 1);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

bus_state_t op_dec_rr(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins;
    case 1: {
        uint8_t p = (opcode_ >> 4) & 3;
        set_reg16(p, get_reg16(p) - 1);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// 16-BIT ADD HL,rr — 11T (M1:4 + internal:7)
// ========================================================================
bus_state_t op_add_hl_rr(bus_state_t pins) {
    switch (step_++) {
    case 0: case 1: case 2: case 3: case 4: case 5:
        return pins;
    case 6: {
        uint8_t p = (opcode_ >> 4) & 3;
        uint16_t hl = get_hl();
        regs_[WZ] = hl + 1; // WZ = HL_before + 1
        alu_add16(hl, get_reg16(p));
        set_hl(hl);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// STACK: PUSH rr — 11T (M1:4 + internal:1 + write_hi:3 + write_lo:3)
// ========================================================================
bus_state_t op_push(bus_state_t pins) {
    uint8_t p = (opcode_ >> 4) & 3;
    uint16_t val = get_reg16_af(p);
    switch (step_++) {
    case 0: return pins; // 1 internal T-state
    case 1:
        regs_[SP]--;
        return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(val >> 8));
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3:
        bus_finish_mem(pins);
        regs_[SP]--;
        return pins;
    case 4: return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(val & 0xFF));
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6:
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// STACK: POP rr — 10T (M1:4 + read_lo:3 + read_hi:3)
// ========================================================================
bus_state_t op_pop(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[SP]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        regs_[SP]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[SP]);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5: {
        uint16_t val = data_latch_ | (static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8);
        regs_[SP]++;
        bus_finish_mem(pins);
        uint8_t p = (opcode_ >> 4) & 3;
        set_reg16_af(p, val);
        transition_to_fetch();
        return pins;
    }
    }
    return pins;
}

// ========================================================================
// EXCHANGE: EX (SP),HL — 19T (M1:4 + read_lo:3 + read_hi:4 + write_hi:3 + write_lo:5)
// ========================================================================
bus_state_t op_ex_sp_hl(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[SP]);       // Read low from (SP)
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[SP] + 1);   // Read high from (SP+1)
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ = data_latch_ | (static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8);
        bus_finish_mem(pins);
        return pins;
    case 6: return pins; // 1 internal T-state
    case 7: return bus_setup_mem_write(pins, regs_[SP] + 1, static_cast<uint8_t>(get_hl() >> 8));
    case 8: if (!wait_check(pins)) return pins; return pins;
    case 9:
        bus_finish_mem(pins);
        return pins;
    case 10: return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(get_hl() & 0xFF));
    case 11: if (!wait_check(pins)) return pins; return pins;
    case 12:
        bus_finish_mem(pins);
        return pins;
    case 13: return pins; // 1st internal T-state
    case 14: // 2nd internal T-state
        set_hl(addr_latch_);
        regs_[WZ] = addr_latch_;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// JUMP: JP nn — 10T (M1:4 + read_lo:3 + read_hi:3)
// ========================================================================
bus_state_t op_jp_nn(bus_state_t pins) {
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
        regs_[PC] = addr_latch_;
        regs_[WZ] = addr_latch_;
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// JUMP: JP cc,nn — 10T (always reads both bytes)
// ========================================================================
bus_state_t op_jp_cc_nn(bus_state_t pins) {
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
        regs_[WZ] = addr_latch_;
        if (test_cc((opcode_ >> 3) & 7)) {
            regs_[PC] = addr_latch_;
        }
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// JUMP RELATIVE: JR e — 12T (M1:4 + read:3 + internal:5)
// ========================================================================
bus_state_t op_jr_e(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: case 4: case 5: case 6: // 4 internal T-states
        return pins;
    case 7: // 5th internal T-state
        regs_[PC] = static_cast<uint16_t>(regs_[PC] + displacement_);
        regs_[WZ] = regs_[PC];
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// JUMP RELATIVE: JR cc,e — 12/7T (conditional: 5 extra if taken)
// ========================================================================
bus_state_t op_jr_cc_e(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
        regs_[PC]++;
        bus_finish_mem(pins);
        // Condition is encoded in bits 4-3 of opcode (cc2: 0=NZ, 1=Z, 2=NC, 3=C)
        if (!test_cc2(((opcode_ >> 3) & 3) + (opcode_ & 0x08 ? 0 : 0))) {
            // Not taken: 7T total (M1:4 + read:3)
            transition_to_fetch();
        }
        return pins;
    case 3: case 4: case 5: case 6: // 4 internal T-states (branch taken)
        return pins;
    case 7: // 5th internal T-state
        regs_[PC] = static_cast<uint16_t>(regs_[PC] + displacement_);
        regs_[WZ] = regs_[PC];
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// DJNZ e — 13/8T (M1:5 + read:3 + internal:5 if taken, M1:5 + read:3 if not)
// Note: M1 is 5T not 4T (1 extra internal T-state at start)
// ========================================================================
bus_state_t op_djnz(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins; // 1 extra internal T-state (M1 was already 4T, this makes it 5T effective)
    case 1: return bus_setup_mem_read(pins, regs_[PC]);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3:
        displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
        regs_[PC]++;
        bus_finish_mem(pins);
        regs_[B]--;
        if (regs_[B] == 0) {
            transition_to_fetch(); // Not taken: 8T
        }
        return pins;
    case 4: case 5: case 6: case 7: // 4 internal T-states (branch taken)
        return pins;
    case 8: // 5th internal T-state
        regs_[PC] = static_cast<uint16_t>(regs_[PC] + displacement_);
        regs_[WZ] = regs_[PC];
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// CALL nn — 17T (M1:4 + read_lo:3 + read_hi:4 + push_hi:3 + push_lo:3)
// ========================================================================
bus_state_t op_call_nn(bus_state_t pins) {
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
    case 6: return pins; // 1 internal T-state
    case 7:
        regs_[SP]--;
        return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(regs_[PC] >> 8));
    case 8: if (!wait_check(pins)) return pins; return pins;
    case 9:
        bus_finish_mem(pins);
        regs_[SP]--;
        return pins;
    case 10: return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(regs_[PC] & 0xFF));
    case 11: if (!wait_check(pins)) return pins; return pins;
    case 12:
        bus_finish_mem(pins);
        regs_[PC] = addr_latch_;
        regs_[WZ] = addr_latch_;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// CALL cc,nn — 17/10T (10T if not taken: reads address but doesn't push)
// ========================================================================
bus_state_t op_call_cc_nn(bus_state_t pins) {
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
        regs_[WZ] = addr_latch_;
        if (!test_cc((opcode_ >> 3) & 7)) {
            transition_to_fetch(); // Not taken: 10T
        }
        return pins;
    case 6: return pins; // 1 internal (taken path)
    case 7:
        regs_[SP]--;
        return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(regs_[PC] >> 8));
    case 8: if (!wait_check(pins)) return pins; return pins;
    case 9:
        bus_finish_mem(pins);
        regs_[SP]--;
        return pins;
    case 10: return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(regs_[PC] & 0xFF));
    case 11: if (!wait_check(pins)) return pins; return pins;
    case 12:
        bus_finish_mem(pins);
        regs_[PC] = addr_latch_;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// RET — 10T (M1:4 + read_lo:3 + read_hi:3)
// ========================================================================
bus_state_t op_ret(bus_state_t pins) {
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
        regs_[PC] = addr_latch_;
        regs_[WZ] = addr_latch_;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// RET cc — 11/5T (M1:5 + read_lo:3 + read_hi:3 if taken, M1:5 if not)
// ========================================================================
bus_state_t op_ret_cc(bus_state_t pins) {
    switch (step_++) {
    case 0: // 1 extra internal T-state (5T M1 total) + condition check
        if (!test_cc((opcode_ >> 3) & 7)) {
            transition_to_fetch(); // Not taken: 5T
        }
        return pins;
    case 1: return bus_setup_mem_read(pins, regs_[SP]);
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_[SP]++;
        bus_finish_mem(pins);
        return pins;
    case 4: return bus_setup_mem_read(pins, regs_[SP]);
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_[SP]++;
        bus_finish_mem(pins);
        regs_[PC] = addr_latch_;
        regs_[WZ] = addr_latch_;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// RST p — 11T (M1:5 + push_hi:3 + push_lo:3)
// ========================================================================
bus_state_t op_rst(bus_state_t pins) {
    switch (step_++) {
    case 0: return pins; // 1 extra internal T-state
    case 1:
        regs_[SP]--;
        return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(regs_[PC] >> 8));
    case 2: if (!wait_check(pins)) return pins; return pins;
    case 3:
        bus_finish_mem(pins);
        regs_[SP]--;
        return pins;
    case 4: return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(regs_[PC] & 0xFF));
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6:
        bus_finish_mem(pins);
        regs_[PC] = opcode_ & 0x38; // RST target = y * 8
        regs_[WZ] = regs_[PC];
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// I/O: IN A,(n) — 11T (M1:4 + read_n:3 + io_read:4)
// ========================================================================
bus_state_t op_in_a_n(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: {
        uint16_t port = (static_cast<uint16_t>(regs_[A]) << 8) | data_latch_;
        addr_latch_ = port; // Save port address for WZ calculation
        return bus_setup_io_read(pins, port);
    }
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5: return pins; // IO automatic TW
    case 6:
        regs_[A] = BUS_GET_DATA(pins);
        bus_finish_io(pins);
        regs_[WZ] = addr_latch_ + 1; // WZ = port address + 1
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// I/O: OUT (n),A — 11T (M1:4 + read_n:3 + io_write:4)
// ========================================================================
bus_state_t op_out_n_a(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: {
        uint16_t port = (static_cast<uint16_t>(regs_[A]) << 8) | data_latch_;
        return bus_setup_io_write(pins, port, regs_[A]);
    }
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5: return pins; // IO automatic TW
    case 6:
        bus_finish_io(pins);
        // WZ: low byte = (n + 1) & 0xFF, high byte = A
        regs_[WZ] = (static_cast<uint16_t>(regs_[A]) << 8) | ((data_latch_ + 1) & 0xFF);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// HALT — now handled inline in decode_and_execute (z80.hpp).
// This handler is kept as a fallback but should not be reached.
// ========================================================================
bus_state_t op_halt(bus_state_t pins) {
    halted_ = true;
    BUS_CLR_BIT(pins, Z80_HALT_BIT); // Assert HALT signal
    transition_to_fetch();
    return pins;
}

// ========================================================================
// SM83-SPECIFIC INSTRUCTION HANDLERS
// ========================================================================
// These are only called when the SM83 trait is active (Game Boy CPU).
// They implement instructions unique to the SM83 that don't exist on Z80.

// STOP — 0x10: Enter low-power mode (also triggers GBC speed switch)
// Reads and discards the next byte (like a 2-byte NOP)
bus_state_t op_sm83_stop(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        regs_[PC]++;  // Skip the byte after STOP
        bus_finish_mem(pins);
        // TODO: actual low-power mode / GBC speed switch
        halted_ = true;
        BUS_CLR_BIT(pins, Z80_HALT_BIT);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD (nn),SP — 0x08: Store SP at address nn (SM83 unique, 20T)
bus_state_t op_sm83_ld_nn_sp(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);    // Read low byte of address
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[PC]);    // Read high byte of address
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        addr_latch_ |= static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8;
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 6: return bus_setup_mem_write(pins, addr_latch_,  // Write SP low
                static_cast<uint8_t>(regs_[SP] & 0xFF));
    case 7: if (!wait_check(pins)) return pins; return pins;
    case 8:
        bus_finish_mem(pins);
        return pins;
    case 9: return bus_setup_mem_write(pins, addr_latch_ + 1,  // Write SP high
                static_cast<uint8_t>(regs_[SP] >> 8));
    case 10: if (!wait_check(pins)) return pins; return pins;
    case 11:
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD (HL+),A — 0x22: Store A at (HL) then increment HL (8T)
bus_state_t op_sm83_ld_hli_a(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_write(pins, regs_[HL], regs_[A]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        bus_finish_mem(pins);
        regs_[HL]++;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD (HL-),A — 0x32: Store A at (HL) then decrement HL (8T)
bus_state_t op_sm83_ld_hld_a(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_write(pins, regs_[HL], regs_[A]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        bus_finish_mem(pins);
        regs_[HL]--;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD A,(HL+) — 0x2A: Load A from (HL) then increment HL (8T)
bus_state_t op_sm83_ld_a_hli(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[HL]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        regs_[A] = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        regs_[HL]++;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD A,(HL-) — 0x3A: Load A from (HL) then decrement HL (8T)
bus_state_t op_sm83_ld_a_hld(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[HL]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        regs_[A] = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        regs_[HL]--;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD ($FF00+n),A — 0xE0: Write A to high page I/O (12T)
bus_state_t op_sm83_ldh_n_a(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = 0xFF00 | BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_write(pins, addr_latch_, regs_[A]);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD A,($FF00+n) — 0xF0: Read A from high page I/O (12T)
bus_state_t op_sm83_ldh_a_n(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = 0xFF00 | BUS_GET_DATA(pins);
        regs_[PC]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, addr_latch_);
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        regs_[A] = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD ($FF00+C),A — 0xE2: Write A to ($FF00+C) (8T)
bus_state_t op_sm83_ldh_c_a(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_write(pins, 0xFF00 | regs_[C], regs_[A]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD A,($FF00+C) — 0xF2: Read A from ($FF00+C) (8T)
bus_state_t op_sm83_ldh_a_c(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, 0xFF00 | regs_[C]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        regs_[A] = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD (nn),A — 0xEA: Store A at 16-bit address (16T)
bus_state_t op_sm83_ld_nn_a(bus_state_t pins) {
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
    case 6: return bus_setup_mem_write(pins, addr_latch_, regs_[A]);
    case 7: if (!wait_check(pins)) return pins; return pins;
    case 8:
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// LD A,(nn) — 0xFA: Load A from 16-bit address (16T)
bus_state_t op_sm83_ld_a_nn(bus_state_t pins) {
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
        regs_[A] = BUS_GET_DATA(pins);
        bus_finish_mem(pins);
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ADD SP,e — 0xE8: Add signed 8-bit immediate to SP (16T)
// Flags: Z=0, N=0, H from bit 3, C from bit 7 (computed on low byte!)
bus_state_t op_sm83_add_sp_e(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2: {
        int8_t e = static_cast<int8_t>(BUS_GET_DATA(pins));
        regs_[PC]++;
        bus_finish_mem(pins);
        uint16_t sp = regs_[SP];
        uint32_t result = sp + e;
        // H and C flags based on unsigned addition of low byte of SP and unsigned e
        uint8_t u = static_cast<uint8_t>(e);
        regs_[F] = (((sp ^ u ^ result) & 0x10) ? Flags::H : 0)
                 | (((sp ^ u ^ result) & 0x100) ? Flags::C : 0);
        regs_[SP] = static_cast<uint16_t>(result);
        return pins;
    }
    case 3: case 4: case 5: // Internal delay (16T total)
        if (step_ == 6) {
            transition_to_fetch();
        }
        return pins;
    }
    return pins;
}

// LD HL,SP+e — 0xF8: Load HL with SP + signed 8-bit immediate (12T)
// Flags same as ADD SP,e: Z=0, N=0, H from bit 3, C from bit 7
bus_state_t op_sm83_ld_hl_sp_e(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[PC]);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2: {
        int8_t e = static_cast<int8_t>(BUS_GET_DATA(pins));
        regs_[PC]++;
        bus_finish_mem(pins);
        uint16_t sp = regs_[SP];
        uint32_t result = sp + e;
        uint8_t u = static_cast<uint8_t>(e);
        regs_[F] = (((sp ^ u ^ result) & 0x10) ? Flags::H : 0)
                 | (((sp ^ u ^ result) & 0x100) ? Flags::C : 0);
        regs_[HL] = static_cast<uint16_t>(result);
        return pins;
    }
    case 3: // Internal delay (12T total)
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// RETI — 0xD9 on SM83: Return from interrupt (enables interrupts)
// Pops PC from stack and sets IME=1 (unlike Z80 RETI which doesn't re-enable)
bus_state_t op_sm83_reti(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_[SP]);   // Pop PC low
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        addr_latch_ = BUS_GET_DATA(pins);
        regs_[SP]++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_[SP]);   // Pop PC high
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        regs_[PC] = addr_latch_ | (static_cast<uint16_t>(BUS_GET_DATA(pins)) << 8);
        regs_[SP]++;
        bus_finish_mem(pins);
        return pins;
    case 6: // Internal delay
        iff1_ = true;  // Re-enable interrupts (IME=1)
        iff2_ = true;
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// SM83 interrupt handler — 20T (5 internal + push PCH:4 + push PCL:4 + jump:4)
// The SM83 uses a simpler interrupt model than Z80:
// - Single interrupt level, level-triggered
// - The system provides the vector via IF & IE registers
// - Vectors are fixed: $0040 (VBlank), $0048 (LCD STAT), $0050 (Timer),
//                       $0058 (Serial), $0060 (Joypad)
// - The CPU pushes PC and jumps to the vector address
// - The system must clear the appropriate IF bit
bus_state_t op_sm83_int(bus_state_t pins) {
    switch (step_++) {
    case 0: case 1: // 2 internal T-states (interrupt response delay)
        return pins;
    case 2: // Push PC high
        regs_[SP]--;
        return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(regs_[PC] >> 8));
    case 3: if (!wait_check(pins)) return pins; return pins;
    case 4:
        bus_finish_mem(pins);
        return pins;
    case 5: // Push PC low
        regs_[SP]--;
        return bus_setup_mem_write(pins, regs_[SP], static_cast<uint8_t>(regs_[PC] & 0xFF));
    case 6: if (!wait_check(pins)) return pins; return pins;
    case 7:
        bus_finish_mem(pins);
        return pins;
    case 8: // Read interrupt vector from system
        // The system should place the vector address on the data bus
        // For now, we read the vector byte that the system provides
        // The Game Boy system tick loop determines which interrupt fires
        // and places the appropriate vector on the bus.
        // Default: RST $0040 (VBlank) — the system overrides via int_data_latch_
        regs_[PC] = 0x0040;  // Default vector, system overrides
        transition_to_fetch();
        return pins;
    }
    return pins;
}

#endif // Z80_SKIP_IMPLEMENTATION

#include "chip/cpu/z80/operations/inc_lint_prevention_footer.hpp"
