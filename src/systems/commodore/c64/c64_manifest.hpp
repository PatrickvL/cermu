#pragma once
// =============================================================================
// c64_manifest.hpp — C64 declarative chip manifest and bus configuration
// =============================================================================
//
// Defines the Commodore 64 chip memory layout for the manifest-driven
// Board<C64BusSpec> + MemoryBus<C64BusSpec> architecture.
//
// Chips are declared in base-address order for readability.  with_sorted_ids()
// assigns chip IDs in size-ascending order regardless of declaration order,
// preserving the optimal shift-addressable layout (flat_mem offset =
// chip_id << 12, no table lookup):
//
//   CHARROM  4 KB  →  1 ID  → base_id =  0   Character ROM
//   ROML     8 KB  →  2 IDs → base_id =  1   Cartridge ROM Low
//   ROMH     8 KB  →  2 IDs → base_id =  3   Cartridge ROM High
//   BASIC    8 KB  →  2 IDs → base_id =  5   BASIC ROM
//   KERNAL   8 KB  →  2 IDs → base_id =  7   KERNAL ROM
//   RAM     64 KB  → 16 IDs → base_id =  9   Main RAM
//
// with_page_banking() sets bank_size=0 on all buffer chips so that each
// 4 KB page of a chip gets its own chip ID.  This enables the PLA to
// assign per-bank chip IDs directly into the page table.
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
#include "core/memory_bus/bus.hpp"
#include "core/board.hpp"
#include "core/typed_manifest.hpp"
#include "core/typed_port.hpp"
#include "chip/memory/memory_chip.hpp"
#include "chip/cpu/fam65xx/mos6510.hpp"
#include "chip/video/vic_ii/vicii_common.hpp"
#include "chip/sound/mos6581.hpp"
#include "chip/memory/mos2114.hpp"
#include "chip/io/mos6526.hpp"

// =============================================================================
// C64-local chip visitor macros
// =============================================================================
//
// These one-liner visitors are used ONLY by C64's FOR_EACH_SYSTEM_CHIP to
// generate manifest entries, chipset fields, binding logic, enum values,
// pointers, and component registration from the single authoritative chip
// row list.  The C64's PLA-driven architecture requires the X-macro approach
// for its chip enum and page-table descriptor table generation.
//

inline constexpr RomFileInfo parse_rom_spec(const char* spec) noexcept {
    if (!spec) return {nullptr, false};
    if (spec[0] == '?') return {spec + 1, true};
    return {spec, false};
}

#define C64_CHIP_VISITOR_MANIFEST_ROW(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    ChipSlot{base, (size_t)(size), (uint32_t)(mask),                                                  \
             0, 0, overlay, resolve_slot_factory<type>(), label, 0, parse_rom_spec(rom_files)},

#define C64_CHIP_VISITOR_DECLARE_FIELD(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    type chip;

#define C64_CHIP_VISITOR_BIND_SEQUENTIAL(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    ctx.bind_chip(slot_idx_++, &ctx.chip);

#define C64_CHIP_VISITOR_REGISTER_COMPONENT(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    ctx.register_component(&ctx.chip);

#define C64_CHIP_VISITOR_DECLARE_POINTER(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    type* chip = nullptr;

#define C64_CHIP_VISITOR_ASSIGN_POINTER(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    this->chip = &ctx.chip;

#define C64_CHIP_VISITOR_NULL_POINTER(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    this->chip = nullptr;

#define C64_CHIP_VISITOR_ENUM_VALUE(ctx, type, chip, base, size, mask, overlay, label, rom_files) \
    chip,

#define C64_CHIP_VISITOR_COUNT_ONE(ctx, type, chip, base, size, mask, overlay, label, rom_files) +1


