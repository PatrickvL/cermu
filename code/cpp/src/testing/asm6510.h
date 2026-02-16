#pragma once
// =============================================================================
// asm6510 — Mini 6502/6510 assembler for test harness code generation
// =============================================================================
// Emits raw machine code into a caller-provided buffer. Supports:
//   - Named opcode emission (no literal hex bytes needed in test code)
//   - Symbolic labels with deferred branch/jump fixup
//   - Arbitrary origin address (set_origin)
//   - Bulk copy to emulator RAM via a callback or manual loop
//
// Usage:
//   uint8_t buf[256];
//   asm6510 a(buf, sizeof(buf), 0x8000);
//   a.sei();
//   a.lda_imm(0x35);
//   a.sta_zp(0x01);
//   auto loop = a.here();     // capture current address as label
//   a.lda_abs(0xD012);
//   a.cmp_imm(100);
//   a.bne(loop);              // branch back to label
//   a.jmp_to(loop);           // absolute jump to label
//   printf("Generated %zu bytes at $%04X\n", a.pos, a.origin);
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

struct asm6510 {
    uint8_t* buf;
    size_t   pos;
    size_t   cap;
    uint16_t origin;  // Base address of buf[0] in 6510 address space

    // --- Constructors ---
    asm6510() : buf(nullptr), pos(0), cap(0), origin(0) {}
    asm6510(uint8_t* buffer, size_t capacity, uint16_t org = 0)
        : buf(buffer), pos(0), cap(capacity), origin(org) {}

    // --- Core emit ---
    void emit(uint8_t b) { if (pos < cap) buf[pos++] = b; }
    void emit2(uint8_t op, uint8_t arg) { emit(op); emit(arg); }
    void emit3(uint8_t op, uint8_t lo, uint8_t hi) { emit(op); emit(lo); emit(hi); }

    // --- Address helpers ---
    /// Current PC (origin + pos)
    uint16_t pc() const { return static_cast<uint16_t>(origin + pos); }
    /// Alias for pc() — returns current emission address as a label
    uint16_t here() const { return pc(); }
    /// Byte offset from start of buffer
    size_t   offset() const { return pos; }
    /// Set origin address (for code at addresses other than 0)
    void set_origin(uint16_t org) { origin = org; }

    // =========================================================================
    //  LOAD / STORE
    // =========================================================================
    // -- Immediate --
    void lda_imm(uint8_t v) { emit2(0xA9, v); }
    void ldx_imm(uint8_t v) { emit2(0xA2, v); }
    void ldy_imm(uint8_t v) { emit2(0xA0, v); }

    // -- Zero page --
    void lda_zp(uint8_t a)  { emit2(0xA5, a); }
    void ldx_zp(uint8_t a)  { emit2(0xA6, a); }
    void ldy_zp(uint8_t a)  { emit2(0xA4, a); }
    void sta_zp(uint8_t a)  { emit2(0x85, a); }
    void stx_zp(uint8_t a)  { emit2(0x86, a); }
    void sty_zp(uint8_t a)  { emit2(0x84, a); }

    // -- Absolute --
    void lda_abs(uint16_t a){ emit3(0xAD, a & 0xFF, a >> 8); }
    void ldx_abs(uint16_t a){ emit3(0xAE, a & 0xFF, a >> 8); }
    void ldy_abs(uint16_t a){ emit3(0xAC, a & 0xFF, a >> 8); }
    void sta_abs(uint16_t a){ emit3(0x8D, a & 0xFF, a >> 8); }
    void stx_abs(uint16_t a){ emit3(0x8E, a & 0xFF, a >> 8); }
    void sty_abs(uint16_t a){ emit3(0x8C, a & 0xFF, a >> 8); }

    // -- Indexed / indirect --
    void sta_abs_x(uint16_t a) { emit3(0x9D, a & 0xFF, a >> 8); }
    void sta_abs_y(uint16_t a) { emit3(0x99, a & 0xFF, a >> 8); }
    void lda_abs_x(uint16_t a) { emit3(0xBD, a & 0xFF, a >> 8); }
    void lda_abs_y(uint16_t a) { emit3(0xB9, a & 0xFF, a >> 8); }
    void lda_ind_y(uint8_t zp) { emit2(0xB1, zp); }
    void sta_ind_y(uint8_t zp) { emit2(0x91, zp); }
    void lda_x_ind(uint8_t zp) { emit2(0xA1, zp); }  // LDA ($zp,X)
    void sta_x_ind(uint8_t zp) { emit2(0x81, zp); }  // STA ($zp,X)
    void lda_zp_x(uint8_t a)   { emit2(0xB5, a); }
    void sta_zp_x(uint8_t a)   { emit2(0x95, a); }

