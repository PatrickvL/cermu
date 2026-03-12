#pragma once

#include "chip/memory/memory_chip_base.h"

struct ChipSlot;  // Forward declaration for factory

// ============================================================================
// RAMChip — read/write memory (DRAM, SRAM)
// ============================================================================
//
// Thin subclass of MemoryChipBase.  is_read_only() returns false (inherited
// from ChipBase default).
//
// Usage:
//   auto ram = std::make_unique<RAMChip>(
//       ChipInfo{"4164", "Various"}, 65536, RAMChip::RAM, &bus_state,
//       "RAM", 0x0000);
//
class RAMChip : public MemoryChipBase {
public:
    using MemoryChipBase::MemoryChipBase;

    /// Factory for Board::create_chips() — creates a RAMChip from a
    /// manifest slot, binds it to the unified buffer, and returns it.
    static ChipBase* create_from_slot(const ChipSlot& slot,
                                      const bus_state_t* system_bus,
                                      uint8_t* buffer);
};
