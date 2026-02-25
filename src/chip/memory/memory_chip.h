#pragma once

#include "../../core/chip.h"
#include <cstddef>
#include <string>

// ============================================================================
// MemoryChip — generic ChipBase wrapper for RAM/ROM chips
// ============================================================================
//
// Provides a renderable chip for passive memory components (DRAM, SRAM, ROM,
// PROM, EPROM) that don't tick but participate in the bus.  The chip snapshots
// the system's bus state at render time so pin signals reflect current activity.
//
// MemoryChip is self-describing: it auto-generates its display_name from the
// ChipInfo part_number, memory type, and size.  Category is always "Memory".
//
// Usage:
//   register_chip(std::make_unique<MemoryChip>(
//       ChipInfo{"4164", "Various"}, 65536, MemoryChip::RAM, &bus_state,
//       "RAM", 0x0000));
//
class MemoryChip : public ChipBase {
public:
    enum MemoryType { RAM, ROM, PROM, EPROM, SRAM };

    /// Construct a MemoryChip.
    /// \param info         Chip identity (part number, manufacturer, etc.)
    /// \param size_bytes   Memory capacity in bytes
    /// \param type         RAM or ROM variant
    /// \param system_bus   Pointer to the live bus_state_t (borrowed, must
    ///                     outlive this chip).  Used to snapshot at render time.
    /// \param short_name   System-specific role label ("RAM", "BASIC", "KERNAL")
    ///                     — nullptr defaults to info.part_number
    /// \param base_address Memory-mapped base address (0 if N/A)
    MemoryChip(ChipInfo     info,
               size_t       size_bytes,
               MemoryType   type,
               const bus_state_t* system_bus,
               const char*  short_name   = nullptr,
               uint16_t     base_address = 0);

    ~MemoryChip() override = default;

    // --- ChipBase interface ---
    bool has_layout_content()   const override { return true; }
    void render_layout_content()       override;

    // --- Accessors ---
    size_t     size_bytes()  const { return size_bytes_; }
    MemoryType memory_type() const { return type_; }

    /// Human-readable label for the memory type enum value.
    static const char* type_label(MemoryType t);

private:
    size_t             size_bytes_;
    MemoryType         type_;
    const bus_state_t* system_bus_;   // Borrowed pointer to system bus state
    std::string        display_name_buf_;  // Owned storage for auto-generated display name
};
