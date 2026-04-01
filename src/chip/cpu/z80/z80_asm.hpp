#pragma once
// =============================================================================
// z80_asm — Mini Z80 assembler for runtime code generation
// =============================================================================
// Emits raw Z80 machine code into a caller-provided buffer. Supports:
//   - Named opcode emission (no literal hex bytes needed)
//   - Symbolic labels with deferred branch/jump fixup
//   - Arbitrary origin address (set_origin)
//   - Full Z80 instruction set including CB/ED prefixes and IX/IY indexing
//
// Used throughout the emulator for:
//   - Player stubs (AY-3-8910 music, SID player via Z80)
//   - Test harness code generation
//   - Any system needing runtime Z80 code construction
//
// Usage:
//   uint8_t buf[256];
//   z80_asm a(buf, sizeof(buf), 0x8000);
//   a.di();
//   a.ld_a_n(0x35);
//   a.ld_sp_nn(0xFFFF);
//   auto loop = a.here();
//   a.ld_a_hl_ind();
//   a.cp_n(0);
//   a.jr_nz(loop);
//   a.halt();
//   log_info("Generated %zu bytes at $%04X\n", a.pos, a.origin);
// =============================================================================

#include "core/cermu.hpp"
#include <cstdint>
#include <cstddef>

struct z80_asm {
    uint8_t* buf;
    size_t   pos;
    size_t   cap;
    uint16_t origin;  // Base address of buf[0] in Z80 address space

    // --- Constructors ---
    z80_asm() : buf(nullptr), pos(0), cap(0), origin(0) {}
    z80_asm(uint8_t* buffer, size_t capacity, uint16_t org = 0)
        : buf(buffer), pos(0), cap(capacity), origin(org) {}

    // --- Core emit ---
    void emit(uint8_t b)                       { if (pos < cap) buf[pos++] = b; }
    void emit2(uint8_t a, uint8_t b)           { emit(a); emit(b); }
    void emit3(uint8_t a, uint8_t b, uint8_t c){ emit(a); emit(b); emit(c); }
    void emit4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
                                               { emit(a); emit(b); emit(c); emit(d); }

    // --- Address helpers ---
    uint16_t pc()     const { return static_cast<uint16_t>(origin + pos); }
    uint16_t here()   const { return pc(); }
    size_t   offset() const { return pos; }
    void set_origin(uint16_t org) { origin = org; }

    // --- 16-bit split helpers (little-endian) ---
    static uint8_t lo(uint16_t v) { return static_cast<uint8_t>(v & 0xFF); }
    static uint8_t hi(uint16_t v) { return static_cast<uint8_t>(v >> 8); }

    // =========================================================================
    //  8-BIT LOADS
    // =========================================================================

    // LD r,r' — register to register
    // Encoding: 01 ddd sss  (d=dest bits 5-3, s=src bits 2-0)
    // B=0, C=1, D=2, E=3, H=4, L=5, (HL)=6, A=7
    void ld_b_b() { emit(0x40); }  void ld_b_c() { emit(0x41); }
    void ld_b_d() { emit(0x42); }  void ld_b_e() { emit(0x43); }
    void ld_b_h() { emit(0x44); }  void ld_b_l() { emit(0x45); }
    void ld_b_a() { emit(0x47); }

    void ld_c_b() { emit(0x48); }  void ld_c_c() { emit(0x49); }
    void ld_c_d() { emit(0x4A); }  void ld_c_e() { emit(0x4B); }
    void ld_c_h() { emit(0x4C); }  void ld_c_l() { emit(0x4D); }
    void ld_c_a() { emit(0x4F); }

    void ld_d_b() { emit(0x50); }  void ld_d_c() { emit(0x51); }
    void ld_d_d() { emit(0x52); }  void ld_d_e() { emit(0x53); }
    void ld_d_h() { emit(0x54); }  void ld_d_l() { emit(0x55); }
    void ld_d_a() { emit(0x57); }

    void ld_e_b() { emit(0x58); }  void ld_e_c() { emit(0x59); }
    void ld_e_d() { emit(0x5A); }  void ld_e_e() { emit(0x5B); }
    void ld_e_h() { emit(0x5C); }  void ld_e_l() { emit(0x5D); }
    void ld_e_a() { emit(0x5F); }

    void ld_h_b() { emit(0x60); }  void ld_h_c() { emit(0x61); }
    void ld_h_d() { emit(0x62); }  void ld_h_e() { emit(0x63); }
    void ld_h_h() { emit(0x64); }  void ld_h_l() { emit(0x65); }
    void ld_h_a() { emit(0x67); }

    void ld_l_b() { emit(0x68); }  void ld_l_c() { emit(0x69); }
    void ld_l_d() { emit(0x6A); }  void ld_l_e() { emit(0x6B); }
    void ld_l_h() { emit(0x6C); }  void ld_l_l() { emit(0x6D); }
    void ld_l_a() { emit(0x6F); }

    void ld_a_b() { emit(0x78); }  void ld_a_c() { emit(0x79); }
    void ld_a_d() { emit(0x7A); }  void ld_a_e() { emit(0x7B); }
    void ld_a_h() { emit(0x7C); }  void ld_a_l() { emit(0x7D); }
    void ld_a_a() { emit(0x7F); }

