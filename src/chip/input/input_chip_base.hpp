#pragma once

#include "core/chip.hpp"

// ============================================================================
// INPUT CHIP BASE — intermediate base for all input/shift-register chips
// ============================================================================
//
// Shared foundation for input chips (CD4021 shift register, ...).
// Sets category_ = "Input" so subtypes don't need to.

class InputChipBase : public ChipBase {
public:
    InputChipBase() { category_ = "Input"; }
    explicit InputChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "Input"; }
};
