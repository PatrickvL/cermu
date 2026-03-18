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

    // Quick decode of common instructions by bits 11-6
    uint8_t bits_11_6 = (opcode >> 6) & 0x3F;

    // NOP: 0100_1110_0111_0001 = $4E71
    if (opcode == 0x4E71) {
        return do_prefetch(pins);
    }

    // RTS: 0100_1110_0111_0101 = $4E75
    if (opcode == 0x4E75) {
        // Pop PC from stack
        // TODO: bus cycles for stack read
        return do_prefetch(pins);
    }

    // RTE: 0100_1110_0111_0011 = $4E73
    if (opcode == 0x4E73) {
        if (!(regs_.sr & SRBits::S)) {
            return exception(pins, Vector::PRIVILEGE_VIOLATION);
        }
        // TODO: restore SR and PC from stack
        return do_prefetch(pins);
    }

    // TRAP #vector: 0100_1110_0100_vvvv
    if ((opcode & 0xFFF0) == 0x4E40) {
        uint8_t trap_vec = opcode & 0x0F;
        return exception(pins, Vector::TRAP_BASE + trap_vec);
    }

    // TODO: remaining group 4 instructions
    return do_prefetch(pins);
}

// ── Group 6: Bcc / BSR / BRA ────────────────────────────────────
inline bus_state_t decode_group6(bus_state_t pins, uint16_t opcode) {
    auto cc = static_cast<Condition>((opcode >> 8) & 0x0F);
    int8_t disp8 = static_cast<int8_t>(opcode & 0xFF);

    int32_t displacement;
    if (disp8 == 0) {
        // Word displacement from extension word
        displacement = static_cast<int16_t>(regs_.irc);
        regs_.pc += 2;
    } else if constexpr (has_long_branch()) {
        if (disp8 == -1) {
            // Long displacement (68020+) from two extension words
            displacement = static_cast<int32_t>(
                (static_cast<uint32_t>(regs_.irc) << 16)); // high word
            regs_.pc += 2;
            displacement |= regs_.irc;  // low word
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
        regs_.pc = regs_.pc + displacement - 2;  // -2 because PC already advanced past opcode
        return do_prefetch(pins);
    }
    if (cc == Condition::F) {
        // BSR — branch to subroutine
        // Push return address
        regs_.a[7] -= 4;
        // TODO: write PC to stack via bus cycles
        regs_.pc = regs_.pc + displacement - 2;
        return do_prefetch(pins);
    }

    // Bcc — conditional branch
    if (test_condition(cc)) {
        regs_.pc = regs_.pc + displacement - 2;
    }
    return do_prefetch(pins);
}

#include "chip/cpu/m680x0/operations/inc_lint_prevention_footer.hpp"