    // LD r,n — 8-bit immediate
    void ld_b_n(uint8_t n) { emit2(0x06, n); }
    void ld_c_n(uint8_t n) { emit2(0x0E, n); }
    void ld_d_n(uint8_t n) { emit2(0x16, n); }
    void ld_e_n(uint8_t n) { emit2(0x1E, n); }
    void ld_h_n(uint8_t n) { emit2(0x26, n); }
    void ld_l_n(uint8_t n) { emit2(0x2E, n); }
    void ld_a_n(uint8_t n) { emit2(0x3E, n); }

    // LD r,(HL) — load from memory at HL
    void ld_b_hl_ind() { emit(0x46); }
    void ld_c_hl_ind() { emit(0x4E); }
    void ld_d_hl_ind() { emit(0x56); }
    void ld_e_hl_ind() { emit(0x5E); }
    void ld_h_hl_ind() { emit(0x66); }
    void ld_l_hl_ind() { emit(0x6E); }
    void ld_a_hl_ind() { emit(0x7E); }

    // LD (HL),r — store to memory at HL
    void ld_hl_ind_b() { emit(0x70); }
    void ld_hl_ind_c() { emit(0x71); }
    void ld_hl_ind_d() { emit(0x72); }
    void ld_hl_ind_e() { emit(0x73); }
    void ld_hl_ind_h() { emit(0x74); }
    void ld_hl_ind_l() { emit(0x75); }
    void ld_hl_ind_a() { emit(0x77); }

    // LD (HL),n — store immediate to memory at HL
    void ld_hl_ind_n(uint8_t n) { emit2(0x36, n); }

    // LD A,(BC/DE/nn) — indirect loads of A
    void ld_a_bc_ind() { emit(0x0A); }
    void ld_a_de_ind() { emit(0x1A); }
    void ld_a_nn_ind(uint16_t nn) { emit3(0x3A, lo(nn), hi(nn)); }

    // LD (BC/DE/nn),A — indirect stores of A
    void ld_bc_ind_a() { emit(0x02); }
    void ld_de_ind_a() { emit(0x12); }
    void ld_nn_ind_a(uint16_t nn) { emit3(0x32, lo(nn), hi(nn)); }

    // LD A,I / LD A,R / LD I,A / LD R,A (ED prefix)
    void ld_a_i() { emit2(0xED, 0x57); }
    void ld_a_r() { emit2(0xED, 0x5F); }
    void ld_i_a() { emit2(0xED, 0x47); }
    void ld_r_a() { emit2(0xED, 0x4F); }

    // =========================================================================
    //  16-BIT LOADS
    // =========================================================================

    void ld_bc_nn(uint16_t nn) { emit3(0x01, lo(nn), hi(nn)); }
    void ld_de_nn(uint16_t nn) { emit3(0x11, lo(nn), hi(nn)); }
    void ld_hl_nn(uint16_t nn) { emit3(0x21, lo(nn), hi(nn)); }
    void ld_sp_nn(uint16_t nn) { emit3(0x31, lo(nn), hi(nn)); }

    // LD HL,(nn) / LD (nn),HL
    void ld_hl_nn_ind(uint16_t nn) { emit3(0x2A, lo(nn), hi(nn)); }
    void ld_nn_ind_hl(uint16_t nn) { emit3(0x22, lo(nn), hi(nn)); }

    // LD rr,(nn) / LD (nn),rr (ED prefix)
    void ld_bc_nn_ind(uint16_t nn) { emit4(0xED, 0x4B, lo(nn), hi(nn)); }
    void ld_de_nn_ind(uint16_t nn) { emit4(0xED, 0x5B, lo(nn), hi(nn)); }
    void ld_sp_nn_ind(uint16_t nn) { emit4(0xED, 0x7B, lo(nn), hi(nn)); }
    void ld_nn_ind_bc(uint16_t nn) { emit4(0xED, 0x43, lo(nn), hi(nn)); }
    void ld_nn_ind_de(uint16_t nn) { emit4(0xED, 0x53, lo(nn), hi(nn)); }
    void ld_nn_ind_sp(uint16_t nn) { emit4(0xED, 0x73, lo(nn), hi(nn)); }

    // LD SP,HL
    void ld_sp_hl() { emit(0xF9); }

    // PUSH / POP
    void push_bc() { emit(0xC5); }
    void push_de() { emit(0xD5); }
    void push_hl() { emit(0xE5); }
    void push_af() { emit(0xF5); }
    void pop_bc()  { emit(0xC1); }
    void pop_de()  { emit(0xD1); }
    void pop_hl()  { emit(0xE1); }
    void pop_af()  { emit(0xF1); }

    // =========================================================================
    //  EXCHANGE
    // =========================================================================

    void ex_af_af()  { emit(0x08); }
    void exx()       { emit(0xD9); }
    void ex_de_hl()  { emit(0xEB); }
    void ex_sp_hl()  { emit(0xE3); }

    // =========================================================================
    //  8-BIT ARITHMETIC AND LOGIC
    // =========================================================================

    // ADD A,r
    void add_a_b() { emit(0x80); }  void add_a_c() { emit(0x81); }
    void add_a_d() { emit(0x82); }  void add_a_e() { emit(0x83); }
    void add_a_h() { emit(0x84); }  void add_a_l() { emit(0x85); }
    void add_a_a() { emit(0x87); }
    void add_a_hl_ind() { emit(0x86); }
    void add_a_n(uint8_t n) { emit2(0xC6, n); }

