#pragma once

#include "core/chip.hpp"

// ============================================================================
// MMU CHIP BASE — intermediate base for memory management unit chips
// ============================================================================
//
// Shared foundation for MMU chips (MOS 8722 C128 MMU, ...).
// Sets category_ = "MMU" so subtypes don't need to.

class MmuChipBase : public ChipBase {
public:
    MmuChipBase() { category_ = "MMU"; }
    explicit MmuChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "MMU"; }
};
