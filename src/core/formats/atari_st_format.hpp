#pragma once
/*
 * atari_st_format.hpp — Atari ST disk and program format
 *
 * Atari ST uses several formats:
 *   .st  — Raw floppy disk image (sectors × tracks × sides, typically 720KB)
 *   .prg — GEM executable with GEMDOS header
 *   .tos — TOS executable (same as PRG but runs in supervisor mode)
 *   .msa — Magic Shadow Archiver compressed disk image
 */
#include "core/formats/format_handler.hpp"

extern const format_descriptor_t ATARI_ST_FORMAT_DESCRIPTOR;
