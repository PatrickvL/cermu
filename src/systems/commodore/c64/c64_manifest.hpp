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

inline constexpr RomFileInfo parse_rom_spec(const char* spec) noexcept {
    if (!spec) return {nullptr, false};
    if (spec[0] == '?') return {spec + 1, true};
    return {spec, false};
}


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
// §1  C64 Chip Manifest — declarative memory layout
// =============================================================================
//
// Chips in declaration order (manifest slot index = C64PlaChipId enum value):
//
//   [0]  MOS 6510       CPU (non-buffer, size=0)
//   [1]  RAM            64 KB @ $0000 (overlay group 1)
//   [2]  ROML           8 KB  @ $8000 (overlay group 1)
//   [3]  BASIC ROM      8 KB  @ $A000 (overlay group 1)
//   [4]  ROMH           8 KB  @ $A000 (overlay group 1)
//   [5]  VIC-II         MMIO  @ $D000 (non-buffer)
//   [6]  CHARROM        4 KB  @ $D000 (overlay group 1)
//   [7]  SID            MMIO  @ $D400 (non-buffer)
//   [8]  Color RAM      MMIO  @ $D800 (non-buffer)
//   [9]  CIA1           MMIO  @ $DC00 (non-buffer)
//   [10] CIA2           MMIO  @ $DD00 (non-buffer)
//   [11] KERNAL         8 KB  @ $E000 (overlay group 1)
//
// with_page_banking() → each buffer chip's 4 KB pages get individual IDs.
// with_sorted_ids()   → IDs assigned in size-ascending order:
//   CHARROM=0, ROML=1, ROMH=3, BASIC=5, KERNAL=7, RAM=9
//

inline constexpr size_t kC64ChipCount = 12;

inline constexpr auto make_c64_manifest() {
    ChipManifest<kC64ChipCount> m = {{
        // [0] MOS 6510 CPU
        ChipSlot{0x0000, 0, 0, 0, 0, 0, resolve_slot_factory<MOS6510>(), "MOS 6510", 0, parse_rom_spec(nullptr)},
        // [1] 64 KB RAM
        ChipSlot{0x0000, 0x10000, 0, 0, 0, 1, resolve_slot_factory<RAMChip>(), "RAM", 0, parse_rom_spec(nullptr)},
        // [2] ROML — cartridge ROM low (8 KB)
        ChipSlot{0x8000, 0x2000, 0, 0, 0, 1, resolve_slot_factory<ROMChip>(), "ROML", 0, parse_rom_spec(nullptr)},
        // [3] BASIC ROM (8 KB)
        ChipSlot{0xA000, 0x2000, 0, 0, 0, 1, resolve_slot_factory<ROMChip>(), "BASIC ROM", 0,
            parse_rom_spec("C64 - 901226-01 - Commodore (F833D117) Basic.rom|"
                           "basic.901226-01.bin|901226-01.bin|basic.rom")},
        // [4] ROMH — cartridge ROM high (8 KB)
        ChipSlot{0xA000, 0x2000, 0, 0, 0, 1, resolve_slot_factory<ROMChip>(), "ROMH", 0, parse_rom_spec(nullptr)},
        // [5] VIC-II (MMIO)
        ChipSlot{0xD000, 0, 0, 0, 0, 0, resolve_slot_factory<vicii_base_t>(), "VIC-II", 0, parse_rom_spec(nullptr)},
        // [6] CHARROM (4 KB)
        ChipSlot{0xD000, 0x1000, 0, 0, 0, 1, resolve_slot_factory<ROMChip>(), "CHARROM", 0,
            parse_rom_spec("C64 - 901225-01 - Commodore (EC4272EE) Characters.rom|"
                           "characters.901225-01.bin|901225-01.bin|chargen.rom|char.rom")},
        // [7] SID (MMIO)
        ChipSlot{0xD400, 0, 0, 0, 0, 0, resolve_slot_factory<mos6581_t>(), "SID", 0, parse_rom_spec(nullptr)},
        // [8] Color RAM (MMIO)
        ChipSlot{0xD800, 0, 0, 0, 0, 0, resolve_slot_factory<MOS2114>(), "Color RAM", 0, parse_rom_spec(nullptr)},
        // [9] CIA1 (MMIO)
        ChipSlot{0xDC00, 0, 0, 0, 0, 0, resolve_slot_factory<mos6526_t>(), "CIA1", 0, parse_rom_spec(nullptr)},
        // [10] CIA2 (MMIO)
        ChipSlot{0xDD00, 0, 0, 0, 0, 0, resolve_slot_factory<mos6526_t>(), "CIA2", 0, parse_rom_spec(nullptr)},
        // [11] KERNAL (8 KB)
        ChipSlot{0xE000, 0x2000, 0, 0, 0, 1, resolve_slot_factory<ROMChip>(), "KERNAL", 0,
            parse_rom_spec("C64 - 901227-03 - Commodore (DBE3E7C7) Kernal.rom|"
                           "kernal.901227-03.bin|901227-03.bin|kernal.rom")},
    }};
    m.chips[5].bank_size = 0x400;   // VIC-II: mirrors across $D000-$D3FF
    m.chips[7].bank_size = 0x400;   // SID: mirrors across $D400-$D7FF
    return m.with_page_banking().with_sorted_ids();
}
inline constexpr ChipManifest<kC64ChipCount> kC64Chips = make_c64_manifest();

