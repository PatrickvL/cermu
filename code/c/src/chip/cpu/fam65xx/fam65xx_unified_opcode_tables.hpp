#pragma once
/*
 * fam65xx_unified_opcode_tables.hpp - Unified Opcode Table System for All MOS 65xx Family Members
 *
 * This file implements a complete template-based opcode table system that:
 * - Uses the unified operations from fam65xx_unified_operations.hpp
 * - Provides processor-specific opcode tables with compile-time selection
 * - Maintains hardware-accurate behavior for all processor variants
 * - Eliminates code duplication across processor families
 *
 * DESIGN PRINCIPLES:
 * - Single source of truth for all opcodes across all processors
 * - Zero runtime overhead - processor differences resolved at compile time
 * - Complete 256-entry tables with proper addressing mode mapping
 * - Hardware-accurate illegal opcode handling per processor type
 * - Integration with the unified feature template system
 */

#ifndef FAM65XX_UNIFIED_OPCODE_TABLES_HPP_INCLUDED
#define FAM65XX_UNIFIED_OPCODE_TABLES_HPP_INCLUDED

#ifdef __cplusplus

#include <array>
#include <cstdint>
#include "fam65xx_types.hpp"
#include "fam65xx_processor_traits.hpp"

// Include tables for type definitions
#include "fam65xx_tables.hpp"

// Include split operations files (eliminates code duplication)
#ifdef AIEMUC_IMPL
extern "C" {
#include "fam65xx_addrmodes.hpp"
#include "fam65xx_ops.hpp"
#include "fam65xx_ops_part2.hpp"
#include "fam65xx_ops_part3.hpp"
#include "fam65xx_ops_rmw.hpp"
#include "fam65xx_ops_illegal.hpp"
#include "fam65xx_ops_65c02.hpp"
}
#endif

