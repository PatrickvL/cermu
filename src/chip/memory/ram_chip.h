#pragma once

#include "memory_chip_base.h"

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
};
