#pragma once
/*
 * m680x0_asm.hpp — Motorola 68000 Mini Assembler (Header-Only)
 *
 * Zero-dependency struct that writes raw 68000 machine code into a
 * caller-provided buffer.  Big-endian output.  Used for test programs,
 * test fixtures, and interactive monitor assembly.
 *
 * Usage:
 *   uint8_t code[256];
 *   m68k_asm a(code, sizeof(code));
 *   a.nop();
 *   a.moveq(0x42, 0);          // MOVEQ #$42,D0
 *   a.bra_s(a.origin());       // BRA.S to start
 */

#include <cstdint>
#include <cstring>

namespace m680x0 {

struct m68k_asm {
    uint8_t* buf;
    int      cap;
    int      pos;
    uint32_t origin_;

    m68k_asm(uint8_t* buffer, int capacity, uint32_t org = 0)
        : buf(buffer), cap(capacity), pos(0), origin_(org) {}

    // ── Position / origin ───────────────────────────────────────

    uint32_t pc() const { return origin_ + static_cast<uint32_t>(pos); }
    uint32_t here() const { return pc(); }
    int      offset() const { return pos; }
    uint32_t origin() const { return origin_; }
    void     set_origin(uint32_t org) { origin_ = org; }

    // ── Emission primitives (big-endian) ────────────────────────

    void emit_byte(uint8_t b) {
        if (pos < cap) buf[pos++] = b;
    }

    void emit_word(uint16_t w) {
        emit_byte(static_cast<uint8_t>(w >> 8));
        emit_byte(static_cast<uint8_t>(w & 0xFF));
    }

    void emit_long(uint32_t l) {
        emit_word(static_cast<uint16_t>(l >> 16));
        emit_word(static_cast<uint16_t>(l & 0xFFFF));
    }

    // ── Raw data ────────────────────────────────────────────────

    void dc_w(uint16_t w) { emit_word(w); }
    void dc_l(uint32_t l) { emit_long(l); }

    // ── Branch displacement helpers ─────────────────────────────

    /// Compute 8-bit signed displacement for Bcc.S
    /// (from the start of the instruction + 2 to target)
    int8_t rel8(uint32_t target) const {
        int32_t diff = static_cast<int32_t>(target) - static_cast<int32_t>(pc());
        return static_cast<int8_t>(diff);
    }

    /// Compute 16-bit signed displacement for Bcc.W
    int16_t rel16(uint32_t target) const {
        int32_t diff = static_cast<int32_t>(target) - static_cast<int32_t>(pc());
        return static_cast<int16_t>(diff);
    }

    /// Emit a forward branch placeholder, returns the operand position for fixup
    int bra_fwd() {
        emit_word(0x6000);  // BRA.W with 0 displacement
        int fixup_pos = pos;
        emit_word(0x0000);  // Placeholder displacement
        return fixup_pos;
    }

    /// Patch a forward branch displacement
    void fixup(int fixup_pos) {
        int16_t disp = static_cast<int16_t>(
            static_cast<int32_t>(pc()) -
            static_cast<int32_t>(origin_ + static_cast<uint32_t>(fixup_pos)));
        buf[fixup_pos]     = static_cast<uint8_t>(disp >> 8);
        buf[fixup_pos + 1] = static_cast<uint8_t>(disp & 0xFF);
    }

    // ── NOP ─────────────────────────────────────────────────────

    void nop() { emit_word(0x4E71); }

    void nops(int n) { for (int i = 0; i < n; ++i) nop(); }

    // ── RESET / RTS / RTE / TRAPV / RTR ─────────────────────────

    void reset()  { emit_word(0x4E70); }
    void rts()    { emit_word(0x4E75); }
    void rte()    { emit_word(0x4E73); }
    void trapv()  { emit_word(0x4E76); }
    void rtr()    { emit_word(0x4E77); }

    // ── STOP ────────────────────────────────────────────────────

    void stop(uint16_t sr_val) {
        emit_word(0x4E72);
        emit_word(sr_val);
    }

    // ── TRAP #vector ────────────────────────────────────────────

