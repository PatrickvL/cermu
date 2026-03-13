#pragma once

/**
 * SCL Format Handler — ZX Spectrum TR-DOS Disk Container
 *
 * The .scl format is a compact representation of TR-DOS filesystem content.
 * It stores file headers and used sectors, omitting empty disk space.
 *
 * File structure:
 *   $0000  8   Signature "SINCLAIR"
 *   $0008  1   Number of files (N)
 *   $0009  N×14  File header entries (TR-DOS catalog minus sector/track)
 *   $0009 + N×14  Sector data (sequential, file by file)
 *
 * Uses the shared trdos_common.hpp types.
 */

#include "core/formats/trdos_common.hpp"

extern const format_descriptor_t SCL_FORMAT_DESCRIPTOR;
