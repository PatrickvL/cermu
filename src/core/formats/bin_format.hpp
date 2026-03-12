#pragma once

/**
 * BIN Format Handler — Raw Binary Files
 *
 * The BIN format is a headerless raw binary: no load address, no metadata.
 * Load address defaults to 0; systems override as needed.
 */

#include "core/formats/format_handler.hpp"

// ============================================================================
// Format Descriptor
// ============================================================================

extern const format_descriptor_t BIN_FORMAT_DESCRIPTOR;
