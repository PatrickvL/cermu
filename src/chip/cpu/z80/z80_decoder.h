#pragma once
/*
 * z80_decoder.h — Z80 CPU Instruction Decoder and Disassembler
 *
 * Decodes and disassembles Z80 family instructions including all prefix
 * combinations (CB, ED, DD/FD, DD CB/FD CB).
 *
 * Follows the fam65xx_decoder pattern: C-compatible functions with
 * snprintf-style return values.
 */

#include <cstddef>
#include <cstdint>

// ============================================================================
// Z80 INSTRUCTION DECODING
// ============================================================================

/**
 * Disassemble a single Z80 instruction.
 * Reads bytes from 'memory' starting at offset 0 (the byte at address 'pc').
 * Handles all prefix combinations (CB, ED, DD, FD, DD CB, FD CB).
 *
 * @param pc       Program counter (for branch target display)
 * @param memory   Pointer to at least 4 bytes starting at pc
 * @param buffer   Output buffer (minimum 48 bytes recommended)
 * @param buf_size Size of output buffer
 * @return         Instruction length in bytes (1-4)
 */
int z80_disassemble(uint16_t pc, const uint8_t* memory, char* buffer, size_t buf_size);

/**
 * Disassemble instruction with address and hex bytes (monitor format).
 * Format: "ADDR  HEXBYTES  MNEMONIC OPERANDS"
 * Example: "0100  DD CB 05 C6  SET 0,(IX+05)"
 *
 * @param pc       Program counter
 * @param memory   Pointer to at least 4 bytes starting at pc
 * @param buffer   Output buffer (minimum 64 bytes recommended)
 * @param buf_size Size of output buffer
 * @return         Instruction length in bytes (1-4)
 */
int z80_disassemble_monitor(uint16_t pc, const uint8_t* memory, char* buffer, size_t buf_size);

/**
 * Get the length of a Z80 instruction in bytes.
 * Does not produce disassembly output.
 *
 * @param memory   Pointer to at least 4 bytes at instruction start
 * @return         Instruction length in bytes (1-4)
 */
int z80_instruction_length(const uint8_t* memory);
