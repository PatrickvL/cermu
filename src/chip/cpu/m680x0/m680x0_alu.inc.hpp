// m680x0_alu.inc.hpp — ALU operations and flag computation (included mid-class)
//
// All 68000 ALU operations are size-aware (.B, .W, .L).
// Flags are computed per-operation according to the 68000 Programmer's
// Reference Manual.  No precomputed tables — direct computation.
//
// CCR flags: X N Z V C
//   X = Extend (same as C for most operations)
//   N = Negative (MSB of result)
//   Z = Zero
//   V = Overflow (signed)
//   C = Carry (unsigned)

// ========================================================================
// Flag computation helpers
// ========================================================================

/// Set N and Z flags for a result of given size
inline void set_nz(uint32_t result, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    result &= mask;
    uint8_t ccr = get_ccr() & ~(Flags::N | Flags::Z);
    if (result == 0)     ccr |= Flags::Z;
    if (result & msb)    ccr |= Flags::N;
    set_ccr(ccr);
}

/// Evaluate a condition code against the current CCR
inline bool test_condition(Condition cc) const {
    uint8_t ccr = get_ccr();
    bool c = (ccr & Flags::C) != 0;
    bool v = (ccr & Flags::V) != 0;
    bool z = (ccr & Flags::Z) != 0;
    bool n = (ccr & Flags::N) != 0;

    switch (cc) {
        case Condition::T:  return true;
        case Condition::F:  return false;
        case Condition::HI: return !c && !z;
        case Condition::LS: return c || z;
        case Condition::CC: return !c;
        case Condition::CS: return c;
        case Condition::NE: return !z;
        case Condition::EQ: return z;
        case Condition::VC: return !v;
        case Condition::VS: return v;
        case Condition::PL: return !n;
        case Condition::MI: return n;
        case Condition::GE: return (n && v) || (!n && !v);
        case Condition::LT: return (n && !v) || (!n && v);
        case Condition::GT: return (n && v && !z) || (!n && !v && !z);
        case Condition::LE: return z || (n && !v) || (!n && v);
    }
    return false;
}

// ========================================================================
// Arithmetic operations
// ========================================================================

/// ADD: dst = dst + src (updates X, N, Z, V, C)
inline uint32_t alu_add(uint32_t src, uint32_t dst, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    src &= mask;
    dst &= mask;
    uint32_t result = src + dst;
    uint32_t carries = src ^ dst ^ result;

    uint8_t ccr = 0;
    if ((result & mask) == 0)              ccr |= Flags::Z;
    if (result & msb)                      ccr |= Flags::N;
    if (result & ~mask)                    ccr |= Flags::C | Flags::X;  // unsigned overflow
    // Signed overflow: same-sign operands produce different-sign result
    if ((~(src ^ dst) & (src ^ result)) & msb) ccr |= Flags::V;
    set_ccr(ccr);
    return result & mask;
}

/// SUB: dst = dst - src (updates X, N, Z, V, C)
inline uint32_t alu_sub(uint32_t src, uint32_t dst, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    src &= mask;
    dst &= mask;
    uint32_t result = dst - src;

    uint8_t ccr = 0;
    if ((result & mask) == 0)              ccr |= Flags::Z;
    if (result & msb)                      ccr |= Flags::N;
    if (dst < src)                         ccr |= Flags::C | Flags::X;  // borrow
    // Signed overflow: different-sign operands, result sign ≠ dst sign
    if (((src ^ dst) & (dst ^ result)) & msb) ccr |= Flags::V;
    set_ccr(ccr);
    return result & mask;
}

/// CMP: dst - src (same as SUB but result is discarded, no X update)
inline void alu_cmp(uint32_t src, uint32_t dst, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    src &= mask;
    dst &= mask;
    uint32_t result = dst - src;

    uint8_t ccr = get_ccr() & Flags::X;  // Preserve X
    if ((result & mask) == 0)              ccr |= Flags::Z;
    if (result & msb)                      ccr |= Flags::N;
    if (dst < src)                         ccr |= Flags::C;
    if (((src ^ dst) & (dst ^ result)) & msb) ccr |= Flags::V;
    set_ccr(ccr);
}