    void trap(uint8_t vec) {
        emit_word(0x4E40 | (vec & 0x0F));
    }

    // ── ILLEGAL ─────────────────────────────────────────────────

    void illegal() { emit_word(0x4AFC); }

    // ── MOVEQ #imm8,Dn ─────────────────────────────────────────

    void moveq(int8_t imm, uint8_t dn) {
        emit_word(0x7000 | (static_cast<uint16_t>(dn & 7) << 9) |
                  (static_cast<uint8_t>(imm)));
    }

    // ── CLR ─────────────────────────────────────────────────────

    void clr_b(uint8_t ea_mode, uint8_t ea_reg) {
        emit_word(0x4200 | (ea_mode << 3) | ea_reg);
    }
    void clr_w(uint8_t ea_mode, uint8_t ea_reg) {
        emit_word(0x4240 | (ea_mode << 3) | ea_reg);
    }
    void clr_l(uint8_t ea_mode, uint8_t ea_reg) {
        emit_word(0x4280 | (ea_mode << 3) | ea_reg);
    }

    // ── MOVE (register to register shortcuts) ───────────────────

    /// MOVE.L Dn,Dm
    void move_l_dn_dn(uint8_t src, uint8_t dst) {
        emit_word(0x2000 | (static_cast<uint16_t>(dst & 7) << 9) | (src & 7));
    }

    /// MOVE.L An,Am
    void move_l_an_an(uint8_t src, uint8_t dst) {
        emit_word(0x2008 | (static_cast<uint16_t>(dst & 7) << 9) |
                  (0x40) | (src & 7));
    }

    /// MOVEA.L <ea>,An
    void movea_l(uint8_t ea_mode, uint8_t ea_reg, uint8_t an) {
        emit_word(0x2040 | (static_cast<uint16_t>(an & 7) << 9) |
                  (ea_mode << 3) | ea_reg);
    }

    // ── LEA <ea>,An ─────────────────────────────────────────────

    void lea(uint8_t ea_mode, uint8_t ea_reg, uint8_t an) {
        emit_word(0x41C0 | (static_cast<uint16_t>(an & 7) << 9) |
                  (ea_mode << 3) | ea_reg);
    }

    /// LEA (d16,An),Am — with displacement word
    void lea_disp(uint8_t an_src, int16_t disp, uint8_t an_dst) {
        lea(5, an_src, an_dst);
        emit_word(static_cast<uint16_t>(disp));
    }

    // ── ADDQ / SUBQ ────────────────────────────────────────────

    void addq_l(uint8_t data, uint8_t ea_mode, uint8_t ea_reg) {
        uint8_t d = data & 7;
        if (data == 8) d = 0;
        emit_word(0x5080 | (static_cast<uint16_t>(d) << 9) |
                  (ea_mode << 3) | ea_reg);
    }

    void subq_l(uint8_t data, uint8_t ea_mode, uint8_t ea_reg) {
        uint8_t d = data & 7;
        if (data == 8) d = 0;
        emit_word(0x5180 | (static_cast<uint16_t>(d) << 9) |
                  (ea_mode << 3) | ea_reg);
    }

    // ── Branch instructions ─────────────────────────────────────

    /// BRA.S to known target
    void bra_s(uint32_t target) {
        int8_t disp = static_cast<int8_t>(
            static_cast<int32_t>(target) - static_cast<int32_t>(pc() + 2));
        emit_word(0x6000 | (static_cast<uint8_t>(disp)));
    }

    /// BRA.W to known target
    void bra_w(uint32_t target) {
        emit_word(0x6000);
        emit_word(static_cast<uint16_t>(rel16(target)));
    }

    /// BSR.S to known target
    void bsr_s(uint32_t target) {
        int8_t disp = static_cast<int8_t>(
            static_cast<int32_t>(target) - static_cast<int32_t>(pc() + 2));
        emit_word(0x6100 | (static_cast<uint8_t>(disp)));
    }

    /// Bcc.S (short branch with condition code)
    void bcc_s(uint8_t cc, uint32_t target) {
        int8_t disp = static_cast<int8_t>(
            static_cast<int32_t>(target) - static_cast<int32_t>(pc() + 2));
        emit_word(static_cast<uint16_t>((0x6000 | (cc << 8)) |
                  static_cast<uint8_t>(disp)));
    }

