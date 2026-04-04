#pragma once
// pla_banking.hpp — Shared C64-family PLA banking snapshot generator
//
// Generates pre-computed MemoryBus snapshots from the C64 PLA (906114-01)
// memory decode logic.  Used by:
//   - C64System:  PLA-driven banking (32 modes × 2 viewers)
//   - C128System: C64e compatibility mode (same PLA equations, different chip IDs)
//
// The PLA has 5 banking inputs: LORAM, HIRAM, CHAREN, EXROM, GAME.
// Combined these yield 32 modes.  For each mode and each 4 KB page the
// PLA selects one of 8 output signals: RAM, BASIC, KERNAL, CHARROM,
// I/O, ROML, ROMH, or unmapped.
//
// Systems provide a PlaChipMapping that translates PLA outputs to their
// bus-specific chip IDs.  The generator probes the PLA for every
// mode × page × viewer and saves snapshots for O(1) mode switching.

#include "chip/logic/pla.hpp"
#include "core/memory_bus/bus.hpp"
#include <array>
#include <cstdint>

// =============================================================================
// PLA output abstraction — independent of any system's manifest chip IDs
// =============================================================================

/// The 8 possible PLA output states (active-low chip-select lines).
enum class PlaOutput : uint8_t {
    ram,        // /CASRAM — main RAM
    basic,      // /BASIC  — BASIC ROM
    kernal,     // /KERNAL — KERNAL ROM
    charrom,    // /CHAROM — character ROM
    io,         // /IO     — I/O page ($D000)
    roml,       // /ROML   — cartridge ROM low
    romh,       // /ROMH   — cartridge ROM high
    unmapped,   // no output asserted — open bus
    count
};

/// Number of PLA banking modes (5 bits: LORAM, HIRAM, CHAREN, EXROM, GAME).
inline constexpr size_t kNumPlaModes = 32;

/// Convert PLA906114 output signals to the abstract PlaOutput enum.
/// Priority matches hardware signal routing (active-low, priority decode).
inline PlaOutput pla_decode_output(const PLA906114& pla) {
    if (!pla.outputs().n_casram)  return PlaOutput::ram;
    if (!pla.outputs().n_basic)   return PlaOutput::basic;
    if (!pla.outputs().n_kernal)  return PlaOutput::kernal;
    if (!pla.outputs().n_io)      return PlaOutput::io;
    if (!pla.outputs().n_charrom) return PlaOutput::charrom;
    if (!pla.outputs().n_roml)    return PlaOutput::roml;
    if (!pla.outputs().n_romh)    return PlaOutput::romh;
    return PlaOutput::unmapped;
}


// =============================================================================
// PLA → bus chip-ID mapping — parameterized per system
// =============================================================================

/// Maps each PlaOutput to a bus-specific chip ID for page-table entries.
/// Systems construct one of these with their manifest-derived chip IDs.
template<typename BusSpec>
struct PlaChipMapping {
    using Bus     = MemoryBus<BusSpec>;
    using PT      = PackingTraits<BusSpec>;
    using ChipId  = typename PT::ChipId;
    using WriteId = typename PT::WriteChipId;

    /// Read chip descriptor: base ID + bank mask for multi-page chips.
    /// For a 4 KB page at index `bank`, the chip ID is:
    ///   ChipId(size_t(base) + (bank & mask))
    struct ReadEntry {
        ChipId  base;
        uint8_t mask;   // (num_4k_banks - 1), or 0 for single-bank chips
    };

    ReadEntry ram;
    ReadEntry basic;
    ReadEntry kernal;
    ReadEntry charrom;
    ReadEntry roml;
    ReadEntry romh;

    // I/O sub-table sentinels (from Bus::indexed_sub_chip(0))
    ChipId  io_sub_read;
    WriteId io_sub_write;

    // No-chip sentinels (from PT::kNoChipSelected)
    ChipId  no_chip_read;
    WriteId no_chip_write;

    // Write mapping — only RAM and I/O accept writes
    WriteId ram_write_base;
    uint8_t ram_write_mask;

    /// Map a PLA output + 4 KB page index to a read chip ID.
    ChipId map_read(PlaOutput out, uint32_t bank) const {
        switch (out) {
            case PlaOutput::ram:      return ChipId(size_t(ram.base) + (bank & ram.mask));
            case PlaOutput::basic:    return ChipId(size_t(basic.base) + (bank & basic.mask));
            case PlaOutput::kernal:   return ChipId(size_t(kernal.base) + (bank & kernal.mask));
            case PlaOutput::charrom:  return ChipId(size_t(charrom.base) + (bank & charrom.mask));
            case PlaOutput::roml:     return ChipId(size_t(roml.base) + (bank & roml.mask));
            case PlaOutput::romh:     return ChipId(size_t(romh.base) + (bank & romh.mask));
            case PlaOutput::io:       return io_sub_read;
            case PlaOutput::unmapped: return no_chip_read;
            default:                  return no_chip_read;
        }
    }

