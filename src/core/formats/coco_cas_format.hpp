#pragma once
/*
 * coco_cas_format.hpp — TRS-80 / CoCo CAS cassette tape format
 *
 * CAS files contain a stream of blocks with leader + sync + data.
 * Block types: $00=filename, $01=data, $FF=EOF.
 * Files within a CAS: BASIC programs, machine code, data.
 */
#include "core/formats/format_handler.hpp"

extern const format_descriptor_t COCO_CAS_FORMAT_DESCRIPTOR;
