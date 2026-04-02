#include "chip/memory/memory_chip_base.hpp"
#include "core/chip_layout.hpp"
#include "core/chip_manifest.hpp"
#include "core/chip_registry.hpp"
#include "core/pin_macros.hpp"
#ifdef CERMU_HAS_GUI
#include "gui/chip_visualization.hpp"
#endif
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

// ============================================================================
// Helpers
// ============================================================================

const char* MemoryChipBase::type_label(MemoryType t) {
    switch (t) {
        case RAM:   return "DRAM";
        case SRAM:  return "SRAM";
        case ROM:   return "ROM";
        case PROM:  return "PROM";
        case EPROM: return "EPROM";
    }
    return "Memory";
}

static std::string format_bytes(size_t bytes) {
    if (bytes >= 1024 * 1024)
        return std::to_string(bytes / (1024 * 1024)) + "MB";
    if (bytes >= 1024)
        return std::to_string(bytes / 1024) + "KB";
    return std::to_string(bytes) + "B";
}

// ============================================================================
// MemoryChipBase — lifecycle
// ============================================================================

MemoryChipBase::MemoryChipBase(ChipInfo     info,
                               size_t       size_bytes,
                               MemoryType   type,
                               const bus_state_t* system_bus,
                               const char*  short_name,
                               uint16_t     base_address)
    : ChipBase(std::move(info))
    , size_bytes_(size_bytes)
    , type_(type)
    , system_bus_(system_bus)
{
    category_     = "Memory";
    short_name_   = short_name;
    base_address_ = base_address;

    // Allocate zero-filled storage
    if (size_bytes_ > 0) {
        data_ = static_cast<uint8_t*>(calloc(1, size_bytes_));
        owns_data_ = true;
    }

    // Auto-generate display name: "{part_number} [{type}] ({size})"
    // Skip type label if part_number already contains it to avoid
    // redundancies like "SRAM SRAM (2KB)".
    const char* label = type_label(type);
    std::string size_str = format_bytes(size_bytes);
    std::string pn(info_.part_number);

    if (pn.find(label) != std::string::npos) {
        display_name_buf_ = pn + " (" + size_str + ")";
    } else {
        display_name_buf_ = pn + " " + label + " (" + size_str + ")";
    }
    display_name_ = display_name_buf_.c_str();

#ifdef CERMU_HAS_CHIP_DEBUG
    register_debug_fields();
#endif
}

MemoryChipBase::~MemoryChipBase() {
    if (owns_data_) {
        free(data_);
    }
    data_ = nullptr;
}

MemoryChipBase::MemoryChipBase(MemoryChipBase&& other) noexcept
    : ChipBase(std::move(other))
    , data_(other.data_)
    , owns_data_(other.owns_data_)
    , size_bytes_(other.size_bytes_)
    , type_(other.type_)
    , system_bus_(other.system_bus_)
    , display_name_buf_(std::move(other.display_name_buf_))
{
    other.data_ = nullptr;
    other.owns_data_ = false;
    display_name_ = display_name_buf_.c_str();
}

MemoryChipBase& MemoryChipBase::operator=(MemoryChipBase&& other) noexcept {
    if (this != &other) {
        if (owns_data_) free(data_);

        ChipBase::operator=(std::move(other));
        data_             = other.data_;
        owns_data_        = other.owns_data_;
        size_bytes_       = other.size_bytes_;
        type_             = other.type_;
        system_bus_       = other.system_bus_;
        display_name_buf_ = std::move(other.display_name_buf_);

        other.data_ = nullptr;
        other.owns_data_ = false;
        display_name_ = display_name_buf_.c_str();
    }
    return *this;
}

// ============================================================================
// External buffer management
// ============================================================================

void MemoryChipBase::bind(uint8_t* external) {
    if (data_ && data_ != external) {
        // Preserve existing data by copying to the target buffer
        memcpy(external, data_, size_bytes_);
        if (owns_data_) free(data_);
    }
    data_ = external;
    owns_data_ = false;
}

void MemoryChipBase::on_bind_buffer(uint8_t* buffer, size_t size,
                                    const char* label, uint16_t base) {
    // For default-constructed value-type chips: set size and metadata first.
    // For factory-constructed chips: size is already set, only bind() is needed.
    if (size_bytes_ == 0) {
        size_bytes_ = size;
        if (label) { set_display_name(label); set_short_name(label); }
        set_base_address(base);
    }
    bind(buffer);
}

void MemoryChipBase::release() {
    if (owns_data_) free(data_);
    data_ = nullptr;
    owns_data_ = false;
}