namespace fam65xx_unified_tables {

// ============================================================================
// COMPILE-TIME OPCODE TABLE GENERATION SYSTEM
// ============================================================================

// Template helper to get operation index for an opcode based on processor features
template<typename ProcessorTag>
constexpr operation_t get_operation_for_opcode(uint8_t opcode) {
    using traits = fam65xx_core::ProcessorTraits<ProcessorTag>;
    
    // Core 6502 opcodes (supported by all processors)
    switch (opcode) {
        // ADC - Add with Carry
        case 0x69: case 0x65: case 0x75: case 0x6D: case 0x7D: case 0x79: case 0x61: case 0x71:
            return OP_ADC;
            
        // AND - Logical AND
        case 0x29: case 0x25: case 0x35: case 0x2D: case 0x3D: case 0x39: case 0x21: case 0x31:
            return OP_AND;
            
        // ASL - Arithmetic Shift Left
        case 0x0A: case 0x06: case 0x16: case 0x0E: case 0x1E:
            return OP_ASL;
            
        // BIT - Bit Test
        case 0x24: case 0x2C:
            return OP_BIT;
            
        // Branch instructions
        case 0x10: return OP_BPL;
        case 0x30: return OP_BMI;
        case 0x50: return OP_BVC;
        case 0x70: return OP_BVS;
        case 0x90: return OP_BCC;
        case 0xB0: return OP_BCS;
        case 0xD0: return OP_BNE;
        case 0xF0: return OP_BEQ;
        
        // Break and interrupts
        case 0x00: return OP_BRK;
        case 0x40: return OP_RTI;
        
        // Clear/Set flags
        case 0x18: return OP_CLC;
        case 0x38: return OP_SEC;
        case 0x58: return OP_CLI;
        case 0x78: return OP_SEI;
        case 0xB8: return OP_CLV;
        case 0xD8: return OP_CLD;
        case 0xF8: return OP_SED;
        
        // Compare instructions
        case 0xC9: case 0xC5: case 0xD5: case 0xCD: case 0xDD: case 0xD9: case 0xC1: case 0xD1:
            return OP_CMP;
        case 0xE0: case 0xE4: case 0xEC:
            return OP_CPX;
        case 0xC0: case 0xC4: case 0xCC:
            return OP_CPY;
            
        // Decrement/Increment
        case 0xC6: case 0xD6: case 0xCE: case 0xDE:
            return OP_DEC;
        case 0xCA: return OP_DEX;
        case 0x88: return OP_DEY;
        case 0xE6: case 0xF6: case 0xEE: case 0xFE:
            return OP_INC;
        case 0xE8: return OP_INX;
        case 0xC8: return OP_INY;
            
        // EOR - Exclusive OR
        case 0x49: case 0x45: case 0x55: case 0x4D: case 0x5D: case 0x59: case 0x41: case 0x51:
            return OP_EOR;
            
        // Jump/Call
        case 0x4C: case 0x6C: return OP_JMP;
        case 0x20: return OP_JSR;
        case 0x60: return OP_RTS;
        
        // Load instructions
        case 0xA9: case 0xA5: case 0xB5: case 0xAD: case 0xBD: case 0xB9: case 0xA1: case 0xB1:
            return OP_LDA;
        case 0xA2: case 0xA6: case 0xB6: case 0xAE: case 0xBE:
            return OP_LDX;
        case 0xA0: case 0xA4: case 0xB4: case 0xAC: case 0xBC:
            return OP_LDY;
            
        // LSR - Logical Shift Right
        case 0x4A: case 0x46: case 0x56: case 0x4E: case 0x5E:
            return OP_LSR;
            
        // NOP
        case 0xEA: return OP_NOP;
        
        // ORA - Logical OR
        case 0x09: case 0x05: case 0x15: case 0x0D: case 0x1D: case 0x19: case 0x01: case 0x11:
            return OP_ORA;
            
        // Stack operations
        case 0x48: return OP_PHA;
        case 0x68: return OP_PLA;
        case 0x08: return OP_PHP;
        case 0x28: return OP_PLP;
        
        // ROL/ROR - Rotate
        case 0x2A: case 0x26: case 0x36: case 0x2E: case 0x3E:
            return OP_ROL;
        case 0x6A: case 0x66: case 0x76: case 0x6E: case 0x7E:
            return OP_ROR;
            
        // SBC - Subtract with Carry
        case 0xE9: case 0xE5: case 0xF5: case 0xED: case 0xFD: case 0xF9: case 0xE1: case 0xF1:
            return OP_SBC;
        // 0xEB is also SBC on NMOS (illegal but functional)
        case 0xEB:
            if constexpr (fam65xx_core::has_illegal_opcodes<ProcessorTag>()) {
                return OP_SBC;
            } else {
                return OP_NOP; // CMOS treats as NOP
            }
            
        // Store instructions
        case 0x85: case 0x95: case 0x8D: case 0x9D: case 0x99: case 0x81: case 0x91:
            return OP_STA;
        case 0x86: case 0x96: case 0x8E:
            return OP_STX;
        case 0x84: case 0x94: case 0x8C:
            return OP_STY;
            
        // Transfer instructions
        case 0xAA: return OP_TAX;
        case 0x8A: return OP_TXA;
        case 0xA8: return OP_TAY;
        case 0x98: return OP_TYA;
        case 0xBA: return OP_TSX;
        case 0x9A: return OP_TXS;
    }
    
    // CMOS enhancements (65C02 and later)
    if constexpr (fam65xx_core::has_cmos_enhancements<ProcessorTag>()) {
        switch (opcode) {
            // BRA - Branch Always
            case 0x80: return OP_BRA;
            
            // STZ - Store Zero
            case 0x64: case 0x74: case 0x9C: case 0x9E:
                return OP_STZ;
                
            // TRB/TSB - Test and Reset/Set Bits
            case 0x14: case 0x1C: return OP_TRB;
            case 0x04: case 0x0C: return OP_TSB;
            
            // PHX/PHY/PLX/PLY - Push/Pull X/Y
            case 0xDA: return OP_PHX;
            case 0xFA: return OP_PLX;
            case 0x5A: return OP_PHY;
            case 0x7A: return OP_PLY;
            
            // WAI/STP - Wait/Stop
            case 0xCB: return OP_WAI;
            case 0xDB: return OP_STP;
            
            // BIT immediate and absolute,X
            case 0x89: case 0x3C: return OP_BIT;
            
            // JMP (abs,X) - 65C02 enhancement
            case 0x7C: return OP_JMP;
            
            // Additional addressing modes for existing instructions
            case 0x72: return OP_ADC; // ADC (zp)
            case 0x32: return OP_AND; // AND (zp)
            case 0xD2: return OP_CMP; // CMP (zp)
            case 0x52: return OP_EOR; // EOR (zp)
            case 0xB2: return OP_LDA; // LDA (zp)
            case 0x12: return OP_ORA; // ORA (zp)
            case 0xF2: return OP_SBC; // SBC (zp)
            case 0x92: return OP_STA; // STA (zp)
        }
    }
    
    // Rockwell 65C02 bit manipulation instructions
    if constexpr (fam65xx_core::has_bit_manipulation<ProcessorTag>()) {
        switch (opcode) {
            // RMB - Reset Memory Bit (only for Rockwell, conflicts with illegal opcodes)
            case 0x07: case 0x17: case 0x27: case 0x37:
            case 0x47: case 0x57: case 0x67: case 0x77:
                return OP_RMB0; // Base op, bit number encoded in opcode
                
            // SMB - Set Memory Bit
            case 0x87: case 0x97: case 0xA7: case 0xB7:
            case 0xC7: case 0xD7: case 0xE7: case 0xF7:
                return OP_SMB0; // Base op, bit number encoded in opcode
                
            // BBR - Branch on Bit Reset
            case 0x0F: case 0x1F: case 0x2F: case 0x3F:
            case 0x4F: case 0x5F: case 0x6F: case 0x7F:
                return OP_BBR0; // Base op, bit number encoded in opcode
                
            // BBS - Branch on Bit Set
            case 0x8F: case 0x9F: case 0xAF: case 0xBF:
            case 0xCF: case 0xDF: case 0xEF: case 0xFF:
                return OP_BBS0; // Base op, bit number encoded in opcode
        }
    }
    
    // 65C816 16-bit extensions
    if constexpr (fam65xx_core::has_16bit_mode<ProcessorTag>()) {
        switch (opcode) {
            // Mode control
            case 0xC2: return OP_REP; // Reset Processor Status
            case 0xE2: return OP_SEP; // Set Processor Status
            case 0xFB: return OP_XCE; // Exchange Carry/Emulation
            
            // Register operations
            case 0xEB: return OP_XBA; // Exchange B and A
            
            // Enhanced stack operations
            case 0xF4: return OP_PEA; // Push Effective Absolute
            case 0xD4: return OP_PEI; // Push Effective Indirect
            case 0x62: return OP_PER; // Push Effective PC Relative
            case 0x8B: return OP_PHB; // Push Data Bank
            case 0x0B: return OP_PHD; // Push Direct Page
            case 0x4B: return OP_PHK; // Push Program Bank
            case 0xAB: return OP_PLB; // Pull Data Bank
            case 0x2B: return OP_PLD; // Pull Direct Page
            
            // Long addressing
            case 0x22: return OP_JSL; // Jump Subroutine Long
            case 0x6B: return OP_RTL; // Return from Subroutine Long
            case 0x5C: return OP_JML; // Jump Long Absolute
            case 0xDC: return OP_JML; // Jump Long Indirect
            
            // Block move
            case 0x54: return OP_MVN; // Move Block Negative
            case 0x44: return OP_MVP; // Move Block Positive
            
            // System instructions
            case 0x02: return OP_COP; // Coprocessor
            case 0x42: return OP_WDM; // William D. Mensch
        }
    }
    
    // Illegal opcodes (NMOS processors only)
    if constexpr (fam65xx_core::has_illegal_opcodes<ProcessorTag>()) {
        switch (opcode) {
            // LAX - Load A and X
            case 0xA7: case 0xB7: case 0xAF: case 0xBF: case 0xA3: case 0xB3:
                return OP_LAX;
                
            // SAX - Store A AND X
            case 0x87: case 0x97: case 0x8F: case 0x83:
                return OP_SAX;
                
            // DCP - Decrement and Compare
            case 0xC7: case 0xD7: case 0xCF: case 0xDF: case 0xDB: case 0xC3: case 0xD3:
                return OP_DCP;
                
            // ISC - Increment and Subtract with Carry
            case 0xE7: case 0xF7: case 0xEF: case 0xFF: case 0xFB: case 0xE3: case 0xF3:
                return OP_ISC;
                
            // SLO - Shift Left and OR
            case 0x07: case 0x17: case 0x0F: case 0x1F: case 0x1B: case 0x03: case 0x13:
                return OP_SLO;
                
            // RLA - Rotate Left and AND
            case 0x27: case 0x37: case 0x2F: case 0x3F: case 0x3B: case 0x23: case 0x33:
                return OP_RLA;
                
            // SRE - Shift Right and EOR
            case 0x47: case 0x57: case 0x4F: case 0x5F: case 0x5B: case 0x43: case 0x53:
                return OP_SRE;
                
            // RRA - Rotate Right and Add
            case 0x67: case 0x77: case 0x6F: case 0x7F: case 0x7B: case 0x63: case 0x73:
                return OP_RRA;
                
            // ANC - AND with Carry
            case 0x0B: case 0x2B:
                return OP_ANC;
                
            // ASR - AND then Shift Right
            case 0x4B:
                return OP_ASR;
                
            // ARR - AND then Rotate Right
            case 0x6B:
                return OP_ARR;
                
            // SBX - Subtract from X
            case 0xCB:
                return OP_SBX;
                
            // Store with AND operations (unstable on hardware)
            case 0x9F: return OP_SHA; // SHA (abs,Y)
            case 0x93: return OP_SHA; // SHA (ind),Y
            case 0x9B: return OP_SHS; // SHS abs,Y (unstable)
            case 0x9E: return OP_SHX; // SHX abs,Y (unstable)
            case 0x9C: return OP_SHY; // SHY abs,X (unstable)
            case 0xBB: return OP_LAS; // LAS abs,Y
            
            // XAA - Unstable operation
            case 0x8B:
                return OP_XAA;
                
            // JAM - Processor lockup
            case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52: 
            case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
                return OP_JAM;
                
            // Various NOP illegal opcodes
            case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
            case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2:
            case 0x04: case 0x44: case 0x64: case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4:
            case 0x0C: case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
                return OP_NOP;
        }
    }
    
    // Default for all undefined opcodes (both NMOS and CMOS)
    return OP_NOP;
}

// Template helper to get addressing mode for an opcode
template<typename ProcessorTag>
constexpr addressing_mode_t get_addressing_mode_for_opcode(uint8_t opcode) {
    using traits = fam65xx_core::ProcessorTraits<ProcessorTag>;
    
    // Complete opcode to addressing mode mapping
    switch (opcode) {
        // Immediate mode instructions
        case 0x69: case 0x29: case 0xC9: case 0xE0: case 0xC0: case 0x49: case 0xA9: case 0xA2: case 0xA0:
        case 0x09: case 0xE9: case 0x80: case 0x89: case 0xEB:
            return AM_IMM;
            
        // Zero page instructions
        case 0x65: case 0x25: case 0x24: case 0xC5: case 0xE4: case 0xC4: case 0xC6: case 0xE6:
        case 0x45: case 0xA5: case 0xA6: case 0xA4: case 0x46: case 0x05: case 0x26: case 0x66:
        case 0xE5: case 0x85: case 0x86: case 0x84: case 0x06: case 0x56: case 0x0E: case 0x1E:
        case 0x64: case 0x04: case 0x14:
            return AM_ZER;
            
        // Zero page,X instructions
        case 0x75: case 0x35: case 0xD5: case 0xD6: case 0xF6: case 0x55: case 0xB5:
        case 0x16: case 0x15: case 0x36: case 0x76: case 0xF5: case 0x95: case 0x94:
        case 0x74: case 0x34: case 0x54:
            return AM_ZPX;
            
        // Zero page,Y instructions
        case 0xB6: case 0x96:
            return AM_ZPY;
            
        // Absolute instructions
        case 0x6D: case 0x2D: case 0x2C: case 0xCD: case 0xEC: case 0xCC: case 0xCE: case 0xEE:
        case 0x4D: case 0x4C: case 0x20: case 0xAD: case 0xAE: case 0xAC: case 0x4E: case 0x0D:
        case 0x2E: case 0x6E: case 0xED: case 0x8D: case 0x8E: case 0x8C: case 0x9C: case 0x0C:
        case 0x1C: case 0x7C:
            return AM_ABS;
            
        // Absolute,X instructions
        case 0x7D: case 0x3D: case 0x3C: case 0xDD: case 0xDE: case 0xFE: case 0x5D: case 0xBD:
        case 0xBC: case 0x5E: case 0x1D: case 0x3E: case 0x7E: case 0xFD: case 0x9D: case 0x9E:
            return AM_ABX;
            
        // Absolute,Y instructions
        case 0x79: case 0x39: case 0xD9: case 0x59: case 0xB9: case 0xBE: case 0x19: case 0xF9: case 0x99:
            return AM_ABY;
            
        // Indirect instructions
        case 0x6C:
            return AM_IND;
            
        // Indexed indirect (zp,X)
        case 0x61: case 0x21: case 0xC1: case 0x41: case 0xA1: case 0x01: case 0xE1: case 0x81:
            return AM_INX;
            
        // Indirect indexed (zp),Y
        case 0x71: case 0x31: case 0xD1: case 0x51: case 0xB1: case 0x11: case 0xF1: case 0x91:
            return AM_INY;
            
        // Zero page indirect (zp) - 65C02+
        case 0x72: case 0x32: case 0xD2: case 0x52: case 0xB2: case 0x12: case 0xF2: case 0x92:
            if constexpr (fam65xx_core::has_cmos_enhancements<ProcessorTag>()) {
                return AM_ZPI;
            }
            break;
            
        // Zero page relative for bit branch instructions - Rockwell only
        case 0x0F: case 0x1F: case 0x2F: case 0x3F: case 0x4F: case 0x5F: case 0x6F: case 0x7F:
        case 0x8F: case 0x9F: case 0xAF: case 0xBF: case 0xCF: case 0xDF: case 0xEF: case 0xFF:
            if constexpr (fam65xx_core::has_bit_manipulation<ProcessorTag>()) {
                return AM_ZPR;
            }
            break;
    }
    
    // All other instructions use implicit/no addressing mode
    return AM_NON;
}

// Template helper to determine if opcode can skip page crossing
template<typename ProcessorTag>
constexpr bool opcode_can_skip_page_cross(uint8_t opcode) {
    // Instructions that might have page crossing penalties but can optimize them away
    // if the page boundary is not crossed
    switch (opcode) {
        // Load instructions with potential page crossing
        case 0xBE: case 0xBC: // LDX/LDY abs,X/Y
        case 0xAD: case 0xBD: case 0xB9: case 0xA1: case 0xB1: // LDA variants
        case 0x71: case 0x31: case 0xD1: case 0x51: case 0x11: case 0xF1: // (zp),Y variants
        case 0x79: case 0x39: case 0xD9: case 0x59: case 0x19: case 0xF9: // abs,Y variants  
        case 0x7D: case 0x3D: case 0xDD: case 0x5D: case 0x1D: case 0xFD: // abs,X variants
            return true;
    }
    return false;
}

// Template helper to determine if opcode is read-modify-write
template<typename ProcessorTag>
constexpr bool opcode_is_rmw(uint8_t opcode) {
    switch (opcode) {
        // Shift and rotate operations
        case 0x06: case 0x16: case 0x0E: case 0x1E: // ASL
        case 0x46: case 0x56: case 0x4E: case 0x5E: // LSR  
        case 0x26: case 0x36: case 0x2E: case 0x3E: // ROL
        case 0x66: case 0x76: case 0x6E: case 0x7E: // ROR
        // Increment/decrement
        case 0xE6: case 0xF6: case 0xEE: case 0xFE: // INC
        case 0xC6: case 0xD6: case 0xCE: case 0xDE: // DEC
        // Test and reset/set bits (65C02)
        case 0x14: case 0x1C: case 0x04: case 0x0C: // TRB/TSB
            return true;
    }
    
    // Illegal RMW operations on NMOS
    if constexpr (fam65xx_core::has_illegal_opcodes<ProcessorTag>()) {
        switch (opcode) {
            case 0x07: case 0x17: case 0x0F: case 0x1F: case 0x1B: case 0x03: case 0x13: // SLO
            case 0x27: case 0x37: case 0x2F: case 0x3F: case 0x3B: case 0x23: case 0x33: // RLA
            case 0x47: case 0x57: case 0x4F: case 0x5F: case 0x5B: case 0x43: case 0x53: // SRE
            case 0x67: case 0x77: case 0x6F: case 0x7F: case 0x7B: case 0x63: case 0x73: // RRA
            case 0xC7: case 0xD7: case 0xCF: case 0xDF: case 0xDB: case 0xC3: case 0xD3: // DCP
            case 0xE7: case 0xF7: case 0xEF: case 0xFF: case 0xFB: case 0xE3: case 0xF3: // ISC
                return true;
        }
    }
    
    // Bit manipulation RMW operations (Rockwell)
    if constexpr (fam65xx_core::has_bit_manipulation<ProcessorTag>()) {
        switch (opcode) {
            case 0x07: case 0x17: case 0x27: case 0x37: case 0x47: case 0x57: case 0x67: case 0x77: // RMB
            case 0x87: case 0x97: case 0xA7: case 0xB7: case 0xC7: case 0xD7: case 0xE7: case 0xF7: // SMB
                return true;
        }
    }
    
    return false;
}

// Template helper to determine if opcode is an illegal store with hardware quirks
template<typename ProcessorTag>
constexpr bool opcode_is_illegal_store(uint8_t opcode) {
    if constexpr (fam65xx_core::has_illegal_opcodes<ProcessorTag>()) {
        switch (opcode) {
            // Illegal store operations that have hardware quirks on page boundary crossing
            case 0x9F: case 0x93: // SHA
            case 0x9B: // SHS  
            case 0x9E: // SHX
            case 0x9C: // SHY
                return true;
                
            // Illegal RMW operations that write to memory with absolute,X/Y addressing
            case 0x1F: case 0x1B: // SLO abs,X/Y
            case 0x3F: case 0x3B: // RLA abs,X/Y
            case 0x5F: case 0x5B: // SRE abs,X/Y
            case 0x7F: case 0x7B: // RRA abs,X/Y
            case 0xDF: case 0xDB: // DCP abs,X/Y
            case 0xFF: case 0xFB: // ISC abs,X/Y
                return true;
        }
    }
    return false;
}

// Generate complete processor-specific opcode table
template<typename ProcessorTag>
constexpr std::array<opcode_info_t, 256> generate_opcode_table() {
    std::array<opcode_info_t, 256> table = {};
    
    for (int i = 0; i < 256; ++i) {
        opcode_info_t& entry = table[i];
        uint8_t opcode = static_cast<uint8_t>(i);
        
        // Get processor-specific operation and addressing mode
        operation_t op = get_operation_for_opcode<ProcessorTag>(opcode);
        addressing_mode_t am = get_addressing_mode_for_opcode<ProcessorTag>(opcode);
        
        // Set up the opcode entry
        entry.op_index = static_cast<uint8_t>(op);
        entry.am_index = static_cast<uint8_t>(am);
        entry.can_skip_page_cross = opcode_can_skip_page_cross<ProcessorTag>(opcode) ? 1 : 0;
        entry.rmw = opcode_is_rmw<ProcessorTag>(opcode) ? 1 : 0;
        entry.illegal_store = opcode_is_illegal_store<ProcessorTag>(opcode) ? 1 : 0;
        entry._reserved = 0;
    }
    
    return table;
}

// Unified opcode table accessor for any processor type
template<typename ProcessorTag>
constexpr const opcode_info_t* get_processor_opcode_table() {
    static constexpr auto table = generate_opcode_table<ProcessorTag>();
    return table.data();
}

// Get specific opcode entry for a processor type
template<typename ProcessorTag>
constexpr opcode_info_t get_processor_opcode_entry(uint8_t opcode) {
    static constexpr auto table = generate_opcode_table<ProcessorTag>();
    return table[opcode];
}

// ============================================================================
// PROCESSOR-SPECIFIC TABLE INSTANCES
// ============================================================================

#ifdef AIEMUC_IMPL

// Generate static tables for each processor (runtime initialization)
namespace tables {
    // Runtime-initialized tables (constexpr has limitations with complex switch statements)
    extern std::array<opcode_info_t, 256> mos6502_table;
    extern std::array<opcode_info_t, 256> mos6510_table;
    extern std::array<opcode_info_t, 256> wdc65c02_table;
    extern std::array<opcode_info_t, 256> rockwell65c02_table;
    extern std::array<opcode_info_t, 256> wdc65c816_table;
    
