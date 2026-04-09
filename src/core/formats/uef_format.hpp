#pragma once

/**
 * UEF Format Handler — Acorn Unified Emulator Format
 *
 * The .uef format stores BBC Micro / Acorn Atom / Acorn Electron cassette
 * data in a chunk-based container.  Files may be gzip-compressed.
 *
 * File structure:
 *   "UEF File!\0" (10 bytes magic)
 *   Minor version (1 byte)
 *   Major version (1 byte)
 *   Chunks:
 *     Type (LE16) + Length (LE32) + Data
 *
 * Key chunk types:
 *   $0100  Implicit-start data block (raw data)
 *   $0104  Defined-format data block (sync, data)
 *   $0110  High-tone (carrier) — gap between blocks
 *   $0112  Integer gap
 *   $0116  Floating-point gap
 *
 * Inside data blocks ($0100/$0104):
 *   Sync byte $2A
 *   Filename (NUL-terminated)
 *   Load address (LE32)
 *   Exec address (LE32)
 *   Block number (LE16)
 *   Block length (LE16)
 *   Block flags (1 byte)
 *   Next file address (LE32) — distance to next header block
 *   Header CRC (LE16)
 *   Data (block_length bytes)
 *   Data CRC (LE16)
 *
 * Uses tape_common.hpp for shared loading logic.
 */

#include "core/formats/format_handler.hpp"

extern const format_descriptor_t UEF_FORMAT_DESCRIPTOR;