// ============================================================================
// Generic memory chip layout — built from address/data pin counts
// ============================================================================
//
// Standard memory chip pinout convention (DIP):
//   Left side:  address pins (low bits), power ground
//   Right side: address pins (high bits), data pins, control, power VCC
//
// Address pins needed:  ceil(log2(size_bytes))
// Data pins:            8 for byte-wide, 4 for nibble-wide (MOS2114)
//
// Common real-world pinouts this mirrors:
//   2KB  SRAM  (6116):  DIP-24  —  11 addr, 8 data, CE/OE/WE, VCC/GND
//   8KB  ROM   (2364):  DIP-24  —  13 addr, 8 data, CE/OE, VCC/GND
//   8KB  SRAM  (6264):  DIP-28  —  13 addr, 8 data, CE/OE/WE, VCC/GND
//   32KB SRAM  (62256): DIP-28  —  15 addr, 8 data, CE/OE/WE, VCC/GND
//   64KB DRAM  (4164):  DIP-16  —  8 addr (muxed), 1 data, RAS/CAS/WE, VCC/GND
//
static ChipLayout create_memory_layout(size_t size_bytes,
                                       MemoryChipBase::MemoryType type) {
    // Determine address pin count
    int addr_bits = 0;
    if (size_bytes > 0) {
        // ceil(log2(size)) — number of address pins needed
        size_t s = size_bytes - 1;
        while (s > 0) { addr_bits++; s >>= 1; }
    }
    if (addr_bits == 0) addr_bits = 1;

    const int data_bits = 8;  // Byte-wide

    // Control pins: CE, OE always present; WE for RAM only
    const int control_pins = (type == MemoryChipBase::RAM ||
                              type == MemoryChipBase::SRAM) ? 3 : 2; // CE+OE+WE or CE+OE
    const int power_pins = 2; // VCC + GND
    const int total_signal_pins = addr_bits + data_bits + control_pins + power_pins;
    const int total_pins = (total_signal_pins + 1) & ~1; // Round up to even for DIP
    const int pins_per_side = total_pins / 2;

    // Select empty DIP layout by pin count (rounded up to standard DIP size)
    ChipLayout layout;
    if (total_pins <= 16)       layout = create_dip16_layout();
    else if (total_pins <= 18)  layout = create_dip18_layout();
    else if (total_pins <= 20)  layout = create_dip20_layout();
    else if (total_pins <= 24)  layout = create_dip24_layout();
    else if (total_pins <= 28)  layout = create_dip28_layout();
    else                        layout = create_dip40_layout();

    // Determine memory type string for display
    const char* type_str = "Memory";
    switch (type) {
        case MemoryChipBase::RAM:   type_str = "DRAM";  break;
        case MemoryChipBase::SRAM:  type_str = "SRAM";  break;
        case MemoryChipBase::ROM:   type_str = "ROM";   break;
        case MemoryChipBase::PROM:  type_str = "PROM";  break;
        case MemoryChipBase::EPROM: type_str = "EPROM"; break;
    }

    layout.markings = {
        type_str,                    // package_variant
        {} // custom_text
    };

    // --- Build pin assignments ---
    // Standard memory chip convention:
    //   Pin 1 to pins_per_side:   left side (top to bottom)
    //   Pin total_pins down to pins_per_side+1: right side (bottom to top in DIP standard,
    //                                           but we push top-to-bottom for display)

    uint8_t pin = 1;

    // Left side: address pins A0..A(n-1) starting from pin 1, then GND at bottom
    int left_addr_pins = pins_per_side - 1; // Reserve 1 for GND
    for (int i = 0; i < left_addr_pins && i < addr_bits; i++) {
        PinLabel lbl = static_cast<PinLabel>(static_cast<int>(PinLabel::A0) + i);
        layout.left_pins.push_back(make_pin(pin, lbl));
        pin++;
    }
    // GND at bottom of left side
    layout.left_pins.push_back(CHIP_PIN(pin, VSS));
    pin++;

    // Right side: built bottom-up (DIP convention: pin pins_per_side+1 is at bottom-right)
    // We'll build an array and reverse it for the display vector.
    //
    // Right side layout (top to bottom for display):
    //   VCC (top)
    //   remaining address pins (if any)
    //   data pins D7..D0
    //   control: OE, WE (RAM only), CE (bottom, near GND counterpart)

    // Top of right side: VCC
    uint8_t right_top_pin = total_pins;
    layout.right_pins.push_back(CHIP_PIN(right_top_pin, VDD));

    uint8_t rpin = right_top_pin - 1;

    // Remaining address pins on right side
    int right_addr_start = left_addr_pins;
    for (int i = right_addr_start; i < addr_bits; i++) {
        PinLabel lbl = static_cast<PinLabel>(static_cast<int>(PinLabel::A0) + i);
        layout.right_pins.push_back(make_pin(rpin, lbl));
        rpin--;
    }

    // Data pins D7..D0 (top to bottom = D7 first)
    for (int i = data_bits - 1; i >= 0 && rpin >= pin; i--) {
        PinLabel lbl = static_cast<PinLabel>(static_cast<int>(PinLabel::D0) + i);
        layout.right_pins.push_back(make_pin(rpin, lbl));
        rpin--;
    }

    // Control pins at bottom of right side
    if (type == MemoryChipBase::RAM || type == MemoryChipBase::SRAM) {
        // WE, OE, CE from bottom up
        if (rpin >= pin) {
            layout.right_pins.push_back(CHIP_PIN(rpin, _WE));
            rpin--;
        }
    }
    if (rpin >= pin) {
        layout.right_pins.push_back(CHIP_PIN(rpin, _OE));
        rpin--;
    }
    if (rpin >= pin) {
        layout.right_pins.push_back(CHIP_PIN(rpin, _CS));
        rpin--;
    }

    // Fill any remaining right-side pins as NC
    while (rpin >= pin) {
        layout.right_pins.push_back(CHIP_PIN(rpin, NC));
        rpin--;
    }

    return layout;
}

