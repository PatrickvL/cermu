#pragma once

#include "core/cermu.hpp"  // Compiler compatibility macros
#include "core/chip_debug_registry.hpp"  // Always included: DECL_EXTRACT
                                  // macros are used at file scope in chip headers
                                  // and need RegEntry/FieldEntry types even in
                                  // non-debug builds.  All data is constexpr —
                                  // zero runtime overhead.

#include "core/component_base.hpp"
#include "core/component_info.hpp"
#include <cstdint>
#include <memory>
#include <vector>
#include "core/system_lines.hpp"

// Forward declarations for layout support
struct ChipLayout;
struct PinSignalState;

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
class ChipBase : public ComponentBase {
public:
    ChipBase() = default;
    explicit ChipBase(ChipInfo info) : info_(std::move(info)) {}
    ~ChipBase() override = default;

    // Movable (needed by MemoryChipBase), non-copyable (inherited from ComponentBase).
    ChipBase(ChipBase&&) noexcept = default;
    ChipBase& operator=(ChipBase&&) noexcept = default;

    // --- ComponentBase interface ---
    const char* name() const override { return display_name(); }

    // --- Identity (non-virtual — data lives here, not in subclasses) ---
    const ChipInfo& chip_info() const { return info_; }

    // --- Registration metadata (how this chip appears in a specific system) ---
    // These fields describe the chip's *placement* in a system: its role name,
    // category for menu grouping, and memory-mapped base address.
    // For owned chips (RAMChip, ChipPlaceholder, PlaChip) these are set in the
    // constructor.  For borrowed chips (CPU, VIA, etc.) the system sets them via
    // the register_chip() overload that accepts placement arguments.
    const char* display_name() const { return display_name_ ? display_name_ : info_.part_number.data(); }
    const char* short_name()   const { return short_name_   ? short_name_   : info_.part_number.data(); }
    const char* category()     const { return category_     ? category_     : ""; }
    uint16_t    base_address() const { return base_address_; }

    // --- GUI content rendering (optional — defaults to nothing) ---
    // Content-only: renders chip info WITHOUT ImGui::Begin/End window framing.
    // The caller (Hardware menu submenu or detached window) provides the window.
#ifdef CERMU_HAS_GUI
    virtual bool has_debug_content() const { return !debug_registry_.empty(); }
    virtual bool has_settings_content() const { return false; }
    virtual void render_debug_content();   // default: two-column layout + registry
    virtual void render_settings_content() {}

    // --- Chip layout support ---
    // Override create_chip_layout() and get_layout_pin_states() to enable chip
    // visualization.  The base render_layout_content() calls these virtuals —
    // individual chips no longer need to override it.
    //
    // get_chip_layout() is a lazy-init accessor: it calls create_chip_layout()
    // once and caches the result.  Chips that need dynamic layouts (e.g.
    // RAMChip with varying pin count) can override get_chip_layout() directly.
    virtual ChipLayout* get_chip_layout() const;
    virtual std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout);
    virtual const char* get_layout_chip_name() const;
    bool has_layout_content() const { return get_chip_layout() != nullptr; }
    virtual void render_layout_content();
#endif

    // --- Debug field registry (semantic data description) ---
    // Chips populate this at construction time.  The renderer reads it.
#ifdef CERMU_HAS_CHIP_DEBUG
    ChipDebugRegistry debug_registry_;
#endif

    // --- Bus chip identity (assigned by Board during bind) ---
    uint16_t bus_chip_id() const { return bus_chip_id_; }
    void set_bus_chip_id(uint16_t id) { bus_chip_id_ = id; }

    // --- Placement metadata setters (used by Board::create_chips) ---
    void set_display_name(const char* name) { display_name_ = name; }
    void set_short_name(const char* name) { short_name_ = name; }
    void set_base_address(uint16_t addr) { base_address_ = addr; }

    // --- Chip reset (opt-in via override) ---
    // Called by Board::reset_chips() during system reset.
    // Chips with internal state override this to clear registers, timers, etc.
    // CPUs use a separate pin-based reset protocol and leave this as no-op.
    // reset() is inherited from ComponentBase with an empty default.

    // --- Bus MMIO interface (opt-in via override) ---
    // Chips that handle register-file access on the memory bus override these.
    // The default returns false / passes bus through unchanged.
    virtual bool has_mmio() const { return false; }
    virtual bus_state_t on_bus_read (bus_state_t bus) noexcept { return bus; }
    virtual bus_state_t on_bus_write(bus_state_t bus) noexcept { return bus; }

    // --- Memory properties (opt-in via override) ---
    // RAMChip and ROM chips override this to return true.
    virtual bool is_read_only() const { return false; }

    // Bus state snapshot — stores the bus state at the end of each tick.
    // Used by the emulation loop for edge detection (A12, NMI) and
    // open-bus decay, and by GUI code for layout pin rendering.
    bus_state_t bus_snapshot_ = 0;

    // ========================================================================
    // CONSOLIDATED REGISTER STORAGE
    // ========================================================================
    // All register-bearing chips use this inline array instead of declaring
    // their own.  Derived constructors call init_regs() or init_split_regs()
    // to set num_regs_.  Chips with separate read/write address spaces
    // (e.g. TIA) store write registers in regs_[0..num_regs_-1] and read
    // registers in regs_[num_regs_..num_regs_+num_read_regs_-1], with
    // read_regs_ pointing to the read region.
    //
    // 128 bytes covers all current chips (max: VIC-II at 66 + TIA at 59).
    static constexpr uint16_t MAX_CHIP_REGS = 128;
    uint8_t  regs_[MAX_CHIP_REGS] = {};     // Primary register file
    uint16_t num_regs_ = 0;                 // Active write-register count
    uint8_t* read_regs_ = nullptr;          // Separate read register view (nullptr → reads from regs_)
    uint16_t num_read_regs_ = 0;            // Read register count (0 if shared with regs_)

protected:
    ChipInfo info_;

    // --- Register storage initializers (call from derived constructors) ---

    /// Configure a single register file (most chips).
    void init_regs(uint16_t count) {
        num_regs_ = count;
    }

    /// Configure split read/write register files (e.g. TIA).
    /// Write registers occupy regs_[0..write_count-1],
    /// read registers occupy regs_[write_count..write_count+read_count-1].
    void init_split_regs(uint16_t write_count, uint16_t read_count) {
        num_regs_      = write_count;
        num_read_regs_ = read_count;
        read_regs_     = &regs_[write_count];
    }

#ifdef CERMU_HAS_GUI
    // Override to create the chip's package layout (pin diagram).
    // Called once by get_chip_layout() and cached.  For variant-aware chips
    // (PAL/NTSC), call invalidate_layout() when the variant changes.
    virtual ChipLayout* create_chip_layout() const { return nullptr; }
    void invalidate_layout() const { layout_initialized_ = false; layout_ = nullptr; }
#endif

    // Registration metadata — set by derived constructors or by register_chip()
    uint16_t bus_chip_id_ = 0xFFFF;        // MemoryBus chip id (0xFFFF = unassigned)

    // Registration metadata — set by derived constructors or by register_chip()
    const char* display_name_ = nullptr;  // "VIA 1 (MOS 6522)" — full UI label
    const char* short_name_   = nullptr;  // "VIA 1" — compact label
    const char* category_     = nullptr;  // "CPU", "Video", "Audio", "I/O", "Memory"
    uint16_t    base_address_ = 0;        // Memory-mapped base address (0 if N/A)

private:
#ifdef CERMU_HAS_GUI
    mutable ChipLayout* layout_ = nullptr;
    mutable bool layout_initialized_ = false;
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