/// ADDX: dst = dst + src + X (updates X, N, Z, V, C; Z cleared only if non-zero)
inline uint32_t alu_addx(uint32_t src, uint32_t dst, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    uint32_t x_in = (get_ccr() & Flags::X) ? 1 : 0;
    src &= mask;
    dst &= mask;
    uint32_t result = src + dst + x_in;

    uint8_t ccr = get_ccr() & Flags::Z;  // Z is cleared only if result non-zero
    if ((result & mask) != 0)              ccr &= ~Flags::Z;
    if (result & msb)                      ccr |= Flags::N;
    if (result & ~mask)                    ccr |= Flags::C | Flags::X;
    if ((~(src ^ dst) & (src ^ result)) & msb) ccr |= Flags::V;
    set_ccr(ccr);
    return result & mask;
}

/// SUBX: dst = dst - src - X (updates X, N, Z, V, C; Z cleared only if non-zero)
inline uint32_t alu_subx(uint32_t src, uint32_t dst, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    uint32_t x_in = (get_ccr() & Flags::X) ? 1 : 0;
    src &= mask;
    dst &= mask;
    uint32_t result = dst - src - x_in;

    uint8_t ccr = get_ccr() & Flags::Z;
    if ((result & mask) != 0)              ccr &= ~Flags::Z;
    if (result & msb)                      ccr |= Flags::N;
    if (dst < (src + x_in))               ccr |= Flags::C | Flags::X;
    if (((src ^ dst) & (dst ^ result)) & msb) ccr |= Flags::V;
    set_ccr(ccr);
    return result & mask;
}

/// NEG: result = 0 - src (updates X, N, Z, V, C)
inline uint32_t alu_neg(uint32_t src, OpSize sz) {
    return alu_sub(src, 0, sz);
}

// ========================================================================
// Logic operations
// ========================================================================

/// AND: dst = dst & src (updates N, Z; clears V, C)
inline uint32_t alu_and(uint32_t src, uint32_t dst, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t result = (src & dst) & mask;
    uint8_t ccr = get_ccr() & Flags::X;  // Preserve X
    if (result == 0)                      ccr |= Flags::Z;
    if (result & msb_mask(sz))            ccr |= Flags::N;
    // V and C always cleared
    set_ccr(ccr);
    return result;
}

/// OR: dst = dst | src (updates N, Z; clears V, C)
inline uint32_t alu_or(uint32_t src, uint32_t dst, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t result = (src | dst) & mask;
    uint8_t ccr = get_ccr() & Flags::X;
    if (result == 0)                      ccr |= Flags::Z;
    if (result & msb_mask(sz))            ccr |= Flags::N;
    set_ccr(ccr);
    return result;
}

/// EOR: dst = dst ^ src (updates N, Z; clears V, C)
inline uint32_t alu_eor(uint32_t src, uint32_t dst, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t result = (src ^ dst) & mask;
    uint8_t ccr = get_ccr() & Flags::X;
    if (result == 0)                      ccr |= Flags::Z;
    if (result & msb_mask(sz))            ccr |= Flags::N;
    set_ccr(ccr);
    return result;
}

/// NOT: dst = ~src (updates N, Z; clears V, C)
inline uint32_t alu_not(uint32_t src, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t result = (~src) & mask;
    uint8_t ccr = get_ccr() & Flags::X;
    if (result == 0)                      ccr |= Flags::Z;
    if (result & msb_mask(sz))            ccr |= Flags::N;
    set_ccr(ccr);
    return result;
}

/// TST: set N, Z from operand; clear V, C
inline void alu_tst(uint32_t src, OpSize sz) {
    uint32_t mask = size_mask(sz);
    src &= mask;
    uint8_t ccr = get_ccr() & Flags::X;
    if (src == 0)                         ccr |= Flags::Z;
    if (src & msb_mask(sz))               ccr |= Flags::N;
    set_ccr(ccr);
}

/// CLR: result = 0 (N cleared, Z set, V cleared, C cleared)
inline void alu_clr(OpSize /*sz*/) {
    uint8_t ccr = (get_ccr() & Flags::X) | Flags::Z;
    set_ccr(ccr);
}

// ========================================================================
// Shift / Rotate operations
// ========================================================================

