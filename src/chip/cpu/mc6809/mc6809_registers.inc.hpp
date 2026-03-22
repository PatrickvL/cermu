// ========================================================================
// mc6809_registers.inc.hpp — Register access helpers (included mid-class)
// ========================================================================
//
// Provides type-safe register access via the 4-bit TFR/EXG register
// encoding, plus convenience accessors for common register accesses.
//
// Included inside mc6809_t<Traits> class body.
// ========================================================================

// ========================================================================
// 16-bit register access by 4-bit TFR/EXG code (0x00-0x07)
// ========================================================================

uint16_t get_reg16(uint8_t code) const {
    switch (code & 0x07) {
    case 0: return regs_.d;     // D (A:B)
    case 1: return regs_.x;
    case 2: return regs_.y;
    case 3: return regs_.u;
    case 4: return regs_.s;
    case 5: return regs_.pc;
    case 6:
        if constexpr (Traits.has_w_register())
            return regs_.w;
        return 0xFFFF;  // Undefined on MC6809
    case 7:
        if constexpr (Traits.has_w_register())
            return regs_.v;
        return 0xFFFF;  // Undefined on MC6809
    default: return 0;
    }
}

void set_reg16(uint8_t code, uint16_t val) {
    switch (code & 0x07) {
    case 0: regs_.d = val; return;
    case 1: regs_.x = val; return;
    case 2: regs_.y = val; return;
    case 3: regs_.u = val; return;
    case 4: regs_.s = val; return;
    case 5: regs_.pc = val; return;
    case 6:
        if constexpr (Traits.has_w_register())
            regs_.w = val;
        return;
    case 7:
        if constexpr (Traits.has_w_register())
            regs_.v = val;
        return;
    }
}

// ========================================================================
// 8-bit register access by 4-bit TFR/EXG code (0x08-0x0F)
// ========================================================================

uint8_t get_reg8(uint8_t code) const {
    switch (code & 0x07) {
    case 0: return regs_.a;     // A
    case 1: return regs_.b;     // B
    case 2: return regs_.cc;    // CC
    case 3: return regs_.dp;    // DP
    case 4:
        if constexpr (Traits.has_w_register())
            return 0;           // Zero register (HD6309)
        return 0xFF;            // Undefined on MC6809
    case 5: return 0xFF;        // Undefined
    case 6:
        if constexpr (Traits.has_w_register())
            return regs_.e;     // E (HD6309)
        return 0xFF;
    case 7:
        if constexpr (Traits.has_w_register())
            return regs_.f;     // F (HD6309)
        return 0xFF;
    default: return 0;
    }
}

void set_reg8(uint8_t code, uint8_t val) {
    switch (code & 0x07) {
    case 0: regs_.a = val; return;
    case 1: regs_.b = val; return;
    case 2: regs_.cc = val; return;
    case 3: regs_.dp = val; return;
    case 4: return;  // Zero register: writes ignored
    case 5: return;  // Undefined
    case 6:
        if constexpr (Traits.has_w_register())
            regs_.e = val;
        return;
    case 7:
        if constexpr (Traits.has_w_register())
            regs_.f = val;
        return;
    }
}

// ========================================================================
// Generic TFR/EXG register access (handles both 8/16-bit via code bit 3)
// ========================================================================

uint16_t get_tfr_reg(uint8_t code) const {
    if (code & 0x08)
        return get_reg8(code);
    return get_reg16(code);
}

void set_tfr_reg(uint8_t code, uint16_t val) {
    if (code & 0x08)
        set_reg8(code, static_cast<uint8_t>(val));
    else
        set_reg16(code, val);
}

// ========================================================================
// Index register access for indexed addressing (2-bit field)
// ========================================================================

uint16_t get_index_reg(uint8_t idx) const {
    switch (idx & 0x03) {
    case 0: return regs_.x;
    case 1: return regs_.y;
    case 2: return regs_.u;
    case 3: return regs_.s;
    default: return 0;
    }
}

void set_index_reg(uint8_t idx, uint16_t val) {
    switch (idx & 0x03) {
    case 0: regs_.x = val; return;
    case 1: regs_.y = val; return;
    case 2: regs_.u = val; return;
    case 3: regs_.s = val; return;
    }
}

// ========================================================================
// Stack operations
// ========================================================================

void push_byte_s(uint8_t val) {
    regs_.s--;
    write_byte(regs_.s, val);
}

void push_word_s(uint16_t val) {
    regs_.s--;
    write_byte(regs_.s, val & 0xFF);
    regs_.s--;
    write_byte(regs_.s, val >> 8);
}

uint8_t pull_byte_s() {
    uint8_t val = read_byte(regs_.s);
    regs_.s++;
    return val;
}

uint16_t pull_word_s() {
    uint8_t hi = read_byte(regs_.s);
    regs_.s++;
    uint8_t lo = read_byte(regs_.s);
    regs_.s++;
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

void push_byte_u(uint8_t val) {
    regs_.u--;
    write_byte(regs_.u, val);
}

void push_word_u(uint16_t val) {
    regs_.u--;
    write_byte(regs_.u, val & 0xFF);
    regs_.u--;
    write_byte(regs_.u, val >> 8);
}

uint8_t pull_byte_u() {
    uint8_t val = read_byte(regs_.u);
    regs_.u++;
    return val;
}

uint16_t pull_word_u() {
    uint8_t hi = read_byte(regs_.u);
    regs_.u++;
    uint8_t lo = read_byte(regs_.u);
    regs_.u++;
    return (static_cast<uint16_t>(hi) << 8) | lo;
}
