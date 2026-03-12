#pragma once

#include <cstdint>

#include "core/chip.h"

// Forward declare C64System
class C64System;

// ============================================================================
// PlaChip — ChipBase wrapper for the C64 PLA
// ============================================================================
/// The PLA's GUI needs access to the whole C64System (banking tables, bus
/// state) rather than just the pla_906114_01_t struct (which is ephemeral and
/// only used during map generation).  PlaChip holds a C64System* back-pointer
/// and implements ChipBase so the PLA can be registered directly.
///
class PlaChip : public ChipBase {
    C64System* c64_;
public:
    explicit PlaChip(C64System* c64)
        : ChipBase(ChipInfo{"PLA", "MOS Technology"}), c64_(c64) {
        display_name_ = "PLA / Address Decoder";
        short_name_   = "PLA";
        category_     = "Bus";
#ifdef CERMU_HAS_CHIP_DEBUG
        register_debug_fields();
#endif
    }

#ifdef CERMU_HAS_GUI
    bool has_settings_content() const override { return true; }

    void render_debug_content() override;  // Complex interactive banking tables — kept as override
    void render_settings_content() override;
    ChipLayout* create_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

private:
#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};
