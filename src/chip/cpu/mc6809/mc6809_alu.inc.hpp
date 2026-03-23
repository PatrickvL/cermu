// ========================================================================
// mc6809_alu.inc.hpp — ALU operations and flag computation
// ========================================================================
//
// Arithmetic, logic, and flag-update helpers for the MC6809 family.
// Included inside mc6809_t<Traits> class body.
// ========================================================================

// ========================================================================
// Flag update helpers
// ========================================================================

/// Set N and Z flags based on 8-bit result
inline void set_nz8(uint8_t val) {
    regs_[CC] &= static_cast<uint8_t>(~(Flags::N | Flags::Z));
    if (val == 0)    regs_[CC] |= Flags::Z;
    if (val & 0x80)  regs_[CC] |= Flags::N;
}

/// Set N and Z flags based on 16-bit result
inline void set_nz16(uint16_t val) {
    regs_[CC] &= static_cast<uint8_t>(~(Flags::N | Flags::Z));
    if (val == 0)      regs_[CC] |= Flags::Z;
    if (val & 0x8000)  regs_[CC] |= Flags::N;
}

/// Set or clear a single CC flag
inline void set_flag(uint8_t flag, bool cond) {
    if (cond)
        regs_[CC] |= flag;
    else
        regs_[CC] &= static_cast<uint8_t>(~flag);
}

/// Test a single CC flag
inline bool test_flag(uint8_t flag) const {
    return (regs_[CC] & flag) != 0;
}

// ========================================================================
// 8-bit ALU operations
// ========================================================================

/// ADD: result = a + b (+ carry if adc)
inline uint8_t alu_add8(uint8_t a, uint8_t b, bool carry_in) {
    uint8_t c = carry_in ? (regs_[CC] & Flags::C) : 0;
    uint16_t result16 = static_cast<uint16_t>(a) + b + c;
    uint8_t result = static_cast<uint8_t>(result16);

    regs_[CC] &= static_cast<uint8_t>(~(Flags::H | Flags::N | Flags::Z | Flags::V | Flags::C));
    if (result == 0)                                regs_[CC] |= Flags::Z;
    if (result & 0x80)                              regs_[CC] |= Flags::N;
    if (result16 & 0x100)                           regs_[CC] |= Flags::C;
    if ((a ^ b ^ result ^ (result16 >> 1)) & 0x80) regs_[CC] |= Flags::V;
    if ((a ^ b ^ result) & 0x10)                    regs_[CC] |= Flags::H;
    return result;
}

/// SUB: result = a - b (- borrow if sbc)
inline uint8_t alu_sub8(uint8_t a, uint8_t b, bool borrow_in) {
    uint8_t c = borrow_in ? (regs_[CC] & Flags::C) : 0;
    uint16_t result16 = static_cast<uint16_t>(a) - b - c;
    uint8_t result = static_cast<uint8_t>(result16);

    regs_[CC] &= static_cast<uint8_t>(~(Flags::N | Flags::Z | Flags::V | Flags::C));
    if (result == 0)                                regs_[CC] |= Flags::Z;
    if (result & 0x80)                              regs_[CC] |= Flags::N;
    if (result16 & 0x100)                           regs_[CC] |= Flags::C;
    if ((a ^ b) & (a ^ result) & 0x80)              regs_[CC] |= Flags::V;
    return result;
}

/// CMP: compare a - b, set flags, discard result
inline void alu_cmp8(uint8_t a, uint8_t b) {
    alu_sub8(a, b, false);
}

/// AND: result = a & b
inline uint8_t alu_and8(uint8_t a, uint8_t b) {
    uint8_t result = a & b;
    set_nz8(result);
    set_flag(Flags::V, false);
    return result;
}

/// OR: result = a | b
inline uint8_t alu_or8(uint8_t a, uint8_t b) {
    uint8_t result = a | b;
    set_nz8(result);
    set_flag(Flags::V, false);
    return result;
}

/// EOR: result = a ^ b
inline uint8_t alu_eor8(uint8_t a, uint8_t b) {
    uint8_t result = a ^ b;
    set_nz8(result);
    set_flag(Flags::V, false);
    return result;
}

/// BIT: test a & b, set flags, don't store
inline void alu_bit8(uint8_t a, uint8_t b) {
    uint8_t result = a & b;
    set_nz8(result);
    set_flag(Flags::V, false);
}

/// NEG: result = -val (two's complement)
inline uint8_t alu_neg8(uint8_t val) {
    uint8_t result = static_cast<uint8_t>(0 - val);
    set_nz8(result);
    set_flag(Flags::C, result != 0);
    set_flag(Flags::V, val == 0x80);
    return result;
}

