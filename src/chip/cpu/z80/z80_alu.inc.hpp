/*
 * z80_alu.inc.hpp — ALU Operations and Flag Tables for Z80 Family
 *
 * Included directly inside the z80_t template class definition.
 * Provides precomputed flag tables, 8-bit and 16-bit ALU operations,
 * and accumulator rotate/shift helpers.
 *
 * NOT a standalone compilation unit — included mid-class in z80.hpp.
 */

// ========================================================================
// PRECOMPUTED FLAG TABLES
// ========================================================================

// Sign, Zero, and undocumented flags for all byte values.
// Z80: S from bit 7, Z if zero, copies bits 5(Y) and 3(X)
// SM83: Z if zero only (S, Y, X are absent → zero constants)
static constexpr std::array<uint8_t, 256> sz53_table = [] {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; i++) {
        if (Fl::S && (i & 0x80)) t[i] |= Fl::S;
        if (Fl::Y && (i & 0x20)) t[i] |= Fl::Y;
        if (Fl::X && (i & 0x08)) t[i] |= Fl::X;
        if (i == 0) t[i] |= Fl::Z;
    }
    return t;
}();

// Parity table: 1 if even parity (set PV), 0 if odd parity
// SM83: PV=0, so all entries are zero (table exists but is unused)
static constexpr std::array<uint8_t, 256> parity_table = [] {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; i++) {
        if (Fl::PV) {
            int bits = 0;
            for (int b = 0; b < 8; b++) bits += (i >> b) & 1;
            if (!(bits & 1)) t[i] |= Fl::PV;
        }
    }
    return t;
}();

// Combined SZ53P table: Sign, Zero, undocumented (3,5), Parity
// SM83: only Z flag (S, Y, X, PV all absent → zero)
static constexpr std::array<uint8_t, 256> sz53p_table = [] {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; i++) {
        if (Fl::S && (i & 0x80)) t[i] |= Fl::S;
        if (Fl::Y && (i & 0x20)) t[i] |= Fl::Y;
        if (Fl::X && (i & 0x08)) t[i] |= Fl::X;
        if (i == 0) t[i] |= Fl::Z;
        if (Fl::PV) {
            int bits = 0;
            for (int b = 0; b < 8; b++) bits += (i >> b) & 1;
            if (!(bits & 1)) t[i] |= Fl::PV;
        }
    }
    return t;
}();

// ========================================================================
// 8-BIT ALU OPERATIONS
// All operate on accumulator (A) and update flags (F).
// ========================================================================

