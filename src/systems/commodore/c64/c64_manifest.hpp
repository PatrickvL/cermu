#pragma once
// =============================================================================
// c64_manifest.hpp — C64 declarative chip manifest and bus configuration
// =============================================================================
//
// Defines the Commodore 64 chip memory layout for the manifest-driven
// Board<C64BusSpec> + MemoryBus<C64BusSpec> architecture.
//
// Chip layout (buffer-backed, 4 KB page size):
//   Slot 0: RAM      — 64 KB at $0000  (ID 0)
//   Slot 1: ROML     —  8 KB at $8000  (ID 1)   Cartridge ROM Low
//   Slot 2: ROMH     —  8 KB at $A000  (ID 2)   Cartridge ROM High
//   Slot 3: BASIC    —  8 KB at $A000  (ID 3)   BASIC ROM
//   Slot 4: KERNAL   —  8 KB at $E000  (ID 4)   KERNAL ROM
//   Slot 5: CHARROM  —  4 KB at $D000  (ID 5)   Character ROM
//
// All buffer chips use overlay_group=1 so that apply() only populates
// chip_info (Phase 0) without mapping any pages (Phase 1).  Page tables
// are fully PLA-driven — each of the 32 banking modes is a ModeSnapshot
// that programs the correct chip at each 4 KB bank.
//
// I/O dispatch ($D000–$DFFF):
//   An IndexedSubTable with 4 bits (16 × 256 B entries) routes accesses
//   to MMIO handlers for VIC-II, SID, Color RAM, CIA1, CIA2, I/O1, I/O2.
//   In PLA modes where I/O is visible, page $D points to the sub-table
//   sentinel; in modes with CHARROM or RAM, it points to the buffer chip.
//
// =============================================================================

#include "core/chip_manifest.hpp"
#include "core/memory_bus/configs.hpp"
#include "core/board.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/cpu/fam65xx/mos6510.hpp"
#include "chip/video/vic_ii/vicii_common.hpp"
#include "chip/sound/mos6581.hpp"
#include "chip/memory/mos2114.hpp"
#include "chip/io/mos6526.hpp"


// =============================================================================
// §1  Chip Manifest
// =============================================================================
//
// bank_size = chip_size so each chip consumes exactly one chip ID.
// addr_mask = 0 (not mirrored) → BusMap derives mask from bank_size.
// overlay_group = 1 for all buffer chips → Phase 1 skips them.
//

inline constexpr auto kC64Chips = make_chip_manifest(
    // ── Buffer-backed chips (PLA-switched) ───────────────────────────────
    //                       base    size   mask  label         cond  bank_sz  ovl
    Slot<RAMChip>  {0x0000, 65536,    0, "RAM",         0, 65536, 1},  // Slot 0
    Slot<ROMChip>  {0x8000,  8192,    0, "ROML",        0,  8192, 1},  // Slot 1
    Slot<ROMChip>  {0xA000,  8192,    0, "ROMH",        0,  8192, 1},  // Slot 2
    Slot<ROMChip>  {0xA000,  8192,    0, "BASIC ROM",   0,  8192, 1},  // Slot 3
    Slot<ROMChip>  {0xE000,  8192,    0, "KERNAL",      0,  8192, 1},  // Slot 4
    Slot<ROMChip>  {0xD000,  4096,    0, "CHARROM",     0,  4096, 1},  // Slot 5

    // ── Non-bus chips (CPU, I/O — no buffer in flat mem) ─────────────────
    Slot<MOS6510>  {0, 0, 0, "MOS 6510"},       // Slot 6 — CPU
    Slot<vicii_t>  {0, 0, 0, "VIC-II"},          // Slot 7
    Slot<mos6581_t>{0, 0, 0, "SID"},             // Slot 8
    Slot<MOS2114>  {0, 0, 0, "Color RAM"},       // Slot 9
    Slot<mos6526_t>{0, 0, 0, "CIA1"},            // Slot 10
    Slot<mos6526_t>{0, 0, 0, "CIA2"}             // Slot 11
);

// =============================================================================
// §3  Type Aliases
// =============================================================================

using C64Bus      = MemoryBus<C64BusSpec>;
using C64Board    = Board<C64BusSpec>;
using C64Snapshot = C64Bus::Snapshot;
using C64PT       = PackingTraits<C64BusSpec>;
using C64ChipId   = C64PT::ChipId;      // MemoryBus read-side chip/bank ID
using C64WriteId  = C64PT::WriteChipId;  // MemoryBus write-side chip/bank ID

// PLA output chip identifier — superset of MemoryBus buffer ChipIds (0-5)
// plus PLA-specific sentinels (kIo=0xFE, kUnmapped=0xFF).
using C64PlaChipId = uint8_t;

