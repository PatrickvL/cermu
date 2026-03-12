#pragma once

#include "chip/memory/memory_chip_base.h"

struct ChipSlot;  // Forward declaration for factory

// ============================================================================
// ROMChip — read-only memory (ROM, PROM, EPROM)
// ============================================================================
//
// Thin subclass of MemoryChipBase.  Overrides is_read_only() to always
// return true, signalling the bus memory system to omit write-page mappings.
//
// Usage:
//   auto rom = std::make_unique<ROMChip>(
//       ChipInfo{"MOS 901227-03", "Commodore"}, 8192, ROMChip::ROM, &bus_state,
//       "KERNAL", 0xE000);
//
class ROMChip : public MemoryChipBase {
public:
    using MemoryChipBase::MemoryChipBase;
    bool is_read_only() const override { return true; }

    /// Factory for Board::create_chips() — creates a ROMChip from a
    /// manifest slot, binds it to the unified buffer, and returns it.
    static ChipBase* create_from_slot(const ChipSlot& slot,
                                      const bus_state_t* system_bus,
                                      uint8_t* buffer);
};
