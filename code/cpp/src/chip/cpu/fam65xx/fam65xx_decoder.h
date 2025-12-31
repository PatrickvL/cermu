/*
 * fam65xx_decoder.h - FAM65XX CPU instruction decoder and disassembler
 *
 * This file contains instruction decoding and disassembly functions for the
 * FAM65XX CPU family, supporting all variants from 6502 to 65C816.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Always include types when in C++ mode (templates need C++ linkage)
#ifdef __cplusplus
#include "fam65xx_types.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// For C compatibility, forward declare the structure
#ifndef __cplusplus
struct opcode_info_t;
typedef struct opcode_info_t opcode_info_t;
#endif

// ============================================================================
// INSTRUCTION DECODING FUNCTIONS
// ============================================================================

/**
 * Disassemble a single instruction using opcode entry and operands
 * @param pc Program counter (address to disassemble)
 * @param entry Opcode table entry containing op_index and am_index
 * @param operand1 First operand byte (if any)
 * @param operand2 Second operand byte (if any)
 * @param buffer Output buffer for disassembled instruction (minimum 32 bytes recommended)
 * @param buffer_size Size of output buffer
 * @return Number of characters written to buffer (excluding null terminator), like snprintf
 */
int fam65xx_disassemble_instruction(uint16_t pc, opcode_info_t entry, uint8_t operand1, uint8_t operand2, char* buffer, size_t buffer_size);

/**
 * Disassemble instruction in VICE monitor format with hex bytes
 * Format: ".,ADDR HEXBYTES DISASM"
 * Example: ".,FD70 B1 C1    LDA ($C1),Y"
 * @param pc Program counter (address to disassemble)
 * @param entry Opcode table entry
 * @param opcode The opcode byte
 * @param operand1 First operand byte (if any)
 * @param operand2 Second operand byte (if any)
 * @param buffer Output buffer (minimum 64 bytes recommended)
 * @param buffer_size Size of output buffer
 * @return Number of characters written to buffer (excluding null terminator)
 */
int fam65xx_disassemble_vice_format(uint16_t pc, opcode_info_t entry, uint8_t opcode, uint8_t operand1, uint8_t operand2, char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
