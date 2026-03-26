#pragma once

#include "core/chip.hpp"

// ============================================================================
// LOGIC CHIP BASE — intermediate base for all discrete logic chips
// ============================================================================
//
// Shared foundation for logic chips (74LS259 addressable latch, 74LS138
// decoder, 74LS245 bus transceiver, PLA, ...).
// Sets category_ = "Logic" so subtypes don't need to.

class LogicChipBase : public ChipBase {
public:
    LogicChipBase() { category_ = "Logic"; }
    explicit LogicChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "Logic"; }
};
