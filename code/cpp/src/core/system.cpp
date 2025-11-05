#include "system.h"
#include <iostream>
#include <memory>

#ifdef _MSC_VER
#pragma message("Compiling with MSVC")
#endif
#ifdef _M_X64
#pragma message("Targeting x64")
#endif
#ifdef _M_IX86
#pragma message("Targeting x86")
#endif
#ifdef __GNUC__
#pragma message("Compiling with GCC")
#endif
#ifdef __clang__
#pragma message("Compiling with Clang")
#endif
#ifdef __x86_64__
#pragma message("Detected x86_64")
#endif
#ifdef __i386__
#pragma message("Detected i386")
#endif

// ============================================================================
// Modern C++ System8Bit Implementation
// ============================================================================

System8Bit::~System8Bit() {
    destroy_all_chips();
}

std::uint8_t System8Bit::register_chip(std::unique_ptr<ChipBase> chip,
                                      std::unique_ptr<ChipDescriptor> desc,
                                      std::uint16_t base_address,
                                      std::size_t size) {
    if (chip_count_ >= MAX_CHIPS) {
        return 0xFF; // Error: too many chips
    }
    
    std::uint8_t id = chip_count_++;
    ChipEntry& entry = chips_[id];
    
    entry.chip = std::move(chip);
    entry.desc = std::move(desc);
    entry.chip_id = id;
    entry.base_address = base_address;
    entry.size = size;
    
    return id;
}

ChipBase* System8Bit::get_chip(std::uint8_t chip_id) {
    if (chip_id >= chip_count_) {
        return nullptr;
    }
    return chips_[chip_id].chip.get();
}

const ChipBase* System8Bit::get_chip(std::uint8_t chip_id) const {
    if (chip_id >= chip_count_) {
        return nullptr;
    }
    return chips_[chip_id].chip.get();
}

void System8Bit::destroy_all_chips() {
    // Modern C++ with RAII - chips are automatically destroyed
    for (std::uint8_t i = 0; i < chip_count_; ++i) {
        chips_[i].chip.reset();
        chips_[i].desc.reset();
    }
    chip_count_ = 0;
}

// ============================================================================
// Legacy C Interface for Compatibility
// ============================================================================

extern "C" {
    std::uint8_t system_chip_register(system_8bit_t* system, void* chip,
                                     ChipDescriptor* desc, std::uint16_t base,
                                     std::size_t size) {
        if (!system || !system->cpp_system) {
            return 0xFF;
        }
        
        // For legacy compatibility, we need to wrap the raw pointers
        // This is a temporary bridge during the conversion process
        auto chip_ptr = std::unique_ptr<ChipBase>(static_cast<ChipBase*>(chip));
        auto desc_ptr = std::unique_ptr<ChipDescriptor>(desc);
        
        return system->cpp_system->register_chip(std::move(chip_ptr),
                                                std::move(desc_ptr),
                                                base, size);
    }

    void system_chips_destroy(system_8bit_t* system) {
        if (system && system->cpp_system) {
            system->cpp_system->destroy_all_chips();
        }
    }
}