    /// BEQ.S shortcut
    void beq_s(uint32_t target) { bcc_s(7, target); }
    /// BNE.S shortcut
    void bne_s(uint32_t target) { bcc_s(6, target); }
    /// BCS.S shortcut
    void bcs_s(uint32_t target) { bcc_s(5, target); }
    /// BCC.S shortcut
    void bcc_s_cc(uint32_t target) { bcc_s(4, target); }
    /// BMI.S shortcut
    void bmi_s(uint32_t target) { bcc_s(11, target); }
    /// BPL.S shortcut
    void bpl_s(uint32_t target) { bcc_s(10, target); }

    // ── DBcc Dn,label ───────────────────────────────────────────

    void dbcc(uint8_t cc, uint8_t dn, uint32_t target) {
        emit_word(0x50C8 | (cc << 8) | (dn & 7));
        emit_word(static_cast<uint16_t>(
            static_cast<int32_t>(target) - static_cast<int32_t>(pc())));
    }

    void dbra(uint8_t dn, uint32_t target) { dbcc(1, dn, target); }

    // ── JSR / JMP (absolute long) ───────────────────────────────

    void jsr_abs(uint32_t addr) {
        emit_word(0x4EB9);  // JSR (xxx).L
        emit_long(addr);
    }

    void jmp_abs(uint32_t addr) {
        emit_word(0x4EF9);  // JMP (xxx).L
        emit_long(addr);
    }

    // ── LINK / UNLK ─────────────────────────────────────────────

    void link(uint8_t an, int16_t disp) {
        emit_word(0x4E50 | (an & 7));
        emit_word(static_cast<uint16_t>(disp));
    }

    void unlk(uint8_t an) {
        emit_word(0x4E58 | (an & 7));
    }

    // ── SWAP Dn ─────────────────────────────────────────────────

    void swap(uint8_t dn) {
        emit_word(0x4840 | (dn & 7));
    }

    // ── EXT ─────────────────────────────────────────────────────

    void ext_w(uint8_t dn) { emit_word(0x4880 | (dn & 7)); }
    void ext_l(uint8_t dn) { emit_word(0x48C0 | (dn & 7)); }

    // ── EXG ─────────────────────────────────────────────────────

    void exg_dd(uint8_t dx, uint8_t dy) {
        emit_word(0xC140 | (static_cast<uint16_t>(dx & 7) << 9) | (dy & 7));
    }
    void exg_aa(uint8_t ax, uint8_t ay) {
        emit_word(0xC148 | (static_cast<uint16_t>(ax & 7) << 9) | (ay & 7));
    }
    void exg_da(uint8_t dx, uint8_t ay) {
        emit_word(0xC188 | (static_cast<uint16_t>(dx & 7) << 9) | (ay & 7));
    }

    // ── TST ─────────────────────────────────────────────────────

    void tst_b(uint8_t ea_mode, uint8_t ea_reg) {
        emit_word(0x4A00 | (ea_mode << 3) | ea_reg);
    }
    void tst_w(uint8_t ea_mode, uint8_t ea_reg) {
        emit_word(0x4A40 | (ea_mode << 3) | ea_reg);
    }
    void tst_l(uint8_t ea_mode, uint8_t ea_reg) {
        emit_word(0x4A80 | (ea_mode << 3) | ea_reg);
    }

    // ── BRA.S *  (branch to self = infinite loop) ───────────────

    void bra_self() {
        emit_word(0x60FE);  // BRA.S $-2
    }

    // ── Vector table helpers (for test fixtures) ────────────────

    /// Emit a reset vector pair: initial SSP and initial PC
    void reset_vectors(uint32_t ssp, uint32_t initial_pc) {
        emit_long(ssp);
        emit_long(initial_pc);
    }

    /// Emit an exception vector (4-byte address)
    void exception_vector(uint32_t handler_addr) {
        emit_long(handler_addr);
    }
};

} // namespace m680x0
