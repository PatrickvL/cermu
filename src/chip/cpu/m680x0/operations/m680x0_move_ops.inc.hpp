// m680x0_move_ops.inc.hpp — Move instruction handlers
//
// Groups 1/2/3 (MOVE.B/.L/.W) and Group 7 (MOVEQ).
//
// Included inside m680x0_t class body with M680X0_TEMPLATE_CONTEXT defined.

#include "chip/cpu/m680x0/operations/inc_lint_prevention.hpp"

// ── Groups 1/2/3: MOVE ──────────────────────────────────────────
inline bus_state_t decode_move(bus_state_t pins, uint16_t opcode, OpSize sz) {
    // MOVE.B / MOVE.W / MOVE.L  — bits 11-6 = dst, bits 5-0 = src
    uint8_t src_mode = instr_ea_mode(opcode);
    uint8_t src_reg  = instr_ea_reg(opcode);
    uint8_t dst_reg  = (opcode >> 9) & 7;
    uint8_t dst_mode = (opcode >> 6) & 7;

    uint32_t value = read_ea(src_mode, src_reg, sz);
    if (address_error_) return pins;

    // MOVEA — destination is An: no flags change, sign-extend .w → .l
    if (dst_mode == 1) {
        if (sz == OpSize::Word)
            value = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(value)));
        set_a(dst_reg, value);
        if (dst_reg == 7) sync_sp();
        return do_prefetch(pins);
    }

    // MOVE sets N, Z, clears V and C — flags update before write on 68000
    alu_tst(value, sz);

    // Real 68000 starts prefetch before the write for -(An) destinations,
    // so the error frame PC is 2 bytes further than the opcode address.
    bool predec_dst = (dst_mode == static_cast<uint8_t>(EAMode::AddrRegPreDec));
    if (predec_dst) regs_.pc += 2;

    bool src_is_memory = (src_mode >= 2 && !(src_mode == 7 && src_reg == 4));
    write_ea(dst_mode, dst_reg, value, sz, src_is_memory);
    if (address_error_) return pins;

    if (predec_dst) regs_.pc -= 2;

    return do_prefetch(pins);
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

    return do_prefetch(pins);
}

#include "chip/cpu/m680x0/operations/inc_lint_prevention_footer.hpp"