void alu_add(uint8_t val) {
    uint16_t result = regs_[A] + val;
    uint8_t lookup = ((regs_[A] & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    regs_[A] = static_cast<uint8_t>(result);
    regs_[F] = sz53_table[regs_[A]]
            | (result & 0x100 ? Fl::C : 0)
            | half_carry_add_table[lookup & 0x07]
            | overflow_add_table[lookup >> 4];
}

void alu_adc(uint8_t val) {
    uint16_t result = regs_[A] + val + (regs_[F] & Fl::C ? 1 : 0);
    uint8_t lookup = ((regs_[A] & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    regs_[A] = static_cast<uint8_t>(result);
    regs_[F] = sz53_table[regs_[A]]
            | (result & 0x100 ? Fl::C : 0)
            | half_carry_add_table[lookup & 0x07]
            | overflow_add_table[lookup >> 4];
}

void alu_sub(uint8_t val) {
    uint16_t result = regs_[A] - val;
    uint8_t lookup = ((regs_[A] & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    regs_[A] = static_cast<uint8_t>(result);
    regs_[F] = sz53_table[regs_[A]]
            | Fl::N
            | (result & 0x100 ? Fl::C : 0)
            | half_carry_sub_table[lookup & 0x07]
            | overflow_sub_table[lookup >> 4];
}

void alu_sbc(uint8_t val) {
    uint16_t result = regs_[A] - val - (regs_[F] & Fl::C ? 1 : 0);
    uint8_t lookup = ((regs_[A] & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    regs_[A] = static_cast<uint8_t>(result);
    regs_[F] = sz53_table[regs_[A]]
            | Fl::N
            | (result & 0x100 ? Fl::C : 0)
            | half_carry_sub_table[lookup & 0x07]
            | overflow_sub_table[lookup >> 4];
}

void alu_and(uint8_t val) {
    regs_[A] &= val;
    regs_[F] = sz53p_table[regs_[A]] | Fl::H;
}

void alu_xor(uint8_t val) {
    regs_[A] ^= val;
    regs_[F] = sz53p_table[regs_[A]];
}

void alu_or(uint8_t val) {
    regs_[A] |= val;
    regs_[F] = sz53p_table[regs_[A]];
}

void alu_cp(uint8_t val) {
    uint16_t result = regs_[A] - val;
    uint8_t lookup = ((regs_[A] & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    // CP: SZ from result, but undocumented flags (3,5) from the OPERAND
    regs_[F] = (sz53_table[static_cast<uint8_t>(result)] & (Fl::S | Fl::Z))
            | (val & (Fl::Y | Fl::X))
            | Fl::N
            | (result & 0x100 ? Fl::C : 0)
            | half_carry_sub_table[lookup & 0x07]
            | overflow_sub_table[lookup >> 4];
}

// Dispatch ALU operation by 3-bit y field (ADD=0 ADC=1 SUB=2 SBC=3 AND=4 XOR=5 OR=6 CP=7)
void alu_op(uint8_t op, uint8_t val) {
    switch (op) {
    case 0: alu_add(val); break;
    case 1: alu_adc(val); break;
    case 2: alu_sub(val); break;
    case 3: alu_sbc(val); break;
    case 4: alu_and(val); break;
    case 5: alu_xor(val); break;
    case 6: alu_or(val);  break;
    case 7: alu_cp(val);  break;
    }
}

// ========================================================================
// 8-BIT INC/DEC (do not affect C flag)
// ========================================================================

uint8_t alu_inc(uint8_t val) {
    uint8_t result = val + 1;
    regs_[F] = (regs_[F] & Fl::C)
            | sz53_table[result]
            | (result == 0x80 ? Fl::PV : 0)
            | ((val & 0x0F) == 0x0F ? Fl::H : 0);
    return result;
}

uint8_t alu_dec(uint8_t val) {
    uint8_t result = val - 1;
    regs_[F] = (regs_[F] & Fl::C)
            | sz53_table[result]
            | Fl::N
            | (result == 0x7F ? Fl::PV : 0)
            | ((val & 0x0F) == 0x00 ? Fl::H : 0);
    return result;
}

// ========================================================================
// 16-BIT ARITHMETIC
// ========================================================================

// ADD HL,rr (11 cycles, no flags except H, C, and undocumented)
void alu_add16(uint16_t& dest, uint16_t val) {
    uint32_t result = dest + val;
    uint8_t lookup = ((dest & 0x0800) >> 11) | ((val & 0x0800) >> 10) | ((result & 0x0800) >> 9);
    dest = static_cast<uint16_t>(result);
    regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::PV))
            | (static_cast<uint8_t>(result >> 8) & (Fl::Y | Fl::X))
            | (result & 0x10000 ? Fl::C : 0)
            | half_carry_add_table[lookup];
}

// ADC HL,rr (15 cycles, full flag update)
void alu_adc16(uint16_t val) {
    uint32_t result = regs_[HL] + val + (regs_[F] & Fl::C ? 1 : 0);
    uint8_t lookup = ((regs_[HL] & 0x8800) >> 11) | ((val & 0x8800) >> 10) | ((result & 0x8800) >> 9);
    regs_[HL] = static_cast<uint16_t>(result);
    regs_[F] = ((result >> 8) & (Fl::S | Fl::Y | Fl::X))
            | (regs_[HL] == 0 ? Fl::Z : 0)
            | (result & 0x10000 ? Fl::C : 0)
            | overflow_add_table[lookup >> 4]
            | half_carry_add_table[lookup & 0x07];
}

// SBC HL,rr (15 cycles, full flag update)
void alu_sbc16(uint16_t val) {
    uint32_t result = regs_[HL] - val - (regs_[F] & Fl::C ? 1 : 0);
    uint8_t lookup = ((regs_[HL] & 0x8800) >> 11) | ((val & 0x8800) >> 10) | ((result & 0x8800) >> 9);
    regs_[HL] = static_cast<uint16_t>(result);
    regs_[F] = ((result >> 8) & (Fl::S | Fl::Y | Fl::X))
            | Fl::N
            | (regs_[HL] == 0 ? Fl::Z : 0)
            | (result & 0x10000 ? Fl::C : 0)
            | overflow_sub_table[lookup >> 4]
            | half_carry_sub_table[lookup & 0x07];
}

// ========================================================================
// HALF-CARRY AND OVERFLOW LOOKUP TABLES
// ========================================================================

static constexpr uint8_t half_carry_add_table[8] = {
    0, Fl::H, Fl::H, Fl::H, 0, 0, 0, Fl::H
};

static constexpr uint8_t half_carry_sub_table[8] = {
    0, 0, Fl::H, 0, Fl::H, 0, Fl::H, Fl::H
};

static constexpr uint8_t overflow_add_table[8] = {
    0, 0, 0, Fl::PV, Fl::PV, 0, 0, 0
};

static constexpr uint8_t overflow_sub_table[8] = {
    0, Fl::PV, 0, 0, 0, 0, Fl::PV, 0
};

// ========================================================================
// ACCUMULATOR ROTATES
// ========================================================================

void alu_rlca() {
    uint8_t c = regs_[A] >> 7;
    regs_[A] = (regs_[A] << 1) | c;
    if constexpr (is_sm83()) {
        regs_[F] = c ? Fl::C : 0;  // SM83: Z=0, N=0, H=0
    } else {
        regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::PV))
                | (regs_[A] & (Fl::Y | Fl::X))
                | (c ? Fl::C : 0);
    }
}

void alu_rrca() {
    uint8_t c = regs_[A] & 0x01;
    regs_[A] = (regs_[A] >> 1) | (c << 7);
    if constexpr (is_sm83()) {
        regs_[F] = c ? Fl::C : 0;
    } else {
        regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::PV))
                | (regs_[A] & (Fl::Y | Fl::X))
                | (c ? Fl::C : 0);
    }
}

void alu_rla() {
    uint8_t c = regs_[A] >> 7;
    regs_[A] = (regs_[A] << 1) | (regs_[F] & Fl::C ? 1 : 0);
    if constexpr (is_sm83()) {
        regs_[F] = c ? Fl::C : 0;
    } else {
        regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::PV))
                | (regs_[A] & (Fl::Y | Fl::X))
                | (c ? Fl::C : 0);
    }
}

void alu_rra() {
    uint8_t c = regs_[A] & 0x01;
    regs_[A] = (regs_[A] >> 1) | ((regs_[F] & Fl::C ? 1 : 0) << 7);
    if constexpr (is_sm83()) {
        regs_[F] = c ? Fl::C : 0;
    } else {
        regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::PV))
                | (regs_[A] & (Fl::Y | Fl::X))
                | (c ? Fl::C : 0);
    }
}

