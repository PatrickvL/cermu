/*
 * fam65xx_decoder.c - FAM65XX CPU instruction decoder and disassembler
 *
 * This file implements instruction decoding and disassembly functions for the
 * FAM65XX CPU family, supporting all variants from 6502 to 65C816.
 */

#include "fam65xx_decoder.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// OPERATION NAME LOOKUP TABLE
// ============================================================================

static const char *op_names[] = {
    // Core 6502 operations (0-56)
    "LDA", "LDX", "LDY",                      // 0-2: Load operations
    "STA", "STX", "STY",                      // 3-5: Store operations
    "ADC", "SBC",                             // 6-7: Arithmetic
    "AND", "ORA", "EOR",                      // 8-10: Logic operations
    "CMP", "CPX", "CPY",                      // 11-13: Compare operations
    "ASL", "LSR", "ROL", "ROR",               // 14-17: Shift/rotate
    "INC", "DEC",                             // 18-19: Increment/decrement
    "INX", "INY", "DEX", "DEY",               // 20-23: Register inc/dec
    "TAX", "TAY", "TXA", "TYA", "TSX", "TXS", // 24-29: Transfer operations
    "PHA", "PHP", "PLA", "PLP",               // 30-33: Stack operations
    "BCC", "BCS", "BEQ", "BNE", "BMI", "BPL", "BVC", "BVS", // 34-41: Branches
    "CLC", "SEC", "CLI", "SEI", "CLD", "SED", "CLV", // 42-48: Flag operations
    "JMP", "JSR", "RTS", "RTI", "BRK",               // 49-53: Control flow
    "BIT", "NOP", "JAM",                             // 54-56: Test/misc

    // Illegal opcodes (57-74)
    "LAX", "SAX", "DCP", "ISC", "SLO", "RLA", "SRE", "RRA", // 57-64: Combo ops
    "ANC", "ASR", "ARR", "SBX",        // 65-68: Special accumulator
    "SHA", "SHS", "SHX", "SHY", "LAS", // 69-73: Store with AND
    "XAA",                             // 74: Special operation

    // 65C02 enhancements (75-84)
    "BRA", "STZ", "TRB", "TSB", "PHX", "PHY", "PLX", "PLY", "WAI",
    "STP", // 75-84

    // Rockwell 65C02 bit manipulation (85-116)
    "RMB0", "RMB1", "RMB2", "RMB3", "RMB4", "RMB5", "RMB6", "RMB7", "SMB0",
    "SMB1", "SMB2", "SMB3", "SMB4", "SMB5", "SMB6", "SMB7", "BBR0", "BBR1",
    "BBR2", "BBR3", "BBR4", "BBR5", "BBR6", "BBR7", "BBS0", "BBS1", "BBS2",
    "BBS3", "BBS4", "BBS5", "BBS6", "BBS7",

    // 65C816 16-bit operations (117+)
    "REP", "SEP", "XBA", "XCE", "COP", "WDM", "PEA", "PER", "PEI", "PHB", "PHD",
    "PHK", "PLB", "PLD", "RTL", "JSL", "JML", "MVN", "MVP"};

// ============================================================================
// ADDRESSING MODE NAME LOOKUP TABLE
// ============================================================================

static const char *am_names[] = {
    "Implicit",                  // 0
    "Accumulator",               // 1
    "Immediate",                 // 2
    "Zero Page",                 // 3
    "Zero Page,X",               // 4
    "Zero Page,Y",               // 5
    "Absolute",                  // 6
    "Absolute,X",                // 7
    "Absolute,Y",                // 8
    "Indirect",                  // 9
    "Indexed Indirect",          // 10 (zp,X)
    "Indirect Indexed",          // 11 (zp),Y
    "Relative",                  // 12
    "ZP Indirect",               // 13 (zp) - 65C02
    "Absolute Indexed Indirect", // 14 (abs,X) - 65C02/65C816
    "Stack Relative",            // 15 - 65C816
};

// ============================================================================
// OPCODE LOOKUP TABLE (simplified version for length calculation)
// ============================================================================

// Instruction lengths for each opcode (1, 2, or 3 bytes)
static const uint8_t instruction_lengths[256] = {
    // 0x00-0x0F
    2, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
    // 0x10-0x1F
    2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
    // 0x20-0x2F
    3, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
    // 0x30-0x3F
    2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
    // 0x40-0x4F
    1, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
    // 0x50-0x5F
    2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
    // 0x60-0x6F
    1, 2, 1, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
    // 0x70-0x7F
    2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
    // 0x80-0x8F
    2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
    // 0x90-0x9F
    2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
    // 0xA0-0xAF
    2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
    // 0xB0-0xBF
    2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
    // 0xC0-0xCF
    2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
    // 0xD0-0xDF
    2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3,
    // 0xE0-0xEF
    2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 2, 3, 3, 3, 3,
    // 0xF0-0xFF
    2, 2, 1, 2, 2, 2, 2, 2, 1, 3, 1, 3, 3, 3, 3, 3};

// Legal opcodes bitmap (256 bits = 32 bytes)
static const uint8_t legal_opcodes[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, // 0x00-0x3F (all legal)
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, // 0x40-0x7F (all legal)
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, // 0x80-0xBF (all legal)
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF // 0xC0-0xFF (all legal)
    // Note: This is simplified - in reality many opcodes are illegal on 6502
};

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

