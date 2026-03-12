#pragma once

#include "core/chip.hpp"

// ============================================================================
// CPU CHIP BASE — intermediate base for all CPU/processor chips
// ============================================================================
//
// Shared foundation for CPU chips (6502 family, Z80 family, 6809, ...).
// Sets category_ = "CPU" so subtypes don't need to.

class CpuChipBase : public ChipBase {
public:
    CpuChipBase() { category_ = "CPU"; }
    explicit CpuChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "CPU"; }
};