// =============================================================================
// §0  C64 Bus Spec — manual spec (not ManifestBusSpec)
// =============================================================================
//
// With page banking (bank_size=0, effective=4096), each buffer chip gets
// ceil(size / 4096) consecutive chip IDs.  Sorted by size ascending:
//
//   CHARROM  4 KB  →  1 ID  → base_id = 0     (IDs  0)
//   ROML     8 KB  →  2 IDs → base_id = 1     (IDs  1-2)
//   ROMH     8 KB  →  2 IDs → base_id = 3     (IDs  3-4)
//   BASIC    8 KB  →  2 IDs → base_id = 5     (IDs  5-6)
//   KERNAL   8 KB  →  2 IDs → base_id = 7     (IDs  7-8)
//   RAM     64 KB  → 16 IDs → base_id = 9     (IDs  9-24)
//
// EnablePartialBus is NOT set: the MOS 2114 colour RAM's 4-bit behaviour is
// handled outside the bus (via the I/O register-file handler on the CPU side,
// and via a dedicated internal fetch on the VIC-II side).
//
// The $D000–$DFFF I/O page is modelled as an indexed sub-table with 4 bits
// (16 × 256 B entries), each routing to a separate MMIO handler (VIC-II,
// SID, Color RAM, CIA1, CIA2, I/O1, I/O2).  Dispatch is O(1): one shift,
// one mask, one table lookup.
//
struct C64BusSpec {
    using AddrType = uint16_t;
    static constexpr size_t AddressBits         = 16;
    static constexpr size_t PageBits            = 12;   // 4 KB pages → 16 pages
    static constexpr size_t NumViewers          = 2;    // CPU=0, VIC-II=1
    static constexpr size_t MaxChipId           = 24;   // 25 bank IDs (CHARROM..RAM with page banking)
    static constexpr size_t MaxWriteChipId      = 24;
    static constexpr bool   EnableMmio          = true;
    static constexpr size_t MaxMmioHandlers     = 8;    // VIC-II, SID, ColorRAM, CIA1, CIA2, I/O1, I/O2, + spare
    static constexpr size_t MaxIndexedSubTables = 1;    // one sub-table: the I/O page
    static constexpr size_t IndexedSubBits      = 4;    // 16 × 256 B entries

    // Shift-addressable: flat_mem offset = chip_id << 12 (no table lookup).
    static constexpr bool   ShiftAddressable    = true;
    static constexpr size_t ShiftBankBits       = 12;   // 4 KB stride

    // CS-tick: enable chip-select field in bus_state_t for self-dispatch.
    static constexpr size_t CsLineBits          = BUS_CS_BITS;
    static constexpr size_t CsMmioChipCount     = 5;    // VIC-II, SID, ColorRAM, CIA1, CIA2

    enum ViewerId : size_t { Cpu = 0, Vic = 1 };
};


// =============================================================================
// §1  C64 Chip Declaration — Single Source of Truth
// =============================================================================
//
// Every C64 chip is declared exactly once in this macro.  All derived
// artifacts — manifest, chipset fields, binding, info tables — are
// generated by applying visitor macros to this list.
//
// Row arguments:
//   V(ctx, type, chip, base, size, mask, overlay, label, rom_files)
//
//   size — flat-memory buffer size in bytes (non-zero for memory chips).
//   mask — address decode mask (0 for memory chips = "use size-1").
//
// All chips become value-typed fields in C64Chipset.  Buffer vs MMIO is
// determined by the size column (size > 0 → buffer, size == 0 → MMIO).
// Board::bind_chip() auto-calls on_bind_buffer() for buffer chips.
//
// Chips are sorted: CPU first, then by (base, overlay, size).  with_sorted_ids()
// assigns chip IDs in size-ascending order regardless of declaration order,
// preserving the optimal shift-addressable layout:
//   CHARROM=0, ROML=1, ROMH=3, BASIC=5, KERNAL=7, RAM=9
//