    // ADC A,r
    void adc_a_b() { emit(0x88); }  void adc_a_c() { emit(0x89); }
    void adc_a_d() { emit(0x8A); }  void adc_a_e() { emit(0x8B); }
    void adc_a_h() { emit(0x8C); }  void adc_a_l() { emit(0x8D); }
    void adc_a_a() { emit(0x8F); }
    void adc_a_hl_ind() { emit(0x8E); }
    void adc_a_n(uint8_t n) { emit2(0xCE, n); }

    // SUB r
    void sub_b() { emit(0x90); }  void sub_c() { emit(0x91); }
    void sub_d() { emit(0x92); }  void sub_e() { emit(0x93); }
    void sub_h() { emit(0x94); }  void sub_l() { emit(0x95); }
    void sub_a() { emit(0x97); }
    void sub_hl_ind() { emit(0x96); }
    void sub_n(uint8_t n) { emit2(0xD6, n); }

    // SBC A,r
    void sbc_a_b() { emit(0x98); }  void sbc_a_c() { emit(0x99); }
    void sbc_a_d() { emit(0x9A); }  void sbc_a_e() { emit(0x9B); }
    void sbc_a_h() { emit(0x9C); }  void sbc_a_l() { emit(0x9D); }
    void sbc_a_a() { emit(0x9F); }
    void sbc_a_hl_ind() { emit(0x9E); }
    void sbc_a_n(uint8_t n) { emit2(0xDE, n); }

    // AND r
    void and_b() { emit(0xA0); }  void and_c() { emit(0xA1); }
    void and_d() { emit(0xA2); }  void and_e() { emit(0xA3); }
    void and_h() { emit(0xA4); }  void and_l() { emit(0xA5); }
    void and_a() { emit(0xA7); }
    void and_hl_ind() { emit(0xA6); }
    void and_n(uint8_t n) { emit2(0xE6, n); }

    // XOR r
    void xor_b() { emit(0xA8); }  void xor_c() { emit(0xA9); }
    void xor_d() { emit(0xAA); }  void xor_e() { emit(0xAB); }
    void xor_h() { emit(0xAC); }  void xor_l() { emit(0xAD); }
    void xor_a() { emit(0xAF); }
    void xor_hl_ind() { emit(0xAE); }
    void xor_n(uint8_t n) { emit2(0xEE, n); }

    // OR r
    void or_b() { emit(0xB0); }  void or_c() { emit(0xB1); }
    void or_d() { emit(0xB2); }  void or_e() { emit(0xB3); }
    void or_h() { emit(0xB4); }  void or_l() { emit(0xB5); }
    void or_a() { emit(0xB7); }
    void or_hl_ind() { emit(0xB6); }
    void or_n(uint8_t n) { emit2(0xF6, n); }

    // CP r
    void cp_b() { emit(0xB8); }  void cp_c() { emit(0xB9); }
    void cp_d() { emit(0xBA); }  void cp_e() { emit(0xBB); }
    void cp_h() { emit(0xBC); }  void cp_l() { emit(0xBD); }
    void cp_a() { emit(0xBF); }
    void cp_hl_ind() { emit(0xBE); }
    void cp_n(uint8_t n) { emit2(0xFE, n); }

    // INC / DEC r
    void inc_b() { emit(0x04); }  void inc_c() { emit(0x0C); }
    void inc_d() { emit(0x14); }  void inc_e() { emit(0x1C); }
    void inc_h() { emit(0x24); }  void inc_l() { emit(0x2C); }
    void inc_a() { emit(0x3C); }
    void inc_hl_ind() { emit(0x34); }

    void dec_b() { emit(0x05); }  void dec_c() { emit(0x0D); }
    void dec_d() { emit(0x15); }  void dec_e() { emit(0x1D); }
    void dec_h() { emit(0x25); }  void dec_l() { emit(0x2D); }
    void dec_a() { emit(0x3D); }
    void dec_hl_ind() { emit(0x35); }

    // =========================================================================
    //  16-BIT ARITHMETIC
    // =========================================================================

    void add_hl_bc() { emit(0x09); }
    void add_hl_de() { emit(0x19); }
    void add_hl_hl() { emit(0x29); }
    void add_hl_sp() { emit(0x39); }

    void adc_hl_bc() { emit2(0xED, 0x4A); }
    void adc_hl_de() { emit2(0xED, 0x5A); }
    void adc_hl_hl() { emit2(0xED, 0x6A); }
    void adc_hl_sp() { emit2(0xED, 0x7A); }

    void sbc_hl_bc() { emit2(0xED, 0x42); }
    void sbc_hl_de() { emit2(0xED, 0x52); }
    void sbc_hl_hl() { emit2(0xED, 0x62); }
    void sbc_hl_sp() { emit2(0xED, 0x72); }

    void inc_bc() { emit(0x03); }
    void inc_de() { emit(0x13); }
    void inc_hl() { emit(0x23); }
    void inc_sp() { emit(0x33); }

    void dec_bc() { emit(0x0B); }
    void dec_de() { emit(0x1B); }
    void dec_hl() { emit(0x2B); }
    void dec_sp() { emit(0x3B); }

    // =========================================================================
    //  ROTATE AND SHIFT (ACCUMULATOR)
    // =========================================================================

    void rlca() { emit(0x07); }
    void rrca() { emit(0x0F); }
    void rla()  { emit(0x17); }
    void rra()  { emit(0x1F); }

    // =========================================================================
    //  CB-PREFIX: ROTATE/SHIFT ON REGISTER
    // =========================================================================
    // CB register encoding: B=0, C=1, D=2, E=3, H=4, L=5, (HL)=6, A=7