/// ASL: arithmetic shift left (updates X, N, Z, V, C)
inline uint32_t alu_asl(uint32_t val, uint8_t count, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    val &= mask;

    uint8_t ccr = get_ccr() & Flags::X;
    bool v_set = false;

    if (count == 0) {
        // No shift — C cleared, V cleared, X unchanged
        ccr &= ~(Flags::C | Flags::V);
    } else {
        for (uint8_t i = 0; i < count; ++i) {
            bool carry = (val & msb) != 0;
            val = (val << 1) & mask;
            if (carry) { ccr |= Flags::C | Flags::X; }
            else       { ccr &= ~(Flags::C | Flags::X); }
            // V is set if MSB changes at any point during the shift
            if (((val & msb) != 0) != carry) v_set = true;
        }
    }

    if (v_set)              ccr |= Flags::V; else ccr &= ~Flags::V;
    if (val == 0)           ccr |= Flags::Z; else ccr &= ~Flags::Z;
    if (val & msb)          ccr |= Flags::N; else ccr &= ~Flags::N;
    set_ccr(ccr);
    return val;
}

/// LSL: logical shift left (updates X, N, Z, C; V always cleared)
inline uint32_t alu_lsl(uint32_t val, uint8_t count, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    val &= mask;

    uint8_t ccr = get_ccr() & Flags::X;
    ccr &= ~Flags::V;  // V always cleared

    if (count == 0) {
        ccr &= ~Flags::C;
    } else {
        for (uint8_t i = 0; i < count; ++i) {
            bool carry = (val & msb) != 0;
            val = (val << 1) & mask;
            if (carry) { ccr |= Flags::C | Flags::X; }
            else       { ccr &= ~(Flags::C | Flags::X); }
        }
    }

    if (val == 0)           ccr |= Flags::Z; else ccr &= ~Flags::Z;
    if (val & msb)          ccr |= Flags::N; else ccr &= ~Flags::N;
    set_ccr(ccr);
    return val;
}

/// ASR: arithmetic shift right (MSB preserved, updates X, N, Z, C; V cleared)
inline uint32_t alu_asr(uint32_t val, uint8_t count, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    val &= mask;

    uint8_t ccr = get_ccr() & Flags::X;
    ccr &= ~Flags::V;

    if (count == 0) {
        ccr &= ~Flags::C;
    } else {
        bool sign = (val & msb) != 0;
        uint8_t bits = size_bytes(sz) * 8;
        if (count <= bits) {
            // Normal shift — iterate count times
            for (uint8_t i = 0; i < count; ++i) {
                bool carry = (val & 1) != 0;
                val = (val >> 1) & mask;
                if (sign) val |= msb;
                if (carry) { ccr |= Flags::C | Flags::X; }
                else       { ccr &= ~(Flags::C | Flags::X); }
            }
        } else {
            // Count > bit width: result = sign fill, C and X cleared
            val = sign ? mask : 0;
            ccr &= ~(Flags::C | Flags::X);
        }
    }

    if ((val & mask) == 0)  ccr |= Flags::Z; else ccr &= ~Flags::Z;
    if (val & msb)          ccr |= Flags::N; else ccr &= ~Flags::N;
    set_ccr(ccr);
    return val;
}

/// LSR: logical shift right (zero fill, updates X, N, Z, C; V cleared)
inline uint32_t alu_lsr(uint32_t val, uint8_t count, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    val &= mask;

    uint8_t ccr = get_ccr() & Flags::X;
    ccr &= ~Flags::V;

    if (count == 0) {
        ccr &= ~Flags::C;
    } else {
        for (uint8_t i = 0; i < count; ++i) {
            bool carry = (val & 1) != 0;
            val = (val >> 1) & mask;
            if (carry) { ccr |= Flags::C | Flags::X; }
            else       { ccr &= ~(Flags::C | Flags::X); }
        }
    }

    if ((val & mask) == 0)  ccr |= Flags::Z; else ccr &= ~Flags::Z;
    if (val & msb)          ccr |= Flags::N; else ccr &= ~Flags::N;
    set_ccr(ccr);
    return val;
}

