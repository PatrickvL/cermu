// =============================================================================
// configs.hpp — Pre-baked bus specs, type aliases, usage examples
// =============================================================================
#pragma once

#include "core/memory_bus/bus.hpp"


// =============================================================================
// §1  Pre-baked specs
// =============================================================================

// ── Commodore 64 ──────────────────────────────────────────────────────────────
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

    enum ViewerId : size_t { Cpu = 0, Vic = 1 };
};

// ── NES / Famicom — CPU bus ───────────────────────────────────────────────────
//
// Chip ids 0–512 address up to 512 KB of PRG-ROM in 1 KB pages.
// The write side has far fewer chips (only writable RAM), so the write chip-id
// field is narrower, keeping the packed page slot at uint16_t.
//
// CsLineBits = 10: the resolved chip id is emitted into bus_state_t after each
// access.  10 bits covers all ids (512 buffer + sentinels).
//
struct NesCpuBusSpec {
    using AddrType = uint16_t;
    static constexpr size_t AddressBits    = 16;
    static constexpr size_t PageBits       = 10;   // 1 KB pages → 64 pages
    static constexpr size_t NumViewers     = 1;
    static constexpr size_t MaxChipId      = 512;  // 512 KB max PRG-ROM / 1 KB per chip-id
    static constexpr size_t MaxWriteChipId = 20;   // WRAM + CIRAM + PRG-RAM
    static constexpr bool   EnableMmio     = true;
    static constexpr size_t MaxMmioHandlers= 4;    // PPU, APU/IO, mapper, exp
    static constexpr size_t CsLineBits     = BUS_CS_BITS;   // global width
    static constexpr size_t CsBitShift     = BUS_CS_SHIFT;  // global position
};

// ── NES / Famicom — PPU bus ───────────────────────────────────────────────────
struct NesPpuBusSpec {
    using AddrType = uint16_t;
    static constexpr size_t AddressBits    = 14;
    static constexpr size_t PageBits       = 10;   // 1 KB pages → 16 pages
    static constexpr size_t NumViewers     = 1;
    static constexpr size_t MaxChipId      = 256;  // 256 KB max CHR / 1 KB
    static constexpr size_t MaxWriteChipId = 12;   // CHR-RAM + CIRAM
    static constexpr bool   EnableMmio     = false;
    static constexpr size_t MaxMmioHandlers= 0;
};

// ── Generic 8-bit microcomputer ───────────────────────────────────────────────
//
// EnablePartialBus = true: covers systems (e.g. Apple I) where 4-bit SRAM is
// directly chip-selected with no register-file handler in front of it.
//
struct Generic8BitBusSpec {
    using AddrType = uint16_t;
    static constexpr size_t AddressBits    = 16;
    static constexpr size_t PageBits       = 12;   // 4 KB pages → 16 pages
    static constexpr size_t NumViewers     = 1;
    static constexpr size_t MaxChipId      = 15;   // up to 16 chips
    static constexpr size_t MaxWriteChipId = 15;
    static constexpr bool   EnableMmio     = true;
    static constexpr size_t MaxMmioHandlers= 4;
    static constexpr bool   EnablePartialBus = true;
};

// ── Minimal / embedded ────────────────────────────────────────────────────────
struct MinimalBusSpec {
    using AddrType = uint16_t;
    static constexpr size_t AddressBits    = 16;
    static constexpr size_t PageBits       = 12;
    static constexpr size_t NumViewers     = 1;
    static constexpr size_t MaxChipId      = 7;
    static constexpr size_t MaxWriteChipId = 7;
    static constexpr bool   EnableMmio     = false;
    static constexpr size_t MaxMmioHandlers= 0;
};


// =============================================================================
// §2  Convenience type aliases
// =============================================================================

using C64Bus    = MemoryBus<C64BusSpec>;
using NesCpuBus = MemoryBus<NesCpuBusSpec>;
using NesPpuBus = MemoryBus<NesPpuBusSpec>;

using CpuView_C64 = BusView<C64BusSpec, C64BusSpec::Cpu>;
using VicView_C64 = BusView<C64BusSpec, C64BusSpec::Vic>;


