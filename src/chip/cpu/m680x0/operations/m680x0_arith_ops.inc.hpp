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
    // ORI, ANDI, EORI, SUBI, ADDI, CMPI, BTST, BCHG, BCLR, BSET, MOVEP
    // TODO: implement
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
