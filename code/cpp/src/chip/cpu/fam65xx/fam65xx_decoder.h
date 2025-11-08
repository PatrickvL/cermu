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

#ifdef __cplusplus
}
#endif

#endif // FAM65XX_DECODER_H