#define C64_FOR_EACH_SYSTEM_CHIP(V, ctx)                                                                         \
    V(ctx, MOS6510,      cpu,      0x0000,       0,      0, 0, "MOS 6510",   nullptr)                             \
    V(ctx, RAMChip,      ram,      0x0000, 0x10000,      0, 1, "RAM",         nullptr)                             \
    V(ctx, ROMChip,      roml,     0x8000,  0x2000,      0, 1, "ROML",        nullptr)                             \
    V(ctx, ROMChip,      basic,    0xA000,  0x2000,      0, 1, "BASIC ROM",                                        \
        "C64 - 901226-01 - Commodore (F833D117) Basic.rom|"                                                      \
        "basic.901226-01.bin|901226-01.bin|basic.rom")                                                           \
    V(ctx, ROMChip,      romh,     0xA000,  0x2000,      0, 1, "ROMH",        nullptr)                             \
    V(ctx, vicii_base_t, vicii,    0xD000,       0,      0, 0, "VIC-II",      nullptr)                             \
    V(ctx, ROMChip,      charrom,  0xD000,  0x1000,      0, 1, "CHARROM",                                          \
        "C64 - 901225-01 - Commodore (EC4272EE) Characters.rom|"                                                 \
        "characters.901225-01.bin|901225-01.bin|chargen.rom|char.rom")                                           \
    V(ctx, mos6581_t,    sid,      0xD400,       0,      0, 0, "SID",         nullptr)                             \
    V(ctx, MOS2114,      colorram, 0xD800,       0,      0, 0, "Color RAM",   nullptr)                             \
    V(ctx, mos6526_t,    cia1,     0xDC00,       0,      0, 0, "CIA1",        nullptr)                             \
    V(ctx, mos6526_t,    cia2,     0xDD00,       0,      0, 0, "CIA2",        nullptr)                             \
    V(ctx, ROMChip,      kernal,   0xE000,  0x2000,      0, 1, "KERNAL",                                           \
        "C64 - 901227-03 - Commodore (DBE3E7C7) Kernal.rom|"                                                     \
        "kernal.901227-03.bin|901227-03.bin|kernal.rom")

// Total chip count (auto-derived from macro)
static constexpr size_t kC64ChipCount = 0 C64_FOR_EACH_SYSTEM_CHIP(C64_CHIP_VISITOR_COUNT_ONE, unused);


// =============================================================================
// §2  Chip Manifest (generated from C64_FOR_EACH_SYSTEM_CHIP)
// =============================================================================
//
// with_page_banking() sets bank_size=0 on all buffer chips so that each
// 4 KB page gets its own chip ID.  This enables per-bank chip-id assignment
// in the PLA page tables and activates the shift-addressable optimization.
//

inline constexpr auto make_c64_manifest() {
    ChipManifest<kC64ChipCount> m = {{
        C64_FOR_EACH_SYSTEM_CHIP(C64_CHIP_VISITOR_MANIFEST_ROW, unused)
    }};
    m.chips[5].bank_size = 0x400;   // VIC-II: mirrors across $D000-$D3FF (4 pages)
    m.chips[7].bank_size = 0x400;   // SID: mirrors across $D400-$D7FF (4 pages)
    return m.with_page_banking().with_sorted_ids();
}
inline constexpr ChipManifest<kC64ChipCount> kC64Chips = make_c64_manifest();

// =============================================================================
// §3  Type Aliases
// =============================================================================

using C64Bus      = MemoryBus<C64BusSpec>;
using C64Board    = Board<C64BusSpec>;
using C64Snapshot = C64Bus::Snapshot;
using C64PT       = PackingTraits<C64BusSpec>;
using C64ChipId   = C64PT::ChipId;      // MemoryBus read-side chip/bank ID
using C64WriteId  = C64PT::WriteChipId;  // MemoryBus write-side chip/bank ID

// PLA chip identifier — one value per manifest slot (in declaration order),
// plus PLA-only sentinels Io and Unmapped.  Enum values == manifest slot
// indices for 0..kC64ChipCount-1, so kC64PlaChipTable[i] is populated
// directly from kC64Chips.chips[i] with no manual find<>() mapping.
enum class C64PlaChipId : uint8_t {
    // Auto-generated from C64_FOR_EACH_SYSTEM_CHIP — order matches manifest
    C64_FOR_EACH_SYSTEM_CHIP(C64_CHIP_VISITOR_ENUM_VALUE, unused)
    // PLA-only sentinels (no manifest slot)
    io, unmapped,
    count
};
inline constexpr size_t kC64PlaChipCount = size_t(C64PlaChipId::count);
static_assert(size_t(C64PlaChipId::io) == kC64ChipCount,
    "Io must be the first sentinel after the manifest chips");