/// COM: result = ~val (one's complement)
inline uint8_t alu_com8(uint8_t val) {
    uint8_t result = ~val;
    set_nz8(result);
    set_flag(Flags::V, false);
    set_flag(Flags::C, true);
    return result;
}

/// INC: result = val + 1
inline uint8_t alu_inc8(uint8_t val) {
    uint8_t result = val + 1;
    set_nz8(result);
    set_flag(Flags::V, val == 0x7F);
    return result;
}

/// DEC: result = val - 1
inline uint8_t alu_dec8(uint8_t val) {
    uint8_t result = val - 1;
    set_nz8(result);
    set_flag(Flags::V, val == 0x80);
    return result;
}

/// TST: test val, set N/Z, clear V
inline void alu_tst8(uint8_t val) {
    set_nz8(val);
    set_flag(Flags::V, false);
}

/// CLR: set to zero, update flags
inline uint8_t alu_clr8() {
    regs_[CC] &= static_cast<uint8_t>(~(Flags::N | Flags::V | Flags::C));
    regs_[CC] |= Flags::Z;
    return 0;
}

/// LSR: logical shift right
inline uint8_t alu_lsr8(uint8_t val) {
    set_flag(Flags::C, val & 0x01);
    uint8_t result = val >> 1;
    set_nz8(result);
    return result;
}

/// ASR: arithmetic shift right (preserve sign)
inline uint8_t alu_asr8(uint8_t val) {
    set_flag(Flags::C, val & 0x01);
    uint8_t result = (val >> 1) | (val & 0x80);
    set_nz8(result);
    return result;
}

/// ASL/LSL: arithmetic/logical shift left
inline uint8_t alu_asl8(uint8_t val) {
    set_flag(Flags::C, (val & 0x80) != 0);
    uint8_t result = val << 1;
    set_nz8(result);
    set_flag(Flags::V, (val ^ result) & 0x80);
    set_flag(Flags::H, (val ^ result) & 0x10);
    return result;
}

/// ROL: rotate left through carry
inline uint8_t alu_rol8(uint8_t val) {
    uint8_t c = test_flag(Flags::C) ? 1 : 0;
    set_flag(Flags::C, (val & 0x80) != 0);
    uint8_t result = (val << 1) | c;
    set_nz8(result);
    set_flag(Flags::V, (val ^ result) & 0x80);
    return result;
}

/// ROR: rotate right through carry
inline uint8_t alu_ror8(uint8_t val) {
    uint8_t c = test_flag(Flags::C) ? 0x80 : 0;
    set_flag(Flags::C, val & 0x01);
    uint8_t result = (val >> 1) | c;
    set_nz8(result);
    return result;
}

/// DAA: decimal adjust accumulator A
inline void alu_daa() {
    uint8_t a = regs_[A];
    uint8_t correction = 0;
    bool c = test_flag(Flags::C);

    if (test_flag(Flags::H) || (a & 0x0F) > 9) {
        correction |= 0x06;
    }
    if (c || a > 0x99 || (a > 0x93 && (a & 0x0F) > 9)) {
        correction |= 0x60;
        c = true;
    }

    regs_[A] = a + correction;
    set_nz8(regs_[A]);
    set_flag(Flags::C, c);
    // V is undefined per Motorola docs
}

// ========================================================================
// 16-bit ALU operations
// ========================================================================

/// ADD16: result = a + b
inline uint16_t alu_add16(uint16_t a, uint16_t b) {
    uint32_t result32 = static_cast<uint32_t>(a) + b;
    uint16_t result = static_cast<uint16_t>(result32);

    regs_[CC] &= static_cast<uint8_t>(~(Flags::N | Flags::Z | Flags::V | Flags::C));
    if (result == 0)                                    regs_[CC] |= Flags::Z;
    if (result & 0x8000)                                regs_[CC] |= Flags::N;
    if (result32 & 0x10000)                             regs_[CC] |= Flags::C;
    if ((a ^ b ^ result ^ (result32 >> 1)) & 0x8000)   regs_[CC] |= Flags::V;
    return result;
}