    // =========================================================================
    //  ARITHMETIC / LOGIC
    // =========================================================================
    void adc_imm(uint8_t v) { emit2(0x69, v); }
    void sbc_imm(uint8_t v) { emit2(0xE9, v); }
    void and_imm(uint8_t v) { emit2(0x29, v); }
    void ora_imm(uint8_t v) { emit2(0x09, v); }
    void eor_imm(uint8_t v) { emit2(0x49, v); }
    void cmp_imm(uint8_t v) { emit2(0xC9, v); }
    void cmp_zp(uint8_t a)  { emit2(0xC5, a); }
    void cmp_abs(uint16_t a){ emit3(0xCD, a & 0xFF, a >> 8); }
    void cpx_imm(uint8_t v) { emit2(0xE0, v); }
    void cpy_imm(uint8_t v) { emit2(0xC0, v); }
    void bit_zp(uint8_t a)  { emit2(0x24, a); }
    void bit_abs(uint16_t a){ emit3(0x2C, a & 0xFF, a >> 8); }

    // -- Zero page RMW --
    void inc_zp(uint8_t a)  { emit2(0xE6, a); }
    void dec_zp(uint8_t a)  { emit2(0xC6, a); }
    void asl_zp(uint8_t a)  { emit2(0x06, a); }
    void lsr_zp(uint8_t a)  { emit2(0x46, a); }
    void rol_zp(uint8_t a)  { emit2(0x26, a); }
    void ror_zp(uint8_t a)  { emit2(0x66, a); }

    // -- Absolute RMW --
    void inc_abs(uint16_t a){ emit3(0xEE, a & 0xFF, a >> 8); }
    void dec_abs(uint16_t a){ emit3(0xCE, a & 0xFF, a >> 8); }

    // -- Accumulator --
    void asl_a() { emit(0x0A); }
    void lsr_a() { emit(0x4A); }
    void rol_a() { emit(0x2A); }
    void ror_a() { emit(0x6A); }

    // =========================================================================
    //  BRANCHES (label-based: pass a uint16_t address from here())
    // =========================================================================
    // Each branch takes the TARGET address; the relative offset is computed
    // automatically from (target - pc_after_branch).

    void bcc(uint16_t target) { emit2(0x90, rel8(target)); }
    void bcs(uint16_t target) { emit2(0xB0, rel8(target)); }
    void beq(uint16_t target) { emit2(0xF0, rel8(target)); }
    void bne(uint16_t target) { emit2(0xD0, rel8(target)); }
    void bmi(uint16_t target) { emit2(0x30, rel8(target)); }
    void bpl(uint16_t target) { emit2(0x10, rel8(target)); }
    void bvc(uint16_t target) { emit2(0x50, rel8(target)); }
    void bvs(uint16_t target) { emit2(0x70, rel8(target)); }

    // =========================================================================
    //  FORWARD BRANCH FIXUP  (for branches to not-yet-emitted code)
    // =========================================================================
    // Usage:
    //   auto fix = a.beq_fwd();   // emit BEQ with placeholder
    //   ... emit code ...
    //   a.fixup(fix);             // patch the BEQ to jump here
    //
    // Returns the buffer offset of the branch operand byte.

    size_t beq_fwd() { size_t f = pos + 1; emit2(0xF0, 0); return f; }
    size_t bne_fwd() { size_t f = pos + 1; emit2(0xD0, 0); return f; }
    size_t bcc_fwd() { size_t f = pos + 1; emit2(0x90, 0); return f; }
    size_t bcs_fwd() { size_t f = pos + 1; emit2(0xB0, 0); return f; }
    size_t bmi_fwd() { size_t f = pos + 1; emit2(0x30, 0); return f; }
    size_t bpl_fwd() { size_t f = pos + 1; emit2(0x10, 0); return f; }

    /// Patch a forward branch fixup to jump to the current position.
    void fixup(size_t operand_offset) {
        // Branch is relative to the byte AFTER the branch instruction (operand_offset + 1)
        int delta = static_cast<int>(pos) - static_cast<int>(operand_offset + 1);
        buf[operand_offset] = static_cast<uint8_t>(static_cast<int8_t>(delta));
    }