const char *fam65xx_get_opcode_name(uint8_t op_index) {
  if (op_index < sizeof(op_names) / sizeof(op_names[0])) {
    return op_names[op_index];
  }
  return "???";
}

const char *fam65xx_get_addressing_mode_name(uint8_t am_index) {
  if (am_index < sizeof(am_names) / sizeof(am_names[0])) {
    return am_names[am_index];
  }
  return "Unknown";
}

// ============================================================================
// COMPACT DISASSEMBLER IMPLEMENTATION
// Based on opcode_entry structure - uses switch on am_index for clarity
// ============================================================================

int fam65xx_disassemble_instruction(uint16_t pc, opcode_info_t entry, uint8_t operand1, uint8_t operand2, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 32) {
        return 0;
    }
    
    uint8_t op_index = entry.op_index;
    uint8_t am_index = entry.am_index;
    const char* mnemonic = fam65xx_get_opcode_name(op_index);
    
    // Start with mnemonic
    int pos = snprintf(buffer, buffer_size, "%s", mnemonic);
    
    // Format operand based on addressing mode
    
    switch (am_index) {
        case 0: // NON - Implicit/Accumulator/Relative
            // Check if it's an accumulator operation
            if (op_index == 14 || op_index == 15 || op_index == 16 || op_index == 17) {
                // ASL, LSR, ROL, ROR in accumulator mode
                pos += snprintf(buffer + pos, buffer_size - pos, " A");
            }
            // Check if it's a branch instruction
            else if (op_index >= 34 && op_index <= 41) {
                // Branch instruction - relative addressing
                int8_t offset = (int8_t)operand1;
                uint16_t target = pc + 2 + offset;
                pos += snprintf(buffer + pos, buffer_size - pos, " $%04X", target);
            }
            break;
            
        case 1: // IMM - Immediate
            pos += snprintf(buffer + pos, buffer_size - pos, " #$%02X", operand1);
            break;
            
        case 2: // ZER - Zero Page
            pos += snprintf(buffer + pos, buffer_size - pos, " $%02X", operand1);
            break;
            
        case 3: // ZPX - Zero Page,X
            pos += snprintf(buffer + pos, buffer_size - pos, " $%02X,X", operand1);
            break;
            
        case 4: // ZPY - Zero Page,Y
            pos += snprintf(buffer + pos, buffer_size - pos, " $%02X,Y", operand1);
            break;
            
        case 5: // ABS - Absolute
            {
                uint16_t addr = operand1 | (operand2 << 8);
                pos += snprintf(buffer + pos, buffer_size - pos, " $%04X", addr);
            }
            break;
            
        case 6: // ABX - Absolute,X
            {
                uint16_t addr = operand1 | (operand2 << 8);
                pos += snprintf(buffer + pos, buffer_size - pos, " $%04X,X", addr);
            }
            break;
            
        case 7: // ABY - Absolute,Y
            {
                uint16_t addr = operand1 | (operand2 << 8);
                pos += snprintf(buffer + pos, buffer_size - pos, " $%04X,Y", addr);
            }
            break;
            
        case 8: // IND - Indirect
            {
                uint16_t addr = operand1 | (operand2 << 8);
                pos += snprintf(buffer + pos, buffer_size - pos, " ($%04X)", addr);
            }
            break;
            
        case 9: // INX - Indexed Indirect (zp,X)
            pos += snprintf(buffer + pos, buffer_size - pos, " ($%02X,X)", operand1);
            break;
            
        case 10: // INY - Indirect Indexed (zp),Y
            pos += snprintf(buffer + pos, buffer_size - pos, " ($%02X),Y", operand1);
            break;
            
        case 11: // ZPR - Zero Page Relative (Rockwell 65C02)
            {
                uint16_t addr = operand1 | (operand2 << 8);
                pos += snprintf(buffer + pos, buffer_size - pos, " $%02X,$%04X", operand1, addr);
            }
            break;
            
        case 12: // ZPI - Zero Page Indirect (65C02)
            pos += snprintf(buffer + pos, buffer_size - pos, " ($%02X)", operand1);
            break;
            
        default:
            // Unknown addressing mode
            pos += snprintf(buffer + pos, buffer_size - pos, " ???");
            break;
    }
    
    // Return number of characters written (like snprintf)
    return pos;
}

int fam65xx_disassemble_vice_format(uint16_t pc, opcode_info_t entry, uint8_t opcode, uint8_t operand1, uint8_t operand2, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 64) {
        return 0;
    }
    
    // Get instruction length
    uint8_t length = instruction_lengths[opcode];
    
    // Disassemble instruction
    char disasm[32];
    fam65xx_disassemble_instruction(pc, entry, operand1, operand2, disasm, sizeof(disasm));
    
    // Format as VICE: ".,ADDR BYTES DISASM"
    int pos = snprintf(buffer, buffer_size, ".,");
    pos += snprintf(buffer + pos, buffer_size - pos, "%04X ", pc);
    
    // Print hex bytes (padded to 9 chars for alignment)
    if (length == 1) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%02X       ", opcode);
    } else if (length == 2) {
        pos += snprintf(buffer + pos, buffer_size - pos, "%02X %02X    ", opcode, operand1);
    } else {
        pos += snprintf(buffer + pos, buffer_size - pos, "%02X %02X %02X ", opcode, operand1, operand2);
    }
    
    pos += snprintf(buffer + pos, buffer_size - pos, "%s", disasm);
    
    return pos;
}
