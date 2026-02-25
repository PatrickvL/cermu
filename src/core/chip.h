#pragma once

#include "cermu.h"  // Compiler compatibility macros
#include <cstdint>
#include <memory>
#include "system_lines.h"

// ============================================================================
// VIDEO STANDARD — TV broadcast standard affecting chip timing
// ============================================================================

/// Video broadcast standard — determines system-wide timing
/// (video, audio, CPU clock).  Stored in ChipIdentity so each chip
/// instance can report which standard it is configured for.
enum class VideoStandard : uint8_t {
    NTSC,           // NTSC (North America, Japan)
    PAL,            // PAL (Europe, Australia)
    PAL_M,          // PAL-M (Brazil)
    SECAM,          // SECAM (France, Eastern Europe)
    CUSTOM          // Custom/configurable timing
};

// ============================================================================
// CHIP IDENTITY — intrinsic metadata about a chip type
// ============================================================================

/// Static identity of a chip type (part number, manufacturer, video standard).
/// Returned by ChipBase::chip_identity().
///
/// The standard field indicates which video-standard variant this chip instance
/// represents.  Standard-agnostic chips (I/O, RAM, ...) leave it at the
/// default (NTSC).  Zero runtime cost: the enum is returned by value
/// together with the two pointers that were already there.
struct ChipIdentity {
    const char* part_number;    // e.g. "MOS6526", "TED7360", "RP2A03"
    const char* manufacturer;   // e.g. "MOS Technology", "Ricoh", "Commodore"
    VideoStandard standard = VideoStandard::NTSC; // Video standard this instance is configured for
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

#ifdef IMGUI_VERSION
    // Bus state snapshot for layout pin rendering.
    // Assigned at the end of each system tick so that GUI code can read
    // the most recent bus state without coupling to the emulation loop.
    bus_state_t bus_snapshot_ = 0;
#endif
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