    // RLC r
    void rlc_b() { emit2(0xCB, 0x00); }  void rlc_c() { emit2(0xCB, 0x01); }
    void rlc_d() { emit2(0xCB, 0x02); }  void rlc_e() { emit2(0xCB, 0x03); }
    void rlc_h() { emit2(0xCB, 0x04); }  void rlc_l() { emit2(0xCB, 0x05); }
    void rlc_a() { emit2(0xCB, 0x07); }
    void rlc_hl_ind() { emit2(0xCB, 0x06); }

    // RRC r
    void rrc_b() { emit2(0xCB, 0x08); }  void rrc_c() { emit2(0xCB, 0x09); }
    void rrc_d() { emit2(0xCB, 0x0A); }  void rrc_e() { emit2(0xCB, 0x0B); }
    void rrc_h() { emit2(0xCB, 0x0C); }  void rrc_l() { emit2(0xCB, 0x0D); }
    void rrc_a() { emit2(0xCB, 0x0F); }
    void rrc_hl_ind() { emit2(0xCB, 0x0E); }

    // RL r
    void rl_b() { emit2(0xCB, 0x10); }  void rl_c() { emit2(0xCB, 0x11); }
    void rl_d() { emit2(0xCB, 0x12); }  void rl_e() { emit2(0xCB, 0x13); }
    void rl_h() { emit2(0xCB, 0x14); }  void rl_l() { emit2(0xCB, 0x15); }
    void rl_a() { emit2(0xCB, 0x17); }
    void rl_hl_ind() { emit2(0xCB, 0x16); }

    // RR r
    void rr_b() { emit2(0xCB, 0x18); }  void rr_c() { emit2(0xCB, 0x19); }
    void rr_d() { emit2(0xCB, 0x1A); }  void rr_e() { emit2(0xCB, 0x1B); }
    void rr_h() { emit2(0xCB, 0x1C); }  void rr_l() { emit2(0xCB, 0x1D); }
    void rr_a() { emit2(0xCB, 0x1F); }
    void rr_hl_ind() { emit2(0xCB, 0x1E); }

    // SLA r
    void sla_b() { emit2(0xCB, 0x20); }  void sla_c() { emit2(0xCB, 0x21); }
    void sla_d() { emit2(0xCB, 0x22); }  void sla_e() { emit2(0xCB, 0x23); }
    void sla_h() { emit2(0xCB, 0x24); }  void sla_l() { emit2(0xCB, 0x25); }
    void sla_a() { emit2(0xCB, 0x27); }
    void sla_hl_ind() { emit2(0xCB, 0x26); }

    // SRA r
    void sra_b() { emit2(0xCB, 0x28); }  void sra_c() { emit2(0xCB, 0x29); }
    void sra_d() { emit2(0xCB, 0x2A); }  void sra_e() { emit2(0xCB, 0x2B); }
    void sra_h() { emit2(0xCB, 0x2C); }  void sra_l() { emit2(0xCB, 0x2D); }
    void sra_a() { emit2(0xCB, 0x2F); }
    void sra_hl_ind() { emit2(0xCB, 0x2E); }

    // SRL r
    void srl_b() { emit2(0xCB, 0x38); }  void srl_c() { emit2(0xCB, 0x39); }
    void srl_d() { emit2(0xCB, 0x3A); }  void srl_e() { emit2(0xCB, 0x3B); }
    void srl_h() { emit2(0xCB, 0x3C); }  void srl_l() { emit2(0xCB, 0x3D); }
    void srl_a() { emit2(0xCB, 0x3F); }
    void srl_hl_ind() { emit2(0xCB, 0x3E); }

    // SLL r (undocumented — shifts 1 into bit 0)
    void sll_b() { emit2(0xCB, 0x30); }  void sll_c() { emit2(0xCB, 0x31); }
    void sll_d() { emit2(0xCB, 0x32); }  void sll_e() { emit2(0xCB, 0x33); }
    void sll_h() { emit2(0xCB, 0x34); }  void sll_l() { emit2(0xCB, 0x35); }
    void sll_a() { emit2(0xCB, 0x37); }
    void sll_hl_ind() { emit2(0xCB, 0x36); }

    // =========================================================================
    //  CB-PREFIX: BIT / RES / SET
    // =========================================================================
    // Generic: bit b,r → CB (01 bbb rrr)
    //          res b,r → CB (10 bbb rrr)
    //          set b,r → CB (11 bbb rrr)

    void bit(uint8_t b, uint8_t r) { emit2(0xCB, 0x40 | (b << 3) | r); }
    void res(uint8_t b, uint8_t r) { emit2(0xCB, 0x80 | (b << 3) | r); }
    void set(uint8_t b, uint8_t r) { emit2(0xCB, 0xC0 | (b << 3) | r); }

    // Named register constants for BIT/RES/SET
    static constexpr uint8_t R_B = 0, R_C = 1, R_D = 2, R_E = 3;
    static constexpr uint8_t R_H = 4, R_L = 5, R_HL_IND = 6, R_A = 7;

    // Convenience: BIT b,(HL) / RES b,(HL) / SET b,(HL)
    void bit_hl(uint8_t b) { bit(b, R_HL_IND); }
    void res_hl(uint8_t b) { res(b, R_HL_IND); }
    void set_hl(uint8_t b) { set(b, R_HL_IND); }

