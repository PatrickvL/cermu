#pragma once

#include "../../core/chip.h"

// ============================================================================
// SOUND CHIP BASE — intermediate base for all sound/audio chips
// ============================================================================
//
// Shared foundation for sound chips (MOS 6581 SID, NES APU, AY-3-8910,
// SN76489, Namco WSG, ...).
// Sets category_ = "Sound" so subtypes don't need to.

class SoundChipBase : public ChipBase {
public:
    SoundChipBase() { category_ = "Sound"; }
    explicit SoundChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "Sound"; }
};
