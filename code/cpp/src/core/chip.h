#pragma once

#include "aiemuc.h"  // Compiler compatibility macros
#include <cstdint>
#include <functional>
#include <memory>
#include "system_lines.h"

// Forward declarations
class ChipBase;

// Modern C++ callback type using std::function for type safety
using chip_callback_t = std::function<bus_state_t(void* chip, bus_state_t bus_state)>;

// Chip descriptor - modernized with C++ features
struct ChipDescriptor {
    const char* description;  // Human-readable description for debugging
    std::unique_ptr<ChipBase> (*create)(ChipDescriptor* desc);
    std::function<void(void* chip, void* bus)> bus_attach;
    std::function<void(void* chip, std::uint8_t bank)> bank_change;
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    std::function<void(void* chip, bool* show_window)> render_debug_window; // Optional GUI debug window callback
    std::function<void(void* chip, bool* show_window)> render_settings_window; // Optional GUI settings window callback
#endif
};

// Chip registry entry - modernized
struct ChipEntry {
    std::unique_ptr<ChipBase> chip;
    std::unique_ptr<ChipDescriptor> desc;
    std::size_t size;
    std::uint16_t base_address;
    std::uint8_t chip_id;
};

// Base class for all chips - provides common interface
class ChipBase {
public:
    virtual ~ChipBase() = default;
    virtual bus_state_t read(bus_state_t bus_state) = 0;
    virtual bus_state_t write(bus_state_t bus_state) = 0;
    virtual void reset() = 0;
    virtual void attach_bus(void* bus) {}
    virtual void bank_change(std::uint8_t bank) {}
    
#ifdef CIMGUI_DEFINE_ENUMS_AND_STRUCTS
    virtual void render_debug_window(bool* show_window) {}
    virtual void render_settings_window(bool* show_window) {}
#endif
};

// ============================================================================
// LEGACY COMPATIBILITY TYPEDEFS - FOR TRANSITION PERIOD ONLY
// ============================================================================

// Legacy compatibility typedefs to allow gradual transition
using chip_descriptor_t = ChipDescriptor;
using chip_entry_t = ChipEntry;