// ============================================================================
// Layout virtuals — generic memory chip DIP diagram
// ============================================================================

#ifdef CERMU_HAS_GUI

ChipLayout* MemoryChipBase::get_chip_layout() const {
    // Prefer subclass custom layout (e.g. MOS2114's hardware-accurate DIP-18)
    ChipLayout* custom = create_chip_layout();
    if (custom) return custom;

    // Fallback: generic layout derived from size_bytes_ and type_
    static thread_local ChipLayout* cached_layout = nullptr;
    static thread_local size_t cached_size = 0;
    static thread_local MemoryType cached_type = RAM;

    if (!cached_layout || cached_size != size_bytes_ || cached_type != type_) {
        static thread_local ChipLayout layout_storage;
        layout_storage = create_memory_layout(size_bytes_, type_);
        layout_storage.chip_info = &info_;
        cached_layout = &layout_storage;
        cached_size = size_bytes_;
        cached_type = type_;
    }

    return cached_layout;
}

std::vector<PinSignalState> MemoryChipBase::get_layout_pin_states(ChipLayout& layout) {
    // Snapshot bus state at render time (passive chip, no tick)
    if (system_bus_) {
        bus_snapshot_ = *system_bus_;
    }

    return populate_pin_states_from_bus(layout, bus_snapshot_);
}

#endif // CERMU_HAS_GUI

#ifdef CERMU_HAS_CHIP_DEBUG
void MemoryChipBase::register_debug_fields() {
    static const char* const type_names[] = {"RAM", "ROM", "PROM", "EPROM", "SRAM"};
    using M = const MemoryChipBase;

    debug_registry_
        .category("Identity")
        .state("Type", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<M*>(c)->type_); },
               type_names, 5)
        .value("Size (bytes)", +[](const ChipBase* c) -> uint32_t { return static_cast<uint32_t>(static_cast<M*>(c)->size_bytes_); })
        .flag("Bound", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->is_bound(); })
        .flag("Owns Data", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->owns_data(); });

    if (base_address_ != 0) {
        debug_registry_.address("Base Address", +[](const ChipBase* c) -> uint32_t { return static_cast<M*>(c)->base_address_; });
    }

    debug_registry_
        .category("Contents")
        .memory("Data",
                +[](const ChipBase* c) -> std::pair<const uint8_t*, size_t> {
                    auto* self = static_cast<M*>(c);
                    return {self->data_, self->size_bytes_};
                },
                base_address_, 256);
}
#endif // CERMU_HAS_CHIP_DEBUG

// ============================================================================
// Slot factories — used by Board::create_chips()
// ============================================================================
// Defined here rather than in the thin subclass headers to keep ChipSlot
// (and its host chip_manifest.hpp) out of the header dependency graph.

#include "chip/memory/ram_chip.hpp"
#include "chip/memory/rom_chip.hpp"

ChipBase* RAMChip::create_from_slot(const ChipSlot& slot,
                                     const bus_state_t* system_bus,
                                     uint8_t* buffer) {
    auto* chip = new RAMChip(
        ChipInfo{"SRAM", "", {}}, slot.size_bytes, SRAM, system_bus,
        slot.label, static_cast<uint16_t>(slot.base_addr));
    if (buffer) chip->bind(buffer);
    return chip;
}

ChipBase* ROMChip::create_from_slot(const ChipSlot& slot,
                                     const bus_state_t* system_bus,
                                     uint8_t* buffer) {
    auto* chip = new ROMChip(
        ChipInfo{"ROM", "", {}}, slot.size_bytes, ROM, system_bus,
        slot.label, static_cast<uint16_t>(slot.base_addr));
    if (buffer) chip->bind(buffer);
    return chip;
}

REGISTER_CHIP("RAM", &RAMChip::create_from_slot)
REGISTER_CHIP("ROM", &ROMChip::create_from_slot)
