#pragma once

#include "../../core/chip.h"

// ============================================================================
// MEMORY CHIP BASE — intermediate base for all memory chips
// ============================================================================
//
// Shared foundation for memory chips (MOS 2114 color RAM, generic
// MemoryChip for RAM/ROM placeholders, ...).
// Sets category_ = "Memory" so subtypes don't need to.

class MemoryChipBase : public ChipBase {
public:
    MemoryChipBase() { category_ = "Memory"; }
    explicit MemoryChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "Memory"; }
};