    /// Map a PLA output + 4 KB page index to a write chip ID.
    /// Only RAM and I/O are writable; all other outputs return no-chip.
    WriteId map_write(PlaOutput out, uint32_t bank) const {
        switch (out) {
            case PlaOutput::ram: return WriteId(size_t(ram_write_base) + (bank & ram_write_mask));
            case PlaOutput::io:  return io_sub_write;
            default:             return no_chip_write;
        }
    }
};


// =============================================================================
// Optional debug info — raw PLA output per mode/page
// =============================================================================

/// Collects raw PlaOutput values for every mode × page combination.
/// Pass to generate_pla_mode_snapshots() for debug GUI support (C64 PLA viewer).
/// Pass nullptr if not needed.
struct PlaDebugInfo {
    PlaOutput cpu_read[kNumPlaModes][16];
    PlaOutput cpu_write[kNumPlaModes][16];
    PlaOutput vic_read[kNumPlaModes][16];
};


// =============================================================================
// Snapshot generator
// =============================================================================

/// Generate all 32 PLA-mode snapshots for CPU and VIC-II viewers.
///
/// For each of the 32 banking modes (5-bit: LORAM, HIRAM, CHAREN, EXROM, GAME):
///   - CPU viewer: probes each 4 KB page for both read and write chip selection.
///   - VIC-II viewer: probes each 4 KB page for read only (VIC doesn't write via PLA).
///
/// Results are saved into the provided snapshot arrays for O(1) mode_switch().
///
/// @param bus          MemoryBus to temporarily program and snapshot.
/// @param mapping      PLA output → bus chip ID translation table.
/// @param cpu_viewer   Viewer index for the CPU (typically 0).
/// @param vic_viewer   Viewer index for VIC-II/VIC-IIe (typically 1).
/// @param cpu_snaps    Output: 32 CPU viewer snapshots.
/// @param vic_snaps    Output: 32 VIC-II viewer snapshots.
/// @param debug        Optional: raw PLA output per mode/page for debug GUIs.
template<typename BusSpec>
void generate_pla_mode_snapshots(
    MemoryBus<BusSpec>& bus,
    const PlaChipMapping<BusSpec>& mapping,
    size_t cpu_viewer,
    size_t vic_viewer,
    std::array<ModeSnapshot<BusSpec>, kNumPlaModes>& cpu_snaps,
    std::array<ModeSnapshot<BusSpec>, kNumPlaModes>& vic_snaps,
    PlaDebugInfo* debug = nullptr)
{
    PLA906114 pla;

    for (int mode = 0; mode < (int)kNumPlaModes; mode++) {
        pla.set_banking_mode((uint8_t)mode);

        // ── CPU viewer ──────────────────────────────────────────────
        pla.inputs().n_cas = false;
        bus.reset_viewer(cpu_viewer);

        for (uint32_t bank = 0; bank < 16; bank++) {
            bus_state_t pla_bus = 0;
            BUS_SET_ADDR(pla_bus, bank << 12);
            BUS_SET_BIT(pla_bus, BUS_AEC_BIT);
            BUS_SET_BIT(pla_bus, BUS_BA_BIT);

            // Read: R/W high
            BUS_SET_BIT(pla_bus, BUS_RW_BIT);
            pla.tick(pla_bus);
            PlaOutput read_chip = pla_decode_output(pla);

            // Write: R/W low
            BUS_CLR_BIT(pla_bus, BUS_RW_BIT);
            pla.tick(pla_bus);
            PlaOutput write_chip = pla_decode_output(pla);

            bus.set_page(cpu_viewer, bank,
                         mapping.map_read(read_chip, bank),
                         mapping.map_write(write_chip, bank));

            if (debug) {
                debug->cpu_read[mode][bank]  = read_chip;
                debug->cpu_write[mode][bank] = write_chip;
            }
        }
        bus.save_snapshot(cpu_viewer, cpu_snaps[mode]);

        // ── VIC-II viewer ───────────────────────────────────────────
        pla.inputs().n_cas = false;
        bus.reset_viewer(vic_viewer);

        for (uint32_t bank = 0; bank < 16; bank++) {
            pla.inputs().va12   = (bank & 0x01) != 0;
            pla.inputs().va13   = (bank & 0x02) != 0;
            pla.inputs().n_va14 = (bank & 0x04) == 0;

            bus_state_t pla_bus = 0;
            BUS_SET_ADDR(pla_bus, bank << 12);
            BUS_SET_BIT(pla_bus, BUS_RW_BIT);
            pla.tick(pla_bus);
            PlaOutput read_chip = pla_decode_output(pla);

            // VIC-II only reads — set read page, write stays no-chip
            bus.set_read_page(vic_viewer, bank,
                              mapping.map_read(read_chip, bank));

            if (debug) {
                debug->vic_read[mode][bank] = read_chip;
            }
        }
        bus.save_snapshot(vic_viewer, vic_snaps[mode]);
    }
}