    // Initialize all tables
    void initialize_tables();
}

#endif // AIEMUC_IMPL

// ============================================================================
// COMPILE-TIME VALIDATION HELPERS
// ============================================================================

// Runtime validation functions (since constexpr has limitations with complex operations)

// Validate that a processor table has the correct number of entries
template<typename ProcessorTag>
bool validate_opcode_table() {
    auto table = generate_opcode_table<ProcessorTag>();
    return table.size() == 256;
}

// Validate that illegal opcodes are properly handled
template<typename ProcessorTag>
bool validate_illegal_opcodes() {
    // Check that illegal opcodes are only present on NMOS processors
    if constexpr (fam65xx_core::has_illegal_opcodes<ProcessorTag>()) {
        // LAX should be available
        auto op_a7 = get_operation_for_opcode<ProcessorTag>(0xA7);
        return op_a7 == OP_LAX;
    } else {
        // LAX should be NOP on CMOS
        auto op_a7 = get_operation_for_opcode<ProcessorTag>(0xA7);
        return op_a7 == OP_NOP;
    }
}

// Validate that enhanced opcodes are only available on appropriate processors
template<typename ProcessorTag>
bool validate_enhanced_opcodes() {
    if constexpr (fam65xx_core::has_cmos_enhancements<ProcessorTag>()) {
        // STZ should be available
        auto op_64 = get_operation_for_opcode<ProcessorTag>(0x64);
        return op_64 == OP_STZ;
    } else {
        // STZ should be NOP or illegal on NMOS
        auto op_64 = get_operation_for_opcode<ProcessorTag>(0x64);
        return op_64 == OP_NOP;
    }
}

// Runtime validation function to check all processors
bool validate_all_processors() {
    bool all_valid = true;
    
    all_valid &= validate_opcode_table<fam65xx_core::MOS6502Tag>();
    all_valid &= validate_opcode_table<fam65xx_core::MOS6510Tag>();
    all_valid &= validate_opcode_table<fam65xx_core::WDC65C02Tag>();
    all_valid &= validate_opcode_table<fam65xx_core::Rockwell65C02Tag>();
    all_valid &= validate_opcode_table<fam65xx_core::WDC65C816Tag>();

    all_valid &= validate_illegal_opcodes<fam65xx_core::MOS6502Tag>();
    all_valid &= validate_illegal_opcodes<fam65xx_core::WDC65C02Tag>();

    all_valid &= validate_enhanced_opcodes<fam65xx_core::WDC65C02Tag>();
    all_valid &= validate_enhanced_opcodes<fam65xx_core::MOS6502Tag>();
    
    return all_valid;
}

} // namespace fam65xx_unified_tables

#endif // __cplusplus

#endif // FAM65XX_UNIFIED_OPCODE_TABLES_HPP_INCLUDED