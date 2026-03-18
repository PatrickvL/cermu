#pragma once
/*
 * m680x0_decoder.hpp — Motorola 680x0 Disassembler (Standalone Library)
 *
 * Pure C-compatible free-function API.  No dependencies on chip or system
 * headers — only <cstdint>.  Links as a static library so debug UIs and
 * test runners can use it without pulling in the full emulator.
 *
 * All functions take a pointer to the instruction bytes at the given PC.
 * The memory pointer must provide at least 10 bytes (maximum 68000
 * instruction length including all extension words).
 */

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/// Disassemble a 680x0 instruction at `pc`.
/// Writes mnemonic + operands into `buffer` (max `buf_size` chars).
/// `memory` points to the raw bytes at `pc` (big-endian, >= 10 bytes).
/// Returns the instruction length in bytes (always even, 2..10).
int m68k_disassemble(uint32_t pc, const uint8_t* memory, char* buffer, int buf_size);

/// Disassemble with monitor format: "ADDR  HEXWORDS  MNEMONIC OPERANDS"
/// Returns the instruction length in bytes.
int m68k_disassemble_monitor(uint32_t pc, const uint8_t* memory, char* buffer, int buf_size);

/// Return the instruction length in bytes without disassembling.
/// Returns 2..10 (always even).
int m68k_instruction_length(uint32_t pc, const uint8_t* memory);

#ifdef __cplusplus
}
#endif