    // =========================================================================
    //  SPECIAL ACCUMULATOR OPERATIONS
    // =========================================================================

    void daa() { emit(0x27); }
    void cpl() { emit(0x2F); }
    void neg() { emit2(0xED, 0x44); }
    void ccf() { emit(0x3F); }
    void scf() { emit(0x37); }

    // RLD / RRD
    void rld() { emit2(0xED, 0x6F); }
    void rrd() { emit2(0xED, 0x67); }

    // =========================================================================
    //  JUMPS
    // =========================================================================

    void jp(uint16_t nn)    { emit3(0xC3, lo(nn), hi(nn)); }
    void jp_hl()            { emit(0xE9); }  // JP (HL) — actually JP HL

    // JP cc,nn — condition codes: NZ=0, Z=1, NC=2, C=3, PO=4, PE=5, P=6, M=7
    void jp_nz(uint16_t nn) { emit3(0xC2, lo(nn), hi(nn)); }
    void jp_z(uint16_t nn)  { emit3(0xCA, lo(nn), hi(nn)); }
    void jp_nc(uint16_t nn) { emit3(0xD2, lo(nn), hi(nn)); }
    void jp_c(uint16_t nn)  { emit3(0xDA, lo(nn), hi(nn)); }
    void jp_po(uint16_t nn) { emit3(0xE2, lo(nn), hi(nn)); }
    void jp_pe(uint16_t nn) { emit3(0xEA, lo(nn), hi(nn)); }
    void jp_p(uint16_t nn)  { emit3(0xF2, lo(nn), hi(nn)); }
    void jp_m(uint16_t nn)  { emit3(0xFA, lo(nn), hi(nn)); }

    // =========================================================================
    //  RELATIVE JUMPS (label-based)
    // =========================================================================
    // Each takes a TARGET address; the signed offset is computed automatically.

    void jr(uint16_t target)    { emit2(0x18, rel8(target)); }
    void jr_nz(uint16_t target) { emit2(0x20, rel8(target)); }
    void jr_z(uint16_t target)  { emit2(0x28, rel8(target)); }
    void jr_nc(uint16_t target) { emit2(0x30, rel8(target)); }
    void jr_c(uint16_t target)  { emit2(0x38, rel8(target)); }
    void djnz(uint16_t target)  { emit2(0x10, rel8(target)); }

    // =========================================================================
    //  FORWARD BRANCH FIXUP
    // =========================================================================
    // Usage:
    //   auto fix = a.jr_nz_fwd();   // emit JR NZ with placeholder
    //   ... emit code ...
    //   a.fixup(fix);               // patch the JR to jump here

    size_t jr_fwd()    { size_t f = pos + 1; emit2(0x18, 0); return f; }
    size_t jr_nz_fwd() { size_t f = pos + 1; emit2(0x20, 0); return f; }
    size_t jr_z_fwd()  { size_t f = pos + 1; emit2(0x28, 0); return f; }
    size_t jr_nc_fwd() { size_t f = pos + 1; emit2(0x30, 0); return f; }
    size_t jr_c_fwd()  { size_t f = pos + 1; emit2(0x38, 0); return f; }
    size_t djnz_fwd()  { size_t f = pos + 1; emit2(0x10, 0); return f; }

    /// Patch a forward branch fixup to jump to the current position.
    void fixup(size_t operand_offset) {
        int delta = static_cast<int>(pos) - static_cast<int>(operand_offset + 1);
        buf[operand_offset] = static_cast<uint8_t>(static_cast<int8_t>(delta));
    }

    // =========================================================================
    //  CALLS AND RETURNS
    // =========================================================================

    void call(uint16_t nn) { emit3(0xCD, lo(nn), hi(nn)); }

    void call_nz(uint16_t nn) { emit3(0xC4, lo(nn), hi(nn)); }
    void call_z(uint16_t nn)  { emit3(0xCC, lo(nn), hi(nn)); }
    void call_nc(uint16_t nn) { emit3(0xD4, lo(nn), hi(nn)); }
    void call_c(uint16_t nn)  { emit3(0xDC, lo(nn), hi(nn)); }
    void call_po(uint16_t nn) { emit3(0xE4, lo(nn), hi(nn)); }
    void call_pe(uint16_t nn) { emit3(0xEC, lo(nn), hi(nn)); }
    void call_p(uint16_t nn)  { emit3(0xF4, lo(nn), hi(nn)); }
    void call_m(uint16_t nn)  { emit3(0xFC, lo(nn), hi(nn)); }

    void ret()    { emit(0xC9); }
    void ret_nz() { emit(0xC0); }
    void ret_z()  { emit(0xC8); }
    void ret_nc() { emit(0xD0); }
    void ret_c()  { emit(0xD8); }
    void ret_po() { emit(0xE0); }
    void ret_pe() { emit(0xE8); }
    void ret_p()  { emit(0xF0); }
    void ret_m()  { emit(0xF8); }

    void reti() { emit2(0xED, 0x4D); }
    void retn() { emit2(0xED, 0x45); }

    // RST (restart vectors)
    void rst(uint8_t vec) { emit(0xC7 | (vec & 0x38)); }
    void rst_00() { emit(0xC7); }
    void rst_08() { emit(0xCF); }
    void rst_10() { emit(0xD7); }
    void rst_18() { emit(0xDF); }
    void rst_20() { emit(0xE7); }
    void rst_28() { emit(0xEF); }
    void rst_30() { emit(0xF7); }
    void rst_38() { emit(0xFF); }