// =============================================================================
// §3  Type Aliases
// =============================================================================

using C64Bus      = MemoryBus<C64BusSpec>;
using C64Snapshot = C64Bus::Snapshot;
using C64PT       = PackingTraits<C64BusSpec>;
using C64ChipId   = C64PT::ChipId;      // MemoryBus read-side chip/bank ID
using C64WriteId  = C64PT::WriteChipId;  // MemoryBus write-side chip/bank ID

// PLA chip identifier — one value per manifest slot (in declaration order),
// plus PLA-only sentinels io and unmapped.  Enum values == manifest slot
// indices for 0..kC64ChipCount-1.
enum class C64PlaChipId : uint8_t {
    cpu = 0, ram, roml, basic, romh, vicii, charrom, sid, colorram, cia1, cia2, kernal,
    // PLA-only sentinels (no manifest slot)
    io, unmapped,
    count
};
inline constexpr size_t kC64PlaChipCount = size_t(C64PlaChipId::count);
static_assert(size_t(C64PlaChipId::io) == kC64ChipCount,
    "io must be the first sentinel after the manifest chips");

// =============================================================================
// §3a  PLA Chip Descriptor Table (central single source)
// =============================================================================
//
// Maps C64PlaChipId enum → {base_id, slot*, bank_mask}.
// Entries 0..kC64ChipCount-1 are derived from the manifest.
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

// Build a PLA descriptor from a manifest slot index.
inline constexpr C64PlaChipDesc pla_chip_desc(size_t i) {
    constexpr size_t pb = C64BusSpec::PageBits;
    size_t num_banks = kC64Chips.chips[i].size_bytes >> pb;
    return {
        C64ChipId(kC64Chips.base_id(i, pb)),
        &kC64Chips.chips[i],
        uint8_t(num_banks ? num_banks - 1 : 0)
    };
}

inline constexpr std::array<C64PlaChipDesc, kC64PlaChipCount> kC64PlaChipTable = {{
    pla_chip_desc(0),   // cpu
    pla_chip_desc(1),   // ram
    pla_chip_desc(2),   // roml
    pla_chip_desc(3),   // basic
    pla_chip_desc(4),   // romh
    pla_chip_desc(5),   // vicii
    pla_chip_desc(6),   // charrom
    pla_chip_desc(7),   // sid
    pla_chip_desc(8),   // colorram
    pla_chip_desc(9),   // cia1
    pla_chip_desc(10),  // cia2
    pla_chip_desc(11),  // kernal
    // PLA-only sentinels
    {C64ChipId(0), &kC64PlaIoSlot,       0},
    {C64ChipId(0), &kC64PlaUnmappedSlot, 0},
}};

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