// =============================================================================
// §3b  Chip IDs (constexpr, derived from manifest)
// =============================================================================
//
// These replace the legacy CHIP_ROML=0,CHIP_RAM=9 strategic numbering.
// MemoryBus uses chip_info[id].base + (addr & mask) — no arithmetic tricks.
//

namespace c64_chip_ids {
    inline constexpr C64ChipId kRam     = C64ChipId(kC64Chips.base_id(kC64Chips.find<RAMChip>(),    12));  // 0
    inline constexpr C64ChipId kRoml    = C64ChipId(kC64Chips.base_id(kC64Chips.find<ROMChip>(),    12));  // 1
    inline constexpr C64ChipId kRomh    = C64ChipId(kC64Chips.base_id(kC64Chips.find<ROMChip>(1),   12));  // 2
    inline constexpr C64ChipId kBasic   = C64ChipId(kC64Chips.base_id(kC64Chips.find<ROMChip>(2),   12));  // 3
    inline constexpr C64ChipId kKernal  = C64ChipId(kC64Chips.base_id(kC64Chips.find<ROMChip>(3),   12));  // 4
    inline constexpr C64ChipId kCharrom = C64ChipId(kC64Chips.base_id(kC64Chips.find<ROMChip>(4),   12));  // 5

    // Sentinel values for PLA outputs that don't map to buffer chips
    inline constexpr C64PlaChipId kIo       = 0xFE;  // I/O region ($D000-$DFFF)
    inline constexpr C64PlaChipId kUnmapped = 0xFF;  // Unmapped / open bus

    // Buffer-backed chip count (for iteration over kRam..kCharrom)
    inline constexpr size_t kBufferChipCount = 6;
}

// =============================================================================
// §3c  Chip Display Helpers (PLA debug GUI)
// =============================================================================

// All PLA chip IDs that can appear in PLA tables (for legend/iteration)
inline constexpr C64PlaChipId kC64AllChipIds[] = {
    C64PlaChipId(c64_chip_ids::kRam),  C64PlaChipId(c64_chip_ids::kRoml),
    C64PlaChipId(c64_chip_ids::kRomh), C64PlaChipId(c64_chip_ids::kBasic),
    C64PlaChipId(c64_chip_ids::kKernal), C64PlaChipId(c64_chip_ids::kCharrom),
    c64_chip_ids::kIo, c64_chip_ids::kUnmapped
};
inline constexpr size_t kC64AllChipIdCount = sizeof(kC64AllChipIds) / sizeof(kC64AllChipIds[0]);

// Short chip name for display
inline const char* c64_chip_title(C64PlaChipId chip_id) {
    using namespace c64_chip_ids;
    switch (chip_id) {
        case kRam:      return "RAM";
        case kRoml:     return "ROML";
        case kRomh:     return "ROMH";
        case kBasic:    return "BASIC";
        case kKernal:   return "KERNAL";
        case kCharrom:  return "CHARROM";
        case kIo:       return "I/O";
        case kUnmapped: return "-";
        default:        return "?";
    }
}

// Chip descriptor for PLA debug tables
struct C64ChipInfo {
    uint16_t base;
    size_t   size;
    const char* label;
};

// Fetch display info for a PLA chip ID.  Returns true if valid.
inline bool c64_chip_info(C64PlaChipId chip_id, C64ChipInfo* out) {
    using namespace c64_chip_ids;
    static constexpr struct { uint16_t base; size_t size; const char* label; } kInfo[] = {
        {0x0000, 65536, "RAM"},               // kRam     = 0
        {0x8000,  8192, "Cartridge ROM Low"}, // kRoml    = 1
        {0xA000,  8192, "Cartridge ROM High"},// kRomh    = 2
        {0xA000,  8192, "BASIC ROM"},         // kBasic   = 3
        {0xE000,  8192, "KERNAL ROM"},        // kKernal  = 4
        {0xD000,  4096, "Character ROM"},     // kCharrom = 5
    };
    if (chip_id < kBufferChipCount) {
        *out = {kInfo[chip_id].base, kInfo[chip_id].size, kInfo[chip_id].label};
        return true;
    }
    if (chip_id == kIo)       { *out = {0xD000, 4096, "I/O"};      return true; }
    if (chip_id == kUnmapped) { *out = {0, 0, "Unmapped"};         return true; }
    *out = {0, 0, "?"};
    return false;
}

// Number of PLA banking modes (5-bit: LORAM, HIRAM, CHAREN, EXROM, GAME)
inline constexpr size_t kC64NumPlaModes = 32;
