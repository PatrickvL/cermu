#pragma once

#include "cermu.h"  // Compiler compatibility macros
#include "component_info.h"
#include <cstdint>
#include <memory>
#include "system_lines.h"

// ============================================================================
// VIDEO STANDARD — TV broadcast standard affecting system timing
// ============================================================================

/// Video broadcast standard — determines system-wide timing
/// (video, audio, CPU clock).  Lives on SystemTiming, NOT on chip identity.
enum class VideoStandard : uint8_t {
    NTSC,           // NTSC (North America, Japan)
    PAL,            // PAL (Europe, Australia)
    PAL_M,          // PAL-M (Brazil)
    SECAM,          // SECAM (France, Eastern Europe)
    CUSTOM          // Custom/configurable timing
};

// ============================================================================
// CHIP BASE — base class for all emulated chips
// ============================================================================
///
/// Every chip registered in a system derives from this, either directly
/// (native C++ chips) or through a ChipPlaceholder (identity-only entries).
///
/// Provides:
///   - Identity:  ChipInfo stored here (part number, manufacturer, …)
///   - GUI:       debug and settings window rendering (optional)
///
/// ChipInfo lives in ChipBase so derived classes don't duplicate it.
/// Derived classes pass their identity to the base constructor (or assign
/// to the protected info_ after construction for late-bound cases like
/// PAL/NTSC chip variants).
///
class ChipBase {
public:
    ChipBase() = default;
    explicit ChipBase(ChipInfo info) : info_(std::move(info)) {}
    virtual ~ChipBase() = default;

    // --- Identity (non-virtual — data lives here, not in subclasses) ---
    const ChipInfo& chip_info() const { return info_; }

    // --- Registration metadata (how this chip appears in a specific system) ---
    // These fields describe the chip's *placement* in a system: its role name,
    // category for menu grouping, and memory-mapped base address.
    // For owned chips (MemoryChip, ChipPlaceholder, PlaChip) these are set in the
    // constructor.  For borrowed chips (CPU, VIA, etc.) the system sets them via
    // the register_chip() overload that accepts placement arguments.
    const char* display_name() const { return display_name_ ? display_name_ : info_.part_number.data(); }
    const char* short_name()   const { return short_name_   ? short_name_   : info_.part_number.data(); }
    const char* category()     const { return category_     ? category_     : ""; }
    uint16_t    base_address() const { return base_address_; }

    // --- GUI content rendering (optional — defaults to nothing) ---
    // Content-only: renders chip info WITHOUT ImGui::Begin/End window framing.
    // The caller (Hardware menu submenu or detached window) provides the window.
    virtual bool has_debug_content() const { return false; }
    virtual bool has_settings_content() const { return false; }
    virtual bool has_layout_content() const { return false; }
    virtual void render_debug_content() {}
    virtual void render_settings_content() {}
    virtual void render_layout_content() {}

    // Bus state snapshot — stores the bus state at the end of each tick.
    // Used by the emulation loop for edge detection (A12, NMI) and
    // open-bus decay, and by GUI code for layout pin rendering.
    bus_state_t bus_snapshot_ = 0;

protected:
    ChipInfo info_;

    // Registration metadata — set by derived constructors or by register_chip()
    const char* display_name_ = nullptr;  // "VIA 1 (MOS 6522)" — full UI label
    const char* short_name_   = nullptr;  // "VIA 1" — compact label
    const char* category_     = nullptr;  // "CPU", "Video", "Audio", "I/O", "Memory"
    uint16_t    base_address_ = 0;        // Memory-mapped base address (0 if N/A)
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
public:
    explicit ChipPlaceholder(ChipInfo info)
        : ChipBase(std::move(info)) {}

    /// Self-describing placeholder — carries its own registration metadata.
    ChipPlaceholder(ChipInfo info, const char* display_name,
                    const char* short_name, const char* category,
                    uint16_t base_address = 0)
        : ChipBase(std::move(info))
    {
        display_name_ = display_name;
        short_name_   = short_name;
        category_     = category;
        base_address_ = base_address;
    }

    // All has_*() default to false from ChipBase — nothing to override.
};

