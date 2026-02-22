#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../../core/chip.h"

// Forward declare c64_t (alias for C64SystemData)
struct C64SystemData;
using c64_t = C64SystemData;

// ============================================================================
// PlaChip — ChipBase wrapper for the C64 PLA
// ============================================================================
/// The PLA's GUI needs access to the whole c64_t system (banking tables, bus
/// state) rather than just the pla_906114_01_t struct (which is ephemeral and
/// only used during map generation).  PlaChip holds a c64_t* back-pointer and
/// implements ChipBase so the PLA can be registered directly.
///
class PlaChip : public ChipBase {
    c64_t* c64_;
public:
    explicit PlaChip(c64_t* c64) : c64_(c64) {}

    ChipIdentity chip_identity() const override { return {"PLA", "MOS Technology"}; }
    bool has_debug_content() const override { return true; }
    bool has_settings_content() const override { return true; }
    bool has_layout_content() const override { return true; }

    void render_debug_content() override;
    void render_settings_content() override;
    void render_layout_content() override;
};


