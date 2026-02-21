#pragma once

#include "cermu.h"  // Compiler compatibility macros
#include <cstdint>
#include <functional>
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
/// (native C++ chips) or through a CChipAdapter wrapper (legacy C-struct chips).
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

    // --- GUI capabilities (optional — defaults to no windows) ---
    virtual bool has_debug_window() const { return false; }
    virtual bool has_settings_window() const { return false; }
    virtual void render_debug_window(bool* show) { (void)show; }
    virtual void render_settings_window(bool* show) { (void)show; }
};

// ============================================================================
// C-STRUCT CHIP ADAPTER — wraps legacy C-struct chips as ChipBase
// ============================================================================
///
/// Non-owning adapter: the raw chip pointer's lifetime is managed by the system
/// class that created it.  Rendering callbacks are captured as std::function so
/// lambdas can close over extra context (chip pointer, titles, etc.).
///
class CChipAdapter : public ChipBase {
    void* chip_ptr_;
    ChipIdentity identity_;
    std::function<void(bool*)> debug_fn_;
    std::function<void(bool*)> settings_fn_;
public:
    CChipAdapter(void* chip, ChipIdentity identity,
                 std::function<void(bool*)> debug_fn = nullptr,
                 std::function<void(bool*)> settings_fn = nullptr)
        : chip_ptr_(chip)
        , identity_(identity)
        , debug_fn_(std::move(debug_fn))
        , settings_fn_(std::move(settings_fn)) {}

    ChipIdentity chip_identity() const override { return identity_; }
    bool has_debug_window() const override { return !!debug_fn_; }
    bool has_settings_window() const override { return !!settings_fn_; }
    void render_debug_window(bool* show) override { if (debug_fn_) debug_fn_(show); }
    void render_settings_window(bool* show) override { if (settings_fn_) settings_fn_(show); }

    /// Access the underlying C-struct chip pointer.
    void* raw_chip() const { return chip_ptr_; }
};

// ============================================================================
// LEGACY TYPES — kept for backward compatibility during transition
// ============================================================================

// Modern C++ callback type using std::function for type safety
using chip_callback_t = std::function<bus_state_t(void* chip, bus_state_t bus_state)>;

/// Chip descriptor - C-style vtable.
/// DEPRECATED: prefer ChipBase virtual methods for new code.
/// Existing C-struct chips still store a pointer to one of these.
struct ChipDescriptor {
    const char* description;  // Human-readable description for debugging
    void* (*create)(ChipDescriptor* desc);  // Create chip instance
    void (*destroy)(void* chip);            // Destroy chip instance
    void (*bus_attach)(void* chip, void* bus);     // Bus attachment (nullable)
    void (*bank_change)(void* chip, std::uint8_t bank);  // Bank change (nullable)
#ifdef IMGUI_VERSION
    void (*render_debug_window)(void* chip, bool* show_window);   // GUI debug window (nullable)
    void (*render_settings_window)(void* chip, bool* show_window); // GUI settings window (nullable)
#endif
};

/// Chip registry entry for legacy System8Bit.
/// DEPRECATED: use SystemChip in EmulatedSystem instead.
struct ChipEntry {
    void* chip;                    // Raw pointer for compatibility
    ChipDescriptor* desc;          // Raw pointer for compatibility
    std::size_t size;
    std::uint16_t base_address;
    std::uint8_t chip_id;
};

// Legacy compatibility typedefs
using chip_descriptor_t = ChipDescriptor;
using chip_entry_t = ChipEntry;