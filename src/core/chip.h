#pragma once

#include "cermu.h"  // Compiler compatibility macros
#include <cstdint>
#include <memory>
#include "system_lines.h"

// ============================================================================
// CHIP IDENTITY — intrinsic metadata about a chip type
// ============================================================================

/// Static identity of a chip type (part number + manufacturer).
/// Returned by ChipBase::chip_identity().
struct ChipIdentity {
    const char* part_number;    // e.g. "MOS6526", "TED7360", "RP2A03"
    const char* manufacturer;   // e.g. "MOS Technology", "Ricoh", "Commodore"
};

// ============================================================================
// CHIP BASE — abstract interface for all emulated chips
// ============================================================================
///
/// Every chip registered in a system implements this interface, either directly
/// (native C++ chips) or through a ChipPlaceholder (identity-only entries).
///
/// Provides:
///   - Identity:  what chip type this is
///   - GUI:       debug and settings window rendering (optional)
///
class ChipBase {
public:
    virtual ~ChipBase() = default;

    // --- Identity ---
    virtual ChipIdentity chip_identity() const = 0;

    // --- GUI content rendering (optional — defaults to nothing) ---
    // Content-only: renders chip info WITHOUT ImGui::Begin/End window framing.
    // The caller (Hardware menu submenu or detached window) provides the window.
    virtual bool has_debug_content() const { return false; }
    virtual bool has_settings_content() const { return false; }
    virtual bool has_layout_content() const { return false; }
    virtual void render_debug_content() {}
    virtual void render_settings_content() {}
    virtual void render_layout_content() {}
};

// ============================================================================
// CHIP PLACEHOLDER — identity-only chip entry (no GUI, no emulation state)
// ============================================================================
///
/// Lightweight ChipBase for chips that appear in the Hardware menu
/// but have no debug/settings/layout GUI.  Typical for RAM, ROM, and
/// other passive components whose emulation is handled elsewhere.
///
class ChipPlaceholder : public ChipBase {
    ChipIdentity identity_;
public:
    explicit ChipPlaceholder(ChipIdentity identity)
        : identity_(identity) {}
    ChipIdentity chip_identity() const override { return identity_; }
    // All has_*() default to false from ChipBase — nothing to override.
};