    // =========================================================================
    //  I/O
    // =========================================================================

    void in_a_n(uint8_t port)  { emit2(0xDB, port); }
    void out_n_a(uint8_t port) { emit2(0xD3, port); }

    // ED-prefix I/O: IN r,(C) / OUT (C),r
    void in_b_c()  { emit2(0xED, 0x40); }
    void in_c_c()  { emit2(0xED, 0x48); }
    void in_d_c()  { emit2(0xED, 0x50); }
    void in_e_c()  { emit2(0xED, 0x58); }
    void in_h_c()  { emit2(0xED, 0x60); }
    void in_l_c()  { emit2(0xED, 0x68); }
    void in_f_c()  { emit2(0xED, 0x70); }  // Undocumented: affects flags only
    void in_a_c()  { emit2(0xED, 0x78); }

    void out_c_b() { emit2(0xED, 0x41); }
    void out_c_c() { emit2(0xED, 0x49); }
    void out_c_d() { emit2(0xED, 0x51); }
    void out_c_e() { emit2(0xED, 0x59); }
    void out_c_h() { emit2(0xED, 0x61); }
    void out_c_l() { emit2(0xED, 0x69); }
    void out_c_0() { emit2(0xED, 0x71); }  // Undocumented: outputs 0
    void out_c_a() { emit2(0xED, 0x79); }

    // =========================================================================
    //  BLOCK TRANSFER / SEARCH
    // =========================================================================

    void ldi()  { emit2(0xED, 0xA0); }
    void ldir() { emit2(0xED, 0xB0); }
    void ldd()  { emit2(0xED, 0xA8); }
    void lddr() { emit2(0xED, 0xB8); }

    void cpi()  { emit2(0xED, 0xA1); }
    void cpir() { emit2(0xED, 0xB1); }
    void cpd()  { emit2(0xED, 0xA9); }
    void cpdr() { emit2(0xED, 0xB9); }

    // =========================================================================
    //  BLOCK I/O
    // =========================================================================

    void ini()  { emit2(0xED, 0xA2); }
    void inir() { emit2(0xED, 0xB2); }
    void ind()  { emit2(0xED, 0xAA); }
    void indr() { emit2(0xED, 0xBA); }

    void outi() { emit2(0xED, 0xA3); }
    void otir() { emit2(0xED, 0xB3); }
    void outd() { emit2(0xED, 0xAB); }
    void otdr() { emit2(0xED, 0xBB); }

    // =========================================================================
    //  CPU CONTROL
    // =========================================================================

    void nop()  { emit(0x00); }
    void halt() { emit(0x76); }
    void di()   { emit(0xF3); }
    void ei()   { emit(0xFB); }

    void im_0() { emit2(0xED, 0x46); }
    void im_1() { emit2(0xED, 0x56); }
    void im_2() { emit2(0xED, 0x5E); }

    // =========================================================================
    //  IX-INDEXED INSTRUCTIONS (DD prefix)
    // =========================================================================

    void ld_ix_nn(uint16_t nn)     { emit4(0xDD, 0x21, lo(nn), hi(nn)); }
    void ld_ix_nn_ind(uint16_t nn) { emit4(0xDD, 0x2A, lo(nn), hi(nn)); }
    void ld_nn_ind_ix(uint16_t nn) { emit4(0xDD, 0x22, lo(nn), hi(nn)); }
    void ld_sp_ix()                { emit2(0xDD, 0xF9); }

    void push_ix() { emit2(0xDD, 0xE5); }
    void pop_ix()  { emit2(0xDD, 0xE1); }

    void add_ix_bc() { emit2(0xDD, 0x09); }
    void add_ix_de() { emit2(0xDD, 0x19); }
    void add_ix_ix() { emit2(0xDD, 0x29); }
    void add_ix_sp() { emit2(0xDD, 0x39); }

    void inc_ix() { emit2(0xDD, 0x23); }
    void dec_ix() { emit2(0xDD, 0x2B); }

    void ex_sp_ix() { emit2(0xDD, 0xE3); }
    void jp_ix()    { emit2(0xDD, 0xE9); }

    // LD r,(IX+d) — load from indexed memory
    void ld_b_ix_d(int8_t d) { emit3(0xDD, 0x46, static_cast<uint8_t>(d)); }
    void ld_c_ix_d(int8_t d) { emit3(0xDD, 0x4E, static_cast<uint8_t>(d)); }
    void ld_d_ix_d(int8_t d) { emit3(0xDD, 0x56, static_cast<uint8_t>(d)); }
    void ld_e_ix_d(int8_t d) { emit3(0xDD, 0x5E, static_cast<uint8_t>(d)); }
    void ld_h_ix_d(int8_t d) { emit3(0xDD, 0x66, static_cast<uint8_t>(d)); }
    void ld_l_ix_d(int8_t d) { emit3(0xDD, 0x6E, static_cast<uint8_t>(d)); }
    void ld_a_ix_d(int8_t d) { emit3(0xDD, 0x7E, static_cast<uint8_t>(d)); }