// ========================================================================
// CB-PREFIX SHIFT/ROTATE OPERATIONS (operate on any 8-bit value)
// ========================================================================

uint8_t alu_rlc(uint8_t val) {
    uint8_t c = val >> 7;
    val = (val << 1) | c;
    regs_[F] = sz53p_table[val] | (c ? Fl::C : 0);
    return val;
}

uint8_t alu_rrc(uint8_t val) {
    uint8_t c = val & 0x01;
    val = (val >> 1) | (c << 7);
    regs_[F] = sz53p_table[val] | (c ? Fl::C : 0);
    return val;
}

uint8_t alu_rl(uint8_t val) {
    uint8_t c = val >> 7;
    val = (val << 1) | (regs_[F] & Fl::C ? 1 : 0);
    regs_[F] = sz53p_table[val] | (c ? Fl::C : 0);
    return val;
}

uint8_t alu_rr(uint8_t val) {
    uint8_t c = val & 0x01;
    val = (val >> 1) | ((regs_[F] & Fl::C ? 1 : 0) << 7);
    regs_[F] = sz53p_table[val] | (c ? Fl::C : 0);
    return val;
}

uint8_t alu_sla(uint8_t val) {
    uint8_t c = val >> 7;
    val <<= 1;
    regs_[F] = sz53p_table[val] | (c ? Fl::C : 0);
    return val;
}