/// ROL: rotate left through carry (C gets last bit out, V cleared, X unaffected)
inline uint32_t alu_rol(uint32_t val, uint8_t count, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    uint8_t bits = size_bytes(sz) * 8;
    val &= mask;

    uint8_t ccr = get_ccr() & (Flags::X);  // X unaffected
    ccr &= ~Flags::V;

    if (count == 0) {
        ccr &= ~Flags::C;
    } else {
        for (uint8_t i = 0; i < count; ++i) {
            bool carry = (val & msb) != 0;
            val = ((val << 1) | (carry ? 1 : 0)) & mask;
            if (carry) ccr |= Flags::C; else ccr &= ~Flags::C;
        }
    }

    if ((val & mask) == 0)  ccr |= Flags::Z; else ccr &= ~Flags::Z;
    if (val & msb)          ccr |= Flags::N; else ccr &= ~Flags::N;
    set_ccr(ccr);
    return val;
}

/// ROR: rotate right (C gets last bit out, V cleared, X unaffected)
inline uint32_t alu_ror(uint32_t val, uint8_t count, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    uint8_t bits = size_bytes(sz) * 8;
    val &= mask;

    uint8_t ccr = get_ccr() & (Flags::X);
    ccr &= ~Flags::V;

    if (count == 0) {
        ccr &= ~Flags::C;
    } else {
        for (uint8_t i = 0; i < count; ++i) {
            bool carry = (val & 1) != 0;
            val = ((val >> 1) | (carry ? msb : 0)) & mask;
            if (carry) ccr |= Flags::C; else ccr &= ~Flags::C;
        }
    }

    if ((val & mask) == 0)  ccr |= Flags::Z; else ccr &= ~Flags::Z;
    if (val & msb)          ccr |= Flags::N; else ccr &= ~Flags::N;
    set_ccr(ccr);
    return val;
}

// ========================================================================
// Multiply / Divide
// ========================================================================

/// ROXL: rotate left through extend (X bit participates in rotation)
inline uint32_t alu_roxl(uint32_t val, uint8_t count, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    uint8_t bits = size_bytes(sz) * 8;
    val &= mask;

    uint8_t ccr = get_ccr();
    bool x_bit = (ccr & Flags::X) != 0;
    ccr &= ~Flags::V;  // V always cleared

    if (count == 0) {
        // C = X (unchanged)
        if (x_bit) ccr |= Flags::C; else ccr &= ~Flags::C;
    } else {
        // Effective rotation width is bits+1 (includes X)
        count %= (bits + 1);
        for (uint8_t i = 0; i < count; ++i) {
            bool carry = (val & msb) != 0;
            val = ((val << 1) | (x_bit ? 1u : 0u)) & mask;
            x_bit = carry;
        }
        if (x_bit) { ccr |= Flags::C | Flags::X; }
        else       { ccr &= ~(Flags::C | Flags::X); }
    }

    if ((val & mask) == 0) ccr |= Flags::Z; else ccr &= ~Flags::Z;
    if (val & msb)         ccr |= Flags::N; else ccr &= ~Flags::N;
    set_ccr(ccr);
    return val;
}

/// ROXR: rotate right through extend (X bit participates in rotation)
inline uint32_t alu_roxr(uint32_t val, uint8_t count, OpSize sz) {
    uint32_t mask = size_mask(sz);
    uint32_t msb  = msb_mask(sz);
    uint8_t bits = size_bytes(sz) * 8;
    val &= mask;

    uint8_t ccr = get_ccr();
    bool x_bit = (ccr & Flags::X) != 0;
    ccr &= ~Flags::V;

    if (count == 0) {
        if (x_bit) ccr |= Flags::C; else ccr &= ~Flags::C;
    } else {
        count %= (bits + 1);
        for (uint8_t i = 0; i < count; ++i) {
            bool carry = (val & 1) != 0;
            val = ((val >> 1) | (x_bit ? msb : 0u)) & mask;
            x_bit = carry;
        }
        if (x_bit) { ccr |= Flags::C | Flags::X; }
        else       { ccr &= ~(Flags::C | Flags::X); }
    }

    if ((val & mask) == 0) ccr |= Flags::Z; else ccr &= ~Flags::Z;
    if (val & msb)         ccr |= Flags::N; else ccr &= ~Flags::N;
    set_ccr(ccr);
    return val;
}

// ========================================================================
// BCD arithmetic
// ========================================================================