// =============================================================================
// §3a  PLA Chip Descriptor Table (central single source)
// =============================================================================
//
// Maps C64PlaChipId enum → {base_id, slot*, bank_mask}.
// Entries 0..kC64ChipCount-1 are visitor-generated from the manifest.
// I/O and Unmapped sentinels point to standalone ChipSlot objects.
//

struct C64PlaChipDesc {
    C64ChipId        base_id;    // MemoryBus base chip ID (from manifest)
    const ChipSlot*  slot;       // → manifest slot (base_addr, size_bytes, label)
    uint8_t          bank_mask;  // (num_4k_banks - 1) for per-bank chip-id conversion
};

// Sentinel ChipSlot objects for PLA-only entries (no manifest slot)
inline constexpr ChipSlot kC64PlaIoSlot       = []() { ChipSlot s{}; s.base_addr = 0xD000; s.size_bytes = 4096; s.label = "I/O"; return s; }();
inline constexpr ChipSlot kC64PlaUnmappedSlot = []() { ChipSlot s{}; s.label = "-"; return s; }();

// ctx = page_bits; chip name indexes kC64Chips via C64PlaChipId enum value
#define C64_PLA_CHIP_DESC_(pb, type, chip, base, size, mask, overlay, label, rom_files)  \
    { C64ChipId(kC64Chips.base_id(size_t(C64PlaChipId::chip), pb)),                              \
      &kC64Chips.chips[size_t(C64PlaChipId::chip)],                                              \
      uint8_t((kC64Chips.chips[size_t(C64PlaChipId::chip)].size_bytes >> pb)                     \
          ? (kC64Chips.chips[size_t(C64PlaChipId::chip)].size_bytes >> pb) - 1 : 0) },

inline constexpr std::array<C64PlaChipDesc, kC64PlaChipCount> kC64PlaChipTable = {{
    C64_FOR_EACH_SYSTEM_CHIP(C64_PLA_CHIP_DESC_, 12)
    // PLA-only sentinels
    {C64ChipId(0), &kC64PlaIoSlot,       0},
    {C64ChipId(0), &kC64PlaUnmappedSlot, 0},
}};
#undef C64_PLA_CHIP_DESC_

// Verify consistency between C64BusSpec and the manifest
static_assert(C64BusSpec::MaxChipId == kC64Chips.max_chip_id(12),
    "C64BusSpec::MaxChipId must match manifest total bank IDs - 1");
static_assert(C64BusSpec::ShiftAddressable == kC64Chips.has_shift_addressable_banks(12),
    "C64BusSpec::ShiftAddressable must match manifest");
static_assert(!C64BusSpec::ShiftAddressable || C64BusSpec::ShiftBankBits == kC64Chips.shift_bank_bits(12),
    "C64BusSpec::ShiftBankBits must match manifest");

// Verify PLA descriptor table base_ids match the manifest
static_assert(size_t(kC64PlaChipTable[size_t(C64PlaChipId::charrom)].base_id) == 0,  "CHARROM base_id");
static_assert(size_t(kC64PlaChipTable[size_t(C64PlaChipId::roml)].base_id)    == 1,  "ROML base_id");
static_assert(size_t(kC64PlaChipTable[size_t(C64PlaChipId::basic)].base_id)   == 3,  "BASIC base_id");
static_assert(size_t(kC64PlaChipTable[size_t(C64PlaChipId::romh)].base_id)    == 5,  "ROMH base_id");
static_assert(size_t(kC64PlaChipTable[size_t(C64PlaChipId::kernal)].base_id)  == 7,  "KERNAL base_id");
static_assert(size_t(kC64PlaChipTable[size_t(C64PlaChipId::ram)].base_id)     == 9,  "RAM base_id");

// Number of PLA banking modes (5-bit: LORAM, HIRAM, CHAREN, EXROM, GAME)
inline constexpr size_t kC64NumPlaModes = 32;
