/*
 * z80_registers.inc.hpp — Register Access Helpers for Z80 Family
 *
 * Included directly inside the z80_t template class definition.
 * Provides register access by 3-bit/2-bit opcode field encoding,
 * IX/IY prefix substitution, and condition code evaluation.
 *
 * NOT a standalone compilation unit — included mid-class in z80.hpp.
 */

// ========================================================================
// 8-bit register access by 3-bit field: B=0 C=1 D=2 E=3 H=4 L=5 (HL)=6 A=7
// Note: index 6 means (HL) — callers must handle memory access separately.
// When DD/FD prefix is active, H→IXH/IYH and L→IXL/IYL (undocumented ops).
// ========================================================================

uint8_t get_reg8(uint8_t idx) const {
    switch (idx) {
    case 0: return regs_.b;
    case 1: return regs_.c;
    case 2: return regs_.d;
    case 3: return regs_.e;
    case 4:
        if constexpr (has_undocumented_ops()) {
            if (ix_iy_prefix_ == 0xDD) return static_cast<uint8_t>(regs_.ix >> 8);
            if (ix_iy_prefix_ == 0xFD) return static_cast<uint8_t>(regs_.iy >> 8);
        }
        return regs_.h;
    case 5:
        if constexpr (has_undocumented_ops()) {
            if (ix_iy_prefix_ == 0xDD) return static_cast<uint8_t>(regs_.ix & 0xFF);
            if (ix_iy_prefix_ == 0xFD) return static_cast<uint8_t>(regs_.iy & 0xFF);
        }
        return regs_.l;
    case 7: return regs_.a;
    default: return 0; // index 6 = (HL), handled by caller
    }
}

void set_reg8(uint8_t idx, uint8_t val) {
    switch (idx) {
    case 0: regs_.b = val; return;
    case 1: regs_.c = val; return;
    case 2: regs_.d = val; return;
    case 3: regs_.e = val; return;
    case 4:
        if constexpr (has_undocumented_ops()) {
            if (ix_iy_prefix_ == 0xDD) { regs_.ix = (regs_.ix & 0x00FF) | (static_cast<uint16_t>(val) << 8); return; }
            if (ix_iy_prefix_ == 0xFD) { regs_.iy = (regs_.iy & 0x00FF) | (static_cast<uint16_t>(val) << 8); return; }
        }
        regs_.h = val; return;
    case 5:
        if constexpr (has_undocumented_ops()) {
            if (ix_iy_prefix_ == 0xDD) { regs_.ix = (regs_.ix & 0xFF00) | val; return; }
            if (ix_iy_prefix_ == 0xFD) { regs_.iy = (regs_.iy & 0xFF00) | val; return; }
        }
        regs_.l = val; return;
    case 7: regs_.a = val; return;
    }
}

// Direct register access (no IX/IY substitution) — for CB prefix ops
uint8_t get_reg8_direct(uint8_t idx) const {
    switch (idx) {
    case 0: return regs_.b;
    case 1: return regs_.c;
    case 2: return regs_.d;
    case 3: return regs_.e;
    case 4: return regs_.h;
    case 5: return regs_.l;
    case 7: return regs_.a;
    default: return 0;
    }
}

void set_reg8_direct(uint8_t idx, uint8_t val) {
    switch (idx) {
    case 0: regs_.b = val; return;
    case 1: regs_.c = val; return;
    case 2: regs_.d = val; return;
    case 3: regs_.e = val; return;
    case 4: regs_.h = val; return;
    case 5: regs_.l = val; return;
    case 7: regs_.a = val; return;
    }
}

// ========================================================================
// HL with IX/IY prefix substitution
// ========================================================================

uint16_t get_hl() const {
    if (ix_iy_prefix_ == 0xDD) return regs_.ix;
    if (ix_iy_prefix_ == 0xFD) return regs_.iy;
    return regs_.hl;
}

void set_hl(uint16_t val) {
    if (ix_iy_prefix_ == 0xDD) { regs_.ix = val; return; }
    if (ix_iy_prefix_ == 0xFD) { regs_.iy = val; return; }
    regs_.hl = val;
}

// Effective address for (HL)/(IX+d)/(IY+d)
uint16_t get_hl_addr() const {
    if (ix_iy_prefix_ == 0xDD) return static_cast<uint16_t>(regs_.ix + displacement_);
    if (ix_iy_prefix_ == 0xFD) return static_cast<uint16_t>(regs_.iy + displacement_);
    return regs_.hl;
}

// Is an IX/IY prefix active?
bool has_ix_iy_prefix() const { return ix_iy_prefix_ != 0; }

// ========================================================================
// 16-bit register pair by 2-bit field: BC=0 DE=1 HL=2 SP=3
// ========================================================================

uint16_t get_reg16(uint8_t idx) const {
    switch (idx) {
    case 0: return regs_.bc;
    case 1: return regs_.de;
    case 2: return get_hl();
    case 3: return regs_.sp;
    default: return 0;
    }
}

void set_reg16(uint8_t idx, uint16_t val) {
    switch (idx) {
    case 0: regs_.bc = val; return;
    case 1: regs_.de = val; return;
    case 2: set_hl(val); return;
    case 3: regs_.sp = val; return;
    }
}

// PUSH/POP variant: BC=0 DE=1 HL=2 AF=3
uint16_t get_reg16_af(uint8_t idx) const {
    switch (idx) {
    case 0: return regs_.bc;
    case 1: return regs_.de;
    case 2: return get_hl();
    case 3: return regs_.af;
    default: return 0;
    }
}

void set_reg16_af(uint8_t idx, uint16_t val) {
    switch (idx) {
    case 0: regs_.bc = val; return;
    case 1: regs_.de = val; return;
    case 2: set_hl(val); return;
    case 3: regs_.af = val; return;
    }
}

// ========================================================================
// Condition code evaluation (3-bit cc field)
// NZ=0 Z=1 NC=2 C=3 PO=4 PE=5 P=6 M=7
// ========================================================================

bool test_cc(uint8_t cc) const {
    switch (cc) {
    case 0: return !(regs_.f & Flags::Z);   // NZ
    case 1: return  (regs_.f & Flags::Z);   // Z
    case 2: return !(regs_.f & Flags::C);   // NC
    case 3: return  (regs_.f & Flags::C);   // C
    case 4: return !(regs_.f & Flags::PV);  // PO (Parity Odd / no overflow)
    case 5: return  (regs_.f & Flags::PV);  // PE (Parity Even / overflow)
    case 6: return !(regs_.f & Flags::S);   // P  (Sign positive)
    case 7: return  (regs_.f & Flags::S);   // M  (Sign minus)
    default: return false;
    }
}

// Test 2-bit condition (for JR NZ/Z/NC/C — only first 4 cc codes)
bool test_cc2(uint8_t cc2) const {
    return test_cc(cc2);
}
