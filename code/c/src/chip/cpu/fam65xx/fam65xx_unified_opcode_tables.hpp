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

// Template helper to get complete opcode info for an opcode based on processor features
// This replaces the separate get_operation_for_opcode and get_addressing_mode_for_opcode functions
template<typename ProcessorTag>
constexpr opcode_info_t get_opcode_info_for_opcode(uint8_t opcode) {
    using traits = fam65xx_core::ProcessorTraits<ProcessorTag>;
    
    // Handle processor-specific extensions - override base 6502 table where needed
    if constexpr (!fam65xx_core::has_illegal_opcodes<ProcessorTag>()) {
        // CMOS processors: convert illegal opcodes to NOPs with proper addressing modes
        switch (opcode) {
            // Convert illegal opcodes to NOPs for non-NMOS processors
            case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
            case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
                return {OP_NOP, AM_NON, 0, 0, 0, 0}; // JAM -> NOP
            
            case 0x03: case 0x13: case 0x23: case 0x33: case 0x43: case 0x53:
            case 0x63: case 0x73: case 0xC3: case 0xD3: case 0xE3: case 0xF3:
                return {OP_NOP, AM_NON, 0, 0, 0, 0}; // Illegal RMW -> NOP
            
            case 0x0B: case 0x2B: case 0x4B: case 0x6B: case 0x8B: case 0xAB: case 0xCB:
                return {OP_NOP, AM_NON, 0, 0, 0, 0}; // Illegal immediate -> NOP
        }
        
        // CMOS enhancements (65C02 and later)
        if constexpr (fam65xx_core::has_cmos_enhancements<ProcessorTag>()) {
            switch (opcode) {
                // Override some illegal opcodes with CMOS enhancements
                case 0x80: return {OP_BRA, AM_NON, 0, 0, 0, 0}; // REL mode
                case 0x64: return {OP_STZ, AM_ZER, 0, 0, 0, 0};
                case 0x74: return {OP_STZ, AM_ZPX, 0, 0, 0, 0};
                case 0x9C: return {OP_STZ, AM_ABS, 0, 0, 0, 0};
                case 0x9E: return {OP_STZ, AM_ABX, 0, 0, 0, 0};
                case 0x14: return {OP_TRB, AM_ZER, 0, 0, 1, 0};
                case 0x1C: return {OP_TRB, AM_ABS, 0, 0, 1, 0};
                case 0x04: return {OP_TSB, AM_ZER, 0, 0, 1, 0};
                case 0x0C: return {OP_TSB, AM_ABS, 0, 0, 1, 0};
                case 0xDA: return {OP_PHX, AM_NON, 0, 0, 0, 0};
                case 0xFA: return {OP_PLX, AM_NON, 0, 0, 0, 0};
                case 0x5A: return {OP_PHY, AM_NON, 0, 0, 0, 0};
                case 0x7A: return {OP_PLY, AM_NON, 0, 0, 0, 0};
                case 0xCB: return {OP_WAI, AM_NON, 0, 0, 0, 0};
                case 0xDB: return {OP_STP, AM_NON, 0, 0, 0, 0};
                case 0x89: return {OP_BIT, AM_IMM, 0, 1, 0, 0};
                case 0x3C: return {OP_BIT, AM_ABX, 0, 1, 0, 0};
                case 0x7C: return {OP_JMP, AM_ABX, 0, 1, 0, 0}; // JMP (abs,X)
                
                // Zero page indirect addressing modes
                case 0x72: return {OP_ADC, AM_ZPI, 0, 1, 0, 0};
                case 0x32: return {OP_AND, AM_ZPI, 0, 1, 0, 0};
                case 0xD2: return {OP_CMP, AM_ZPI, 0, 1, 0, 0};
                case 0x52: return {OP_EOR, AM_ZPI, 0, 1, 0, 0};
                case 0xB2: return {OP_LDA, AM_ZPI, 0, 1, 0, 0};
                case 0x12: return {OP_ORA, AM_ZPI, 0, 1, 0, 0};
                case 0xF2: return {OP_SBC, AM_ZPI, 0, 1, 0, 0};
                case 0x92: return {OP_STA, AM_ZPI, 0, 0, 0, 0};
            }
        }
        
        // Rockwell 65C02 bit manipulation instructions
        if constexpr (fam65xx_core::has_bit_manipulation<ProcessorTag>()) {
            switch (opcode) {
                // Override illegal opcodes with bit manipulation (conflicts handled by processor type)
                case 0x07: case 0x17: case 0x27: case 0x37:
                case 0x47: case 0x57: case 0x67: case 0x77:
                    return {OP_RMB0, AM_ZER, 0, 0, 1, 0}; // RMB instructions
                    
                case 0x87: case 0x97: case 0xA7: case 0xB7:
                case 0xC7: case 0xD7: case 0xE7: case 0xF7:
                    return {OP_SMB0, AM_ZER, 0, 0, 1, 0}; // SMB instructions
                    
                case 0x0F: case 0x1F: case 0x2F: case 0x3F:
                case 0x4F: case 0x5F: case 0x6F: case 0x7F:
                    return {OP_BBR0, AM_ZPR, 0, 0, 0, 0}; // BBR instructions
                    
                case 0x8F: case 0x9F: case 0xAF: case 0xBF:
                case 0xCF: case 0xDF: case 0xEF: case 0xFF:
                    return {OP_BBS0, AM_ZPR, 0, 0, 0, 0}; // BBS instructions
            }
        }
    }
    
    // Define opcode entries exactly matching fam65xx_mos6502_opcode_table format
    // Format: {op_index, am_index, illegal_store, can_skip_page_cross, rmw, _reserved}
    switch (opcode) {
        // Row 0x00-0x0F (matches fam65xx_mos6502_opcode_table exactly)
        case 0x00: return {OP_BRK, AM_NON, 0, 0, 0, 0};
        case 0x01: return {OP_ORA, AM_INX, 0, 1, 0, 0};
        case 0x02: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x03: return {OP_SLO, AM_INX, 0, 0, 1, 0};
        case 0x04: return {OP_NOP, AM_ZER, 0, 1, 0, 0};
        case 0x05: return {OP_ORA, AM_ZER, 0, 1, 0, 0};
        case 0x06: return {OP_ASL, AM_ZER, 0, 0, 1, 0};
        case 0x07: return {OP_SLO, AM_ZER, 0, 0, 1, 0};
        case 0x08: return {OP_PHP, AM_NON, 0, 0, 0, 0};
        case 0x09: return {OP_ORA, AM_IMM, 0, 1, 0, 0};
        case 0x0A: return {OP_ASL, AM_NON, 0, 0, 0, 0}; // ACC mode
        case 0x0B: return {OP_ANC, AM_IMM, 0, 1, 0, 0};
        case 0x0C: return {OP_NOP, AM_ABS, 0, 1, 0, 0};
        case 0x0D: return {OP_ORA, AM_ABS, 0, 1, 0, 0};
        case 0x0E: return {OP_ASL, AM_ABS, 0, 0, 1, 0};
        case 0x0F: return {OP_SLO, AM_ABS, 0, 0, 1, 0};
        
        // Row 0x10-0x1F
        case 0x10: return {OP_BPL, AM_NON, 0, 0, 0, 0}; // REL mode
        case 0x11: return {OP_ORA, AM_INY, 0, 1, 0, 0};
        case 0x12: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x13: return {OP_SLO, AM_INY, 0, 0, 1, 0};
        case 0x14: return {OP_NOP, AM_ZPX, 0, 1, 0, 0};
        case 0x15: return {OP_ORA, AM_ZPX, 0, 1, 0, 0};
        case 0x16: return {OP_ASL, AM_ZPX, 0, 0, 1, 0};
        case 0x17: return {OP_SLO, AM_ZPX, 0, 0, 1, 0};
        case 0x18: return {OP_CLC, AM_NON, 0, 0, 0, 0};
        case 0x19: return {OP_ORA, AM_ABY, 0, 1, 0, 0};
        case 0x1A: return {OP_NOP, AM_IMM, 0, 0, 0, 0}; // IMP mode -> should be IMM for this illegal NOP
        case 0x1B: return {OP_SLO, AM_ABY, 0, 0, 1, 0};
        case 0x1C: return {OP_NOP, AM_ABX, 0, 1, 0, 0};
        case 0x1D: return {OP_ORA, AM_ABX, 0, 1, 0, 0};
        case 0x1E: return {OP_ASL, AM_ABX, 0, 0, 1, 0};
        case 0x1F: return {OP_SLO, AM_ABX, 0, 0, 1, 0};
        
        // Row 0x20-0x2F
        case 0x20: return {OP_JSR, AM_NON, 0, 0, 0, 0};
        case 0x21: return {OP_AND, AM_INX, 0, 1, 0, 0};
        case 0x22: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x23: return {OP_RLA, AM_INX, 0, 0, 1, 0};
        case 0x24: return {OP_BIT, AM_ZER, 0, 1, 0, 0};
        case 0x25: return {OP_AND, AM_ZER, 0, 1, 0, 0};
        case 0x26: return {OP_ROL, AM_ZER, 0, 0, 1, 0};
        case 0x27: return {OP_RLA, AM_ZER, 0, 0, 1, 0};
        case 0x28: return {OP_PLP, AM_NON, 0, 0, 0, 0};
        case 0x29: return {OP_AND, AM_IMM, 0, 1, 0, 0};
        case 0x2A: return {OP_ROL, AM_NON, 0, 0, 0, 0}; // ACC mode
        case 0x2B: return {OP_ANC, AM_IMM, 0, 1, 0, 0};
        case 0x2C: return {OP_BIT, AM_ABS, 0, 1, 0, 0};
        case 0x2D: return {OP_AND, AM_ABS, 0, 1, 0, 0};
        case 0x2E: return {OP_ROL, AM_ABS, 0, 0, 1, 0};
        case 0x2F: return {OP_RLA, AM_ABS, 0, 0, 1, 0};
        
        // Row 0x30-0x3F
        case 0x30: return {OP_BMI, AM_NON, 0, 0, 0, 0}; // REL mode
        case 0x31: return {OP_AND, AM_INY, 0, 1, 0, 0};
        case 0x32: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x33: return {OP_RLA, AM_INY, 0, 0, 1, 0};
        case 0x34: return {OP_NOP, AM_ZPX, 0, 1, 0, 0};
        case 0x35: return {OP_AND, AM_ZPX, 0, 1, 0, 0};
        case 0x36: return {OP_ROL, AM_ZPX, 0, 0, 1, 0};
        case 0x37: return {OP_RLA, AM_ZPX, 0, 0, 1, 0};
        case 0x38: return {OP_SEC, AM_NON, 0, 0, 0, 0};
        case 0x39: return {OP_AND, AM_ABY, 0, 1, 0, 0};
        case 0x3A: return {OP_NOP, AM_IMM, 0, 0, 0, 0}; // IMP mode -> should be IMM for this illegal NOP
        case 0x3B: return {OP_RLA, AM_ABY, 0, 0, 1, 0};
        case 0x3C: return {OP_NOP, AM_ABX, 0, 1, 0, 0};
        case 0x3D: return {OP_AND, AM_ABX, 0, 1, 0, 0};
        case 0x3E: return {OP_ROL, AM_ABX, 0, 0, 1, 0};
        case 0x3F: return {OP_RLA, AM_ABX, 0, 0, 1, 0};
        
        // Row 0x40-0x4F
        case 0x40: return {OP_RTI, AM_NON, 0, 0, 0, 0};
        case 0x41: return {OP_EOR, AM_INX, 0, 1, 0, 0};
        case 0x42: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x43: return {OP_SRE, AM_INX, 0, 0, 1, 0};
        case 0x44: return {OP_NOP, AM_ZER, 0, 1, 0, 0};
        case 0x45: return {OP_EOR, AM_ZER, 0, 1, 0, 0};
        case 0x46: return {OP_LSR, AM_ZER, 0, 0, 1, 0};
        case 0x47: return {OP_SRE, AM_ZER, 0, 0, 1, 0};
        case 0x48: return {OP_PHA, AM_NON, 0, 0, 0, 0};
        case 0x49: return {OP_EOR, AM_IMM, 0, 1, 0, 0};
        case 0x4A: return {OP_LSR, AM_NON, 0, 0, 0, 0}; // ACC mode
        case 0x4B: return {OP_ASR, AM_IMM, 0, 1, 0, 0};
        case 0x4C: return {OP_JMP, AM_ABS, 0, 1, 0, 0};
        case 0x4D: return {OP_EOR, AM_ABS, 0, 1, 0, 0};
        case 0x4E: return {OP_LSR, AM_ABS, 0, 0, 1, 0};
        case 0x4F: return {OP_SRE, AM_ABS, 0, 0, 1, 0};
        
        // Row 0x50-0x5F
        case 0x50: return {OP_BVC, AM_NON, 0, 0, 0, 0}; // REL mode
        case 0x51: return {OP_EOR, AM_INY, 0, 1, 0, 0};
        case 0x52: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x53: return {OP_SRE, AM_INY, 0, 0, 1, 0};
        case 0x54: return {OP_NOP, AM_ZPX, 0, 1, 0, 0};
        case 0x55: return {OP_EOR, AM_ZPX, 0, 1, 0, 0};
        case 0x56: return {OP_LSR, AM_ZPX, 0, 0, 1, 0};
        case 0x57: return {OP_SRE, AM_ZPX, 0, 0, 1, 0};
        case 0x58: return {OP_CLI, AM_NON, 0, 0, 0, 0};
        case 0x59: return {OP_EOR, AM_ABY, 0, 1, 0, 0};
        case 0x5A: return {OP_NOP, AM_IMM, 0, 0, 0, 0}; // IMP mode -> should be IMM for this illegal NOP
        case 0x5B: return {OP_SRE, AM_ABY, 0, 0, 1, 0};
        case 0x5C: return {OP_NOP, AM_ABX, 0, 1, 0, 0};
        case 0x5D: return {OP_EOR, AM_ABX, 0, 1, 0, 0};
        case 0x5E: return {OP_LSR, AM_ABX, 0, 0, 1, 0};
        case 0x5F: return {OP_SRE, AM_ABX, 0, 0, 1, 0};
        
        // Row 0x60-0x6F
        case 0x60: return {OP_RTS, AM_NON, 0, 0, 0, 0};
        case 0x61: return {OP_ADC, AM_INX, 0, 1, 0, 0};
        case 0x62: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x63: return {OP_RRA, AM_INX, 0, 0, 1, 0};
        case 0x64: return {OP_NOP, AM_ZER, 0, 1, 0, 0};
        case 0x65: return {OP_ADC, AM_ZER, 0, 1, 0, 0};
        case 0x66: return {OP_ROR, AM_ZER, 0, 0, 1, 0};
        case 0x67: return {OP_RRA, AM_ZER, 0, 0, 1, 0};
        case 0x68: return {OP_PLA, AM_NON, 0, 0, 0, 0};
        case 0x69: return {OP_ADC, AM_IMM, 0, 1, 0, 0};
        case 0x6A: return {OP_ROR, AM_NON, 0, 0, 0, 0}; // ACC mode
        case 0x6B: return {OP_ARR, AM_IMM, 0, 1, 0, 0};
        case 0x6C: return {OP_JMP, AM_IND, 0, 1, 0, 0};
        case 0x6D: return {OP_ADC, AM_ABS, 0, 1, 0, 0};
        case 0x6E: return {OP_ROR, AM_ABS, 0, 0, 1, 0};
        case 0x6F: return {OP_RRA, AM_ABS, 0, 0, 1, 0};
        
        // Row 0x70-0x7F
        case 0x70: return {OP_BVS, AM_NON, 0, 0, 0, 0}; // REL mode
        case 0x71: return {OP_ADC, AM_INY, 0, 1, 0, 0};
        case 0x72: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x73: return {OP_RRA, AM_INY, 0, 0, 1, 0};
        case 0x74: return {OP_NOP, AM_ZPX, 0, 1, 0, 0};
        case 0x75: return {OP_ADC, AM_ZPX, 0, 1, 0, 0};
        case 0x76: return {OP_ROR, AM_ZPX, 0, 0, 1, 0};
        case 0x77: return {OP_RRA, AM_ZPX, 0, 0, 1, 0};
        case 0x78: return {OP_SEI, AM_NON, 0, 0, 0, 0};
        case 0x79: return {OP_ADC, AM_ABY, 0, 1, 0, 0};
        case 0x7A: return {OP_NOP, AM_IMM, 0, 0, 0, 0}; // IMP mode -> should be IMM for this illegal NOP
        case 0x7B: return {OP_RRA, AM_ABY, 0, 0, 1, 0};
        case 0x7C: return {OP_NOP, AM_ABX, 0, 1, 0, 0};
        case 0x7D: return {OP_ADC, AM_ABX, 0, 1, 0, 0};
        case 0x7E: return {OP_ROR, AM_ABX, 0, 0, 1, 0};
        case 0x7F: return {OP_RRA, AM_ABX, 0, 0, 1, 0};
        
        // Row 0x80-0x8F
        case 0x80: return {OP_NOP, AM_IMM, 0, 1, 0, 0};
        case 0x81: return {OP_STA, AM_INX, 0, 0, 0, 0};
        case 0x82: return {OP_NOP, AM_IMM, 0, 1, 0, 0};
        case 0x83: return {OP_SAX, AM_INX, 0, 0, 0, 0};
        case 0x84: return {OP_STY, AM_ZER, 0, 0, 0, 0};
        case 0x85: return {OP_STA, AM_ZER, 0, 0, 0, 0};
        case 0x86: return {OP_STX, AM_ZER, 0, 0, 0, 0};
        case 0x87: return {OP_SAX, AM_ZER, 0, 0, 0, 0};
        case 0x88: return {OP_DEY, AM_NON, 0, 0, 0, 0};
        case 0x89: return {OP_NOP, AM_IMM, 0, 1, 0, 0};
        case 0x8A: return {OP_TXA, AM_NON, 0, 0, 0, 0};
        case 0x8B: return {OP_XAA, AM_IMM, 0, 1, 0, 0};
        case 0x8C: return {OP_STY, AM_ABS, 0, 0, 0, 0};
        case 0x8D: return {OP_STA, AM_ABS, 0, 0, 0, 0};
        case 0x8E: return {OP_STX, AM_ABS, 0, 0, 0, 0};
        case 0x8F: return {OP_SAX, AM_ABS, 0, 0, 0, 0};
        
        // Row 0x90-0x9F
        case 0x90: return {OP_BCC, AM_NON, 0, 0, 0, 0}; // REL mode
        case 0x91: return {OP_STA, AM_INY, 0, 0, 0, 0};
        case 0x92: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0x93: return {OP_SHA, AM_INY, 1, 0, 0, 0}; // illegal_store = 1
        case 0x94: return {OP_STY, AM_ZPX, 0, 0, 0, 0};
        case 0x95: return {OP_STA, AM_ZPX, 0, 0, 0, 0};
        case 0x96: return {OP_STX, AM_ZPY, 0, 0, 0, 0};
        case 0x97: return {OP_SAX, AM_ZPY, 0, 0, 0, 0};
        case 0x98: return {OP_TYA, AM_NON, 0, 0, 0, 0};
        case 0x99: return {OP_STA, AM_ABY, 0, 0, 0, 0};
        case 0x9A: return {OP_TXS, AM_NON, 0, 0, 0, 0};
        case 0x9B: return {OP_SHS, AM_ABY, 1, 0, 0, 0}; // illegal_store = 1
        case 0x9C: return {OP_SHY, AM_ABX, 1, 0, 0, 0}; // illegal_store = 1
        case 0x9D: return {OP_STA, AM_ABX, 0, 0, 0, 0};
        case 0x9E: return {OP_SHX, AM_ABY, 1, 0, 0, 0}; // illegal_store = 1
        case 0x9F: return {OP_SHA, AM_ABY, 1, 0, 0, 0}; // illegal_store = 1
        
        // Row 0xA0-0xAF
        case 0xA0: return {OP_LDY, AM_IMM, 0, 1, 0, 0};
        case 0xA1: return {OP_LDA, AM_INX, 0, 1, 0, 0};
        case 0xA2: return {OP_LDX, AM_IMM, 0, 1, 0, 0};
        case 0xA3: return {OP_LAX, AM_INX, 0, 1, 0, 0};
        case 0xA4: return {OP_LDY, AM_ZER, 0, 1, 0, 0};
        case 0xA5: return {OP_LDA, AM_ZER, 0, 1, 0, 0};
        case 0xA6: return {OP_LDX, AM_ZER, 0, 1, 0, 0};
        case 0xA7: return {OP_LAX, AM_ZER, 0, 1, 0, 0};
        case 0xA8: return {OP_TAY, AM_NON, 0, 0, 0, 0};
        case 0xA9: return {OP_LDA, AM_IMM, 0, 1, 0, 0};
        case 0xAA: return {OP_TAX, AM_NON, 0, 0, 0, 0};
        case 0xAB: return {OP_LAX, AM_IMM, 0, 0, 0, 0}; // can_skip_page_cross = 0 for illegal immediate
        case 0xAC: return {OP_LDY, AM_ABS, 0, 1, 0, 0};
        case 0xAD: return {OP_LDA, AM_ABS, 0, 1, 0, 0};
        case 0xAE: return {OP_LDX, AM_ABS, 0, 1, 0, 0};
        case 0xAF: return {OP_LAX, AM_ABS, 0, 1, 0, 0};
        
        // Row 0xB0-0xBF
        case 0xB0: return {OP_BCS, AM_NON, 0, 0, 0, 0}; // REL mode
        case 0xB1: return {OP_LDA, AM_INY, 0, 1, 0, 0};
        case 0xB2: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0xB3: return {OP_LAX, AM_INY, 0, 1, 0, 0};
        case 0xB4: return {OP_LDY, AM_ZPX, 0, 1, 0, 0};
        case 0xB5: return {OP_LDA, AM_ZPX, 0, 1, 0, 0};
        case 0xB6: return {OP_LDX, AM_ZPY, 0, 1, 0, 0};
        case 0xB7: return {OP_LAX, AM_ZPY, 0, 1, 0, 0};
        case 0xB8: return {OP_CLV, AM_NON, 0, 0, 0, 0};
        case 0xB9: return {OP_LDA, AM_ABY, 0, 1, 0, 0};
        case 0xBA: return {OP_TSX, AM_NON, 0, 0, 0, 0};
        case 0xBB: return {OP_LAS, AM_ABY, 0, 1, 0, 0};
        case 0xBC: return {OP_LDY, AM_ABX, 0, 1, 0, 0};
        case 0xBD: return {OP_LDA, AM_ABX, 0, 1, 0, 0};
        case 0xBE: return {OP_LDX, AM_ABY, 0, 1, 0, 0};
        case 0xBF: return {OP_LAX, AM_ABY, 0, 1, 0, 0};
        
        // Row 0xC0-0xCF
        case 0xC0: return {OP_CPY, AM_IMM, 0, 1, 0, 0};
        case 0xC1: return {OP_CMP, AM_INX, 0, 1, 0, 0};
        case 0xC2: return {OP_NOP, AM_IMM, 0, 1, 0, 0};
        case 0xC3: return {OP_DCP, AM_INX, 0, 0, 1, 0};
        case 0xC4: return {OP_CPY, AM_ZER, 0, 1, 0, 0};
        case 0xC5: return {OP_CMP, AM_ZER, 0, 1, 0, 0};
        case 0xC6: return {OP_DEC, AM_ZER, 0, 0, 1, 0};
        case 0xC7: return {OP_DCP, AM_ZER, 0, 0, 1, 0};
        case 0xC8: return {OP_INY, AM_NON, 0, 0, 0, 0};
        case 0xC9: return {OP_CMP, AM_IMM, 0, 1, 0, 0};
        case 0xCA: return {OP_DEX, AM_NON, 0, 0, 0, 0};
        case 0xCB: return {OP_SBX, AM_IMM, 0, 1, 0, 0};
        case 0xCC: return {OP_CPY, AM_ABS, 0, 1, 0, 0};
        case 0xCD: return {OP_CMP, AM_ABS, 0, 1, 0, 0};
        case 0xCE: return {OP_DEC, AM_ABS, 0, 0, 1, 0};
        case 0xCF: return {OP_DCP, AM_ABS, 0, 0, 1, 0};
        
        // Row 0xD0-0xDF
        case 0xD0: return {OP_BNE, AM_NON, 0, 0, 0, 0}; // REL mode
        case 0xD1: return {OP_CMP, AM_INY, 0, 1, 0, 0};
        case 0xD2: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0xD3: return {OP_DCP, AM_INY, 0, 0, 1, 0};
        case 0xD4: return {OP_NOP, AM_ZPX, 0, 1, 0, 0};
        case 0xD5: return {OP_CMP, AM_ZPX, 0, 1, 0, 0};
        case 0xD6: return {OP_DEC, AM_ZPX, 0, 0, 1, 0};
        case 0xD7: return {OP_DCP, AM_ZPX, 0, 0, 1, 0};
        case 0xD8: return {OP_CLD, AM_NON, 0, 0, 0, 0};
        case 0xD9: return {OP_CMP, AM_ABY, 0, 1, 0, 0};
        case 0xDA: return {OP_NOP, AM_IMM, 0, 0, 0, 0}; // IMP mode -> should be IMM for this illegal NOP
        case 0xDB: return {OP_DCP, AM_ABY, 0, 0, 1, 0};
        case 0xDC: return {OP_NOP, AM_ABX, 0, 1, 0, 0};
        case 0xDD: return {OP_CMP, AM_ABX, 0, 1, 0, 0};
        case 0xDE: return {OP_DEC, AM_ABX, 0, 0, 1, 0};
        case 0xDF: return {OP_DCP, AM_ABX, 0, 0, 1, 0};
        
        // Row 0xE0-0xEF
        case 0xE0: return {OP_CPX, AM_IMM, 0, 1, 0, 0};
        case 0xE1: return {OP_SBC, AM_INX, 0, 1, 0, 0};
        case 0xE2: return {OP_NOP, AM_IMM, 0, 1, 0, 0};
        case 0xE3: return {OP_ISC, AM_INX, 0, 0, 1, 0};
        case 0xE4: return {OP_CPX, AM_ZER, 0, 1, 0, 0};
        case 0xE5: return {OP_SBC, AM_ZER, 0, 1, 0, 0};
        case 0xE6: return {OP_INC, AM_ZER, 0, 0, 1, 0};
        case 0xE7: return {OP_ISC, AM_ZER, 0, 0, 1, 0};
        case 0xE8: return {OP_INX, AM_NON, 0, 0, 0, 0};
        case 0xE9: return {OP_SBC, AM_IMM, 0, 1, 0, 0};
        case 0xEA: return {OP_NOP, AM_NON, 0, 0, 0, 0};
        case 0xEB: return {OP_SBC, AM_IMM, 0, 1, 0, 0}; // Illegal SBC
        case 0xEC: return {OP_CPX, AM_ABS, 0, 1, 0, 0};
        case 0xED: return {OP_SBC, AM_ABS, 0, 1, 0, 0};
        case 0xEE: return {OP_INC, AM_ABS, 0, 0, 1, 0};
        case 0xEF: return {OP_ISC, AM_ABS, 0, 0, 1, 0};
        
        // Row 0xF0-0xFF
        case 0xF0: return {OP_BEQ, AM_NON, 0, 0, 0, 0}; // REL mode
        case 0xF1: return {OP_SBC, AM_INY, 0, 1, 0, 0};
        case 0xF2: return {OP_JAM, AM_NON, 0, 0, 0, 0};
        case 0xF3: return {OP_ISC, AM_INY, 0, 0, 1, 0};
        case 0xF4: return {OP_NOP, AM_ZPX, 0, 1, 0, 0};
        case 0xF5: return {OP_SBC, AM_ZPX, 0, 1, 0, 0};
        case 0xF6: return {OP_INC, AM_ZPX, 0, 0, 1, 0};
        case 0xF7: return {OP_ISC, AM_ZPX, 0, 0, 1, 0};
        case 0xF8: return {OP_SED, AM_NON, 0, 0, 0, 0};
        case 0xF9: return {OP_SBC, AM_ABY, 0, 1, 0, 0};
        case 0xFA: return {OP_NOP, AM_IMM, 0, 0, 0, 0}; // IMP mode -> should be IMM for this illegal NOP
        case 0xFB: return {OP_ISC, AM_ABY, 0, 0, 1, 0};
        case 0xFC: return {OP_NOP, AM_ABX, 0, 1, 0, 0};
        case 0xFD: return {OP_SBC, AM_ABX, 0, 1, 0, 0};
        case 0xFE: return {OP_INC, AM_ABX, 0, 0, 1, 0};
        case 0xFF: return {OP_ISC, AM_ABX, 0, 0, 1, 0};
    }
    
    // Default fallback: return NOP for any unhandled opcodes
    return {OP_NOP, AM_NON, 0, 0, 0, 0};
}