uint8_t alu_sra(uint8_t val) {
    uint8_t c = val & 0x01;
    val = (val >> 1) | (val & 0x80); // Preserve sign bit
    regs_[F] = sz53p_table[val] | (c ? Fl::C : 0);
    return val;
}

uint8_t alu_sll(uint8_t val) {
    // Undocumented: SLL shifts left and sets bit 0
    uint8_t c = val >> 7;
    val = (val << 1) | 0x01;
    regs_[F] = sz53p_table[val] | (c ? Fl::C : 0);
    return val;
}

uint8_t alu_srl(uint8_t val) {
    uint8_t c = val & 0x01;
    val >>= 1;
    regs_[F] = sz53p_table[val] | (c ? Fl::C : 0);
    return val;
}

// BIT test — sets Z, H, clears N. Undocumented flags from the value.
void alu_bit(uint8_t bit, uint8_t val) {
    uint8_t result = val & (1 << bit);
    regs_[F] = (regs_[F] & Fl::C)
            | Fl::H
            | (result ? 0 : (Fl::Z | Fl::PV))
            | (result & Fl::S)
            | (val & (Fl::Y | Fl::X));
}

// BIT test for (HL)/(IX+d)/(IY+d) — undocumented flags from high byte of WZ (MEMPTR)
void alu_bit_hl(uint8_t bit, uint8_t val) {
    uint8_t result = val & (1 << bit);
    regs_[F] = (regs_[F] & Fl::C)
            | Fl::H
            | (result ? 0 : (Fl::Z | Fl::PV))
            | (result & Fl::S)
            | (regs_[W] & (Fl::Y | Fl::X));
}

// ========================================================================
// SPECIAL ALU OPERATIONS
// ========================================================================

void alu_daa() {
    if constexpr (is_sm83()) {
        // SM83 DAA: same correction logic, but flags are simpler
        // Only Z, N (preserved), H=0, C (set if correction overflows)
        uint8_t a = regs_[A];
        uint8_t c = regs_[F] & Fl::C;
        if (regs_[F] & Fl::N) {
            // After subtraction
            if (c) a -= 0x60;
            if (regs_[F] & Fl::H) a -= 0x06;
        } else {
            // After addition
            if (c || a > 0x99) { a += 0x60; c = Fl::C; }
            if ((regs_[F] & Fl::H) || (a & 0x0F) > 0x09) a += 0x06;
        }
        regs_[A] = a;
        regs_[F] = (regs_[F] & Fl::N)
                | c
                | (a == 0 ? Fl::Z : 0);
    } else {
        uint8_t a = regs_[A];
        uint8_t correction = 0;
        uint8_t c = 0;

        if ((regs_[F] & Fl::H) || (a & 0x0F) > 0x09) correction |= 0x06;
        if ((regs_[F] & Fl::C) || a > 0x99) { correction |= 0x60; c = Fl::C; }

        if (regs_[F] & Fl::N) {
            regs_[A] -= correction;
        } else {
            regs_[A] += correction;
        }

        regs_[F] = sz53p_table[regs_[A]]
                | (regs_[F] & Fl::N)
                | ((regs_[A] ^ a) & Fl::H)
                | c;
    }
}

