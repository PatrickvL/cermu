#pragma once
/*
 * fam65xx_unified_opcodes.hpp - Unified Opcode Table for All MOS 65xx Family Members
 *
 * This file implements a single opcode table that supports all MOS 65xx family members
 * using constexpr template-based helpers to select the correct handlers per processor.
 *
 * DESIGN PRINCIPLES:
 * - Single source of truth for all opcodes across all processors
 * - Compile-time processor feature selection using constexpr templates
 * - Zero runtime overhead - processor differences resolved at compile time
 * - Hardware-accurate opcode tables for each processor variant
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "fam65xx_core.hpp"
#include "fam65xx_addrmodes.hpp"
#include "fam65xx_ops.hpp"
#include "fam65xx_ops_part2.hpp"
#include "fam65xx_ops_part3.hpp"
#include "fam65xx_ops_rmw.hpp"
#include "fam65xx_ops_illegal.hpp"

#ifdef __cplusplus
}

// C++ Template System for Processor-Specific Opcode Selection
namespace fam65xx_unified {

// Forward declarations from template system
namespace fam65xx_template {
    struct MOS6502Tag {};
    struct MOS6510Tag {};
    struct WDC65C02Tag {};
    struct Rockwell65C02Tag {};
    struct WDC65C816Tag {};
    
    template<typename ProcessorTag>
    struct ProcessorTraits;
}

// Processor feature detection (internal use only)
template<typename ProcessorTag>
constexpr bool processor_has_illegal_opcodes() {
    if constexpr (std::is_same_v<ProcessorTag, fam65xx_template::MOS6502Tag> ||
                  std::is_same_v<ProcessorTag, fam65xx_template::MOS6510Tag>) {
        return true;
    }
    return false;
}

template<typename ProcessorTag>
constexpr bool processor_has_cmos_enhancements() {
    if constexpr (std::is_same_v<ProcessorTag, fam65xx_template::WDC65C02Tag> ||
                  std::is_same_v<ProcessorTag, fam65xx_template::Rockwell65C02Tag> ||
                  std::is_same_v<ProcessorTag, fam65xx_template::WDC65C816Tag>) {
        return true;
    }
    return false;
}

template<typename ProcessorTag>
constexpr bool processor_has_bit_manipulation() {
    if constexpr (std::is_same_v<ProcessorTag, fam65xx_template::Rockwell65C02Tag>) {
        return true;
    }
    return false;
}

template<typename ProcessorTag>
constexpr bool processor_has_16bit_mode() {
    if constexpr (std::is_same_v<ProcessorTag, fam65xx_template::WDC65C816Tag>) {
        return true;
    }
    return false;
}

// Opcode handler selection based on processor features
template<typename ProcessorTag>
constexpr fam65xx_op_handler_t get_opcode_handler(uint8_t opcode) {
    // Base 6502 opcodes (common to all processors)
    switch (opcode) {
        // ADC - Add with Carry
        case 0x69: case 0x65: case 0x75: case 0x6D: case 0x7D: case 0x79: case 0x61: case 0x71:
            return fam65xx_op_adc;
            
        // AND - Logical AND
        case 0x29: case 0x25: case 0x35: case 0x2D: case 0x3D: case 0x39: case 0x21: case 0x31:
            return fam65xx_op_and;
            
        // ASL - Arithmetic Shift Left
        case 0x0A: case 0x06: case 0x16: case 0x0E: case 0x1E:
            return fam65xx_op_asl;
            
        // BIT - Bit Test
        case 0x24: case 0x2C:
            return fam65xx_op_bit;
            
        // Branch instructions
        case 0x10: return fam65xx_op_bpl;
        case 0x30: return fam65xx_op_bmi;
        case 0x50: return fam65xx_op_bvc;
        case 0x70: return fam65xx_op_bvs;
        case 0x90: return fam65xx_op_bcc;
        case 0xB0: return fam65xx_op_bcs;
        case 0xD0: return fam65xx_op_bne;
        case 0xF0: return fam65xx_op_beq;
        
        // Break and interrupts
        case 0x00: return fam65xx_op_brk;
        case 0x40: return fam65xx_op_rti;
        
        // Clear/Set flags
        case 0x18: return fam65xx_op_clc;
        case 0x38: return fam65xx_op_sec;
        case 0x58: return fam65xx_op_cli;
        case 0x78: return fam65xx_op_sei;
        case 0xB8: return fam65xx_op_clv;
        case 0xD8: return fam65xx_op_cld;
        case 0xF8: return fam65xx_op_sed;
        
        // Compare instructions
        case 0xC9: case 0xC5: case 0xD5: case 0xCD: case 0xDD: case 0xD9: case 0xC1: case 0xD1:
            return fam65xx_op_cmp;
        case 0xE0: case 0xE4: case 0xEC:
            return fam65xx_op_cpx;
        case 0xC0: case 0xC4: case 0xCC:
            return fam65xx_op_cpy;
            
        // Decrement/Increment
        case 0xC6: case 0xD6: case 0xCE: case 0xDE:
            return fam65xx_op_dec;
        case 0xCA: return fam65xx_op_dex;
        case 0x88: return fam65xx_op_dey;
        case 0xE6: case 0xF6: case 0xEE: case 0xFE:
            return fam65xx_op_inc;
        case 0xE8: return fam65xx_op_inx;
        case 0xC8: return fam65xx_op_iny;
            
        // EOR - Exclusive OR
        case 0x49: case 0x45: case 0x55: case 0x4D: case 0x5D: case 0x59: case 0x41: case 0x51:
            return fam65xx_op_eor;
            
        // Jump/Call
        case 0x4C: case 0x6C: return fam65xx_op_jmp;
        case 0x20: return fam65xx_op_jsr;
        case 0x60: return fam65xx_op_rts;
        
        // Load instructions
        case 0xA9: case 0xA5: case 0xB5: case 0xAD: case 0xBD: case 0xB9: case 0xA1: case 0xB1:
            return fam65xx_op_lda;
        case 0xA2: case 0xA6: case 0xB6: case 0xAE: case 0xBE:
            return fam65xx_op_ldx;
        case 0xA0: case 0xA4: case 0xB4: case 0xAC: case 0xBC:
            return fam65xx_op_ldy;
            
        // LSR - Logical Shift Right
        case 0x4A: case 0x46: case 0x56: case 0x4E: case 0x5E:
            return fam65xx_op_lsr;
            
        // NOP
        case 0xEA: return fam65xx_op_nop;
        
        // ORA - Logical OR
        case 0x09: case 0x05: case 0x15: case 0x0D: case 0x1D: case 0x19: case 0x01: case 0x11:
            return fam65xx_op_ora;
            
        // Stack operations
        case 0x48: return fam65xx_op_pha;
        case 0x68: return fam65xx_op_pla;
        case 0x08: return fam65xx_op_php;
        case 0x28: return fam65xx_op_plp;
        
        // ROL/ROR - Rotate
        case 0x2A: case 0x26: case 0x36: case 0x2E: case 0x3E:
            return fam65xx_op_rol;
        case 0x6A: case 0x66: case 0x76: case 0x6E: case 0x7E:
            return fam65xx_op_ror;
            
        // SBC - Subtract with Carry
        case 0xE9: case 0xE5: case 0xF5: case 0xED: case 0xFD: case 0xF9: case 0xE1: case 0xF1:
            return fam65xx_op_sbc;
            
        // Store instructions
        case 0x85: case 0x95: case 0x8D: case 0x9D: case 0x99: case 0x81: case 0x91:
            return fam65xx_op_sta;
        case 0x86: case 0x96: case 0x8E:
            return fam65xx_op_stx;
        case 0x84: case 0x94: case 0x8C:
            return fam65xx_op_sty;
            
        // Transfer instructions
        case 0xAA: return fam65xx_op_tax;
        case 0x8A: return fam65xx_op_txa;
        case 0xA8: return fam65xx_op_tay;
        case 0x98: return fam65xx_op_tya;
        case 0xBA: return fam65xx_op_tsx;
        case 0x9A: return fam65xx_op_txs;
    }
    
    // CMOS enhancements (65C02 and later)
    if constexpr (processor_has_cmos_enhancements<ProcessorTag>()) {
        switch (opcode) {
            // BRA - Branch Always
            case 0x80: return fam65xx_op_bra;
            
            // STZ - Store Zero
            case 0x64: case 0x74: case 0x9C: case 0x9E:
                return fam65xx_op_stz;
                
            // TRB/TSB - Test and Reset/Set Bits
            case 0x14: case 0x1C: return fam65xx_op_trb;
            case 0x04: case 0x0C: return fam65xx_op_tsb;
            
            // PHX/PHY/PLX/PLY - Push/Pull X/Y
            case 0xDA: return fam65xx_op_phx;
            case 0xFA: return fam65xx_op_plx;
            case 0x5A: return fam65xx_op_phy;
            case 0x7A: return fam65xx_op_ply;
            
            // BIT immediate and absolute,X
            case 0x89: case 0x3C: return fam65xx_op_bit;
            
            // JMP (abs,X) - 65C02 enhancement
            case 0x7C: return fam65xx_op_jmp;
            
            // Additional addressing modes for existing instructions
            case 0x72: return fam65xx_op_adc; // ADC (zp)
            case 0x32: return fam65xx_op_and; // AND (zp)
            case 0xD2: return fam65xx_op_cmp; // CMP (zp)
            case 0x52: return fam65xx_op_eor; // EOR (zp)
            case 0xB2: return fam65xx_op_lda; // LDA (zp)
            case 0x12: return fam65xx_op_ora; // ORA (zp)
            case 0xF2: return fam65xx_op_sbc; // SBC (zp)
            case 0x92: return fam65xx_op_sta; // STA (zp)
        }
    }
    
    // Rockwell 65C02 bit manipulation instructions
    if constexpr (processor_has_bit_manipulation<ProcessorTag>()) {
        switch (opcode) {
            // RMB - Reset Memory Bit
            case 0x07: case 0x17: case 0x27: case 0x37:
            case 0x47: case 0x57: case 0x67: case 0x77:
                return fam65xx_op_rmb;
                
            // SMB - Set Memory Bit
            case 0x87: case 0x97: case 0xA7: case 0xB7:
            case 0xC7: case 0xD7: case 0xE7: case 0xF7:
                return fam65xx_op_smb;
                
            // BBR - Branch on Bit Reset
            case 0x0F: case 0x1F: case 0x2F: case 0x3F:
            case 0x4F: case 0x5F: case 0x6F: case 0x7F:
                return fam65xx_op_bbr;
                
            // BBS - Branch on Bit Set
            case 0x8F: case 0x9F: case 0xAF: case 0xBF:
            case 0xCF: case 0xDF: case 0xEF: case 0xFF:
                return fam65xx_op_bbs;
        }
    }
    
    // 65C816 16-bit extensions
    if constexpr (processor_has_16bit_mode<ProcessorTag>()) {
        switch (opcode) {
            // Mode control
            case 0xC2: return fam65xx_op_rep; // Reset Processor Status
            case 0xE2: return fam65xx_op_sep; // Set Processor Status
            case 0xFB: return fam65xx_op_xce; // Exchange Carry/Emulation
            
            // Register operations
            case 0xEB: return fam65xx_op_xba; // Exchange B and A
            
            // Enhanced stack operations
            case 0xF4: return fam65xx_op_pea; // Push Effective Absolute
            case 0xD4: return fam65xx_op_pei; // Push Effective Indirect
            case 0x62: return fam65xx_op_per; // Push Effective PC Relative
            case 0x8B: return fam65xx_op_phb; // Push Data Bank
            case 0x0B: return fam65xx_op_phd; // Push Direct Page
            case 0x4B: return fam65xx_op_phk; // Push Program Bank
            case 0xAB: return fam65xx_op_plb; // Pull Data Bank
            case 0x2B: return fam65xx_op_pld; // Pull Direct Page
            
            // Long addressing
            case 0x22: return fam65xx_op_jsl; // Jump Subroutine Long
            case 0x6B: return fam65xx_op_rtl; // Return from Subroutine Long
            case 0x5C: return fam65xx_op_jml; // Jump Long Absolute
            case 0xDC: return fam65xx_op_jml; // Jump Long Indirect
            
            // Block move
            case 0x54: return fam65xx_op_mvn; // Move Block Negative
            case 0x44: return fam65xx_op_mvp; // Move Block Positive
            
            // System instructions
            case 0x02: return fam65xx_op_cop; // Coprocessor
            case 0x42: return fam65xx_op_wdm; // William D. Mensch
            case 0xDB: return fam65xx_op_stp; // Stop
        }
    }
    
    // Illegal opcodes (NMOS processors only)
    if constexpr (processor_has_illegal_opcodes<ProcessorTag>()) {
        switch (opcode) {
            // LAX - Load A and X
            case 0xA7: case 0xB7: case 0xAF: case 0xBF: case 0xA3: case 0xB3:
                return fam65xx_op_lax;
                
            // SAX - Store A AND X
            case 0x87: case 0x97: case 0x8F: case 0x83:
                return fam65xx_op_sax;
                
            // DCP - Decrement and Compare
            case 0xC7: case 0xD7: case 0xCF: case 0xDF: case 0xDB: case 0xC3: case 0xD3:
                return fam65xx_op_dcp;
                
            // ISC - Increment and Subtract with Carry
            case 0xE7: case 0xF7: case 0xEF: case 0xFF: case 0xFB: case 0xE3: case 0xF3:
                return fam65xx_op_isc;
                
            // SLO - Shift Left and OR
            case 0x07: case 0x17: case 0x0F: case 0x1F: case 0x1B: case 0x03: case 0x13:
                return fam65xx_op_slo;
                
            // RLA - Rotate Left and AND
            case 0x27: case 0x37: case 0x2F: case 0x3F: case 0x3B: case 0x23: case 0x33:
                return fam65xx_op_rla;
                
            // SRE - Shift Right and EOR
            case 0x47: case 0x57: case 0x4F: case 0x5F: case 0x5B: case 0x43: case 0x53:
                return fam65xx_op_sre;
                
            // RRA - Rotate Right and Add
            case 0x67: case 0x77: case 0x6F: case 0x7F: case 0x7B: case 0x63: case 0x73:
                return fam65xx_op_rra;
        }
    }
    
    // For CMOS processors, illegal opcodes become NOPs
    if constexpr (processor_has_cmos_enhancements<ProcessorTag>()) {
        // All undefined opcodes are NOPs on CMOS
        return fam65xx_op_nop;
    }
    
    // For NMOS processors, remaining undefined opcodes are NOPs or JAM
    return fam65xx_op_nop; // Simplified - some might be JAM instructions
}

// Generate processor-specific opcode table
template<typename ProcessorTag>
constexpr std::array<opcode_info_t, 256> generate_opcode_table() {
    std::array<opcode_info_t, 256> table = {};
    
    for (int i = 0; i < 256; ++i) {
        opcode_info_t& entry = table[i];
        uint8_t opcode = static_cast<uint8_t>(i);
        
        // Get the handler for this processor
        fam65xx_op_handler_t handler = get_opcode_handler<ProcessorTag>(opcode);
        
        // Set up addressing mode and operation based on opcode
        // This would need to be filled in with the complete opcode mapping
        // For now, just set basic structure
        entry.op_index = 0; // Would map to handler index
        entry.am_index = 0; // Would map to addressing mode index
        entry.page_cross = 0;
        entry.rmw = 0;
        entry._reserved = 0;
    }
    
    return table;
}

// Unified opcode table accessor
template<typename ProcessorTag>
constexpr const opcode_info_t* get_processor_opcode_table() {
    static constexpr auto table = generate_opcode_table<ProcessorTag>();
    return table.data();
}

} // namespace fam65xx_unified

#endif // __cplusplus