/// ABCD: BCD add src + dst + X → result
/// Updates X, C (carry), N (MSB of result), Z (sticky: only cleared, never set)
/// V = unadjusted bit7 clear AND result bit7 set
inline uint8_t alu_abcd(uint8_t src, uint8_t dst) {
    uint8_t x = (get_ccr() & Flags::X) ? 1 : 0;
    uint8_t init_z = (get_ccr() & Flags::Z) ? 1 : 0;

    uint32_t low = (src & 0x0F) + (dst & 0x0F) + x;
    uint32_t corf = (low > 9) ? 6 : 0;
    low += corf;
    uint32_t high = (src & 0xF0) + (dst & 0xF0) + (low & 0xF0);
    uint8_t carry = 0;
    if (high > 0x90) { high += 0x60; carry = 1; }
    uint8_t result = static_cast<uint8_t>((high & 0xF0) | (low & 0x0F));

    uint8_t unadj = static_cast<uint8_t>(src + dst + x);
    uint8_t ccr = 0;
    if (carry)                                        ccr |= Flags::X | Flags::C;
    if (result & 0x80)                                ccr |= Flags::N;
    if (result == 0 && init_z)                        ccr |= Flags::Z;
    if (!(unadj & 0x80) && (result & 0x80))           ccr |= Flags::V;
    set_ccr(ccr);
    return result;
}

/// SBCD: BCD subtract dst - src - X → result
/// Same flag behavior as ABCD but for subtraction
inline uint8_t alu_sbcd(uint8_t src, uint8_t dst) {
    uint8_t x = (get_ccr() & Flags::X) ? 1 : 0;
    uint8_t init_z = (get_ccr() & Flags::Z) ? 1 : 0;

    int32_t binary = static_cast<int32_t>(dst) - src - x;
    uint8_t result = static_cast<uint8_t>(binary & 0xFF);

    // Low nibble correction: half-borrow from original operands
    uint8_t corf = ((dst & 0xF) < ((src & 0xF) + x)) ? 6 : 0;

    // High nibble correction and carry detection
    uint8_t carry = 0;
    if (binary < 0) {
        // Full borrow: apply both corrections
        result = static_cast<uint8_t>((result - corf - 0x60) & 0xFF);
        carry = 1;
    } else if (result < corf) {
        // Low correction wraps into high nibble
        result = static_cast<uint8_t>((result - corf) & 0xFF);
        carry = 1;
    } else {
        result = static_cast<uint8_t>((result - corf) & 0xFF);
    }

    uint8_t unadj = static_cast<uint8_t>((dst - src - x) & 0xFF);
    uint8_t ccr = 0;
    if (carry)                                        ccr |= Flags::X | Flags::C;
    if (result & 0x80)                                ccr |= Flags::N;
    if (result == 0 && init_z)                        ccr |= Flags::Z;
    if ((unadj & 0x80) && !(result & 0x80))           ccr |= Flags::V;
    set_ccr(ccr);
    return result;
}

/// MULU: unsigned 16×16 → 32 multiply (updates N, Z; clears V, C)
inline uint32_t alu_mulu(uint16_t src, uint16_t dst) {
    uint32_t result = static_cast<uint32_t>(src) * static_cast<uint32_t>(dst);
    uint8_t ccr = get_ccr() & Flags::X;
    if (result == 0)                         ccr |= Flags::Z;
    if (result & 0x80000000)                 ccr |= Flags::N;
    set_ccr(ccr);
    return result;
}

/// MULS: signed 16×16 → 32 multiply (updates N, Z; clears V, C)
inline uint32_t alu_muls(int16_t src, int16_t dst) {
    int32_t result = static_cast<int32_t>(src) * static_cast<int32_t>(dst);
    uint32_t uresult = static_cast<uint32_t>(result);
    uint8_t ccr = get_ccr() & Flags::X;
    if (uresult == 0)                        ccr |= Flags::Z;
    if (uresult & 0x80000000)                ccr |= Flags::N;
    set_ccr(ccr);
    return uresult;
}

// ========================================================================
// Division timing helpers
// ========================================================================

