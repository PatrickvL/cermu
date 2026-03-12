#pragma once

#include "core/chip.h"

// ============================================================================
// I/O CHIP BASE — intermediate base for all I/O controller chips
// ============================================================================
//
// Shared foundation for I/O chips (MOS 6522 VIA, MOS 6526 CIA, PIA 6532,
// PIA 6820, Intel 8255, Z80 PIO, Z80 CTC, ...).
// Sets category_ = "I/O" so subtypes don't need to.

class IoChipBase : public ChipBase {
public:
    IoChipBase() { category_ = "I/O"; }
    explicit IoChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "I/O"; }
};
