#pragma once
#ifndef FAM65XX_CORE_VARIANTS_HPP_INCLUDED
#define FAM65XX_CORE_VARIANTS_HPP_INCLUDED

/*
 * fam65xx_core_variants.hpp - Compile-Time CPU Variant System for MOS 65xx Family
 *
 * This implements a single unified CPU variant system with:
 * - Compile-time constexpr opcode table generation
 * - Per-CPU-core addressing mode and operation lookup tables
 * - Processor-specific handler variants
 * - Zero runtime overhead through complete constexpr evaluation
 */

#ifdef __cplusplus

#include "fam65xx_core.hpp"
#include "fam65xx_variants.hpp"
#include "fam65xx_tables.hpp"
#include <array>
#include <cstring>

namespace fam65xx_variants {

// ============================================================================
// COMPILE-TIME CONSTEXPR ADDRESSING MODE AND OPERATION TABLE GENERATION
// ============================================================================

// Forward declare processor-specific addressing mode handlers
extern "C" {
    // Standard addressing mode handlers
    bus_state_t am_zp(fam65xx_t* cpu, bus_state_t pins);
    bus_state_t am_zpx(fam65xx_t* cpu, bus_state_t pins);
    bus_state_t am_zpy(fam65xx_t* cpu, bus_state_t pins);
    bus_state_t am_abs(fam65xx_t* cpu, bus_state_t pins);
    bus_state_t am_abx(fam65xx_t* cpu, bus_state_t pins);
    bus_state_t am_aby(fam65xx_t* cpu, bus_state_t pins);
    bus_state_t am_ind(fam65xx_t* cpu, bus_state_t pins);
    bus_state_t am_idx(fam65xx_t* cpu, bus_state_t pins);
    bus_state_t am_idy(fam65xx_t* cpu, bus_state_t pins);
    
    // Enhanced addressing mode handlers
    bus_state_t am_zpi(fam65xx_t* cpu, bus_state_t pins);    // 65C02 (zp)
    bus_state_t am_abi(fam65xx_t* cpu, bus_state_t pins);    // 65C816 (nnnn,X)
    bus_state_t am_sr(fam65xx_t* cpu, bus_state_t pins);     // 65C816 n,S
    bus_state_t am_sri(fam65xx_t* cpu, bus_state_t pins);    // 65C816 (n,S),Y
    bus_state_t am_zpr(fam65xx_t* cpu, bus_state_t pins);    // Rockwell nn,label
}

// Template function to generate processor-specific addressing mode table
template<typename ProcessorTag>
constexpr std::array<cycle_fn_t, AM_COUNT> generate_am_table() {
    std::array<cycle_fn_t, AM_COUNT> table{};
    
    // Standard addressing modes (available on all processors)
    table[AM_NON] = nullptr;    // No handler needed
    table[AM_IMM] = nullptr;    // Handled in operation
    table[AM_ZER] = am_zp;      // Zero page
    table[AM_ZPX] = am_zpx;     // Zero page,X
    table[AM_ZPY] = am_zpy;     // Zero page,Y
    table[AM_ABS] = am_abs;     // Absolute
    table[AM_ABX] = am_abx;     // Absolute,X
    table[AM_ABY] = am_aby;     // Absolute,Y
    table[AM_IND] = am_ind;     // Indirect
    table[AM_INX] = am_idx;     // Indexed indirect
    table[AM_INY] = am_idy;     // Indirect indexed
    
    // Enhanced addressing modes (processor-specific)
    if constexpr (ProcessorTraits<ProcessorTag>::has_cmos_enhancements) {
        table[AM_ZPI] = am_zpi; // Zero page indirect - 65C02+
    }
    
    if constexpr (ProcessorTraits<ProcessorTag>::has_16bit_mode) {
        table[AM_ABI] = am_abi; // Absolute indexed indirect - 65C816
        table[AM_SR] = am_sr;   // Stack relative - 65C816
        table[AM_SRI] = am_sri; // Stack relative indirect indexed - 65C816
    }
    
    if constexpr (ProcessorTraits<ProcessorTag>::has_bit_manipulation) {
        table[AM_ZPR] = am_zpr; // Zero page relative - Rockwell
    }
    
    return table;
}

// Template function to generate processor-specific operation table
template<typename ProcessorTag>
constexpr std::array<cycle_fn_t, OP_COUNT> generate_op_table() {
    std::array<cycle_fn_t, OP_COUNT> table{};
    
    // Forward declarations for operations - will be linked from other files
    // Load/Store operations
    extern bus_state_t op_lda(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_ldx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_ldy(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sta(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_stx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sty(fam65xx_t* cpu, bus_state_t pins);
    
    // Arithmetic
    extern bus_state_t op_adc(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sbc(fam65xx_t* cpu, bus_state_t pins);
    
    // Logic
    extern bus_state_t op_and(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_ora(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_eor(fam65xx_t* cpu, bus_state_t pins);
    
    // Compare
    extern bus_state_t op_cmp(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_cpx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_cpy(fam65xx_t* cpu, bus_state_t pins);
    
    // Shift/rotate
    extern bus_state_t op_asl(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_lsr(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_rol(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_ror(fam65xx_t* cpu, bus_state_t pins);
    
    // Increment/decrement
    extern bus_state_t op_inc(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_dec(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_inx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_iny(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_dex(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_dey(fam65xx_t* cpu, bus_state_t pins);
    
    // Transfer
    extern bus_state_t op_tax(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_tay(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_txa(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_tya(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_tsx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_txs(fam65xx_t* cpu, bus_state_t pins);
    
    // Stack
    extern bus_state_t op_pha(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_php(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_pla(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_plp(fam65xx_t* cpu, bus_state_t pins);
    
    // Branch
    extern bus_state_t op_bcc(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_bcs(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_beq(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_bne(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_bmi(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_bpl(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_bvc(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_bvs(fam65xx_t* cpu, bus_state_t pins);
    
    // Flag
    extern bus_state_t op_clc(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sec(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_cli(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sei(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_cld(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sed(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_clv(fam65xx_t* cpu, bus_state_t pins);
    
    // Control
    extern bus_state_t op_jmp(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_jsr(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_rts(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_rti(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_brk(fam65xx_t* cpu, bus_state_t pins);
    
    // Other
    extern bus_state_t op_bit(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_nop(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_jam(fam65xx_t* cpu, bus_state_t pins);
    
    // 65C02 enhanced operations
    extern bus_state_t op_bra(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_stz(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_trb(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_tsb(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_phx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_phy(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_plx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_ply(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_wai(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_stp(fam65xx_t* cpu, bus_state_t pins);
    
    // Illegal operations
    extern bus_state_t op_lax(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sax(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_dcp(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_isc(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_slo(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_rla(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sre(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_rra(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_anc(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_asr(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_arr(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sbx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_sha(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_shs(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_shx(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_shy(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_las(fam65xx_t* cpu, bus_state_t pins);
    extern bus_state_t op_xaa(fam65xx_t* cpu, bus_state_t pins);
    
    // Populate standard operations
    table[OP_LDA] = op_lda; table[OP_LDX] = op_ldx; table[OP_LDY] = op_ldy;
    table[OP_STA] = op_sta; table[OP_STX] = op_stx; table[OP_STY] = op_sty;
    table[OP_ADC] = op_adc; table[OP_SBC] = op_sbc;
    table[OP_AND] = op_and; table[OP_ORA] = op_ora; table[OP_EOR] = op_eor;
    table[OP_CMP] = op_cmp; table[OP_CPX] = op_cpx; table[OP_CPY] = op_cpy;
    table[OP_ASL] = op_asl; table[OP_LSR] = op_lsr; table[OP_ROL] = op_rol; table[OP_ROR] = op_ror;
    table[OP_INC] = op_inc; table[OP_DEC] = op_dec;
    table[OP_INX] = op_inx; table[OP_INY] = op_iny; table[OP_DEX] = op_dex; table[OP_DEY] = op_dey;
    table[OP_TAX] = op_tax; table[OP_TAY] = op_tay; table[OP_TXA] = op_txa; table[OP_TYA] = op_tya;
    table[OP_TSX] = op_tsx; table[OP_TXS] = op_txs;
    table[OP_PHA] = op_pha; table[OP_PHP] = op_php; table[OP_PLA] = op_pla; table[OP_PLP] = op_plp;
    table[OP_BCC] = op_bcc; table[OP_BCS] = op_bcs; table[OP_BEQ] = op_beq; table[OP_BNE] = op_bne;
    table[OP_BMI] = op_bmi; table[OP_BPL] = op_bpl; table[OP_BVC] = op_bvc; table[OP_BVS] = op_bvs;
    table[OP_CLC] = op_clc; table[OP_SEC] = op_sec; table[OP_CLI] = op_cli; table[OP_SEI] = op_sei;
    table[OP_CLD] = op_cld; table[OP_SED] = op_sed; table[OP_CLV] = op_clv;
    table[OP_JMP] = op_jmp; table[OP_JSR] = op_jsr; table[OP_RTS] = op_rts; table[OP_RTI] = op_rti;
    table[OP_BRK] = op_brk; table[OP_BIT] = op_bit; table[OP_NOP] = op_nop; table[OP_JAM] = op_jam;
    
    // 65C02 enhanced operations
    if constexpr (ProcessorTraits<ProcessorTag>::has_cmos_enhancements) {
        table[OP_BRA] = op_bra; table[OP_STZ] = op_stz;
        table[OP_TRB] = op_trb; table[OP_TSB] = op_tsb;
        table[OP_PHX] = op_phx; table[OP_PHY] = op_phy;
        table[OP_PLX] = op_plx; table[OP_PLY] = op_ply;
        table[OP_WAI] = op_wai; table[OP_STP] = op_stp;
    }
    
    // Illegal operations (only on NMOS processors)
    if constexpr (ProcessorTraits<ProcessorTag>::has_illegal_opcodes) {
        table[OP_LAX] = op_lax; table[OP_SAX] = op_sax; table[OP_DCP] = op_dcp; table[OP_ISC] = op_isc;
        table[OP_SLO] = op_slo; table[OP_RLA] = op_rla; table[OP_SRE] = op_sre; table[OP_RRA] = op_rra;
        table[OP_ANC] = op_anc; table[OP_ASR] = op_asr; table[OP_ARR] = op_arr; table[OP_SBX] = op_sbx;
        table[OP_SHA] = op_sha; table[OP_SHS] = op_shs; table[OP_SHX] = op_shx; table[OP_SHY] = op_shy;
        table[OP_LAS] = op_las; table[OP_XAA] = op_xaa;
    }
    
    return table;
}

// Compile-time opcode table generator - generates complete 256-entry table at compile time
template<typename ProcessorTag>
constexpr std::array<opcode_info_t, 256> generate_opcode_table() {
    std::array<opcode_info_t, 256> table{};
    
    // MOS 6502 Base Instructions (common to all processors)
    table[0x00] = {AM_NON, 0, 0, 0, 0, OP_BRK};       // BRK
    table[0x01] = {AM_INX, 0, 0, 1, 0, OP_ORA};       // ORA (zp,X)
    table[0x05] = {AM_ZER, 0, 0, 1, 0, OP_ORA};       // ORA zp
    table[0x06] = {AM_ZER, 0, 0, 0, 1, OP_ASL};       // ASL zp
    table[0x08] = {AM_NON, 0, 0, 0, 0, OP_PHP};       // PHP
    table[0x09] = {AM_IMM, 0, 0, 1, 0, OP_ORA};       // ORA #nn
    table[0x0A] = {AM_ACC, 0, 0, 0, 0, OP_ASL};       // ASL A
    table[0x0D] = {AM_ABS, 0, 0, 1, 0, OP_ORA};       // ORA nnnn
    table[0x0E] = {AM_ABS, 0, 0, 0, 1, OP_ASL};       // ASL nnnn
    
    table[0x10] = {AM_REL, 0, 0, 0, 0, OP_BPL};       // BPL
    table[0x11] = {AM_INY, 0, 0, 1, 0, OP_ORA};       // ORA (zp),Y
    table[0x15] = {AM_ZPX, 0, 0, 1, 0, OP_ORA};       // ORA zp,X
    table[0x16] = {AM_ZPX, 0, 0, 0, 1, OP_ASL};       // ASL zp,X
    table[0x18] = {AM_NON, 0, 0, 0, 0, OP_CLC};       // CLC
    table[0x19] = {AM_ABY, 0, 0, 1, 0, OP_ORA};       // ORA nnnn,Y
    table[0x1D] = {AM_ABX, 0, 0, 1, 0, OP_ORA};       // ORA nnnn,X
    table[0x1E] = {AM_ABX, 0, 0, 0, 1, OP_ASL};       // ASL nnnn,X
    
    // Continue with more base instructions...
    table[0x20] = {AM_NON, 0, 0, 0, 0, OP_JSR};       // JSR
    table[0x21] = {AM_INX, 0, 0, 1, 0, OP_AND};       // AND (zp,X)
    table[0x24] = {AM_ZER, 0, 0, 1, 0, OP_BIT};       // BIT zp
    table[0x25] = {AM_ZER, 0, 0, 1, 0, OP_AND};       // AND zp
    table[0x26] = {AM_ZER, 0, 0, 0, 1, OP_ROL};       // ROL zp
    table[0x28] = {AM_NON, 0, 0, 0, 0, OP_PLP};       // PLP
    table[0x29] = {AM_IMM, 0, 0, 1, 0, OP_AND};       // AND #nn
    table[0x2A] = {AM_ACC, 0, 0, 0, 0, OP_ROL};       // ROL A
    table[0x2C] = {AM_ABS, 0, 0, 1, 0, OP_BIT};       // BIT nnnn
    table[0x2D] = {AM_ABS, 0, 0, 1, 0, OP_AND};       // AND nnnn
    table[0x2E] = {AM_ABS, 0, 0, 0, 1, OP_ROL};       // ROL nnnn
    
    // Processor-specific handling for opcode 0x80
    if constexpr (ProcessorTraits<ProcessorTag>::has_cmos_enhancements) {
        table[0x80] = {AM_REL, 0, 0, 0, 0, OP_BRA};   // BRA (65C02+)
    } else if constexpr (ProcessorTraits<ProcessorTag>::has_illegal_opcodes) {
        table[0x80] = {AM_IMM, 0, 0, 1, 0, OP_NOP};   // Illegal NOP (6502)
    } else {
        table[0x80] = {AM_NON, 0, 0, 0, 0, OP_NOP};   // Regular NOP
    }
    
    // 65C02 Enhanced Instructions
    if constexpr (ProcessorTraits<ProcessorTag>::has_cmos_enhancements) {
        table[0x12] = {AM_ZPI, 0, 0, 1, 0, OP_ORA};   // ORA (zp) - 65C02
        table[0x32] = {AM_ZPI, 0, 0, 1, 0, OP_AND};   // AND (zp) - 65C02
        table[0x52] = {AM_ZPI, 0, 0, 1, 0, OP_EOR};   // EOR (zp) - 65C02
        table[0x64] = {AM_ZER, 0, 0, 0, 0, OP_STZ};   // STZ zp - 65C02
        table[0x72] = {AM_ZPI, 0, 0, 1, 0, OP_ADC};   // ADC (zp) - 65C02
        table[0x74] = {AM_ZPX, 0, 0, 0, 0, OP_STZ};   // STZ zp,X - 65C02
        table[0x92] = {AM_ZPI, 0, 0, 0, 0, OP_STA};   // STA (zp) - 65C02
        table[0x9C] = {AM_ABS, 0, 0, 0, 0, OP_STZ};   // STZ nnnn - 65C02
        table[0x9E] = {AM_ABX, 0, 0, 0, 0, OP_STZ};   // STZ nnnn,X - 65C02
        table[0xB2] = {AM_ZPI, 0, 0, 1, 0, OP_LDA};   // LDA (zp) - 65C02
        table[0xD2] = {AM_ZPI, 0, 0, 1, 0, OP_CMP};   // CMP (zp) - 65C02
        table[0xF2] = {AM_ZPI, 0, 0, 1, 0, OP_SBC};   // SBC (zp) - 65C02
        
        // Stack operations
        table[0xDA] = {AM_NON, 0, 0, 0, 0, OP_PHX};   // PHX - 65C02
        table[0xFA] = {AM_NON, 0, 0, 0, 0, OP_PLX};   // PLX - 65C02
        table[0x5A] = {AM_NON, 0, 0, 0, 0, OP_PHY};   // PHY - 65C02
        table[0x7A] = {AM_NON, 0, 0, 0, 0, OP_PLY};   // PLY - 65C02
        
        // Test and modify bits
        table[0x04] = {AM_ZER, 0, 0, 1, 0, OP_TSB};   // TSB zp - 65C02
        table[0x0C] = {AM_ABS, 0, 0, 1, 0, OP_TSB};   // TSB nnnn - 65C02
        table[0x14] = {AM_ZER, 0, 0, 1, 0, OP_TRB};   // TRB zp - 65C02
        table[0x1C] = {AM_ABS, 0, 0, 1, 0, OP_TRB};   // TRB nnnn - 65C02
        
        // Enhanced BIT
        table[0x34] = {AM_ZPX, 0, 0, 1, 0, OP_BIT};   // BIT zp,X - 65C02
        table[0x3C] = {AM_ABX, 0, 0, 1, 0, OP_BIT};   // BIT nnnn,X - 65C02
        table[0x89] = {AM_IMM, 0, 0, 1, 0, OP_BIT};   // BIT #nn - 65C02
        
        // Enhanced jumps
        table[0x7C] = {AM_ABI, 0, 0, 0, 0, OP_JMP};   // JMP (nnnn,X) - 65C02
        
        // Increment/Decrement A
        table[0x1A] = {AM_NON, 0, 0, 0, 0, OP_INC};   // INC A - 65C02
        table[0x3A] = {AM_NON, 0, 0, 0, 0, OP_DEC};   // DEC A - 65C02
        
        // Wait/Stop
        table[0xCB] = {AM_NON, 0, 0, 0, 0, OP_WAI};   // WAI - 65C02
        table[0xDB] = {AM_NON, 0, 0, 0, 0, OP_STP};   // STP - 65C02
    }
    
    // Rockwell 65C02 Bit Manipulation
    if constexpr (ProcessorTraits<ProcessorTag>::has_bit_manipulation) {
        // Reset Memory Bit (RMB0-7)
        table[0x07] = {AM_ZER, 0, 0, 0, 0, OP_RMB0};  // RMB0 zp
        table[0x17] = {AM_ZER, 0, 0, 0, 0, OP_RMB1};  // RMB1 zp
        table[0x27] = {AM_ZER, 0, 0, 0, 0, OP_RMB2};  // RMB2 zp
        table[0x37] = {AM_ZER, 0, 0, 0, 0, OP_RMB3};  // RMB3 zp
        table[0x47] = {AM_ZER, 0, 0, 0, 0, OP_RMB4};  // RMB4 zp
        table[0x57] = {AM_ZER, 0, 0, 0, 0, OP_RMB5};  // RMB5 zp
        table[0x67] = {AM_ZER, 0, 0, 0, 0, OP_RMB6};  // RMB6 zp
        table[0x77] = {AM_ZER, 0, 0, 0, 0, OP_RMB7};  // RMB7 zp
        
        // Set Memory Bit (SMB0-7)
        table[0x87] = {AM_ZER, 0, 0, 0, 0, OP_SMB0};  // SMB0 zp
        table[0x97] = {AM_ZER, 0, 0, 0, 0, OP_SMB1};  // SMB1 zp
        table[0xA7] = {AM_ZER, 0, 0, 0, 0, OP_SMB2};  // SMB2 zp
        table[0xB7] = {AM_ZER, 0, 0, 0, 0, OP_SMB3};  // SMB3 zp
        table[0xC7] = {AM_ZER, 0, 0, 0, 0, OP_SMB4};  // SMB4 zp
        table[0xD7] = {AM_ZER, 0, 0, 0, 0, OP_SMB5};  // SMB5 zp
        table[0xE7] = {AM_ZER, 0, 0, 0, 0, OP_SMB6};  // SMB6 zp
        table[0xF7] = {AM_ZER, 0, 0, 0, 0, OP_SMB7};  // SMB7 zp
        
        // Branch on Bit Reset/Set (BBR/BBS0-7)
        table[0x0F] = {AM_ZPR, 0, 0, 0, 0, OP_BBR0};  // BBR0 zp,label
        table[0x1F] = {AM_ZPR, 0, 0, 0, 0, OP_BBR1};  // BBR1 zp,label
        table[0x2F] = {AM_ZPR, 0, 0, 0, 0, OP_BBR2};  // BBR2 zp,label
        table[0x3F] = {AM_ZPR, 0, 0, 0, 0, OP_BBR3};  // BBR3 zp,label
        table[0x4F] = {AM_ZPR, 0, 0, 0, 0, OP_BBR4};  // BBR4 zp,label
        table[0x5F] = {AM_ZPR, 0, 0, 0, 0, OP_BBR5};  // BBR5 zp,label
        table[0x6F] = {AM_ZPR, 0, 0, 0, 0, OP_BBR6};  // BBR6 zp,label
        table[0x7F] = {AM_ZPR, 0, 0, 0, 0, OP_BBR7};  // BBR7 zp,label
        
        table[0x8F] = {AM_ZPR, 0, 0, 0, 0, OP_BBS0};  // BBS0 zp,label
        table[0x9F] = {AM_ZPR, 0, 0, 0, 0, OP_BBS1};  // BBS1 zp,label
        table[0xAF] = {AM_ZPR, 0, 0, 0, 0, OP_BBS2};  // BBS2 zp,label
        table[0xBF] = {AM_ZPR, 0, 0, 0, 0, OP_BBS3};  // BBS3 zp,label
        table[0xCF] = {AM_ZPR, 0, 0, 0, 0, OP_BBS4};  // BBS4 zp,label
        table[0xDF] = {AM_ZPR, 0, 0, 0, 0, OP_BBS5};  // BBS5 zp,label
        table[0xEF] = {AM_ZPR, 0, 0, 0, 0, OP_BBS6};  // BBS6 zp,label
        table[0xFF] = {AM_ZPR, 0, 0, 0, 0, OP_BBS7};  // BBS7 zp,label
    }
    
    // Illegal opcodes for NMOS processors
    if constexpr (ProcessorTraits<ProcessorTag>::has_illegal_opcodes) {
        table[0x03] = {AM_INX, 0, 0, 0, 1, OP_SLO};   // SLO (zp,X)
        table[0x07] = {AM_ZER, 0, 0, 0, 1, OP_SLO};   // SLO zp
        table[0x0B] = {AM_IMM, 0, 0, 1, 0, OP_ANC};   // ANC #nn
        table[0x0F] = {AM_ABS, 0, 0, 0, 1, OP_SLO};   // SLO nnnn
        
        table[0x13] = {AM_INY, 0, 0, 0, 1, OP_SLO};   // SLO (zp),Y
        table[0x17] = {AM_ZPX, 0, 0, 0, 1, OP_SLO};   // SLO zp,X
        table[0x1B] = {AM_ABY, 0, 0, 0, 1, OP_SLO};   // SLO nnnn,Y
        table[0x1F] = {AM_ABX, 0, 0, 0, 1, OP_SLO};   // SLO nnnn,X
        
        table[0x23] = {AM_INX, 0, 0, 0, 1, OP_RLA};   // RLA (zp,X)
        table[0x27] = {AM_ZER, 0, 0, 0, 1, OP_RLA};   // RLA zp
        table[0x2B] = {AM_IMM, 0, 0, 1, 0, OP_ANC};   // ANC #nn
        table[0x2F] = {AM_ABS, 0, 0, 0, 1, OP_RLA};   // RLA nnnn
        
        // More illegal opcodes...
        table[0xA3] = {AM_INX, 0, 0, 1, 0, OP_LAX};   // LAX (zp,X)
        table[0xA7] = {AM_ZER, 0, 0, 1, 0, OP_LAX};   // LAX zp
        table[0xAB] = {AM_IMM, 0, 0, 1, 0, OP_LAX};   // LAX #nn
        table[0xAF] = {AM_ABS, 0, 0, 1, 0, OP_LAX};   // LAX nnnn
        
        table[0x83] = {AM_INX, 0, 0, 0, 0, OP_SAX};   // SAX (zp,X)
        table[0x87] = {AM_ZER, 0, 0, 0, 0, OP_SAX};   // SAX zp
        table[0x8F] = {AM_ABS, 0, 0, 0, 0, OP_SAX};   // SAX nnnn
        table[0x97] = {AM_ZPY, 0, 0, 0, 0, OP_SAX};   // SAX zp,Y
        
        // JAM instructions
        table[0x02] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0x12] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM  
        table[0x22] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0x32] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0x42] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0x52] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0x62] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0x72] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0x92] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0xB2] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0xD2] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
        table[0xF2] = {AM_NON, 0, 0, 0, 0, OP_JAM};   // JAM
    } else {
        // CMOS processors: illegal opcodes become NOPs
        for (int i = 0; i < 256; i++) {
            if (table[i].op_index == 0 && table[i].am_index == 0) { // Unassigned
                table[i] = {AM_NON, 0, 0, 0, 0, OP_NOP};
            }
        }
    }
    
    return table;
}

// ============================================================================
// PER-CPU-CORE TEMPLATE CLASS WITH COMPILE-TIME TABLES
// ============================================================================

template<typename ProcessorTag>
class CPU_Core {
public:
    // Compile-time generated tables - each CPU core has its own
    static constexpr auto opcode_table = generate_opcode_table<ProcessorTag>();
    static constexpr auto addressing_mode_table = generate_am_table<ProcessorTag>();
    static constexpr auto operation_table = generate_op_table<ProcessorTag>();
    
private:
    fam65xx_t cpu_state;
    
    // Processor-specific behavior flags
    static constexpr bool has_illegal_opcodes = ProcessorTraits<ProcessorTag>::has_illegal_opcodes;
    static constexpr bool has_cmos_enhancements = ProcessorTraits<ProcessorTag>::has_cmos_enhancements;
    static constexpr bool has_bit_manipulation = ProcessorTraits<ProcessorTag>::has_bit_manipulation;
    static constexpr bool has_16bit_mode = ProcessorTraits<ProcessorTag>::has_16bit_mode;
    static constexpr bool has_io_port = ProcessorTraits<ProcessorTag>::has_io_port;
    static constexpr bool has_nmos_bugs = ProcessorTraits<ProcessorTag>::has_nmos_bugs;
    static constexpr bool decimal_affects_nz = ProcessorTraits<ProcessorTag>::decimal_affects_nz;
    
public:
    // Constructor
    CPU_Core() {
        // Initialize CPU state
        std::memset(&cpu_state, 0, sizeof(cpu_state));
        
        // Set processor-specific defaults
        if constexpr (has_io_port) {
            // Initialize 6510 I/O port state
            cpu_state.reg8[REG_ZPL] = 0x00;  // Port direction
            cpu_state.reg8[REG_ZPH] = 0x00;  // Port data
        }
        
        // Set stack pointer high byte
        cpu_state.reg8[REG_SPH] = 0x01;
        
        // Set unused flag (always 1)
        cpu_state.reg8[REG_P] = FLAG_U;
    }
    
    // Main execution interface
    bus_state_t tick(bus_state_t pins) {
        // Get current opcode entry from our compile-time table
        uint8_t opcode = cpu_state.reg8[REG_IR];
        const auto& entry = opcode_table[opcode];
        
        // Execute addressing mode handler (processor-specific)
        if (entry.am_index < addressing_mode_table.size() && 
            addressing_mode_table[entry.am_index] != nullptr) {
            pins = addressing_mode_table[entry.am_index](&cpu_state, pins);
        }
        
        // Execute operation handler (processor-specific)  
        if (entry.op_index < operation_table.size() &&
            operation_table[entry.op_index] != nullptr) {
            pins = operation_table[entry.op_index](&cpu_state, pins);
        }
        
        return pins;
    }
    
    // State accessors
    fam65xx_t* get_state() { return &cpu_state; }
    const fam65xx_t* get_state() const { return &cpu_state; }
    
    // Register accessors with processor-specific behavior
    uint8_t get_a() const { return cpu_state.reg8[REG_A]; }
    uint8_t get_x() const { return cpu_state.reg8[REG_X]; }
    uint8_t get_y() const { return cpu_state.reg8[REG_Y]; }
    uint8_t get_p() const { return cpu_state.reg8[REG_P]; }
    uint16_t get_pc() const { return cpu_state.reg16[REG_PC]; }
    
    void set_a(uint8_t val) { cpu_state.reg8[REG_A] = val; }
    void set_x(uint8_t val) { cpu_state.reg8[REG_X] = val; }
    void set_y(uint8_t val) { cpu_state.reg8[REG_Y] = val; }
    void set_p(uint8_t val) { cpu_state.reg8[REG_P] = val | FLAG_U; } // U always set
    void set_pc(uint16_t val) { cpu_state.reg16[REG_PC] = val; }
    
    // Processor feature queries
    static constexpr bool supports_illegal_opcodes() { return has_illegal_opcodes; }
    static constexpr bool supports_cmos_enhancements() { return has_cmos_enhancements; }
    static constexpr bool supports_bit_manipulation() { return has_bit_manipulation; }
    static constexpr bool supports_16bit_mode() { return has_16bit_mode; }
    static constexpr bool supports_io_port() { return has_io_port; }
    
    // Get compile-time table access
    static constexpr const auto& get_opcode_table() { return opcode_table; }
    static constexpr const auto& get_addressing_mode_table() { return addressing_mode_table; }
    static constexpr const auto& get_operation_table() { return operation_table; }
};

// Type aliases for convenience  
using MOS6502_Core = CPU_Core<MOS6502Tag>;
using MOS6510_Core = CPU_Core<MOS6510Tag>;
using WDC65C02_Core = CPU_Core<WDC65C02Tag>;
using Rockwell65C02_Core = CPU_Core<Rockwell65C02Tag>;
using WDC65C816_Core = CPU_Core<WDC65C816Tag>;

// Import the processor tags into current namespace
using MOS6502Tag = fam65xx_variants::MOS6502Tag;
using MOS6510Tag = fam65xx_variants::MOS6510Tag;
using WDC65C02Tag = fam65xx_variants::WDC65C02Tag;
using Rockwell65C02Tag = fam65xx_variants::Rockwell65C02Tag;
using WDC65C816Tag = fam65xx_variants::WDC65C816Tag;

} // namespace fam65xx_variants

#endif // __cplusplus

#endif // FAM65XX_CORE_VARIANTS_HPP_INCLUDED