/// SUB16: result = a - b
inline uint16_t alu_sub16(uint16_t a, uint16_t b) {
    uint32_t result32 = static_cast<uint32_t>(a) - b;
    uint16_t result = static_cast<uint16_t>(result32);

    regs_[CC] &= static_cast<uint8_t>(~(Flags::N | Flags::Z | Flags::V | Flags::C));
    if (result == 0)                               regs_[CC] |= Flags::Z;
    if (result & 0x8000)                           regs_[CC] |= Flags::N;
    if (result32 & 0x10000)                        regs_[CC] |= Flags::C;
    if ((a ^ b) & (a ^ result) & 0x8000)           regs_[CC] |= Flags::V;
    return result;
}

/// CMP16: compare a - b, set flags, discard result
inline void alu_cmp16(uint16_t a, uint16_t b) {
    alu_sub16(a, b);
}

/// LD16: load a 16-bit value, set N/Z/V
inline void alu_ld16_flags(uint16_t val) {
    set_nz16(val);
    set_flag(Flags::V, false);
}

// ========================================================================
// Condition code evaluation (for branch instructions)
// ========================================================================

/// BRA/BRN
bool cc_always()  const { return true; }
bool cc_never()   const { return false; }

/// BHI: C=0 AND Z=0
bool cc_hi() const { return !(regs_[CC] & (Flags::C | Flags::Z)); }
/// BLS: C=1 OR Z=1
bool cc_ls() const { return (regs_[CC] & (Flags::C | Flags::Z)) != 0; }
/// BCC/BHS: C=0
bool cc_cc() const { return !(regs_[CC] & Flags::C); }
/// BCS/BLO: C=1
bool cc_cs() const { return (regs_[CC] & Flags::C) != 0; }
/// BNE: Z=0
bool cc_ne() const { return !(regs_[CC] & Flags::Z); }
/// BEQ: Z=1
bool cc_eq() const { return (regs_[CC] & Flags::Z) != 0; }
/// BVC: V=0
bool cc_vc() const { return !(regs_[CC] & Flags::V); }
/// BVS: V=1
bool cc_vs() const { return (regs_[CC] & Flags::V) != 0; }
/// BPL: N=0
bool cc_pl() const { return !(regs_[CC] & Flags::N); }
/// BMI: N=1
bool cc_mi() const { return (regs_[CC] & Flags::N) != 0; }
/// BGE: N^V=0  (N=bit3, V=bit1; shift by 2 to align)
bool cc_ge() const { return !(((regs_[CC] >> 2) ^ regs_[CC]) & Flags::V); }
/// BLT: N^V=1
bool cc_lt() const { return (((regs_[CC] >> 2) ^ regs_[CC]) & Flags::V) != 0; }
/// BGT: Z=0 AND N^V=0
bool cc_gt() const { return !(regs_[CC] & Flags::Z) && cc_ge(); }
/// BLE: Z=1 OR N^V=1
bool cc_le() const { return (regs_[CC] & Flags::Z) || cc_lt(); }

/// Evaluate condition code by opcode nibble (lower 4 bits of branch opcode)
bool eval_cc(uint8_t cond) const {
    switch (cond & 0x0F) {
    case 0x0: return cc_always();  // BRA
    case 0x1: return cc_never();   // BRN
    case 0x2: return cc_hi();      // BHI
    case 0x3: return cc_ls();      // BLS
    case 0x4: return cc_cc();      // BCC/BHS
    case 0x5: return cc_cs();      // BCS/BLO
    case 0x6: return cc_ne();      // BNE
    case 0x7: return cc_eq();      // BEQ
    case 0x8: return cc_vc();      // BVC
    case 0x9: return cc_vs();      // BVS
    case 0xA: return cc_pl();      // BPL
    case 0xB: return cc_mi();      // BMI
    case 0xC: return cc_ge();      // BGE
    case 0xD: return cc_lt();      // BLT
    case 0xE: return cc_gt();      // BGT
    case 0xF: return cc_le();      // BLE
    default:  return false;
    }
}

// ========================================================================
// MUL: 8×8 → 16 unsigned multiply (A × B → D)
// ========================================================================

inline void alu_mul() {
    uint16_t result = static_cast<uint16_t>(regs_[A]) * regs_[B];
    regs_[D] = result;
    set_flag(Flags::Z, result == 0);
    set_flag(Flags::C, (result & 0x80) != 0);  // Bit 7 of result → C
}

// ========================================================================
// SEX: sign extend B → D (B → A:B where A = sign extension)
// ========================================================================

inline void alu_sex() {
    regs_[A] = (regs_[B] & 0x80) ? 0xFF : 0x00;
    set_nz16(regs_[D]);  // HD6309 sets flags based on 16-bit D
    set_flag(Flags::V, false);  // not officially documented, but cleared
}