void alu_cpl() {
    regs_[A] = ~regs_[A];
    regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::PV | Fl::C))
            | (regs_[A] & (Fl::Y | Fl::X))
            | Fl::H | Fl::N;
}

void alu_neg() {
    uint8_t val = regs_[A];
    regs_[A] = 0;
    alu_sub(val);
}

void alu_ccf() {
    if constexpr (is_sm83()) {
        // SM83 CCF: Z unchanged, N=0, H=0, C=!C
        regs_[F] = (regs_[F] & Fl::Z)
                | ((regs_[F] & Fl::C) ^ Fl::C);
    } else {
        // Z80 CCF: preserves S,Z,PV. H=old C. Y/X from A or A|F.
        uint8_t yx = q_saved_ ? (regs_[A] & (Fl::Y | Fl::X))
                              : ((regs_[A] | regs_[F]) & (Fl::Y | Fl::X));
        regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::PV))
                | ((regs_[F] & Fl::C) ? Fl::H : 0)
                | yx
                | ((regs_[F] & Fl::C) ^ Fl::C);
    }
}

void alu_scf() {
    if constexpr (is_sm83()) {
        // SM83 SCF: Z unchanged, N=0, H=0, C=1
        regs_[F] = (regs_[F] & Fl::Z) | Fl::C;
    } else {
        uint8_t yx = q_saved_ ? (regs_[A] & (Fl::Y | Fl::X))
                              : ((regs_[A] | regs_[F]) & (Fl::Y | Fl::X));
        regs_[F] = (regs_[F] & (Fl::S | Fl::Z | Fl::PV))
                | yx
                | Fl::C;
    }
}

// RLD: Rotate left digit (A and (HL))
// (HL) = (HL low nibble → high nibble, A low nibble → (HL) low nibble)
// A = (A high nibble, old (HL) high nibble → A low nibble)
void alu_rld(uint8_t& mem) {
    uint8_t old_mem = mem;
    mem = static_cast<uint8_t>((old_mem << 4) | (regs_[A] & 0x0F));
    regs_[A] = (regs_[A] & 0xF0) | (old_mem >> 4);
    regs_[F] = (regs_[F] & Fl::C) | sz53p_table[regs_[A]];
}

// RRD: Rotate right digit
void alu_rrd(uint8_t& mem) {
    uint8_t old_mem = mem;
    mem = static_cast<uint8_t>((regs_[A] << 4) | (old_mem >> 4));
    regs_[A] = (regs_[A] & 0xF0) | (old_mem & 0x0F);
    regs_[F] = (regs_[F] & Fl::C) | sz53p_table[regs_[A]];
}

// Dispatch CB shift/rotate by 3-bit operation field
uint8_t cb_shift_op(uint8_t op, uint8_t val) {
    switch (op) {
    case 0: return alu_rlc(val);
    case 1: return alu_rrc(val);
    case 2: return alu_rl(val);
    case 3: return alu_rr(val);
    case 4: return alu_sla(val);
    case 5: return alu_sra(val);
    case 6:
        // SM83: SWAP (swap upper and lower nybbles)
        // Z80: SLL (undocumented shift left with bit 0 set)
        if constexpr (is_sm83()) {
            return alu_swap(val);
        } else {
            return alu_sll(val);
        }
    case 7: return alu_srl(val);
    default: return val;
    }
}

// ========================================================================
// SM83 SWAP — Swap upper and lower nybbles (CB 30-37)
// Flags: Z set if result is 0, N=0, H=0, C=0
// ========================================================================

uint8_t alu_swap(uint8_t val) {
    val = static_cast<uint8_t>((val >> 4) | (val << 4));
    regs_[F] = (val == 0) ? Fl::Z : 0;
    return val;
}
