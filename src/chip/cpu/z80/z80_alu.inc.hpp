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

// Sign, Zero, and undocumented flags (bits 3,5) for all byte values.
// Entry = SZ flags | X flag (bit 3) | Y flag (bit 5)
static constexpr std::array<uint8_t, 256> sz53_table = [] {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; i++) {
        t[i] = static_cast<uint8_t>(i & (Flags::S | Flags::Y | Flags::X));
        if (i == 0) t[i] |= Flags::Z;
    }
    return t;
}();

// Parity table: 1 if even parity (set PV), 0 if odd parity
static constexpr std::array<uint8_t, 256> parity_table = [] {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; i++) {
        int bits = 0;
        for (int b = 0; b < 8; b++) bits += (i >> b) & 1;
        t[i] = (bits & 1) ? 0 : Flags::PV;
    }
    return t;
}();

// Combined SZ53P table: Sign, Zero, undocumented (3,5), Parity
static constexpr std::array<uint8_t, 256> sz53p_table = [] {
    std::array<uint8_t, 256> t{};
    for (int i = 0; i < 256; i++) {
        t[i] = static_cast<uint8_t>(i & (Flags::S | Flags::Y | Flags::X));
        if (i == 0) t[i] |= Flags::Z;
        // Parity
        int bits = 0;
        for (int b = 0; b < 8; b++) bits += (i >> b) & 1;
        if (!(bits & 1)) t[i] |= Flags::PV;
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
            | (result & 0x100 ? Flags::C : 0)
            | half_carry_add_table[lookup & 0x07]
            | overflow_add_table[lookup >> 4];
}

void alu_adc(uint8_t val) {
    uint16_t result = regs_[A] + val + (regs_[F] & Flags::C);
    uint8_t lookup = ((regs_[A] & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    regs_[A] = static_cast<uint8_t>(result);
    regs_[F] = sz53_table[regs_[A]]
            | (result & 0x100 ? Flags::C : 0)
            | half_carry_add_table[lookup & 0x07]
            | overflow_add_table[lookup >> 4];
}

void alu_sub(uint8_t val) {
    uint16_t result = regs_[A] - val;
    uint8_t lookup = ((regs_[A] & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    regs_[A] = static_cast<uint8_t>(result);
    regs_[F] = sz53_table[regs_[A]]
            | Flags::N
            | (result & 0x100 ? Flags::C : 0)
            | half_carry_sub_table[lookup & 0x07]
            | overflow_sub_table[lookup >> 4];
}

void alu_sbc(uint8_t val) {
    uint16_t result = regs_[A] - val - (regs_[F] & Flags::C);
    uint8_t lookup = ((regs_[A] & 0x88) >> 3) | ((val & 0x88) >> 2) | ((result & 0x88) >> 1);
    regs_[A] = static_cast<uint8_t>(result);
    regs_[F] = sz53_table[regs_[A]]
            | Flags::N
            | (result & 0x100 ? Flags::C : 0)
            | half_carry_sub_table[lookup & 0x07]
            | overflow_sub_table[lookup >> 4];
}

void alu_and(uint8_t val) {
    regs_[A] &= val;
    regs_[F] = sz53p_table[regs_[A]] | Flags::H;
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
    regs_[F] = (sz53_table[static_cast<uint8_t>(result)] & (Flags::S | Flags::Z))
            | (val & (Flags::Y | Flags::X))
            | Flags::N
            | (result & 0x100 ? Flags::C : 0)
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
    regs_[F] = (regs_[F] & Flags::C)
            | sz53_table[result]
            | (result == 0x80 ? Flags::PV : 0)
            | ((val ^ result) & Flags::H);
    return result;
}

uint8_t alu_dec(uint8_t val) {
    uint8_t result = val - 1;
    regs_[F] = (regs_[F] & Flags::C)
            | sz53_table[result]
            | Flags::N
            | (result == 0x7F ? Flags::PV : 0)
            | ((val ^ result) & Flags::H);
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
    regs_[F] = (regs_[F] & (Flags::S | Flags::Z | Flags::PV))
            | (static_cast<uint8_t>(result >> 8) & (Flags::Y | Flags::X))
            | (result & 0x10000 ? Flags::C : 0)
            | half_carry_add_table[lookup];
}

// ADC HL,rr (15 cycles, full flag update)
void alu_adc16(uint16_t val) {
    uint32_t result = regs_[HL] + val + (regs_[F] & Flags::C);
    uint8_t lookup = ((regs_[HL] & 0x8800) >> 11) | ((val & 0x8800) >> 10) | ((result & 0x8800) >> 9);
    regs_[HL] = static_cast<uint16_t>(result);
    regs_[F] = ((result >> 8) & (Flags::S | Flags::Y | Flags::X))
            | (regs_[HL] == 0 ? Flags::Z : 0)
            | (result & 0x10000 ? Flags::C : 0)
            | overflow_add_table[lookup >> 4]
            | half_carry_add_table[lookup & 0x07];
}

// SBC HL,rr (15 cycles, full flag update)
void alu_sbc16(uint16_t val) {
    uint32_t result = regs_[HL] - val - (regs_[F] & Flags::C);
    uint8_t lookup = ((regs_[HL] & 0x8800) >> 11) | ((val & 0x8800) >> 10) | ((result & 0x8800) >> 9);
    regs_[HL] = static_cast<uint16_t>(result);
    regs_[F] = ((result >> 8) & (Flags::S | Flags::Y | Flags::X))
            | Flags::N
            | (regs_[HL] == 0 ? Flags::Z : 0)
            | (result & 0x10000 ? Flags::C : 0)
            | overflow_sub_table[lookup >> 4]
            | half_carry_sub_table[lookup & 0x07];
}

// ========================================================================
// HALF-CARRY AND OVERFLOW LOOKUP TABLES
// ========================================================================

static constexpr uint8_t half_carry_add_table[8] = {
    0, Flags::H, Flags::H, Flags::H, 0, 0, 0, Flags::H
};

static constexpr uint8_t half_carry_sub_table[8] = {
    0, 0, Flags::H, 0, Flags::H, 0, Flags::H, Flags::H
};

static constexpr uint8_t overflow_add_table[8] = {
    0, 0, 0, Flags::PV, Flags::PV, 0, 0, 0
};

static constexpr uint8_t overflow_sub_table[8] = {
    0, Flags::PV, 0, 0, 0, 0, Flags::PV, 0
};

// ========================================================================
// ACCUMULATOR ROTATES
// ========================================================================

void alu_rlca() {
    uint8_t c = regs_[A] >> 7;
    regs_[A] = (regs_[A] << 1) | c;
    regs_[F] = (regs_[F] & (Flags::S | Flags::Z | Flags::PV))
            | (regs_[A] & (Flags::Y | Flags::X))
            | c;
}

void alu_rrca() {
    uint8_t c = regs_[A] & 0x01;
    regs_[A] = (regs_[A] >> 1) | (c << 7);
    regs_[F] = (regs_[F] & (Flags::S | Flags::Z | Flags::PV))
            | (regs_[A] & (Flags::Y | Flags::X))
            | c;
}

void alu_rla() {
    uint8_t c = regs_[A] >> 7;
    regs_[A] = (regs_[A] << 1) | (regs_[F] & Flags::C);
    regs_[F] = (regs_[F] & (Flags::S | Flags::Z | Flags::PV))
            | (regs_[A] & (Flags::Y | Flags::X))
            | c;
}

void alu_rra() {
    uint8_t c = regs_[A] & 0x01;
    regs_[A] = (regs_[A] >> 1) | ((regs_[F] & Flags::C) << 7);
    regs_[F] = (regs_[F] & (Flags::S | Flags::Z | Flags::PV))
            | (regs_[A] & (Flags::Y | Flags::X))
            | c;
}

// ========================================================================
// CB-PREFIX SHIFT/ROTATE OPERATIONS (operate on any 8-bit value)
// ========================================================================

uint8_t alu_rlc(uint8_t val) {
    uint8_t c = val >> 7;
    val = (val << 1) | c;
    regs_[F] = sz53p_table[val] | c;
    return val;
}

uint8_t alu_rrc(uint8_t val) {
    uint8_t c = val & 0x01;
    val = (val >> 1) | (c << 7);
    regs_[F] = sz53p_table[val] | c;
    return val;
}

uint8_t alu_rl(uint8_t val) {
    uint8_t c = val >> 7;
    val = (val << 1) | (regs_[F] & Flags::C);
    regs_[F] = sz53p_table[val] | c;
    return val;
}

uint8_t alu_rr(uint8_t val) {
    uint8_t c = val & 0x01;
    val = (val >> 1) | ((regs_[F] & Flags::C) << 7);
    regs_[F] = sz53p_table[val] | c;
    return val;
}

uint8_t alu_sla(uint8_t val) {
    uint8_t c = val >> 7;
    val <<= 1;
    regs_[F] = sz53p_table[val] | c;
    return val;
}

uint8_t alu_sra(uint8_t val) {
    uint8_t c = val & 0x01;
    val = (val >> 1) | (val & 0x80); // Preserve sign bit
    regs_[F] = sz53p_table[val] | c;
    return val;
}

uint8_t alu_sll(uint8_t val) {
    // Undocumented: SLL shifts left and sets bit 0
    uint8_t c = val >> 7;
    val = (val << 1) | 0x01;
    regs_[F] = sz53p_table[val] | c;
    return val;
}

uint8_t alu_srl(uint8_t val) {
    uint8_t c = val & 0x01;
    val >>= 1;
    regs_[F] = sz53p_table[val] | c;
    return val;
}

// BIT test — sets Z, H, clears N. Undocumented flags from the value.
void alu_bit(uint8_t bit, uint8_t val) {
    uint8_t result = val & (1 << bit);
    regs_[F] = (regs_[F] & Flags::C)
            | Flags::H
            | (result ? 0 : (Flags::Z | Flags::PV))
            | (result & Flags::S)
            | (val & (Flags::Y | Flags::X));
}

// BIT test for (HL)/(IX+d)/(IY+d) — undocumented flags from high byte of WZ (MEMPTR)
void alu_bit_hl(uint8_t bit, uint8_t val) {
    uint8_t result = val & (1 << bit);
    regs_[F] = (regs_[F] & Flags::C)
            | Flags::H
            | (result ? 0 : (Flags::Z | Flags::PV))
            | (result & Flags::S)
            | (regs_[W] & (Flags::Y | Flags::X));
}

// ========================================================================
// SPECIAL ALU OPERATIONS
// ========================================================================

void alu_daa() {
    if constexpr (is_sm83()) {
        // SM83 DAA: same correction logic, but flags are simpler
        // Only Z, N (preserved), H=0, C (set if correction overflows)
        uint8_t a = regs_[A];
        uint8_t c = regs_[F] & Flags::C;
        if (regs_[F] & Flags::N) {
            // After subtraction
            if (c) a -= 0x60;
            if (regs_[F] & Flags::H) a -= 0x06;
        } else {
            // After addition
            if (c || a > 0x99) { a += 0x60; c = Flags::C; }
            if ((regs_[F] & Flags::H) || (a & 0x0F) > 0x09) a += 0x06;
        }
        regs_[A] = a;
        regs_[F] = (regs_[F] & Flags::N)
                | c
                | (a == 0 ? Flags::Z : 0);
    } else {
        uint8_t a = regs_[A];
        uint8_t correction = 0;
        uint8_t c = 0;

        if ((regs_[F] & Flags::H) || (a & 0x0F) > 0x09) correction |= 0x06;
        if ((regs_[F] & Flags::C) || a > 0x99) { correction |= 0x60; c = Flags::C; }

        if (regs_[F] & Flags::N) {
            regs_[A] -= correction;
        } else {
            regs_[A] += correction;
        }

        regs_[F] = sz53p_table[regs_[A]]
                | (regs_[F] & Flags::N)
                | ((regs_[A] ^ a) & Flags::H)
                | c;
    }
}

void alu_cpl() {
    regs_[A] = ~regs_[A];
    regs_[F] = (regs_[F] & (Flags::S | Flags::Z | Flags::PV | Flags::C))
            | (regs_[A] & (Flags::Y | Flags::X))
            | Flags::H | Flags::N;
}

void alu_neg() {
    uint8_t val = regs_[A];
    regs_[A] = 0;
    alu_sub(val);
}

void alu_ccf() {
    // Y/X flags: if previous instruction modified flags (q_saved_), use A only;
    // otherwise use (A | F) to include old flag bits in Y/X.
    uint8_t yx = q_saved_ ? (regs_[A] & (Flags::Y | Flags::X))
                          : ((regs_[A] | regs_[F]) & (Flags::Y | Flags::X));
    regs_[F] = (regs_[F] & (Flags::S | Flags::Z | Flags::PV))
            | ((regs_[F] & Flags::C) ? Flags::H : 0)
            | yx
            | ((regs_[F] & Flags::C) ^ Flags::C);
}

void alu_scf() {
    uint8_t yx = q_saved_ ? (regs_[A] & (Flags::Y | Flags::X))
                          : ((regs_[A] | regs_[F]) & (Flags::Y | Flags::X));
    regs_[F] = (regs_[F] & (Flags::S | Flags::Z | Flags::PV))
            | yx
            | Flags::C;
}

// RLD: Rotate left digit (A and (HL))
// (HL) = (HL low nibble → high nibble, A low nibble → (HL) low nibble)
// A = (A high nibble, old (HL) high nibble → A low nibble)
void alu_rld(uint8_t& mem) {
    uint8_t old_mem = mem;
    mem = static_cast<uint8_t>((old_mem << 4) | (regs_[A] & 0x0F));
    regs_[A] = (regs_[A] & 0xF0) | (old_mem >> 4);
    regs_[F] = (regs_[F] & Flags::C) | sz53p_table[regs_[A]];
}

// RRD: Rotate right digit
void alu_rrd(uint8_t& mem) {
    uint8_t old_mem = mem;
    mem = static_cast<uint8_t>((regs_[A] << 4) | (old_mem >> 4));
    regs_[A] = (regs_[A] & 0xF0) | (old_mem & 0x0F);
    regs_[F] = (regs_[F] & Flags::C) | sz53p_table[regs_[A]];
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
    regs_[F] = (val == 0) ? Flags::Z : 0;
    return val;
}
