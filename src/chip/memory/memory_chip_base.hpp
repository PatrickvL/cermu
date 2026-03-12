#pragma once

#include "core/chip.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// ============================================================================
// MEMORY CHIP BASE — intermediate base for all memory chips
// ============================================================================
//
// Shared foundation for memory chips (RAMChip, ROMChip, MOS 2114 color RAM).
// Each MemoryChipBase owns a byte buffer that backs the emulated memory
// region.  It is also a ChipBase so it can be registered in the Hardware menu
// and rendered with a pin layout.
//
// **Storage ownership:**
//   * By default the constructor allocates (calloc) the buffer — `owns_data_`
//     is true and the destructor frees it.
//   * `bind(ptr)` redirects the chip's data pointer to an external buffer
//     (e.g. a unified memory buffer for cache-friendly access).  Any owned
//     data is copied to the target first, then freed.  After binding,
//     `owns_data_` is false.
//   * `release()` drops the data pointer (freeing if owned) without
//     reallocating — useful when unmapping a memory region.
//
// Subtypes: RAMChip (writable), ROMChip (read-only), MOS2114 (color RAM
//           with its own inline storage).
//

class MemoryChipBase : public ChipBase {
public:
    enum MemoryType { RAM, ROM, PROM, EPROM, SRAM };

    /// Construct a MemoryChipBase with allocated (zero-filled) storage.
    /// \param info         Chip identity (part number, manufacturer, etc.)
    /// \param size_bytes   Memory capacity in bytes
    /// \param type         RAM or ROM variant
    /// \param system_bus   Pointer to the live bus_state_t (borrowed, must
    ///                     outlive this chip).  Used to snapshot at render time.
    /// \param short_name   System-specific role label ("RAM", "BASIC", "KERNAL")
    ///                     — nullptr defaults to info.part_number
    /// \param base_address Memory-mapped base address (0 if N/A)
    MemoryChipBase(ChipInfo     info,
                   size_t       size_bytes,
                   MemoryType   type,
                   const bus_state_t* system_bus,
                   const char*  short_name   = nullptr,
                   uint16_t     base_address = 0);

    ~MemoryChipBase() override;

    // Non-copyable, movable
    MemoryChipBase(const MemoryChipBase&) = delete;
    MemoryChipBase& operator=(const MemoryChipBase&) = delete;
    MemoryChipBase(MemoryChipBase&& other) noexcept;
    MemoryChipBase& operator=(MemoryChipBase&& other) noexcept;

    // --- ChipBase interface ---
#ifdef CERMU_HAS_GUI
    ChipLayout* get_chip_layout() const override;
    std::vector<PinSignalState> get_layout_pin_states(ChipLayout& layout) override;
#endif

    // --- Data access -------------------------------------------------
    uint8_t*       data()       { return data_; }
    const uint8_t* data() const { return data_; }

    uint8_t& operator[](size_t i)       { return data_[i]; }
    uint8_t  operator[](size_t i) const { return data_[i]; }

    // --- External buffer management ----------------------------------

    /// Bind this chip's storage to an external buffer.
    /// If the chip currently owns data, it is copied into \p external first
    /// and then freed.  After this call `owns_data()` returns false.
    void bind(uint8_t* external);

    /// Release the data pointer.  Frees owned memory and sets the pointer
    /// to nullptr.  Use when un-mapping a region (e.g. cartridge removal).
    void release();

    /// True when data() points to an externally-owned buffer.
    bool is_bound() const { return data_ != nullptr && !owns_data_; }

    /// True when this chip owns the allocation behind data().
    bool owns_data() const { return owns_data_; }

    // --- Metadata accessors ------------------------------------------
    size_t     size_bytes()  const { return size_bytes_; }
    MemoryType memory_type() const { return type_; }

    /// Human-readable label for the memory type enum value.
    static const char* type_label(MemoryType t);

protected:
    // Simple constructors for subclasses that manage their own storage
    // (e.g. MOS2114 color RAM).
    MemoryChipBase() { category_ = "Memory"; }
    explicit MemoryChipBase(ChipInfo info) : ChipBase(std::move(info)) { category_ = "Memory"; }

private:
    uint8_t*           data_ = nullptr;      // Byte storage for emulated memory
    bool               owns_data_ = false;   // True → destructor frees data_
    size_t             size_bytes_ = 0;
    MemoryType         type_ = RAM;
    const bus_state_t* system_bus_ = nullptr; // Borrowed pointer to system bus state
    std::string        display_name_buf_;     // Owned storage for auto-generated display name

#ifdef CERMU_HAS_CHIP_DEBUG
    void register_debug_fields();
#endif
};