    // LD (IX+d),r — store to indexed memory
    void ld_ix_d_b(int8_t d) { emit3(0xDD, 0x70, static_cast<uint8_t>(d)); }
    void ld_ix_d_c(int8_t d) { emit3(0xDD, 0x71, static_cast<uint8_t>(d)); }
    void ld_ix_d_d(int8_t d) { emit3(0xDD, 0x72, static_cast<uint8_t>(d)); }
    void ld_ix_d_e(int8_t d) { emit3(0xDD, 0x73, static_cast<uint8_t>(d)); }
    void ld_ix_d_h(int8_t d) { emit3(0xDD, 0x74, static_cast<uint8_t>(d)); }
    void ld_ix_d_l(int8_t d) { emit3(0xDD, 0x75, static_cast<uint8_t>(d)); }
    void ld_ix_d_a(int8_t d) { emit3(0xDD, 0x77, static_cast<uint8_t>(d)); }

    // LD (IX+d),n — store immediate to indexed memory
    void ld_ix_d_n(int8_t d, uint8_t n) { emit4(0xDD, 0x36, static_cast<uint8_t>(d), n); }

    // Arithmetic with (IX+d)
    void add_a_ix_d(int8_t d) { emit3(0xDD, 0x86, static_cast<uint8_t>(d)); }
    void adc_a_ix_d(int8_t d) { emit3(0xDD, 0x8E, static_cast<uint8_t>(d)); }
    void sub_ix_d(int8_t d)   { emit3(0xDD, 0x96, static_cast<uint8_t>(d)); }
    void sbc_a_ix_d(int8_t d) { emit3(0xDD, 0x9E, static_cast<uint8_t>(d)); }
    void and_ix_d(int8_t d)   { emit3(0xDD, 0xA6, static_cast<uint8_t>(d)); }
    void xor_ix_d(int8_t d)   { emit3(0xDD, 0xAE, static_cast<uint8_t>(d)); }
    void or_ix_d(int8_t d)    { emit3(0xDD, 0xB6, static_cast<uint8_t>(d)); }
    void cp_ix_d(int8_t d)    { emit3(0xDD, 0xBE, static_cast<uint8_t>(d)); }
    void inc_ix_d(int8_t d)   { emit3(0xDD, 0x34, static_cast<uint8_t>(d)); }
    void dec_ix_d(int8_t d)   { emit3(0xDD, 0x35, static_cast<uint8_t>(d)); }

