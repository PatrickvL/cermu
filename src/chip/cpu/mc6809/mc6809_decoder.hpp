#pragma once
/*
 * mc6809_decoder.hpp — MC6809 Disassembler / Assembler
 *
 * Standalone decoder for the Motorola 6809 instruction set, including
 * page 2 ($10) and page 3 ($11) prefix opcodes, and all indexed
 * addressing modes.  Follows the Z80/M680x0 decoder API convention.
 *
 * HD6309 extensions are decoded when present but always recognized
 * (illegal-opcode display falls back to "???").
 */

#include <cstdint>
#include <cstddef>

// ============================================================================
// DISASSEMBLER
// ============================================================================

/// Disassemble one MC6809 instruction at `pc` from `memory`.
/// Writes the mnemonic + operands into `buffer`.
/// Returns: instruction length in bytes (1–5).
int mc6809_disassemble(uint16_t pc, const uint8_t* memory,
                       char* buffer, size_t buf_size);

/// Monitor-format disassembly: "XXXX HH HH HH ... MNEMONIC OPERANDS"
/// Returns: instruction length in bytes.
int mc6809_disassemble_monitor(uint16_t pc, const uint8_t* memory,
                               char* buffer, size_t buf_size);

/// Return instruction length without producing output.
/// Returns: instruction length in bytes (1–5).
int mc6809_instruction_length(const uint8_t* memory);

// ============================================================================
// ASSEMBLER
// ============================================================================

/// Assemble one MC6809 instruction from a mnemonic string.
/// Writes machine code bytes into `out` (must be at least 5 bytes).
/// `pc` is the current program counter (needed for relative branches).
/// Returns: number of bytes written, or -1 on error.
int mc6809_assemble(const char* text, uint16_t pc,
                    uint8_t* out, size_t out_size);
