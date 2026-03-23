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
    case 0: return regs_[REG_B];
    case 1: return regs_[REG_C];
    case 2: return regs_[REG_D];
    case 3: return regs_[REG_E];
    case 4:
        if constexpr (has_undocumented_ops()) {
            if (ix_iy_prefix_ == 0xDD) return static_cast<uint8_t>(regs_[REG_IX] >> 8);
            if (ix_iy_prefix_ == 0xFD) return static_cast<uint8_t>(regs_[REG_IY] >> 8);
        }
        return regs_[REG_H];
    case 5:
        if constexpr (has_undocumented_ops()) {
            if (ix_iy_prefix_ == 0xDD) return static_cast<uint8_t>(regs_[REG_IX] & 0xFF);
            if (ix_iy_prefix_ == 0xFD) return static_cast<uint8_t>(regs_[REG_IY] & 0xFF);
        }
        return regs_[REG_L];
    case 7: return regs_[REG_A];
    default: return 0; // index 6 = (HL), handled by caller
    }
}

void set_reg8(uint8_t idx, uint8_t val) {
    switch (idx) {
    case 0: regs_[REG_B] = val; return;
    case 1: regs_[REG_C] = val; return;
    case 2: regs_[REG_D] = val; return;
    case 3: regs_[REG_E] = val; return;
    case 4:
        if constexpr (has_undocumented_ops()) {
            if (ix_iy_prefix_ == 0xDD) { regs_[REG_IX] = (regs_[REG_IX] & 0x00FF) | (static_cast<uint16_t>(val) << 8); return; }
            if (ix_iy_prefix_ == 0xFD) { regs_[REG_IY] = (regs_[REG_IY] & 0x00FF) | (static_cast<uint16_t>(val) << 8); return; }
        }
        regs_[REG_H] = val; return;
    case 5:
        if constexpr (has_undocumented_ops()) {
            if (ix_iy_prefix_ == 0xDD) { regs_[REG_IX] = (regs_[REG_IX] & 0xFF00) | val; return; }
            if (ix_iy_prefix_ == 0xFD) { regs_[REG_IY] = (regs_[REG_IY] & 0xFF00) | val; return; }
        }
        regs_[REG_L] = val; return;
    case 7: regs_[REG_A] = val; return;
    }
}

// Direct register access (no IX/IY substitution) — for CB prefix ops
uint8_t get_reg8_direct(uint8_t idx) const {
    switch (idx) {
    case 0: return regs_[REG_B];
    case 1: return regs_[REG_C];
    case 2: return regs_[REG_D];
    case 3: return regs_[REG_E];
    case 4: return regs_[REG_H];
    case 5: return regs_[REG_L];
    case 7: return regs_[REG_A];
    default: return 0;
    }
}

void set_reg8_direct(uint8_t idx, uint8_t val) {
    switch (idx) {
    case 0: regs_[REG_B] = val; return;
    case 1: regs_[REG_C] = val; return;
    case 2: regs_[REG_D] = val; return;
    case 3: regs_[REG_E] = val; return;
    case 4: regs_[REG_H] = val; return;
    case 5: regs_[REG_L] = val; return;
    case 7: regs_[REG_A] = val; return;
    }
}

// ========================================================================
// HL with IX/IY prefix substitution
// ========================================================================

uint16_t get_hl() const {
    if (ix_iy_prefix_ == 0xDD) return regs_[REG_IX];
    if (ix_iy_prefix_ == 0xFD) return regs_[REG_IY];
    return regs_[REG_HL];
}

void set_hl(uint16_t val) {
    if (ix_iy_prefix_ == 0xDD) { regs_[REG_IX] = val; return; }
    if (ix_iy_prefix_ == 0xFD) { regs_[REG_IY] = val; return; }
    regs_[REG_HL] = val;
}

// Effective address for (HL)/(IX+d)/(IY+d)
uint16_t get_hl_addr() const {
    if (ix_iy_prefix_ == 0xDD) return static_cast<uint16_t>(regs_[REG_IX] + displacement_);
    if (ix_iy_prefix_ == 0xFD) return static_cast<uint16_t>(regs_[REG_IY] + displacement_);
    return regs_[REG_HL];
}

// Is an IX/IY prefix active?
bool has_ix_iy_prefix() const { return ix_iy_prefix_ != 0; }

// ========================================================================
// 16-bit register pair by 2-bit field: BC=0 DE=1 HL=2 SP=3
// ========================================================================

uint16_t get_reg16(uint8_t idx) const {
    switch (idx) {
    case 0: return regs_[REG_BC];
    case 1: return regs_[REG_DE];
    case 2: return get_hl();
    case 3: return regs_[REG_SP];
    default: return 0;
    }
}

void set_reg16(uint8_t idx, uint16_t val) {
    switch (idx) {
    case 0: regs_[REG_BC] = val; return;
    case 1: regs_[REG_DE] = val; return;
    case 2: set_hl(val); return;
    case 3: regs_[REG_SP] = val; return;
    }
}

// PUSH/POP variant: BC=0 DE=1 HL=2 AF=3
uint16_t get_reg16_af(uint8_t idx) const {
    switch (idx) {
    case 0: return regs_[REG_BC];
    case 1: return regs_[REG_DE];
    case 2: return get_hl();
    case 3: return regs_[REG_AF];
    default: return 0;
    }
}

void set_reg16_af(uint8_t idx, uint16_t val) {
    switch (idx) {
    case 0: regs_[REG_BC] = val; return;
    case 1: regs_[REG_DE] = val; return;
    case 2: set_hl(val); return;
    case 3: regs_[REG_AF] = val; return;
    }
}

// ========================================================================
// Condition code evaluation (3-bit cc field)
// NZ=0 Z=1 NC=2 C=3 PO=4 PE=5 P=6 M=7
// ========================================================================

bool test_cc(uint8_t cc) const {
    switch (cc) {
    case 0: return !(regs_[REG_F] & Flags::Z);   // NZ
    case 1: return  (regs_[REG_F] & Flags::Z);   // Z
    case 2: return !(regs_[REG_F] & Flags::C);   // NC
    case 3: return  (regs_[REG_F] & Flags::C);   // C
    case 4: return !(regs_[REG_F] & Flags::PV);  // PO (Parity Odd / no overflow)
    case 5: return  (regs_[REG_F] & Flags::PV);  // PE (Parity Even / overflow)
    case 6: return !(regs_[REG_F] & Flags::S);   // P  (Sign positive)
    case 7: return  (regs_[REG_F] & Flags::S);   // M  (Sign minus)
    default: return false;
    }
}

// Test 2-bit condition (for JR NZ/Z/NC/C — only first 4 cc codes)
bool test_cc2(uint8_t cc2) const {
    return test_cc(cc2);
}
