/*
 * z80_cb_ops.inc.hpp — CB-Prefix Instruction Handlers for Z80
 *
 * Handles all CB-prefixed instructions: rotate, shift, bit test, set, reset.
 * The CB opcode has already been fetched as a second M1 cycle; these handlers
 * manage the execution phase.
 *
 * CB opcode encoding:
 *   x = bits 7-6: 0=shift/rotate, 1=BIT, 2=RES, 3=SET
 *   y = bits 5-3: operation (shift type) or bit number
 *   z = bits 2-0: register (B=0 C=1 D=2 E=3 H=4 L=5 (HL)=6 A=7)
 *
 * Included inside z80_t class with Z80_TEMPLATE_CONTEXT defined.
 */

#include "inc_lint_prevention.hpp"

#ifndef Z80_SKIP_IMPLEMENTATION

// ========================================================================
// CB prefix: shift/rotate/bit on register — 8T total (M1:4 + M1:4)
// Decode and execute are handled inline in cb_decode, these are helpers
// for (HL) variants which need extra memory access M-cycles.
// ========================================================================

// CB shift/rotate on (HL) — 15T total (M1:4 + M1:4 + read:4 + write:3)
// With DD/FD: on (IX+d)/(IY+d) — displacement already read before CB
bus_state_t op_cb_shift_hl(bus_state_t pins) {
    uint16_t addr = get_hl_addr();
    if (has_ix_iy_prefix()) regs_.wz = addr;
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, addr);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        data_latch_ = cb_shift_op((cb_opcode_ >> 3) & 7, BUS_GET_DATA(pins));
        bus_finish_mem(pins);
        return pins;
    case 3: return pins; // 1 internal T-state
    case 4: return bus_setup_mem_write(pins, addr, data_latch_);
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6:
        bus_finish_mem(pins);
        // DD/FD CB: also copy result to register z (if z != 6)
        if (has_ix_iy_prefix()) {
            uint8_t z = cb_opcode_ & 7;
            if (z != 6) set_reg8_direct(z, data_latch_);
        }
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// CB BIT test on (HL) — 12T total (M1:4 + M1:4 + read:4)
bus_state_t op_cb_bit_hl(bus_state_t pins) {
    uint16_t addr = get_hl_addr();
    if (has_ix_iy_prefix()) regs_.wz = addr;
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, addr);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        alu_bit_hl((cb_opcode_ >> 3) & 7, BUS_GET_DATA(pins));
        bus_finish_mem(pins);
        return pins;
    case 3: // 1 internal T-state
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// CB SET/RES on (HL) — 15T total (M1:4 + M1:4 + read:4 + write:3)
bus_state_t op_cb_setres_hl(bus_state_t pins) {
    uint16_t addr = get_hl_addr();
    if (has_ix_iy_prefix()) regs_.wz = addr;
    uint8_t bit = (cb_opcode_ >> 3) & 7;
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, addr);
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2: {
        uint8_t val = BUS_GET_DATA(pins);
        if ((cb_opcode_ & 0xC0) == 0xC0)
            data_latch_ = val | (1 << bit);     // SET
        else
            data_latch_ = val & ~(1 << bit);    // RES
        bus_finish_mem(pins);
        return pins;
    }
    case 3: return pins; // 1 internal T-state
    case 4: return bus_setup_mem_write(pins, addr, data_latch_);
    case 5: if (!wait_check(pins)) return pins; return pins;
    case 6:
        bus_finish_mem(pins);
        // DD/FD CB: also copy result to register z (if z != 6)
        if (has_ix_iy_prefix()) {
            uint8_t z = cb_opcode_ & 7;
            if (z != 6) set_reg8_direct(z, data_latch_);
        }
        transition_to_fetch();
        return pins;
    }
    return pins;
}

// ========================================================================
// DD CB / FD CB prefix: indexed bit operations
// After DD/FD CB, we need to read displacement, then the actual CB opcode.
// This handler is entered when DD/FD prefix is active and CB is encountered.
// ========================================================================
bus_state_t op_ddfd_cb(bus_state_t pins) {
    switch (step_++) {
    case 0: return bus_setup_mem_read(pins, regs_.pc); // Read displacement
    case 1: if (!wait_check(pins)) return pins; return pins;
    case 2:
        displacement_ = static_cast<int8_t>(BUS_GET_DATA(pins));
        regs_.pc++;
        bus_finish_mem(pins);
        return pins;
    case 3: return bus_setup_mem_read(pins, regs_.pc); // Read CB opcode
    case 4: if (!wait_check(pins)) return pins; return pins;
    case 5:
        cb_opcode_ = BUS_GET_DATA(pins);
        regs_.pc++;
        bus_finish_mem(pins);
        return pins;
    case 6: return pins; // 1st idle T-state
    case 7: { // 2nd idle T-state — dispatch based on CB opcode
        uint8_t x = (cb_opcode_ >> 6) & 3;
        if (x == 0) {
            transition_to(&z80_t::op_cb_shift_hl);
        } else if (x == 1) {
            transition_to(&z80_t::op_cb_bit_hl);
        } else {
            transition_to(&z80_t::op_cb_setres_hl);
        }
        return pins;
    }
    }
    return pins;
}

#endif // Z80_SKIP_IMPLEMENTATION

#include "inc_lint_prevention_footer.hpp"
