/*
 * fam65xx_decoder.h - FAM65XX CPU instruction decoder and disassembler
 * 
 * This file contains instruction decoding and disassembly functions for the
 * FAM65XX CPU family, supporting all variants from 6502 to 65C816.
 */

#ifndef FAM65XX_DECODER_H
#define FAM65XX_DECODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// INSTRUCTION DECODING FUNCTIONS
// ============================================================================

/**
 * Get the name of an operation based on its operation index
 * @param op_index Operation index from the opcode entry
 * @return String name of the operation (e.g., "LDA", "STA", etc.)
 */
const char* fam65xx_get_opcode_name(uint8_t op_index);

/**
 * Get the name of an addressing mode based on its addressing mode index
 * @param am_index Addressing mode index from the opcode entry
 * @return String name of the addressing mode (e.g., "Immediate", "Absolute", etc.)
 */
const char* fam65xx_get_addressing_mode_name(uint8_t am_index);

/**
 * Format a complete instruction disassembly
 * @param opcode The raw opcode byte
 * @param operand1 First operand byte (if applicable)
 * @param operand2 Second operand byte (if applicable)
 * @param pc Program counter value for relative addressing
 * @param buffer Output buffer for the formatted instruction
 * @param buffer_size Size of the output buffer
 * @return Length of the formatted string, or -1 on error
 */
int fam65xx_disassemble_instruction(uint8_t opcode, uint8_t operand1, uint8_t operand2, 
                                   uint16_t pc, char* buffer, size_t buffer_size);

/**
 * Get the instruction length in bytes for a given opcode
 * @param opcode The opcode to analyze
 * @return Instruction length (1, 2, or 3 bytes)
 */
int fam65xx_get_instruction_length(uint8_t opcode);

/**
 * Check if an opcode is a legal 6502 instruction
 * @param opcode The opcode to check
 * @return true if legal, false if illegal/undocumented
 */
bool fam65xx_is_legal_opcode(uint8_t opcode);

#ifdef __cplusplus
}
#endif

#endif // FAM65XX_DECODER_H