/// Compute the variable loop cost (in clocks) of the DIVU restoring division.
/// The MC68000 performs 16 iterations of shift-and-subtract:
///   Steps 0-14: MSB set → 4 clocks, compare+subtract → 6 clocks, no subtract → 8 clocks
///   Step 15: always 6 clocks (fixed)
/// Caller must ensure no overflow (dividend >> 16 < divisor).
inline uint8_t divu_loop_cost(uint32_t dividend, uint16_t divisor) {
    uint32_t hdivisor = static_cast<uint32_t>(divisor) << 16;
    uint8_t cost = 0;

    for (int i = 0; i < 16; i++) {
        uint32_t msb = dividend & 0x80000000;
        dividend <<= 1;

        if (i == 15) {
            if (msb || dividend >= hdivisor)
                dividend -= hdivisor;
            cost += 6;  // last step always 6
        } else if (msb) {
            dividend -= hdivisor;
            cost += 4;  // MSB set: shift + forced subtract
        } else if (dividend >= hdivisor) {
            dividend -= hdivisor;
            cost += 6;  // compare + subtract
        } else {
            cost += 8;  // compare only, no subtract
        }
    }
    return cost;
}

/// Compute idle clocks for DIVU Dn form (no overflow).
/// Total clocks = idle + 4 (prefetch).
inline uint8_t divu_idle_clocks(uint32_t dividend, uint16_t divisor) {
    return 6 + divu_loop_cost(dividend, divisor);
}

/// DIVU: unsigned 32÷16 → 16q:16r (updates N, Z, V, C; C always cleared)
/// Returns true if division succeeded, false on divide by zero or overflow
inline bool alu_divu(uint32_t dst, uint16_t src, uint16_t& quotient, uint16_t& remainder) {
    if (src == 0) return false;  // Division by zero → exception

    uint32_t result = dst / src;
    if (result > 0xFFFF) {
        // Overflow — V set, other flags undefined
        uint8_t ccr = (get_ccr() & Flags::X) | Flags::V;
        set_ccr(ccr);
        return true;  // No exception, but result undefined
    }

    quotient  = static_cast<uint16_t>(result);
    remainder = static_cast<uint16_t>(dst % src);

    uint8_t ccr = get_ccr() & Flags::X;
    if (quotient == 0)                       ccr |= Flags::Z;
    if (quotient & 0x8000)                   ccr |= Flags::N;
    set_ccr(ccr);
    return true;
}

/// DIVS: signed 32÷16 → 16q:16r
/// Returns true if division succeeded, false on divide by zero
inline bool alu_divs(int32_t dst, int16_t src, int16_t& quotient, int16_t& remainder) {
    if (src == 0) return false;

    int32_t result = dst / src;
    if (result > 32767 || result < -32768) {
        uint8_t ccr = (get_ccr() & Flags::X) | Flags::V;
        set_ccr(ccr);
        return true;
    }

    quotient  = static_cast<int16_t>(result);
    remainder = static_cast<int16_t>(dst % src);

    uint8_t ccr = get_ccr() & Flags::X;
    if (static_cast<uint16_t>(quotient) == 0) ccr |= Flags::Z;
    if (quotient < 0)                         ccr |= Flags::N;
    set_ccr(ccr);
    return true;
}

// ========================================================================
// Bit manipulation
// ========================================================================

/// BTST: test bit — sets Z if bit is zero (other flags unaffected)
inline void alu_btst(uint32_t val, uint8_t bit_num) {
    uint8_t ccr = get_ccr();
    if (val & (1u << bit_num)) ccr &= ~Flags::Z;
    else                       ccr |= Flags::Z;
    set_ccr(ccr);
}

// ========================================================================
// EXT — sign extend
// ========================================================================

/// EXT.W: byte → word (sign extend low byte of Dn to word)
inline uint32_t alu_ext_bw(uint32_t val) {
    int8_t b = static_cast<int8_t>(val & 0xFF);
    uint16_t result = static_cast<uint16_t>(static_cast<int16_t>(b));
    uint8_t ccr = get_ccr() & Flags::X;
    if (result == 0)           ccr |= Flags::Z;
    if (result & 0x8000)       ccr |= Flags::N;
    set_ccr(ccr);
    return result;
}

/// EXT.L: word → long (sign extend low word of Dn to long)
inline uint32_t alu_ext_wl(uint32_t val) {
    int16_t w = static_cast<int16_t>(val & 0xFFFF);
    uint32_t result = static_cast<uint32_t>(static_cast<int32_t>(w));
    uint8_t ccr = get_ccr() & Flags::X;
    if (result == 0)                ccr |= Flags::Z;
    if (result & 0x80000000)        ccr |= Flags::N;
    set_ccr(ccr);
    return result;
}
