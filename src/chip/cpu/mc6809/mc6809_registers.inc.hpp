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
    case 0: return regs_[D];     // D (A:B)
    case 1: return regs_[X];
    case 2: return regs_[Y];
    case 3: return regs_[U];
    case 4: return regs_[S];
    case 5: return regs_[PC];
    case 6:
        if constexpr (Traits.has_w_register())
            return regs_[W];
        return 0xFFFF;  // Undefined on MC6809
    case 7:
        if constexpr (Traits.has_w_register())
            return regs_[V];
        return 0xFFFF;  // Undefined on MC6809
    default: return 0;
    }
}

void set_reg16(uint8_t code, uint16_t val) {
    switch (code & 0x07) {
    case 0: regs_[D] = val; return;
    case 1: regs_[X] = val; return;
    case 2: regs_[Y] = val; return;
    case 3: regs_[U] = val; return;
    case 4: regs_[S] = val; return;
    case 5: regs_[PC] = val; return;
    case 6:
        if constexpr (Traits.has_w_register())
            regs_[W] = val;
        return;
    case 7:
        if constexpr (Traits.has_w_register())
            regs_[V] = val;
        return;
    }
}

// ========================================================================
// 8-bit register access by 4-bit TFR/EXG code (0x08-0x0F)
// ========================================================================

uint8_t get_reg8(uint8_t code) const {
    switch (code & 0x07) {
    case 0: return regs_[A];     // A
    case 1: return regs_[B];     // B
    case 2: return regs_[CC];    // CC
    case 3: return regs_[DP];    // DP
    case 4:
        if constexpr (Traits.has_w_register())
            return 0;           // Zero register (HD6309)
        return 0xFF;            // Undefined on MC6809
    case 5: return 0xFF;        // Undefined
    case 6:
        if constexpr (Traits.has_w_register())
            return regs_[E];     // E (HD6309)
        return 0xFF;
    case 7:
        if constexpr (Traits.has_w_register())
            return regs_[F];     // F (HD6309)
        return 0xFF;
    default: return 0;
    }
}

void set_reg8(uint8_t code, uint8_t val) {
    switch (code & 0x07) {
    case 0: regs_[A] = val; return;
    case 1: regs_[B] = val; return;
    case 2: regs_[CC] = val; return;
    case 3: regs_[DP] = val; return;
    case 4: return;  // Zero register: writes ignored
    case 5: return;  // Undefined
    case 6:
        if constexpr (Traits.has_w_register())
            regs_[E] = val;
        return;
    case 7:
        if constexpr (Traits.has_w_register())
            regs_[F] = val;
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
    case 0: return regs_[X];
    case 1: return regs_[Y];
    case 2: return regs_[U];
    case 3: return regs_[S];
    default: return 0;
    }
}

void set_index_reg(uint8_t idx, uint16_t val) {
    switch (idx & 0x03) {
    case 0: regs_[X] = val; return;
    case 1: regs_[Y] = val; return;
    case 2: regs_[U] = val; return;
    case 3: regs_[S] = val; return;
    }
}

// ========================================================================
// Stack operations
// ========================================================================

void push_byte_s(uint8_t val) {
    regs_[S]--;
    write_byte(regs_[S], val);
}

void push_word_s(uint16_t val) {
    regs_[S]--;
    write_byte(regs_[S], val & 0xFF);
    regs_[S]--;
    write_byte(regs_[S], val >> 8);
}

uint8_t pull_byte_s() {
    uint8_t val = read_byte(regs_[S]);
    regs_[S]++;
    return val;
}

uint16_t pull_word_s() {
    uint8_t hi = read_byte(regs_[S]);
    regs_[S]++;
    uint8_t lo = read_byte(regs_[S]);
    regs_[S]++;
    return (static_cast<uint16_t>(hi) << 8) | lo;
}

void push_byte_u(uint8_t val) {
    regs_[U]--;
    write_byte(regs_[U], val);
}

void push_word_u(uint16_t val) {
    regs_[U]--;
    write_byte(regs_[U], val & 0xFF);
    regs_[U]--;
    write_byte(regs_[U], val >> 8);
}

uint8_t pull_byte_u() {
    uint8_t val = read_byte(regs_[U]);
    regs_[U]++;
    return val;
}

uint16_t pull_word_u() {
    uint8_t hi = read_byte(regs_[U]);
    regs_[U]++;
    uint8_t lo = read_byte(regs_[U]);
    regs_[U]++;
    return (static_cast<uint16_t>(hi) << 8) | lo;
}
