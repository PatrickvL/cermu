#pragma once
/*
 * pce_format.hpp — NEC PC Engine / TurboGrafx-16 HuCard format
 *
 * PC Engine HuCard ROMs are raw binary images (.pce).
 * Sizes: 256KB, 384KB, 512KB, 768KB, 1MB.
 * Some dumps include a 512-byte copier header (total size % 8192 != 0).
 */
#include "core/formats/format_handler.hpp"

extern const format_descriptor_t PCE_FORMAT_DESCRIPTOR;
