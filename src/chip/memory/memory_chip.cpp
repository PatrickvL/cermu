#include "memory_chip.h"
#include "../../core/chip_layout.h"
#include "../../core/pin_macros.h"
#ifdef CERMU_HAS_GUI
#include "../../gui/chip_visualization.h"
#endif
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

// ============================================================================
// Helpers
// ============================================================================

const char* MemoryChip::type_label(MemoryType t) {
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
// MemoryChip — lifecycle
// ============================================================================

MemoryChip::MemoryChip(ChipInfo     info,
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
    short_name_   = short_name;
    category_     = "Memory";
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

#ifdef CERMU_HAS_GUI
    register_debug_fields();
#endif
}

MemoryChip::~MemoryChip() {
    if (owns_data_) {
        free(data_);
    }
    data_ = nullptr;
}

MemoryChip::MemoryChip(MemoryChip&& other) noexcept
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

MemoryChip& MemoryChip::operator=(MemoryChip&& other) noexcept {
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

void MemoryChip::bind(uint8_t* external) {
    if (data_ && data_ != external) {
        // Preserve existing data by copying to the target buffer
        memcpy(external, data_, size_bytes_);
        if (owns_data_) free(data_);
    }
    data_ = external;
    owns_data_ = false;
}

void MemoryChip::release() {
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
                                       MemoryChip::MemoryType type,
                                       std::string_view part_number,
                                       std::string_view manufacturer) {
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
    const int control_pins = (type == MemoryChip::RAM ||
                              type == MemoryChip::SRAM) ? 3 : 2; // CE+OE+WE or CE+OE
    const int power_pins = 2; // VCC + GND
    const int total_signal_pins = addr_bits + data_bits + control_pins + power_pins;
    const int total_pins = (total_signal_pins + 1) & ~1; // Round up to even for DIP
    const int pins_per_side = total_pins / 2;

    // Select DIP base layout by pin count
    ChipLayout layout;
    if (total_pins <= 16)       layout = create_dip16_layout();
    else if (total_pins <= 18)  layout = create_dip18_layout();
    else if (total_pins <= 20)  layout = create_dip20_layout();
    else if (total_pins <= 24)  layout = create_dip24_layout();
    else if (total_pins <= 28)  layout = create_dip28_layout();
    else                        layout = create_dip40_layout();

    layout.left_pins.clear();
    layout.right_pins.clear();

    // Determine memory type string for display
    const char* type_str = "Memory";
    switch (type) {
        case MemoryChip::RAM:   type_str = "DRAM";  break;
        case MemoryChip::SRAM:  type_str = "SRAM";  break;
        case MemoryChip::ROM:   type_str = "ROM";   break;
        case MemoryChip::PROM:  type_str = "PROM";  break;
        case MemoryChip::EPROM: type_str = "EPROM"; break;
    }

    layout.markings = {
        part_number,                 // part_number
        manufacturer,                // manufacturer
        type_str,                    // package_variant
        {},                          // date_code
        {},                          // lot_number
        {},                          // custom_text
        true,                        // show_part_number
        !manufacturer.empty(),       // show_manufacturer
        true,                        // show_package_variant
        false                        // show_date_code
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
    layout.left_pins.push_back(PIN(pin, VSS));
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
    layout.right_pins.push_back(PIN(right_top_pin, VDD));

    uint8_t rpin = right_top_pin - 1;

    // Remaining address pins on right side
    int right_addr_start = left_addr_pins;
    for (int i = right_addr_start; i < addr_bits; i++) {
        PinLabel lbl = static_cast<PinLabel>(static_cast<int>(PinLabel::A0) + i);
        layout.right_pins.push_back(make_pin(rpin, lbl));
        rpin--;
    }

    // Data pins D7..D0 (top to bottom = D7 first)
    for (int i = data_bits - 1; i >= 0 && rpin > pin; i--) {
        PinLabel lbl = static_cast<PinLabel>(static_cast<int>(PinLabel::D0) + i);
        layout.right_pins.push_back(make_pin(rpin, lbl));
        rpin--;
    }

    // Control pins at bottom of right side
    if (type == MemoryChip::RAM || type == MemoryChip::SRAM) {
        // WE, OE, CE from bottom up
        if (rpin > pin) {
            layout.right_pins.push_back(PIN(rpin, _WE));
            rpin--;
        }
    }
    if (rpin > pin) {
        layout.right_pins.push_back(PIN(rpin, _OE));
        rpin--;
    }
    if (rpin > pin) {
        layout.right_pins.push_back(PIN(rpin, _CS));
        rpin--;
    }

    // Fill any remaining right-side pins as NC
    while (rpin > pin) {
        layout.right_pins.push_back(PIN(rpin, NC));
        rpin--;
    }

    return layout;
}

// ============================================================================
// Layout virtuals — generic memory chip DIP diagram
// ============================================================================

#ifdef CERMU_HAS_GUI

ChipLayout* MemoryChip::get_chip_layout() const {
    static thread_local ChipLayout* cached_layout = nullptr;
    static thread_local size_t cached_size = 0;
    static thread_local MemoryType cached_type = RAM;

    // Rebuild layout if parameters changed (typically stable after first call)
    if (!cached_layout || cached_size != size_bytes_ || cached_type != type_) {
        static thread_local ChipLayout layout_storage;
        layout_storage = create_memory_layout(size_bytes_, type_,
                                              info_.part_number,
                                              info_.manufacturer);
        cached_layout = &layout_storage;
        cached_size = size_bytes_;
        cached_type = type_;
    }

    return cached_layout;
}

std::vector<PinSignalState> MemoryChip::get_layout_pin_states(ChipLayout& layout) {
    // Snapshot bus state at render time (passive chip, no tick)
    if (system_bus_) {
        bus_snapshot_ = *system_bus_;
    }

    return populate_pin_states_from_bus(layout, bus_snapshot_);
}

#endif // CERMU_HAS_GUI

void MemoryChip::register_debug_fields() {
    static const char* const type_names[] = {"RAM", "ROM", "PROM", "EPROM", "SRAM"};
    using M = const MemoryChip;

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