    // DD CB prefix: BIT/RES/SET on (IX+d)
    void bit_ix_d(uint8_t b, int8_t d) { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x46 | (b << 3)); }
    void res_ix_d(uint8_t b, int8_t d) { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x86 | (b << 3)); }
    void set_ix_d(uint8_t b, int8_t d) { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0xC6 | (b << 3)); }

    // DD CB prefix: rotate/shift on (IX+d)
    void rlc_ix_d(int8_t d) { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x06); }
    void rrc_ix_d(int8_t d) { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x0E); }
    void rl_ix_d(int8_t d)  { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x16); }
    void rr_ix_d(int8_t d)  { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x1E); }
    void sla_ix_d(int8_t d) { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x26); }
    void sra_ix_d(int8_t d) { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x2E); }
    void srl_ix_d(int8_t d) { emit4(0xDD, 0xCB, static_cast<uint8_t>(d), 0x3E); }

    // =========================================================================
    //  IY-INDEXED INSTRUCTIONS (FD prefix)
    // =========================================================================

    void ld_iy_nn(uint16_t nn)     { emit4(0xFD, 0x21, lo(nn), hi(nn)); }
    void ld_iy_nn_ind(uint16_t nn) { emit4(0xFD, 0x2A, lo(nn), hi(nn)); }
    void ld_nn_ind_iy(uint16_t nn) { emit4(0xFD, 0x22, lo(nn), hi(nn)); }
    void ld_sp_iy()                { emit2(0xFD, 0xF9); }

    void push_iy() { emit2(0xFD, 0xE5); }
    void pop_iy()  { emit2(0xFD, 0xE1); }

    void add_iy_bc() { emit2(0xFD, 0x09); }
    void add_iy_de() { emit2(0xFD, 0x19); }
    void add_iy_iy() { emit2(0xFD, 0x29); }
    void add_iy_sp() { emit2(0xFD, 0x39); }

    void inc_iy() { emit2(0xFD, 0x23); }
    void dec_iy() { emit2(0xFD, 0x2B); }

    void ex_sp_iy() { emit2(0xFD, 0xE3); }
    void jp_iy()    { emit2(0xFD, 0xE9); }

    // LD r,(IY+d) — load from indexed memory
    void ld_b_iy_d(int8_t d) { emit3(0xFD, 0x46, static_cast<uint8_t>(d)); }
    void ld_c_iy_d(int8_t d) { emit3(0xFD, 0x4E, static_cast<uint8_t>(d)); }
    void ld_d_iy_d(int8_t d) { emit3(0xFD, 0x56, static_cast<uint8_t>(d)); }
    void ld_e_iy_d(int8_t d) { emit3(0xFD, 0x5E, static_cast<uint8_t>(d)); }
    void ld_h_iy_d(int8_t d) { emit3(0xFD, 0x66, static_cast<uint8_t>(d)); }
    void ld_l_iy_d(int8_t d) { emit3(0xFD, 0x6E, static_cast<uint8_t>(d)); }
    void ld_a_iy_d(int8_t d) { emit3(0xFD, 0x7E, static_cast<uint8_t>(d)); }

    // LD (IY+d),r — store to indexed memory
    void ld_iy_d_b(int8_t d) { emit3(0xFD, 0x70, static_cast<uint8_t>(d)); }
    void ld_iy_d_c(int8_t d) { emit3(0xFD, 0x71, static_cast<uint8_t>(d)); }
    void ld_iy_d_d(int8_t d) { emit3(0xFD, 0x72, static_cast<uint8_t>(d)); }
    void ld_iy_d_e(int8_t d) { emit3(0xFD, 0x73, static_cast<uint8_t>(d)); }
    void ld_iy_d_h(int8_t d) { emit3(0xFD, 0x74, static_cast<uint8_t>(d)); }
    void ld_iy_d_l(int8_t d) { emit3(0xFD, 0x75, static_cast<uint8_t>(d)); }
    void ld_iy_d_a(int8_t d) { emit3(0xFD, 0x77, static_cast<uint8_t>(d)); }

    // LD (IY+d),n — store immediate to indexed memory
    void ld_iy_d_n(int8_t d, uint8_t n) { emit4(0xFD, 0x36, static_cast<uint8_t>(d), n); }

    // Arithmetic with (IY+d)
    void add_a_iy_d(int8_t d) { emit3(0xFD, 0x86, static_cast<uint8_t>(d)); }
    void adc_a_iy_d(int8_t d) { emit3(0xFD, 0x8E, static_cast<uint8_t>(d)); }
    void sub_iy_d(int8_t d)   { emit3(0xFD, 0x96, static_cast<uint8_t>(d)); }
    void sbc_a_iy_d(int8_t d) { emit3(0xFD, 0x9E, static_cast<uint8_t>(d)); }
    void and_iy_d(int8_t d)   { emit3(0xFD, 0xA6, static_cast<uint8_t>(d)); }
    void xor_iy_d(int8_t d)   { emit3(0xFD, 0xAE, static_cast<uint8_t>(d)); }
    void or_iy_d(int8_t d)    { emit3(0xFD, 0xB6, static_cast<uint8_t>(d)); }
    void cp_iy_d(int8_t d)    { emit3(0xFD, 0xBE, static_cast<uint8_t>(d)); }
    void inc_iy_d(int8_t d)   { emit3(0xFD, 0x34, static_cast<uint8_t>(d)); }
    void dec_iy_d(int8_t d)   { emit3(0xFD, 0x35, static_cast<uint8_t>(d)); }

    // FD CB prefix: BIT/RES/SET on (IY+d)
    void bit_iy_d(uint8_t b, int8_t d) { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x46 | (b << 3)); }
    void res_iy_d(uint8_t b, int8_t d) { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x86 | (b << 3)); }
    void set_iy_d(uint8_t b, int8_t d) { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0xC6 | (b << 3)); }

    // FD CB prefix: rotate/shift on (IY+d)
    void rlc_iy_d(int8_t d) { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x06); }
    void rrc_iy_d(int8_t d) { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x0E); }
    void rl_iy_d(int8_t d)  { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x16); }
    void rr_iy_d(int8_t d)  { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x1E); }
    void sla_iy_d(int8_t d) { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x26); }
    void sra_iy_d(int8_t d) { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x2E); }
    void srl_iy_d(int8_t d) { emit4(0xFD, 0xCB, static_cast<uint8_t>(d), 0x3E); }

    // =========================================================================
    //  COMPOSITE HELPERS
    // =========================================================================

    /// Emit N NOP instructions (useful for cycle-exact delay padding)
    void nops(int n) { for (int i = 0; i < n; i++) nop(); }

    /// JP to current PC (infinite loop). Commonly used as a halt-like stub.
    void jp_self() { jp(pc()); }

    /// JR to self (tighter 2-byte infinite loop)
    void jr_self() { jr(pc()); }

    /// Load 16-bit immediate into HL: LD HL,nn
    void load_hl(uint16_t addr) { ld_hl_nn(addr); }

    /// Store A to absolute address: LD (nn),A
    void store_a(uint16_t addr) { ld_nn_ind_a(addr); }

    /// Load A from absolute address: LD A,(nn)
    void load_a(uint16_t addr) { ld_a_nn_ind(addr); }

    /// Block fill: LD (HL),val; LD DE,HL+1; LD BC,count-1; LDIR
    /// Fills count bytes starting at HL with val.
    /// Clobbers A, BC, DE, HL.
    void block_fill(uint16_t addr, uint8_t val, uint16_t count) {
        ld_hl_nn(addr);
        ld_a_n(val);
        ld_hl_ind_a();
        if (count > 1) {
            ld_de_nn(addr + 1);
            ld_bc_nn(count - 1);
            ldir();
        }
    }

    /// Block copy: LD HL,src; LD DE,dst; LD BC,count; LDIR
    /// Clobbers BC, DE, HL.
    void block_copy(uint16_t src, uint16_t dst, uint16_t count) {
        ld_hl_nn(src);
        ld_de_nn(dst);
        ld_bc_nn(count);
        ldir();
    }

    /// Emit raw data byte (for inline data tables)
    void db(uint8_t b) { emit(b); }

    /// Emit raw data word (little-endian)
    void dw(uint16_t w) { emit(lo(w)); emit(hi(w)); }

private:
    /// Compute signed 8-bit relative offset for JR/DJNZ from current pos+2 to target
    uint8_t rel8(uint16_t target) {
        // Offset is relative to PC AFTER the 2-byte instruction
        uint16_t pc_after = static_cast<uint16_t>(origin + pos + 2);
        int delta = static_cast<int>(target) - static_cast<int>(pc_after);
        return static_cast<uint8_t>(static_cast<int8_t>(delta));
    }
};
