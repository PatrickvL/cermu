#pragma once

#include "../../core/chip.h"
#include <cstddef>

// ============================================================================
// MemoryChip — generic ChipBase wrapper for RAM/ROM chips
// ============================================================================
//
// Provides a renderable chip for passive memory components (DRAM, SRAM, ROM,
// PROM, EPROM) that don't tick but participate in the bus.  The chip snapshots
// the system's bus state at render time so pin signals reflect current activity.
//
// Usage:
//   auto* chip = new MemoryChip("4164", "NEC", 65536, MemoryChip::RAM,
//                               &system_bus_state);
//   register_chip(chip, "RAM (64KB)", "RAM", "Memory", 0x0000);
//
class MemoryChip : public ChipBase {
public:
    enum MemoryType { RAM, ROM, PROM, EPROM, SRAM };

    /// Construct a MemoryChip.
    /// \param part_number  Chip part number for display (e.g. "4164", "2332")
    /// \param manufacturer Chip manufacturer
    /// \param size_bytes   Memory capacity in bytes
    /// \param type         RAM or ROM variant
    /// \param system_bus   Pointer to the live bus_state_t (borrowed, must
    ///                     outlive this chip).  Used to snapshot at render time.
    MemoryChip(const char* part_number,
               const char* manufacturer,
               size_t      size_bytes,
               MemoryType  type,
               const bus_state_t* system_bus);

    ~MemoryChip() override = default;

    // --- ChipBase interface ---
    ChipIdentity chip_identity() const override;
    bool has_layout_content()   const override { return true; }
    void render_layout_content()       override;

private:
    const char*        part_number_;
    const char*        manufacturer_;
    size_t             size_bytes_;
    MemoryType         type_;
    const bus_state_t* system_bus_;   // Borrowed pointer to system bus state
};
