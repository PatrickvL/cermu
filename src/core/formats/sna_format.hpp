#pragma once

/**
 * SNA Format Handler — ZX Spectrum Snapshot Files
 *
 * The .sna format is the most widely supported Spectrum snapshot format.
 * It stores a complete machine state: Z80 registers + 48KB RAM dump.
 *
 * 48K format: 27-byte header + 49152 bytes RAM = 49179 bytes total
 * 128K format: 49179 bytes + 4-byte extension + 5×16384 RAM banks
 *
 * The PC is not stored in the header; instead it is pushed onto the stack
 * (emulating the Mirage Microdriver's behavior).  The loader must pop PC
 * from SP and simulate a RETN.
 *
 * Header layout (27 bytes):
 *   $00  I register
 *   $01  HL' (word)
 *   $03  DE' (word)
 *   $05  BC' (word)
 *   $07  AF' (word)
 *   $09  HL  (word)
 *   $0B  DE  (word)
 *   $0D  BC  (word)
 *   $0F  IY  (word)
 *   $11  IX  (word)
 *   $13  IFF2 (bit 2: 1=EI, 0=DI)
 *   $14  R register
 *   $15  AF  (word)
 *   $17  SP  (word)
 *   $19  Interrupt mode (0/1/2)
 *   $1A  Border color (0-7)
 *   $1B  48KB RAM dump ($4000-$FFFF)
 */

#include "core/formats/format_handler.hpp"

// ============================================================================
// SNA Header Structure
// ============================================================================

struct sna_header_t {
    uint8_t  i_reg;
    uint16_t hl_prime, de_prime, bc_prime, af_prime;
    uint16_t hl, de, bc, iy, ix;
    uint8_t  iff2;       // Bit 2: interrupt enabled
    uint8_t  r_reg;
    uint16_t af, sp;
    uint8_t  int_mode;   // 0, 1, or 2
    uint8_t  border;     // 0-7

    // 128K extension (present when file > 49179 bytes)
    uint16_t pc;         // PC (128K only; 48K pops from stack)
    uint8_t  port_7ffd;  // Last value written to $7FFD
    uint8_t  trdos_rom;  // TR-DOS ROM paged (1) or not (0)
};

/**
 * Parse a 27-byte SNA header from raw data.
 * @return true on success.
 */
bool sna_parse_header(const uint8_t* data, size_t size, sna_header_t* out);

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t SNA_FORMAT_DESCRIPTOR;