    // =========================================================================
    //  JUMPS / CALLS
    // =========================================================================
    void jmp(uint16_t a)    { emit3(0x4C, a & 0xFF, a >> 8); }
    void jmp_ind(uint16_t a){ emit3(0x6C, a & 0xFF, a >> 8); }
    void jsr(uint16_t a)    { emit3(0x20, a & 0xFF, a >> 8); }
    void rts()              { emit(0x60); }
    void rti()              { emit(0x40); }
    void brk()              { emit(0x00); }

    /// JMP to current PC (infinite loop). Commonly used as a halt.
    void jmp_self() { jmp(pc()); }

    // =========================================================================
    //  REGISTER TRANSFERS / STACK / FLAGS
    // =========================================================================
    void tax() { emit(0xAA); }
    void txa() { emit(0x8A); }
    void tay() { emit(0xA8); }
    void tya() { emit(0x98); }
    void tsx() { emit(0xBA); }
    void txs() { emit(0x9A); }
    void pha() { emit(0x48); }
    void pla() { emit(0x68); }
    void php() { emit(0x08); }
    void plp() { emit(0x28); }
    void inx() { emit(0xE8); }
    void dex() { emit(0xCA); }
    void iny() { emit(0xC8); }
    void dey() { emit(0x88); }

    void clc() { emit(0x18); }
    void sec() { emit(0x38); }
    void cli() { emit(0x58); }
    void sei() { emit(0x78); }
    void cld() { emit(0xD8); }
    void sed() { emit(0xF8); }
    void clv() { emit(0xB8); }

    void nop() { emit(0xEA); }

    // =========================================================================
    //  COMPOSITE HELPERS
    // =========================================================================

    /// Emit N NOP instructions (useful for cycle-exact delay padding)
    void nops(int n) { for (int i = 0; i < n; i++) nop(); }

    /// Load 16-bit immediate into a zero-page pointer: LDA #lo; STA zp; LDA #hi; STA zp+1
    void load_ptr(uint8_t zp, uint16_t addr) {
        lda_imm(addr & 0xFF); sta_zp(zp);
        lda_imm(addr >> 8);   sta_zp(zp + 1);
    }

    /// Store 8-bit immediate to absolute: LDA #val; STA addr  (clobbers A)
    void store_imm(uint16_t addr, uint8_t val) {
        lda_imm(val); sta_abs(addr);
    }

    /// Advance a 16-bit pointer at zp/zp+1 by `delta` bytes (clobbers A)
    void add_ptr(uint8_t zp, uint8_t delta) {
        clc();
        lda_zp(zp);
        adc_imm(delta);
        sta_zp(zp);
        lda_zp(zp + 1);
        adc_imm(0);
        sta_zp(zp + 1);
    }

    /// Poll loop: wait for $D012 == raster_line.  Clobbers A.
    ///   wait: LDA $D012; CMP #line; BNE wait
    void wait_raster(uint8_t raster_line) {
        auto wait = here();
        lda_abs(0xD012);
        cmp_imm(raster_line);
        bne(wait);
    }

    /// Poll loop: wait for ($D012 AND mask) == value.  Clobbers A.
    void wait_raster_masked(uint8_t mask, uint8_t value) {
        auto wait = here();
        lda_abs(0xD012);
        and_imm(mask);
        cmp_imm(value);
        bne(wait);
    }

    // =========================================================================
    //  C64 I/O CONSTANTS (convenience)
    // =========================================================================
    static constexpr uint16_t VIC_D011 = 0xD011;
    static constexpr uint16_t VIC_D012 = 0xD012;
    static constexpr uint16_t VIC_D016 = 0xD016;
    static constexpr uint16_t VIC_D018 = 0xD018;
    static constexpr uint16_t VIC_D019 = 0xD019;
    static constexpr uint16_t VIC_D01A = 0xD01A;
    static constexpr uint16_t VIC_D020 = 0xD020;
    static constexpr uint16_t VIC_D021 = 0xD021;
    static constexpr uint16_t CPU_PORT = 0x0001;

private:
    /// Compute signed 8-bit relative offset for branch from current pos+2 to target
    uint8_t rel8(uint16_t target) {
        // The branch offset is relative to the byte AFTER the 2-byte branch instruction.
        // At the point rel8() is called, pos points to the branch opcode byte.
        // After emitting 2 bytes (opcode + operand), PC will be at origin + pos + 2.
        uint16_t pc_after = static_cast<uint16_t>(origin + pos + 2);
        int delta = static_cast<int>(target) - static_cast<int>(pc_after);
        return static_cast<uint8_t>(static_cast<int8_t>(delta));
    }
};