// =============================================================================
// §3  Usage examples
// =============================================================================
//
// ── How chip selection works ────────────────────────────────────────────────
//
//  Each page in the per-viewer chip table holds a compact binary-encoded
//  chip-select id.  The hot path does:
//
//    chip_id = chip_table[page_of(addr)]
//    if chip_id < kNoChipSelected:
//        data = flat_mem[(chip_id << PageBits) | offset]
//
//  Values ≥ kNoChipSelected are sentinels: open-bus, sub-table dispatch,
//  or MMIO handler.  This is the software equivalent of a PLA asserting
//  one /CS line per bus cycle.
//
// ── C64 I/O page — indexed sub-table ────────────────────────────────────────
//
//  The C64 I/O page ($D000–$DFFF) is a 4 KB page split into 16 × 256 B
//  sub-regions.  Each maps to a different MMIO handler.  Dispatch is O(1):
//  one shift + one mask + one table lookup.
//
//    C64Bus bus;
//    const int hVicII  = bus.register_handler({vicii,   vicii_read,   vicii_write});
//    const int hSID    = bus.register_handler({sid,     sid_read,     sid_write});
//    const int hColRAM = bus.register_handler({colram,  colram_read,  colram_write});
//    const int hCIA1   = bus.register_handler({cia1,    cia1_read,    cia1_write});
//    const int hCIA2   = bus.register_handler({cia2,    cia2_read,    cia2_write});
//    const int hIO1    = bus.register_handler({nullptr, unmapped_read, unmapped_write});
//    const int hIO2    = bus.register_handler({nullptr, unmapped_read, unmapped_write});
//
//    using PT = PackingTraits<C64BusSpec>;
//
//    // Create indexed sub-table: 4 bits → 16 entries, bit_shift=8 (bits 11-8)
//    const int io_sub = bus.add_indexed_sub_table(C64BusSpec::Cpu, 4, 8);
//
//    // Helper to create MMIO sentinel chip ids
//    auto mmio_rd = [](int h) { return PT::ChipId(PT::kRegChipBase + h); };
//    auto mmio_wr = [](int h) { return PT::WriteChipId(PT::kRegChipBaseWrite + h); };
//
//    // Populate entries: VIC-II ($D000-$D3FF), SID ($D400-$D7FF), etc.
//    for (int i = 0;  i < 4;  ++i)
//        bus.set_indexed_entry(0, io_sub, i, mmio_rd(hVicII), mmio_wr(hVicII));
//    for (int i = 4;  i < 8;  ++i)
//        bus.set_indexed_entry(0, io_sub, i, mmio_rd(hSID), mmio_wr(hSID));
//    for (int i = 8;  i < 12; ++i)
//        bus.set_indexed_entry(0, io_sub, i, mmio_rd(hColRAM), mmio_wr(hColRAM));
//    bus.set_indexed_entry(0, io_sub, 12, mmio_rd(hCIA1), mmio_wr(hCIA1));
//    bus.set_indexed_entry(0, io_sub, 13, mmio_rd(hCIA2), mmio_wr(hCIA2));
//    bus.set_indexed_entry(0, io_sub, 14, mmio_rd(hIO1),  mmio_wr(hIO1));
//    bus.set_indexed_entry(0, io_sub, 15, mmio_rd(hIO2),  mmio_wr(hIO2));
//
//    // In PLA modes where I/O is visible: route page $D to the sub-table
//    bus.map_to_indexed_sub(C64BusSpec::Cpu, 0xD, io_sub);
//
//    // In modes where CHARROM or RAM is at $D000: just set the page's chip id
//    // directly — the sub-table is bypassed.
//
// ── C16 TED — masked sub-table ──────────────────────────────────────────────
//
//  The C16's $FF00 page has 64 bytes of TED registers overlaying whatever
//  the base chip is (KERNAL ROM or RAM).  The base chip changes at runtime
//  when TED toggles rom_enabled.
//
//    C16Bus bus;
//    const int hTED = bus.register_handler({ted, ted_read, ted_write});
//    using PT = PackingTraits<C16BusSpec>;
//
//    // Base = KERNAL ROM chip (initially ROM visible after reset)
//    const int ted_sub = bus.add_masked_sub_table(0,
//                            kernal_chip_for_ff, PT::kNoChipSelectedWrite);
//
//    // TED regs: $FF00–$FF3F  →  mask 0xFFC0, match 0xFF00
//    bus.add_masked_region(0, ted_sub, 0xFFC0, 0xFF00,
//                          PT::ChipId(PT::kRegChipBase + hTED),
//                          PT::WriteChipId(PT::kRegChipBaseWrite + hTED));
//
//    // Route page $FF to the masked sub-table
//    bus.map_to_masked_sub(0, 0xFF, ted_sub);
//
//    // When TED toggles rom_enabled:
//    bus.set_masked_base(0, ted_sub, ram_chip_ff, WriteChipId(ram_chip_ff));
//
// ── Recursive sub-tables (multi-level) ──────────────────────────────────────
//
//  Sub-table entries use the same chip-id encoding as the main page table.
//  An entry can be a direct chip, kNoChipSelected, an MMIO sentinel, or
//  another sub-table sentinel.  This enables arbitrary recursion:
//
//    // Level 0: page $D → indexed sub-table 0  (16 entries, bits 11-8)
//    // Level 1: entry 5 of sub-table 0 → masked sub-table 0
//    // Level 2: masked sub-table 0 → base chip or region-specific MMIO
//
//    const int lvl1 = bus.add_indexed_sub_table(0, 4, 8);  // 16 entries
//    const int lvl2 = bus.add_masked_sub_table(0, base_rd, base_wr);
//    bus.add_masked_region(0, lvl2, ...);
//
//    // Point indexed entry 5 to the masked sub-table
//    bus.set_indexed_entry(0, lvl1, 5,
//                          MemoryBus<Spec>::masked_sub_chip(lvl2),
//                          MemoryBus<Spec>::masked_sub_write_chip(lvl2));
//
//    // Point page $D to the first-level indexed sub-table
//    bus.map_to_indexed_sub(0, 0xD, lvl1);
//
// ── Decision guide: where does the bitmix belong? ────────────────────────────
//
//  EnablePartialBus is the right tool ONLY when:
//    • The narrow chip is directly chip-selected (chip_id < kNoChipSelected)
//    • No register-file handler sits in front of the address range
//    • The chip is not fetched by a dedicated side-channel
//
//  Otherwise apply bitmix at the call site (handler or chip fetch routine).
//
// ── C64 colour RAM (MOS 2114) — bitmix NOT on the bus ────────────────────────
//
//  The 2114 at $D800–$DBFF only drives D0–D3.  Both access paths use bitmix
//  at the call site, not via EnablePartialBus:
//
//  CPU path (I/O sub-table → ColorRAM MMIO handler):
//    bus_state_t colram_read(void* ctx, bus_state_t bus) noexcept {
//        const uint16_t addr = BUS_GET_ADDR(bus);
//        const uint8_t nibble = colram[addr & 0x03FFu] & 0x0Fu;
//        BUS_BITMIX_DATA(bus, nibble, uint8_t(0x0F));
//        return bus;
//    }
//
//  VIC-II path (internal fetch, not through MemoryBus):
//    void vic_fetch_colour(Vic& vic, uint16_t vc) {
//        vic.colour_latch = colram[vc & 0x03FFu] & 0x0Fu;
//    }
//
// ── Apple I RAM (MOS 2114) — bitmix IS on the bus ────────────────────────────
//
//  The Apple I 2114 pairs are directly chip-selected (no handler):
//    MemoryBus<Generic8BitBusSpec> bus;
//    bus.set_chip_data_mask(kRamCsLo, 0x0Fu);  // D0–D3
//    bus.set_chip_data_mask(kRamCsHi, 0xF0u);  // D4–D7
//
// ── Snapshot save/load does not include sub-tables ───────────────────────────
//
//  Snapshots capture main-level chip tables only.  Sub-table configurations
//  are secondary decode logic, orthogonal to PLA bank switching.  Update
//  sub-table base chips (e.g. set_masked_base) explicitly when needed.
//
// ── CS-enabled workflow: resolve → chip tick → service ───────────────────────
//
//  When CsLineBits > 0, the bus supports a two-phase access model that
//  mirrors real hardware address decode → chip /CS assertion:
//
//  Phase 1 — Address decode (once per bus cycle):
//    bus = mem_bus.resolve(viewer_id, bus);
//
//    resolve() reads the address from bus, performs the page-table lookup
//    (including sub-table resolution), and embeds the terminal chip id in
//    the bus_state_t CS field.  No data transfer happens.
//
//  Phase 2 — Chip ticks (each chip, every cycle):
//    void my_chip_tick(bus_state_t& bus) {
//        advance_internal_counters();   // always, regardless of CS
//        if (NesCpuBus::get_cs_from_bus(bus) != MY_CHIP_ID) return;
//
//        // Selected: handle the bus access
//        if (BUS_GET_BIT(bus, BUS_RW_BIT))
//            BUS_SET_DATA(bus, registers_[BUS_GET_ADDR(bus) & reg_mask]);
//        else
//            registers_[BUS_GET_ADDR(bus) & reg_mask] = BUS_GET_DATA(bus);
//    }
//
//    MMIO-only chips (SID, CIA, PPU regs, …) handle register access
//    directly — they know their own register layout.
//
//    Buffer-backed chips (RAM, ROM) call service_read/service_write instead:
//
//    void ram_tick(NesCpuBus& mem_bus, bus_state_t& bus) {
//        if (NesCpuBus::get_cs_from_bus(bus) != MY_CHIP_ID) return;
//        bus = BUS_GET_BIT(bus, BUS_RW_BIT)
//            ? mem_bus.service_read(bus)
//            : mem_bus.service_write(bus);
//    }
//
//    service_read/service_write extract the chip id from the CS field (already
//    embedded by resolve) and perform the flat-mem transfer.  This
//    avoids a redundant page-table lookup.
//
//  Non-CS systems (CsLineBits absent or 0) use the callback workflow:
//    bus = mem_bus.tick(viewer_id, bus);
//    — performs page lookup, buffer access, and MMIO handler dispatch.
//
// ── Bus floating (NES open-bus behaviour) ────────────────────────────────────
//
//  On the NES (and similar), undriven data lines float toward VCC over time.
//  Model this with a pre-read float step:
//
//    // Simple: all data bits → 1 before every read
//    BUS_FLOAT_DATA_HIGH(bus);
//    bus = mem_bus.resolve(viewer_id, bus);
//
//    // Gradual decay via LFSR (more accurate, passes NES test ROMs):
//    lfsr = lfsr16_step(lfsr);
//    BUS_FLOAT_DATA_DECAY_HIGH(bus, uint8_t(lfsr));
//    bus = mem_bus.resolve(viewer_id, bus);
//
//  Reads from connected chips overwrite the floated data.  Reads from
//  unconnected addresses (kNoChipSelected) leave the floated bits intact,
//  modelling the impedance-driven open-bus behaviour.
//
// =============================================================================