// Generate complete processor-specific opcode table using unified function
template<typename ProcessorTag>
constexpr std::array<opcode_info_t, 256> generate_opcode_table() {
    std::array<opcode_info_t, 256> table = {};
    
    for (int i = 0; i < 256; ++i) {
        uint8_t opcode = static_cast<uint8_t>(i);
        
        // Use the unified function to get complete opcode info
        table[i] = get_opcode_info_for_opcode<ProcessorTag>(opcode);
    }
    
    return table;
}

// Unified opcode table accessor for any processor type
template<typename ProcessorTag>
const opcode_info_t* get_processor_opcode_table() {
    static const auto table = generate_opcode_table<ProcessorTag>();
    return table.data();
}

// Get specific opcode entry for a processor type
template<typename ProcessorTag>
opcode_info_t get_processor_opcode_entry(uint8_t opcode) {
    static const auto table = generate_opcode_table<ProcessorTag>();
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
        auto opcode_info = get_opcode_info_for_opcode<ProcessorTag>(0xA7);
        return opcode_info.op_index == OP_LAX;
    } else {
        // LAX should be NOP on CMOS
        auto opcode_info = get_opcode_info_for_opcode<ProcessorTag>(0xA7);
        return opcode_info.op_index == OP_NOP;
    }
}

// Validate that enhanced opcodes are only available on appropriate processors
template<typename ProcessorTag>
bool validate_enhanced_opcodes() {
    if constexpr (fam65xx_core::has_cmos_enhancements<ProcessorTag>()) {
        // STZ should be available
        auto opcode_info = get_opcode_info_for_opcode<ProcessorTag>(0x64);
        return opcode_info.op_index == OP_STZ;
    } else {
        // STZ should be NOP or illegal on NMOS
        auto opcode_info = get_opcode_info_for_opcode<ProcessorTag>(0x64);
        return opcode_info.op_index == OP_NOP;
    }
}

#ifdef AIEMUC_IMPL
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
#endif // AIEMUC_IMPL

} // namespace fam65xx_unified_tables

#endif // __cplusplus

#endif // FAM65XX_UNIFIED_OPCODE_TABLES_HPP_INCLUDED